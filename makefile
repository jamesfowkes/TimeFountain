NAME=test

AVR_DIR=C:/WinAVR-20100110/bin

CC=$(AVR_DIR)/avr-gcc
DUMP=$(AVR_DIR)/avr-objdump

MCU_TARGET=atmega168
LIBS_DIR = C:/Users/fowkes_james/Documents/GitHub/Code-Library

OPT_LEVEL=3

ERROR_FILE="error.txt"

INCLUDE_DIRS = \
	-I$(LIBS_DIR)/AVR \
	-I$(LIBS_DIR)/Common \
	-I$(LIBS_DIR)/Devices \
	-I$(LIBS_DIR)/Generics \
	-I$(LIBS_DIR)/Utility

CFILES = \
	main.c \
	$(LIBS_DIR)/AVR/lib_clk.c \
	$(LIBS_DIR)/AVR/lib_io.c \
	$(LIBS_DIR)/AVR/lib_fuses.c \
	$(LIBS_DIR)/AVR/lib_tmr_common.c \
	$(LIBS_DIR)/AVR/lib_tmr8.c \
	$(LIBS_DIR)/AVR/lib_tmr8_tick.c \
	$(LIBS_DIR)/AVR/lib_tmr16.c \
	$(LIBS_DIR)/AVR/lib_adc.c \
	$(LIBS_DIR)/AVR/lib_pcint.c \
	$(LIBS_DIR)/Generics/memorypool.c \

OPTS = \
	-g \
	-Wall \
	-Wextra \
	-DF_CPU=8000000 \
	-DMEMORY_POOL_BYTES=128 \
	--std=c99

LDFLAGS = \
	-Wl

OBJDEPS=$(CFILES:.c=.o)

all: init $(NAME).elf errors

init:
	@rm -f $(ERROR_FILE)
	
$(NAME).elf: $(OBJDEPS)
	$(CC) $(INCLUDE_DIRS) $(OPTS) $(LDFLAGS) -O$(OPT_LEVEL) -mmcu=$(MCU_TARGET) -o $@ $^

%.o:%.c
	$(CC) $(INCLUDE_DIRS) $(OPTS) -O$(OPT_LEVEL) -mmcu=$(MCU_TARGET) -c $< -o $@ 2>>$(ERROR_FILE)

dump:
	$(DUMP) -h -S $(NAME).elf > $(NAME).lst

errors:
	@echo "Errors and Warnings:"
	@cat $(ERROR_FILE)
clean:
	rm -rf $(NAME).elf
	rm -rf $(OBJDEPS)