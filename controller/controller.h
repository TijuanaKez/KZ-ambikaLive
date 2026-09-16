// Copyright 2011 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// -----------------------------------------------------------------------------
//
// Part data structure.

#ifndef CONTROLLER_CONTROLLER_H_
#define CONTROLLER_CONTROLLER_H_

#include "avrlib/base.h"

namespace ambika {
  
static const uint32_t kSampleRateNum = 2000000L;
static const uint32_t kSampleRateDen = 51L;

// One control signal sample is generated for each 40 audio sample.
static const uint8_t kControlRate = 40;
  
const uint8_t kNumArpeggiatorPatterns = 30;
const uint8_t kNumParts = 6;
const uint8_t kNumVoices = 6;

// KZ MOD: 0x13 was the first published KZ Ambika Live baseline built on the
// modern AVR GCC 9 toolchain. 0x14 added deferred library loading and the
// working SysEx name query. 0x15 is the stack fix: LTO off on the controller,
// bounded TIMER1 re-entrancy, and the undo snapshot compiled out.
// Displayed as v1.5 on the OS information page.
const uint8_t kSystemVersion = 0x15;

}  // namespace ambika

#endif  // CONTROLLER_CONTROLLER_H_
