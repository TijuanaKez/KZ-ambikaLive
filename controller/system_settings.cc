// Copyright 2009 Emilie Gillet.
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
// Global system settings.

#include "controller/resources.h"
#include "controller/storage.h"
#include "controller/system_settings.h"

#include <avr/eeprom.h>

namespace ambika {

/* static */
SystemSettingsData SystemSettings::data_;

static constexpr SystemSettingsData::Parameters init_settings PROGMEM = {
    .midi_in_mask = 0,
    .midi_out_mode = MIDI_OUT_THRU,
    .show_help = 1,
    .snap = 0,
    .autobackup = 1,
    .voicecard_leds = 1,
    .swap_leds_colors = 1,
    .midi_cc_map = CCMAP_AMBIKA,
    .launchkey_mode = 0,
    .padding = {0},
    .checksum = 4 // .checksum is the sum of all previous data values
};

static constexpr uint8_t settingsSize = sizeof(SystemSettingsData::Parameters);

/* static */
void SystemSettings::Save() {
  data_.checksum() = Storage::Checksum(&data_, settingsSize - 1);
  eeprom_write_block(data_.params_addr(), reinterpret_cast<void*>(E2END - settingsSize), settingsSize);
}

/* static */
void SystemSettings::Init(bool force_reset) {
  eeprom_read_block(data_.params_addr(), reinterpret_cast<void*>(E2END - settingsSize), settingsSize);
  if (data_.checksum() != Storage::Checksum(data_.params_addr(), settingsSize - 1) || force_reset) {
    ResourcesManager::Load(&init_settings, 0, data_.params_addr());
  }
}

/* extern */
SystemSettings system_settings;

}  // namespace ambika
