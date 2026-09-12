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
// A page which allows up to 8 user-selected parameters to be tweaked.

#include "controller/ui_pages/performance_page.h"

#include "avrlib/string.h"

#include "common/features.h"

#include "controller/display.h"
#include "controller/leds.h"
#include "controller/multi.h"

namespace ambika {

/* static */
uint8_t PerformancePage::note_ = 60;

/* static */
uint8_t PerformancePage::note_active_ = 0;

/* static */
uint8_t PerformancePage::OnIncrement(int8_t increment) {
  if (note_active_) {
    if ((note_ < 0x7f && increment > 0) ||
        (note_ > 0 && increment < 0)) {
      multi.StartNote(note_ + increment);
      multi.StopNote(note_);
      note_ += increment;
    }
    return 1;
  }
  // TODO what to return?
  return 0;
}

/* static */
uint8_t PerformancePage::OnClick() {
  if (!note_active_) {
    multi.StartNote(note_);
    note_active_ = 1;
  } else {
    multi.StopNote(note_);
    note_active_ = 0;
  }
  return 1;
}

/* static */
uint8_t PerformancePage::OnKey(uint8_t key) {
// KZ TODO - Make this a system setting

#ifndef DISABLE_PART_MUTES
  if (key <= SWITCH_4){
      multi.ToggleMute(key);
    return 1;
  } else if (key == SWITCH_5){
      multi.SyncPartClocks();
  } else {
      return ParameterEditor::OnKey(key);
  }
#else
  return ParameterEditor::OnKey(key);
#endif
}

/* static */
void PerformancePage::UpdateLeds() {
  leds.set_pixel(LED_7, 0xf0);
  if (multi.running()) {
    if (multi.step() >= 0x8) {
      leds.set_pixel(LED_STATUS, 0x0f);
    }
    leds.set_pixel(LED_1 + (multi.step() & 0x07), 0x03);
  }
  #ifndef DISABLE_PART_MUTES
  // KZ MOD - Set the first 4 LEDs according to Part Mutes
  for (uint8_t led = LED_1; led <= LED_4; led++){
    if (!multi.IsPartMuted(led)){
      leds.set_pixel(led, 0xf0);
    }
  }
#endif
}

}  // namespace ambika
