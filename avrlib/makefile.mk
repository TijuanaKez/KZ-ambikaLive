# Copyright 2009 Emilie Gillet.
#
# Author: Emilie Gillet (emilie.o.gillet@gmail.com)
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# This needs to be set to path for relevant avr-gcc version. Mac users can use CrossPack.
AVRLIB_TOOLS_PATH ?= /usr/local/Cellar/avr-gcc/9.3.0/bin/
# Some tools are missing from newer revisions. Point this to where the old tools live.
AVRLIB_TOOLS_PATH_LEGACY ?= /usr/local/Cellar/avr-binutils/2.34/bin/
BUILD_ROOT     = build/
BUILD_DIR      = $(BUILD_ROOT)$(TARGET)/
PROGRAMMER     ?= usbasp
PROGRAMMER_PORT ?= usb
AVRDUDE_ERASE  ?= no
AVRDUDE_LOCK   ?= yes

ifeq ($(FAMILY),tiny)
MCU            = attiny$(MCU_NAME)
DMCU           = t$(MCU_NAME)
MCU_DEFINE     = ATTINY$(MCU_NAME)
else
ifeq ($(MCU_NAME),atmega2560)
MCU=atmega2560
DMCU=atmega2560
MCU_DEFINE=ATMEGA2560
else
ifeq ($(FAMILY),mega)
MCU            = atmega$(MCU_NAME)
DMCU           = atmega$(MCU_NAME)
MCU_DEFINE     = ATMEGA$(MCU_NAME)
else
MCU            = atmega$(MCU_NAME)p
DMCU           = m$(MCU_NAME)p
MCU_DEFINE     = ATMEGA$(MCU_NAME)P
endif
endif
endif

F_CPU          ?= 20000000

VPATH          = $(PACKAGES)
CC_FILES       = $(notdir $(wildcard $(patsubst %,%/*.cc,$(PACKAGES))))
C_FILES        = $(notdir $(wildcard $(patsubst %,%/*.c,$(PACKAGES))))
AS_FILES       = $(notdir $(wildcard $(patsubst %,%/*.s,$(PACKAGES))))
OBJ_FILES      = $(CC_FILES:.cc=.o) $(C_FILES:.c=.o) $(AS_FILES:.s=.o)
OBJS           = $(patsubst %,$(BUILD_DIR)%,$(OBJ_FILES))
DEPS           = $(OBJS:.o=.d)

TARGET_BIN     = $(BUILD_DIR)$(TARGET).bin
TARGET_ELF     = $(BUILD_DIR)$(TARGET).elf
TARGET_HEX     = $(BUILD_DIR)$(TARGET).hex
TARGETS        = $(BUILD_DIR)$(TARGET).*
DEP_FILE       = $(BUILD_DIR)depends.mk

CC             = $(AVRLIB_TOOLS_PATH)avr-gcc
CXX            = $(AVRLIB_TOOLS_PATH)avr-g++
OBJCOPY        = $(AVRLIB_TOOLS_PATH_LEGACY)avr-objcopy
OBJDUMP        = $(AVRLIB_TOOLS_PATH_LEGACY)avr-objdump
AR             = $(AVRLIB_TOOLS_PATH)avr-ar
SIZE           = $(AVRLIB_TOOLS_PATH)avr-size
NM             = $(AVRLIB_TOOLS_PATH)avr-nm
AVRDUDE        = $(AVRLIB_TOOLS_PATH)avrdude
REMOVE         = rm -f
CAT            = cat

OPTIMISATION_LEVEL ?= -Os
COMPILE_FLAGS = \
			-ffreestanding \
			-fno-common \
			-fshort-enums \
			-nostdlib \
			-fno-move-loop-invariants \
			-fverbose-asm \
			-fdata-sections \
			-ffunction-sections \
			-flto \
			-mrelax
# optional extra flags.
# -mcall-prologues may cause code to be slower as registers are saved and popped
EXTRA_FLAGS ?= -mcall-prologues

WARNING_FLAGS = -Wall -Wextra -pedantic -Wno-narrowing -Winline #-Wno-unused-parameter

CPPFLAGS  = -g $(OPTIMISATION_LEVEL) -I. \
			-mmcu=$(MCU) \
			$(WARNING_FLAGS) \
			$(COMPILE_FLAGS) \
			$(EXTRA_FLAGS) \
			-DF_CPU=$(F_CPU) \
			$(EXTRA_DEFINES) \
			$(MMC_CONFIG) \
			-D$(MCU_DEFINE) \
			-DSERIAL_RX_0 \
			#-Wsign-conversion -Wconversion
# these are from https://bitbashing.io/embedded-cpp.html
CFLAGS        = -std=c11
CXXFLAGS      = -std=c++2a -fno-exceptions -fno-non-call-exceptions \
				-fno-use-cxa-atexit -fno-rtti
ASFLAGS       = -mmcu=$(MCU) -I. -x assembler-with-cpp
LDFLAGS       = -mmcu=$(MCU) -lm -Os -flto -Wl,--relax -Wl,--gc-sections$(EXTRA_LD_FLAGS)

# ------------------------------------------------------------------------------
# Source compiling
# ------------------------------------------------------------------------------

$(BUILD_DIR)%.o: %.cc
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

$(BUILD_DIR)%.o: %.c
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

$(BUILD_DIR)%.o: %.s
	$(CC) -c $(CPPFLAGS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)%.d: %.cc
	$(CXX) -MM $(CPPFLAGS) $(CXXFLAGS) $< -MF $@ -MT $(@:.d=.o)

$(BUILD_DIR)%.d: %.c
	$(CC) -MM $(CPPFLAGS) $(CFLAGS) $< -MF $@ -MT $(@:.d=.o)

$(BUILD_DIR)%.d: %.s
	$(CC) -MM $(CPPFLAGS) $(ASFLAGS) $< -MF $@ -MT $(@:.d=.o)


# ------------------------------------------------------------------------------
# Object file conversion
# ------------------------------------------------------------------------------

$(BUILD_DIR)%.hex: $(BUILD_DIR)%.elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

$(BUILD_DIR)%.bin: $(BUILD_DIR)%.elf
	$(OBJCOPY) -O binary -R .eeprom $< $@

$(BUILD_DIR)%.eep: $(BUILD_DIR)%.elf
	-$(OBJCOPY) -j .eeprom --set-section-flags=.eeprom="alloc,load" \
		--change-section-lma .eeprom=0 -O ihex $< $@

$(BUILD_DIR)%.lss: $(BUILD_DIR)%.elf
	$(OBJDUMP) -h -S $< > $@

$(BUILD_DIR)%.sym: $(BUILD_DIR)%.elf
	$(NM) -n $< > $@

# ------------------------------------------------------------------------------
# AVRDude
# ------------------------------------------------------------------------------

AVRDUDE_COM_OPTS = -V -p $(DMCU)
AVRDUDE_ISP_OPTS = -c $(PROGRAMMER) -P $(PROGRAMMER_PORT)

ifeq ($(AVRDUDE_LOCK),no)
AVRDUDE_LOCK_OPTS =
else
AVRDUDE_LOCK_OPTS ?= -U lock:w:0x$(LOCK):m
endif

ifeq ($(AVRDUDE_ERASE),no)
AVRDUDE_ERASE_OPTS = 
else
AVRDUDE_ERASE_OPTS ?= -D
endif
# ------------------------------------------------------------------------------
# Main targets
# ------------------------------------------------------------------------------

all:    $(BUILD_DIR) $(TARGET_HEX)

$(BUILD_DIR):
		mkdir -p $(BUILD_DIR)

$(TARGET_ELF):  $(OBJS)
		$(CC) $(LDFLAGS) -o $@ $(OBJS) $(SYS_OBJS)# -lc

$(DEP_FILE):  $(BUILD_DIR) $(DEPS)
		cat $(DEPS) > $(DEP_FILE)

bin:	$(TARGET_BIN)


upload:    $(TARGET_HEX)
		$(AVRDUDE) $(AVRDUDE_ERASE_OPTS) $(AVRDUDE_COM_OPTS) $(AVRDUDE_ISP_OPTS) \
			-B 1 -U flash:w:$(TARGET_HEX):i $(AVRDUDE_LOCK_OPTS) 

slow_upload:    $(TARGET_HEX)
		$(AVRDUDE) $(AVRDUDE_ERASE_OPTS) $(AVRDUDE_COM_OPTS) $(AVRDUDE_ISP_OPTS) \
			-B 4 -U flash:w:$(TARGET_HEX):i $(AVRDUDE_LOCK_OPTS)

clean:
		$(REMOVE) $(OBJS) $(TARGETS) $(DEP_FILE) $(DEPS)

depends:  $(DEPS)
		cat $(DEPS) > $(DEP_FILE)

$(TARGET).size: $(TARGET_ELF)
		$(OBJDUMP) -P mem-usage $(TARGET_ELF) > $(TARGET).size
		$(SIZE) $(TARGET_ELF) >> $(TARGET).size

$(BUILD_DIR)$(TARGET).top_symbols: $(TARGET_ELF)
		$(NM) $(TARGET_ELF) --size-sort -C -f bsd -r > $@

size: $(TARGET).size
	cat $(TARGET).size

# KZ MOD: restored from Pichenettes' original makefile, where it was lost in the
# MachFour refactor. Static SRAM is data + bss; his note says it must stay under
# 4096 - 128 for the controller, i.e. 3968, reserving 128 bytes for the stack.
#
# Treat that 128 as optimistic. Measured peak stack on this firmware is 240 to
# 336 bytes, mostly FatFs -- f_rename alone has 87 bytes of locals -- so passing
# this check does NOT prove the build fits. scripts/build_firmware.py enforces
# the same limit, and the LOW reading on the diagnostic page is what actually
# settles it.
ramsize: $(TARGET).size
	@awk '/^Program:/ { next } \
	      /^Data:/ { gsub(/[^0-9]/, "", $$2); print "static SRAM (data+bss): " $$2 } \
	     ' $(TARGET).size 2>/dev/null; \
	$(SIZE) $(TARGET_ELF) | awk 'NR==2 { \
	  printf "data %s + bss %s = %d bytes static SRAM\n", $$2, $$3, $$2+$$3; \
	  if ($$2+$$3 >= 3968) print "OVER the 3968 limit"; \
	  else printf "%d bytes left for the stack\n", 4096-($$2+$$3); }'

size_report:  build/$(TARGET)/$(TARGET).lss build/$(TARGET)/$(TARGET).top_symbols

.PHONY: all clean depends upload resources

include $(DEP_FILE)

# ------------------------------------------------------------------------------
# Midi files for firmware update
# ------------------------------------------------------------------------------

HEX2SYSEX = python tools/hex2sysex/hex2sysex.py

$(BUILD_DIR)%.mid: $(BUILD_DIR)%.hex
	$(HEX2SYSEX) $(SYSEX_FLAGS) -o $@ $<

$(BUILD_DIR)%.syx: $(BUILD_DIR)%.hex
	$(HEX2SYSEX) $(SYSEX_FLAGS) --syx -o $@ $<

$(BUILD_DIR)%_old_bootloader.mid: $(BUILD_DIR)%.hex
	$(HEX2SYSEX) $(SYSEX_FLAGS) --obsolete_manufacturer_id -o $@ $<

$(BUILD_DIR)%_old_bootloader.syx: $(BUILD_DIR)%.hex
	$(HEX2SYSEX) $(SYSEX_FLAGS) --obsolete_manufacturer_id --syx -o $@ $<


midi: $(BUILD_DIR)$(TARGET).mid

syx: $(BUILD_DIR)$(TARGET).syx

old_midi: $(BUILD_DIR)$(TARGET)_old_bootloader.mid

old_syx: $(BUILD_DIR)$(TARGET)_old_bootloader.syx

# ------------------------------------------------------------------------------
# EEPROM image write
# ------------------------------------------------------------------------------

GOLDEN_EEPROM_FILE = $(TARGET)/data/$(TARGET)_eeprom_golden.hex
GOLDEN_FLASH_FILE = $(TARGET)/data/$(TARGET)_flash_golden.hex

eeprom_backup:
	$(AVRDUDE) $(AVRDUDE_COM_OPTS) $(AVRDUDE_ISP_OPTS) \
		-U eeprom:r:$(GOLDEN_EEPROM_FILE):i

eeprom_restore:
	$(AVRDUDE) $(AVRDUDE_COM_OPTS) $(AVRDUDE_ISP_OPTS) \
		-U eeprom:w:$(GOLDEN_EEPROM_FILE):i -U lock:w:0x$(LOCK):m

flash_backup:
	$(AVRDUDE) $(AVRDUDE_COM_OPTS) $(AVRDUDE_ISP_OPTS) \
		-U flash:r:$(GOLDEN_FLASH_FILE):i

flash_restore:
	$(AVRDUDE) $(AVRDUDE_COM_OPTS) $(AVRDUDE_ISP_OPTS) \
		-U flash:w:$(GOLDEN_FLASH_FILE):i -U lock:w:0x$(LOCK):m


# ------------------------------------------------------------------------------
# Resources
# ------------------------------------------------------------------------------

RESOURCE_COMPILER = avrlib/tools/resources_compiler.py
# Use python3 (or alias python to python3 on macOS).
resources: $(wildcard $(RESOURCES)/*.py) $(RESOURCE_COMPILER)
		python3 $(RESOURCE_COMPILER) $(RESOURCES)/resources.py


# ------------------------------------------------------------------------------
# Publish a firmware version on the website
# ------------------------------------------------------------------------------

REMOTE_HOST = mutable-instruments.net
REMOTE_USER = mutable
REMOTE_PATH = public_html/static/firmware

publish: syx midi
	scp $(BUILD_DIR)$(TARGET).mid $(REMOTE_USER)@$(REMOTE_HOST):$(REMOTE_PATH)/$(TARGET)_$(VERSION).mid
		scp $(BUILD_DIR)$(TARGET).hex $(REMOTE_USER)@$(REMOTE_HOST):$(REMOTE_PATH)//$(TARGET)_$(VERSION).hex
		scp $(BUILD_DIR)$(TARGET).syx $(REMOTE_USER)@$(REMOTE_HOST):$(REMOTE_PATH)//$(TARGET)_$(VERSION).syx

publish_old: old_midi old_syx
	scp $(BUILD_DIR)$(TARGET)_old_bootloader.mid $(REMOTE_USER)@$(REMOTE_HOST):$(REMOTE_PATH)/$(TARGET)_$(VERSION).mid
		scp $(BUILD_DIR)$(TARGET).hex $(REMOTE_USER)@$(REMOTE_HOST):$(REMOTE_PATH)//$(TARGET)_$(VERSION).hex
		scp $(BUILD_DIR)$(TARGET)_old_bootloader.syx $(REMOTE_USER)@$(REMOTE_HOST):$(REMOTE_PATH)//$(TARGET)_$(VERSION).syx

# ------------------------------------------------------------------------------
# Set fuses
# ------------------------------------------------------------------------------

terminal:
		$(AVRDUDE) $(AVRDUDE_COM_OPTS) $(AVRDUDE_ISP_OPTS) -e -tuF

fuses:
		$(AVRDUDE) $(AVRDUDE_COM_OPTS) $(AVRDUDE_ISP_OPTS) -B 10 -e -u \
			-U efuse:w:0x$(EFUSE):m \
			-U hfuse:w:0x$(HFUSE):m \
			-U lfuse:w:0x$(LFUSE):m \
			-U lock:w:0x$(LOCK):m

# ------------------------------------------------------------------------------
# Program (fuses + firmware) a blank chip
# ------------------------------------------------------------------------------

bootstrap: bake

bake:	$(FIRMWARE)
		$(AVRDUDE) $(AVRDUDE_COM_OPTS) $(AVRDUDE_ISP_OPTS) -B 10 -e -u \
			-U efuse:w:0x$(EFUSE):m \
			-U hfuse:w:0x$(HFUSE):m \
			-U lfuse:w:0x$(LFUSE):m \
			-U lock:w:0x$(LOCK):m
		$(AVRDUDE) $(AVRDUDE_COM_OPTS) $(AVRDUDE_ISP_OPTS) -B 1 \
			-U flash:w:$(TARGET_HEX):i -U lock:w:0x$(LOCK):m
