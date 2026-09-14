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
// System settings data structure.

#ifndef CONTROLLER_SYSTEM_SETTINGS_H_
#define CONTROLLER_SYSTEM_SETTINGS_H_

#include "avrlib/base.h"

#include <avr/pgmspace.h>
#include <stddef.h>

namespace ambika {

// KZ MOD
enum CCMap : uint8_t {
  CCMAP_AMBIKA,
  CCMAP_SHRUTHI_XT,
  CCMAP_LAUNCHKEY,
  CCMAP_LAST
};

enum MidiOutMode : uint8_t {
  MIDI_OUT_THRU,
  MIDI_OUT_SEQUENCER,
  MIDI_OUT_CONTROLLER,
  MIDI_OUT_CHAIN,
  MIDI_OUT_FULL
};

// has to match members of SystemSettingsData::Parameters
enum SystemSettingsParameter : uint8_t {
  PRM_SYSTEM_MIDI_IN_MASK,
  PRM_SYSTEM_MIDI_OUT_MODE,
  PRM_SYSTEM_SHOW_HELP,
  PRM_SYSTEM_SNAP,
  PRM_SYSTEM_AUTOBACKUP,
  PRM_SYSTEM_VOICECARD_LEDS,
  PRM_SYSTEM_VOICECARD_SWAP_LEDS_COLORS,
  PRM_SYSTEM_CC_MAP,
  PRM_SYSTEM_LAUNCHKEY_MODE
};

struct SystemSettingsData {
  struct Parameters {
    uint8_t midi_in_mask;
    MidiOutMode midi_out_mode;
    uint8_t show_help;
    uint8_t snap;
    uint8_t autobackup;
    uint8_t voicecard_leds;
    uint8_t swap_leds_colors;
    CCMap midi_cc_map;
    uint8_t launchkey_mode;
    uint8_t padding[6]; // KZ MOD Reduced from 8 to 6 to allow for extra 2 settings
    uint8_t checksum;
  };

private:
  union Data {
    Parameters params;
    uint8_t bytes[sizeof(Parameters)];

  };

  Data data;

public:
  inline uint8_t* bytes() {
    return data.bytes;
  }
  inline Parameters* params_addr() {
    return &data.params;
  }

  inline uint8_t& midi_in_mask() {
    return data.params.midi_in_mask;
  }
  inline MidiOutMode& midi_out_mode() {
    return data.params.midi_out_mode;
  }
  inline uint8_t& show_help() {
    return data.params.show_help;
  }
  inline uint8_t& snap() {
    return data.params.snap;
  }
  inline uint8_t& autobackup() {
    return data.params.autobackup;
  }
  inline uint8_t& voicecard_leds() {
    return data.params.voicecard_leds;
  }
  inline uint8_t& swap_leds_colors() {
    return data.params.swap_leds_colors;
  }
  inline CCMap& midi_cc_map() { // KZ MOD - getters for new params
    return data.params.midi_cc_map;
  }
  inline uint8_t& launchkey_mode() { // KZ MOD - getters for new params
    return data.params.launchkey_mode;
  }
  inline uint8_t& checksum() { 
    return data.params.checksum;
  }
};

static_assert(sizeof(SystemSettingsData::Parameters) == 16,
              "Preserve the 16-byte EEPROM settings record");
static_assert(offsetof(SystemSettingsData::Parameters, midi_cc_map) == PRM_SYSTEM_CC_MAP &&
              offsetof(SystemSettingsData::Parameters, launchkey_mode) == PRM_SYSTEM_LAUNCHKEY_MODE &&
              offsetof(SystemSettingsData::Parameters, checksum) == 15,
              "Settings parameter IDs must match stored byte offsets");

class SystemSettings {
 public:
  SystemSettings() = default;
  
  static void Save();
  static void Init(bool force_reset);
  
  static inline SystemSettingsData& data() {
    return data_;
  }

  static inline uint8_t rx_sysex() {
    return !byteAnd(data_.midi_in_mask(), 1);
  }
  static inline uint8_t rx_program_change() {
    return !byteAnd(data_.midi_in_mask(), 2);
  }
  static inline uint8_t rx_nrpn() {
    return !byteAnd(data_.midi_in_mask(), 4);
  }
  static inline uint8_t rx_cc() {
    return !byteAnd(data_.midi_in_mask(), 8);
  }
  static inline void setValue(uint8_t address, uint8_t value) {
    data_.bytes()[address] = value;
  }
  static inline uint8_t getValue(uint8_t address) {
    return data_.bytes()[address];
  }

 private:
  static SystemSettingsData data_;
 
  DISALLOW_COPY_AND_ASSIGN(SystemSettings);
};

extern SystemSettings system_settings;

}  // namespace ambika

#endif  // CONTROLLER_SYSTEM_SETTINGS_H_
