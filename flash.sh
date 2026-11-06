echo "flash"
#st-flash --reset write  build/stm32f411_midi.bin 0x8000000
st-flash --reset --format ihex write  build/stm32f411_midi.hex
