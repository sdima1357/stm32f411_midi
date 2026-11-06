#include "synth.h"

#include <math.h>
#include <string.h>

#define SINE_TABLE_SIZE 1024
#define SINE_TABLE_MASK (SINE_TABLE_SIZE - 1)
#define PI_F 3.14159265358979323846f
#define DEFAULT_MASTER_VOLUME 0.55f
#define SYNTH_MAX_DRUM_VOICES 8
#define DECLICK_SAMPLES 32

typedef enum {
    ENV_OFF = 0,
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE
} EnvStage;

typedef struct {
    uint8_t active;
    uint8_t channel;
    uint8_t note;
    uint8_t method;
    uint8_t stage;
    uint8_t held;
    uint8_t sustain_held;
    uint8_t ignore_note_off;
    uint8_t mip;
    uint8_t is_drum;
    uint8_t declick;
    uint32_t age;

    float gain;
    float env;
    float env_attack_inc;
    float env_decay_coef;
    float env_sustain;
    float env_release_coef;

    float base_inc;
    float pitch_ratio;
    float phase;
    float phase_inc;

    float mod_phase;
    float last_mod;
    float fm_ratio;
    float fm_index;
    float fm_feedback;

    uint8_t frame_seg;
    float frame_frac;
    float frame_frac_inc;
    uint32_t wt_base0;
    uint32_t wt_base1;
    float declick_value;
    float last_out;
    const Instrument *inst;
} Voice;

static Voice voices[SYNTH_MAX_VOICES];
static uint32_t voice_age_counter;
static float master_volume = DEFAULT_MASTER_VOLUME;

static uint8_t channel_program[16];
static float channel_volume[16];
static float channel_expression[16];
static uint8_t channel_sustain[16];
static float channel_pitch_ratio[16];

static float sine_table[SINE_TABLE_SIZE + 1];

/* Frame morph times in ms, indexed by frame count. */
static const uint16_t wt_frame_times[6][5] = {
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {0, 200, 0, 0, 0},
    {0, 80, 300, 0, 0},
    {0, 30, 120, 400, 0},
    {0, 20, 80, 250, 700},
};

static inline float wrap01(float x)
{
    x -= (float)((int)x);
    if (x < 0.0f) {
        x += 1.0f;
    }
    return x;
}

static inline float sine_lookup(float phase)
{
    float x = wrap01(phase) * (float)SINE_TABLE_SIZE;
    int i = (int)x;
    float f = x - (float)i;
    i &= SINE_TABLE_MASK;
    return sine_table[i] + (sine_table[i + 1] - sine_table[i]) * f;
}

static inline float soft_clip(float x)
{
    if (x > 1.5f) {
        return 1.0f;
    }
    if (x < -1.5f) {
        return -1.0f;
    }
    return x - (4.0f / 27.0f) * x * x * x;
}

static void setup_env(Voice *v, const Instrument *inst)
{
    const float sr = (float)SYNTH_SAMPLE_RATE;
    float attack = (float)inst->attack_ms * 0.001f;
    float decay = (float)inst->decay_ms * 0.001f;
    float release = (float)inst->release_ms * 0.001f;

    v->env = 0.0f;
    v->stage = ENV_ATTACK;
    v->env_sustain = (float)inst->sustain * (1.0f / 255.0f);

    if (attack < 0.0005f) {
        v->env = 1.0f;
        v->stage = ENV_DECAY;
        v->env_attack_inc = 1.0f;
    } else {
        v->env_attack_inc = 1.0f / (attack * sr);
    }

    if (decay < 0.0005f || v->env_sustain >= 0.999f) {
        v->env_decay_coef = 0.0f;
    } else {
        v->env_decay_coef = expf(-5.0f / (decay * sr));
    }

    if (release < 0.0005f) {
        v->env_release_coef = 0.0f;
    } else {
        v->env_release_coef = expf(-5.0f / (release * sr));
    }
}

static inline float env_update(Voice *v)
{
    switch (v->stage) {
    case ENV_ATTACK:
        v->env += v->env_attack_inc;
        if (v->env >= 1.0f) {
            v->env = 1.0f;
            v->stage = ENV_DECAY;
        }
        break;
    case ENV_DECAY:
        if (v->env_decay_coef <= 0.0f) {
            v->env = v->env_sustain;
            v->stage = ENV_SUSTAIN;
        } else {
            v->env = v->env_sustain + (v->env - v->env_sustain) * v->env_decay_coef;
            if (v->env <= v->env_sustain + 0.0005f) {
                v->env = v->env_sustain;
                v->stage = ENV_SUSTAIN;
            }
        }
        break;
    case ENV_SUSTAIN:
        v->env = v->env_sustain;
        break;
    case ENV_RELEASE:
        if (v->env_release_coef <= 0.0f) {
            v->env = 0.0f;
        } else {
            v->env *= v->env_release_coef;
        }
        if (v->env < 0.0001f) {
            v->env = 0.0f;
            v->stage = ENV_OFF;
        }
        break;
    default:
        v->env = 0.0f;
        break;
    }
    return v->env;
}

static Voice *alloc_voice(int is_drum)
{
    int i;
    int drum_count = 0;
    Voice *best = 0;

    for (i = 0; i < SYNTH_MAX_VOICES; i++) {
        if (voices[i].active && voices[i].is_drum) {
            drum_count++;
        }
    }

    /*
     * Keep drums from eating the whole polyphony.  A new drum hit steals the
     * oldest drum once the drum pool is full.
     */
    if (is_drum && drum_count >= SYNTH_MAX_DRUM_VOICES) {
        for (i = 0; i < SYNTH_MAX_VOICES; i++) {
            if (voices[i].active && voices[i].is_drum) {
                if (!best || voices[i].age < best->age) {
                    best = &voices[i];
                }
            }
        }
        return best;
    }

    for (i = 0; i < SYNTH_MAX_VOICES; i++) {
        if (!voices[i].active) {
            return &voices[i];
        }
    }

    /* Prefer stealing a released voice with the lowest envelope. */
    for (i = 0; i < SYNTH_MAX_VOICES; i++) {
        if (voices[i].stage == ENV_RELEASE) {
            if (!best || voices[i].env < best->env) {
                best = &voices[i];
            }
        }
    }
    if (best) {
        return best;
    }

    /* A melodic note should steal a drum before another melodic voice. */
    if (!is_drum) {
        for (i = 0; i < SYNTH_MAX_VOICES; i++) {
            if (voices[i].active && voices[i].is_drum) {
                if (!best || voices[i].age < best->age) {
                    best = &voices[i];
                }
            }
        }
        if (best) {
            return best;
        }
    }

    /* Otherwise steal the oldest voice. */
    best = &voices[0];
    for (i = 1; i < SYNTH_MAX_VOICES; i++) {
        if (voices[i].age < best->age) {
            best = &voices[i];
        }
    }
    return best;
}

void synth_init(void)
{
    int i;
    master_volume = DEFAULT_MASTER_VOLUME;
    for (i = 0; i <= SINE_TABLE_SIZE; i++) {
        sine_table[i] = sinf(2.0f * PI_F * (float)i / (float)SINE_TABLE_SIZE);
    }
    synth_reset();
}

void synth_set_master_volume(float volume)
{
    if (volume < 0.0f) {
        volume = 0.0f;
    }
    if (volume > 2.0f) {
        volume = 2.0f;
    }
    master_volume = volume;
}

void synth_reset(void)
{
    int i;
    memset(voices, 0, sizeof(voices));
    voice_age_counter = 0;
    for (i = 0; i < 16; i++) {
        channel_program[i] = 0;
        channel_volume[i] = 1.0f;
        channel_expression[i] = 1.0f;
        channel_sustain[i] = 0;
        channel_pitch_ratio[i] = 1.0f;
    }
}

void synth_program_change(uint8_t channel, uint8_t program)
{
    if (channel < 16 && channel != 9) {
        channel_program[channel] = program & 127;
    }
}

void synth_note_on(uint8_t channel, uint8_t note, uint8_t velocity)
{
    const Instrument *inst;
    Voice *v;
    float freq;
    float vel_gain;
    float old_last = 0.0f;
    int was_active;
    int is_drum;

    if (channel > 15 || velocity == 0) {
        return;
    }

    is_drum = (channel == 9) ? 1 : 0;

    if (channel == 9) {
        if (note < GM_DRUM_FIRST || note >= GM_DRUM_FIRST + GM_NUM_DRUMS) {
            return;
        }
        inst = &gm_drums[note - GM_DRUM_FIRST];
        if (inst->method == 0xFF) {
            return;
        }
    } else {
        inst = &gm_instruments[channel_program[channel] & 127];
    }

    v = alloc_voice(is_drum);
    if (!v) {
        return;
    }
    was_active = v->active;
    if (was_active) {
        old_last = v->last_out;
    }
    memset(v, 0, sizeof(*v));

    v->active = 1;
    v->channel = channel;
    v->note = note;
    v->method = inst->method;
    v->inst = inst;
    v->age = ++voice_age_counter;
    v->held = 1;
    v->sustain_held = 0;
    v->is_drum = (uint8_t)is_drum;
    v->ignore_note_off = (inst->flags & 1) != 0;
    v->pitch_ratio = channel_pitch_ratio[channel];
    if (was_active) {
        v->declick = DECLICK_SAMPLES;
        v->declick_value = old_last;
    }

    vel_gain = (float)velocity * (1.0f / 127.0f);
    vel_gain *= vel_gain;
    v->gain = ((float)inst->gain * (1.0f / 255.0f)) * vel_gain;

    setup_env(v, inst);

    freq = 440.0f * powf(2.0f, ((float)note - 69.0f) / 12.0f);

    if (inst->method == 2) {
        /* Spectral wavetable */
        v->base_inc = freq * (float)inst->frame_len / (float)SYNTH_SAMPLE_RATE;
        v->phase = 0.0f;
        v->mip = (note > inst->root + 12) ? 1 : 0;
        v->frame_seg = 0;
        v->frame_frac = 0.0f;
        v->frame_frac_inc = 0.0f;
        {
            uint8_t nf = inst->frames > 5 ? 5 : inst->frames;
            int mips = (inst->flags & 4) ? 2 : 1;
            int mip = v->mip;
            if (mip >= mips) {
                mip = mips - 1;
            }
            v->wt_base0 = inst->data_off + ((uint32_t)mip) * inst->frame_len;
            v->wt_base1 = (nf > 1)
                ? inst->data_off + ((uint32_t)mips + (uint32_t)mip) * inst->frame_len
                : v->wt_base0;
            if (nf > 1) {
                const uint16_t *ft = wt_frame_times[nf];
                float t0 = (float)ft[0] * 0.001f;
                float t1 = (float)ft[1] * 0.001f;
                if (t1 > t0) {
                    v->frame_frac_inc = 1.0f / ((t1 - t0) * (float)SYNTH_SAMPLE_RATE);
                }
            }
        }
    } else if (inst->method == 1) {
        /* 2-operator FM */
        v->base_inc = freq / (float)SYNTH_SAMPLE_RATE;
        v->phase = 0.0f;
        v->mod_phase = 0.0f;
        v->last_mod = 0.0f;
        v->fm_ratio = (float)inst->fm_ratio * (1.0f / 16.0f);
        v->fm_index = (float)inst->fm_index * (1.0f / 16.0f);
        v->fm_feedback = (float)inst->fm_feedback * (1.0f / 255.0f);
    } else {
        /* PCM one-shot */
        if (inst->flags & 2) {
            v->base_inc = (float)PCM_SAMPLE_RATE / (float)SYNTH_SAMPLE_RATE;
        } else {
            float semis = (float)note - (float)inst->root;
            v->base_inc = ((float)PCM_SAMPLE_RATE / (float)SYNTH_SAMPLE_RATE) *
                          powf(2.0f, semis / 12.0f);
        }
        v->phase = 0.0f;
    }

    v->phase_inc = v->base_inc * v->pitch_ratio;
}

void synth_note_off(uint8_t channel, uint8_t note)
{
    int i;
    for (i = 0; i < SYNTH_MAX_VOICES; i++) {
        Voice *v = &voices[i];
        if (v->active && v->channel == channel && v->note == note && v->held) {
            v->held = 0;
            if (v->ignore_note_off) {
                continue;
            }
            if (channel_sustain[channel]) {
                v->sustain_held = 1;
            } else {
                v->stage = ENV_RELEASE;
            }
        }
    }
}

void synth_control_change(uint8_t channel, uint8_t cc, uint8_t value)
{
    int i;

    if (channel > 15) {
        return;
    }

    switch (cc) {
    case 7:
        channel_volume[channel] = (float)value * (1.0f / 127.0f);
        break;
    case 11:
        channel_expression[channel] = (float)value * (1.0f / 127.0f);
        break;
    case 64:
        channel_sustain[channel] = (value >= 64) ? 1 : 0;
        if (!channel_sustain[channel]) {
            for (i = 0; i < SYNTH_MAX_VOICES; i++) {
                Voice *v = &voices[i];
                if (v->active && v->channel == channel && v->sustain_held) {
                    v->sustain_held = 0;
                    if (!v->ignore_note_off) {
                        v->stage = ENV_RELEASE;
                    }
                }
            }
        }
        break;
    case 121:
        channel_volume[channel] = 1.0f;
        channel_expression[channel] = 1.0f;
        channel_sustain[channel] = 0;
        channel_pitch_ratio[channel] = 1.0f;
        for (i = 0; i < SYNTH_MAX_VOICES; i++) {
            Voice *v = &voices[i];
            if (v->active && v->channel == channel) {
                v->pitch_ratio = 1.0f;
                v->phase_inc = v->base_inc;
            }
        }
        break;
    case 123:
        synth_all_notes_off(channel);
        break;
    default:
        break;
    }
}

void synth_pitch_bend(uint8_t channel, int16_t bend)
{
    int i;
    float semis;
    float ratio;

    if (channel > 15) {
        return;
    }
    if (bend < -8192) {
        bend = -8192;
    }
    if (bend > 8191) {
        bend = 8191;
    }

    semis = ((float)bend * (1.0f / 8192.0f)) * 2.0f;
    ratio = powf(2.0f, semis / 12.0f);
    channel_pitch_ratio[channel] = ratio;

    for (i = 0; i < SYNTH_MAX_VOICES; i++) {
        Voice *v = &voices[i];
        if (v->active && v->channel == channel) {
            v->pitch_ratio = ratio;
            v->phase_inc = v->base_inc * ratio;
        }
    }
}

void synth_all_notes_off(uint8_t channel)
{
    int i;
    for (i = 0; i < SYNTH_MAX_VOICES; i++) {
        Voice *v = &voices[i];
        if (v->active && v->channel == channel) {
            v->held = 0;
            v->sustain_held = 0;
            if (!v->ignore_note_off) {
                v->stage = ENV_RELEASE;
            }
        }
    }
}

static inline float render_voice(Voice *v)
{
    const Instrument *inst = v->inst;
    float env;
    float out = 0.0f;
    float ch_gain;

    env = env_update(v);
    if (v->stage == ENV_OFF) {
        v->active = 0;
        return 0.0f;
    }

    if (v->method == 2) {
        /* Spectral wavetable */
        uint8_t nf = inst->frames;
        uint8_t seg;
        uint8_t f0;
        uint8_t f1;
        float ff;
        float ph;
        int i0;
        int i1;
        float pf;
        float s0, s1;

        if (nf == 0) {
            v->active = 0;
            return 0.0f;
        }
        if (nf > 5) {
            nf = 5;
        }

        seg = v->frame_seg;
        if (seg >= nf) {
            seg = nf - 1;
        }
        f0 = seg;
        f1 = (seg + 1 < nf) ? (uint8_t)(seg + 1) : seg;
        ff = v->frame_frac;

        ph = v->phase;
        i0 = (int)ph;
        pf = ph - (float)i0;
        i1 = (i0 + 1) & ((int)inst->frame_len - 1);

        s0 = (float)wt_pool[v->wt_base0 + (uint32_t)i0];
        s0 += ((float)wt_pool[v->wt_base0 + (uint32_t)i1] - s0) * pf;
        if (f0 != f1) {
            s1 = (float)wt_pool[v->wt_base1 + (uint32_t)i0];
            s1 += ((float)wt_pool[v->wt_base1 + (uint32_t)i1] - s1) * pf;
            s0 += (s1 - s0) * ff;
        }
        out = s0 * (1.0f / 32768.0f);

        v->phase += v->phase_inc;
        if (v->phase >= (float)inst->frame_len) {
            v->phase -= (float)inst->frame_len;
        }

        /* O(1) frame morph: advance the crossfade fraction, not a time search. */
        if (v->frame_frac_inc > 0.0f) {
            v->frame_frac += v->frame_frac_inc;
            if (v->frame_frac >= 1.0f) {
                v->frame_seg++;
                if (v->frame_seg >= nf - 1) {
                    v->frame_seg = nf - 1;
                    v->frame_frac = 0.0f;
                    v->frame_frac_inc = 0.0f;
                    v->wt_base0 = v->wt_base1;
                } else {
                    const uint16_t *ft = wt_frame_times[nf];
                    float t0 = (float)ft[v->frame_seg] * 0.001f;
                    float t1 = (float)ft[v->frame_seg + 1] * 0.001f;
                    int mips = (inst->flags & 4) ? 2 : 1;
                    int mip = v->mip;
                    if (mip >= mips) {
                        mip = mips - 1;
                    }
                    v->frame_frac = 0.0f;
                    v->frame_frac_inc = 1.0f / ((t1 - t0) * (float)SYNTH_SAMPLE_RATE);
                    v->wt_base0 = inst->data_off +
                        ((uint32_t)v->frame_seg * (uint32_t)mips + (uint32_t)mip) * inst->frame_len;
                    v->wt_base1 = inst->data_off +
                        ((uint32_t)(v->frame_seg + 1) * (uint32_t)mips + (uint32_t)mip) * inst->frame_len;
                }
            }
        }
    } else if (v->method == 1) {
        /* 2-operator FM */
        /* The fitted index is in radians; sine_lookup takes cycles. */
        float mod = sine_lookup(v->mod_phase + v->fm_feedback * v->last_mod * (1.0f / (2.0f * PI_F)));
        float idx = v->fm_index * (1.0f / (2.0f * PI_F));

        v->last_mod = mod;
        out = sine_lookup(v->phase + idx * mod);

        v->phase = wrap01(v->phase + v->phase_inc);
        v->mod_phase = wrap01(v->mod_phase + v->phase_inc * v->fm_ratio);
    } else {
        /* PCM one-shot */
        uint32_t len = inst->data_len;
        float pos = v->phase;
        int i0;
        int i1;
        float pf;
        float s0, s1;

        if (len < 2 || pos >= (float)(len - 1)) {
            v->active = 0;
            return 0.0f;
        }
        i0 = (int)pos;
        pf = pos - (float)i0;
        i1 = i0 + 1;
        if (i1 >= (int)len) {
            i1 = (int)len - 1;
        }
        s0 = (float)pcm_pool[inst->data_off + (uint32_t)i0];
        s1 = (float)pcm_pool[inst->data_off + (uint32_t)i1];
        out = (s0 + (s1 - s0) * pf) * (1.0f / 128.0f);

        v->phase += v->phase_inc;
    }

    ch_gain = channel_volume[v->channel] * channel_expression[v->channel];
    out = out * env * v->gain * ch_gain;

    /* Crossfade from the stolen voice's last sample to avoid a click. */
    if (v->declick > 0) {
        float f = (float)v->declick * (1.0f / (float)DECLICK_SAMPLES);
        out = out * (1.0f - f) + v->declick_value * f;
        v->declick--;
    }

    v->last_out = out;
    return out;
}

void synth_render_i16(int16_t *out, uint32_t frames)
{
    uint32_t n;
    int i;

    for (n = 0; n < frames; n++) {
        float mix = 0.0f;
        for (i = 0; i < SYNTH_MAX_VOICES; i++) {
            if (voices[i].active) {
                mix += render_voice(&voices[i]);
            }
        }
        mix *= master_volume;
        mix = soft_clip(mix);
        out[n] = (int16_t)(mix * 32767.0f);
    }
}

int synth_active_voices(void)
{
    int i;
    int count = 0;
    for (i = 0; i < SYNTH_MAX_VOICES; i++) {
        if (voices[i].active) {
            count++;
        }
    }
    return count;
}
