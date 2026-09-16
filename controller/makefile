# Copyright 2011 Emilie Gillet.
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

VERSION        = 0.1
MCU_NAME       = 644
TARGET         = ambika_controller
PACKAGES       = avrlib \
		avrlib/devices \
		avrlib/filesystem \
		avrlib/third_party/ff \
		common \
		controller \
		controller/ui_pages
EXTRA_DEFINES  = -DDISABLE_DEFAULT_UART_RX_ISR

# KZ MOD: no LTO on the controller. See avrlib/makefile.mk -- it costs about
# 2 KB of flash, which this target has spare, and buys back stack, which it does
# not. Pichenettes' original build did not use LTO either.
LTO_FLAGS      =
RESOURCES      = controller/resources
SYSEX_FLAGS    = --page_size=128 --device_id=4
MMC_CONFIG     = \
		-DMMC_CS_PORT=PORTB \
		-DMMC_CS_BIT=4 \
		-DMMC_NO_SPI_INITIALIZATION

LFUSE          = ff
HFUSE          = d2
EFUSE          = fd
LOCK           = 2f

include avrlib/makefile.mk

include $(DEP_FILE)
