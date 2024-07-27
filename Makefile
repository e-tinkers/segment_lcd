# MCU=attiny824
# MCU=attiny3224
MCU=attiny3227
# MCU=attiny3217
F_CPU=10000000UL

clean: main.elf
	rm main.elf

build: main.c
	avr-gcc main.c -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -o main.elf

flash: main.elf
	avrdude -c serialupdi -P /dev/tty.usbserial-* -p $(MCU) -U flash:w:main.elf
all: build flash
