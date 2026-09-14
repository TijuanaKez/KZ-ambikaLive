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
// Main definitions.

#ifndef VOICECARD_VOICECARD_H_
#define VOICECARD_VOICECARD_H_

#include "avrlib/base.h"

#include <avr/pgmspace.h>

namespace ambika {

//#define ALTERNATIVE_CODE

// One control signal sample is generated for each 40 audio sample.
static constexpr uint8_t kControlRate = 40;

// The latency is 1ms, with a buffer storing 4ms of audio.
static constexpr uint8_t kAudioBlockSize = kControlRate;

// KZ MOD: 0x12 is the first voice card image built on the modern AVR GCC 9
// toolchain; 0x13 fixes the oscillator dispatch table. The version is
// display-only -- the controller never gates on it --
// so bumping it is safe, and it is the only way to tell from the OS information
// page which cards are running a new build. Displayed as v1.2.
constexpr uint8_t kSystemVersion = 0x14;

static const auto kFirmwareUpdateFlagPtr = reinterpret_cast<uint8_t*>(E2END);

enum VoicecardFirmwareUpdateStatus : uint8_t {
  FIRMWARE_UPDATE_DONE = 0,
  FIRMWARE_UPDATE_REQUESTED = 1,
  FIRMWARE_UPDATE_PROBING_BOOT = 2,
  FIRMWARE_UPDATE_PROBING_BOOT_SECOND_TRY = 3,
  FIRMWARE_UPDATE_PROBING_BOOT_THIRD_TRY = 4,
  FIRMWARE_UPDATE_PROBING_BOOT_FOURTH_TRY = 5,
  FIRMWARE_UPDATE_PROBING_BOOT_LAST_TRY = 6,
};

// KZ MOD: audio render headroom, read over SPI with
// COMMAND_GET_AUDIO_HEADROOM. audio_drain_peak is the largest free space seen
// in the audio buffer just before rendering a block, so it rises as the
// renderer falls behind; audio_starved is set if the buffer ever ran dry.
extern volatile uint8_t audio_drain_peak;
extern volatile uint8_t audio_starved;

}  // namespace ambika

#endif  // VOICECARD_VOICECARD_H_
