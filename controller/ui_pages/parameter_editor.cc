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
// A page which allows up to 8 parameters to be tweaked.

#include "controller/ui_pages/parameter_editor.h"

#include "avrlib/string.h"

#include "controller/display.h"
#include "controller/leds.h"
#include "controller/multi.h"
#include "controller/parameter.h"
#include "controller/system_settings.h"

namespace ambika {

/* static */
ParameterEditor::SnapMask ParameterEditor::snapped_;

// TODO code duplication in the three functions below
/* static */
uint8_t ParameterEditor::parameter_index(uint8_t control_id) {
  uint8_t parameter_id = info_->data[control_id];
  if (parameter_id >= 0xf0 && parameter_id <= 0xf7) {
    return multi.data().knobAssignment(lowNibble(parameter_id)).parameter;
  } else {
    return parameter_id;
  }
}

/* static */
uint8_t ParameterEditor::part_index(uint8_t control_id) {
  uint8_t parameter_id = info_->data[control_id];
  if (parameter_id >= 0xf0 && parameter_id <= 0xf7) {
    return multi.data().knobAssignment(lowNibble(parameter_id)).part;
  } else {
    return ui.active_part();
  }
}

/* static */
uint8_t ParameterEditor::instance_index(uint8_t control_id) {
  uint8_t parameter_id = info_->data[control_id];
  if (parameter_id >= 0xf0 && parameter_id <= 0xf7) {
    return multi.data().knobAssignment(lowNibble(parameter_id)).instance;
  } else {
    if (parameter_id == 0xff) {
      return 0xff;
    } else {
      const Parameter& parameter = parameter_manager.parameter(parameter_id);
      if (parameter.indexed_by != 0xff) {
        return ui.state().bytes()[parameter.indexed_by];
      } else {
        return 0;
      }
    }
  }
}

/* static */
void ParameterEditor::OnInit(PageInfo* info) {
  UiPage::OnInit(info);
  SetActiveControl(ACTIVE_CONTROL_FIRST);
  snapped_ = 0;
}

/* static */
void ParameterEditor::SetActiveControl(ActiveControl active_control) {
  if (active_control == ACTIVE_CONTROL_FIRST) {
    active_control_ = -1;
    OnIncrement(1);
  } else {
    active_control_ = 8;
    OnIncrement(-1);
  }
}

/* static */
uint8_t ParameterEditor::OnIncrement(int8_t increment) {
  if (edit_mode_ != EDIT_IDLE) {
    int8_t active = active_control_;
    parameter_manager.Increment(parameter_index(active), part_index(active), instance_index(active), increment, false);
    edit_mode_ = EDIT_STARTED_BY_ENCODER;
  } else {
    int8_t new_control = active_control_ + increment;
    while (
        new_control >= 0 &&
        new_control < kNumParametersPerPage &&
        info_->data[new_control] == 0xff) {
      new_control += increment;
    }
    if (new_control < 0) {
      ui.ShowPageRelative(-1);
    } else if (new_control >= 8) {
      ui.ShowPageRelative(1);
    } else {
      active_control_ = new_control;
    }
  }
  return 1;
}

/* static */
bool ParameterEditor::OnIncrementAndCycle(int8_t parameter_index, int8_t part) {
  const Parameter& parameter = parameter_manager.parameter(parameter_index);
  bool isLastPage = false;
  if (parameter_manager.GetValue(parameter, part, 0) >= parameter.max_value){
    parameter_manager.SetValue(parameter, part, 0, 0, 1);
    isLastPage = true;
  } else {
    parameter_manager.Increment(parameter, part, 0, 1, false);
  }
  return isLastPage;
}

/* static */
uint8_t ParameterEditor::OnPot(uint8_t index, uint8_t value) {
  uint8_t parameter_id = parameter_index(index);
  if (parameter_id == 0xff) {
    return 1;
  }
  const Parameter& parameter = parameter_manager.parameter(parameter_id);
  if (system_settings.data().snap()) {
    SnapMask mask = (1u << index);
    // If this pot has not reached the right position yet, test if the position
    // of the pot matches the value of the parameter.
    // Pots used to scroll among UI pages are not subject to snap.
    if (!(snapped_ & mask)) {
      if (parameter_manager.is_snapped(parameter, part_index(index), instance_index(index), value) ||
            parameter.level == PARAMETER_LEVEL_UI) {
        snapped_ |= mask;
      } else {
        // Not there yet!
        return 1;
      }
    }
  }
  active_control_ = index;
  parameter_manager.Scale(parameter, part_index(active_control_), instance_index(active_control_), value);
  edit_mode_ = EDIT_STARTED_BY_POT;
  return 1;
}

/* static */
void ParameterEditor::UpdateScreen() {
  uint8_t detailed_info_line = 2;
  // If we are in editing mode, draw the detailed information for the edited
  // parameter. Disable this detailed information when "help" is set to off.
  if (edit_mode_ && system_settings.data().show_help()) {
    uint8_t parameter_id = parameter_index(active_control_);
    // Depending on the active control, draw on first or second line.
    uint8_t line = active_control_ < 4 ? 0 : 1;
    // Leave one char of space for the status icon.
    char* buffer = display.line_buffer(line) + 1;
    const Parameter& parameter = parameter_manager.parameter(parameter_id);
    if (parameter.level != PARAMETER_LEVEL_UI) {
      detailed_info_line = line;
      // Include the part number in the name of the parameter iff the current control is a custom knob.
      auto part = highNibbleUnshifted(info_->data[active_control_]) == 0xf0 ? part_index(active_control_) : 0xff;
      parameter.PrintObject(part, instance_index(active_control_), buffer, 19);
      buffer[19] = kDelimiter;
      uint8_t value = parameter_manager.GetValue(parameter, part_index(active_control_), instance_index(active_control_));
      parameter.Print(value, &buffer[20], 11, 7);
    }
  }
  
  // Draw the 8 cells with parameter names and values.
  // TODO code duplication with the next for loop after this one (not nested)
  for (uint8_t i = 0; i < kNumParametersPerPage; ++i) {
    uint8_t parameter_id = parameter_index(i);
    uint8_t line = i < 4 ? 0 : 1;
    if (line == detailed_info_line) {
      continue;
    }
    uint8_t row = 10 * (i % 4);
    char* buffer = display.line_buffer(line) + row;
    if (row != 0) {
      buffer[0] = kDelimiter;
    }
    if ((row + 10) != kLcdWidth) {
      buffer[10] = kDelimiter;
    }
    if (parameter_id == 0xf8) {
    // KEZ mod ->more
      strncpy_P(&buffer[4], PSTR("more->"), 6);
    } else if (parameter_id == 0xf9) {
    // KEZ mod <-back
      strncpy_P(&buffer[4], PSTR("<-back"), 6);
    } else if (parameter_id != 0xff) {
      const Parameter& parameter = parameter_manager.parameter(parameter_id);
      uint8_t value = parameter_manager.GetValue(parameter, part_index(i), instance_index(i));


      // Use up to 4 letters for ordinary parameters names, 6 for page names
      uint8_t name_width = 4;
      uint8_t value_width = 4;
      if (parameter.level == PARAMETER_LEVEL_UI) {
        name_width = 6;
        value_width = 2;
      }
      parameter.Print(value, &buffer[1], name_width, value_width);

      if (i == active_control_) {
        // Phase57 mod: make the whole active parameter name uppercase.
        // actually nah
        //for (uint8_t c = 1; c < name_width + 1 ; ++c) {
        //  if (buffer[c] >= 'a' && buffer[c] <= 'z') {
        //    buffer[c] -= 0x20;
        //  }
        //}
        if (buffer[1] >= 'a' && buffer[1] <= 'z') {
          buffer[1] -= 0x20;
        }
      }
    }
  }
  
  for (uint8_t i = 0; i < kNumParametersPerPage; ++i) {
    uint8_t parameter_id = info_->data[i];
    uint8_t line = i < 4 ? 0 : 1;
    if (line == detailed_info_line) {
      continue;
    }
    uint8_t row = 10 * (i % 4);
    char* buffer = display.line_buffer(line) + row;
    if (parameter_id == 0xff) {
      buffer[0] = ' ';
      buffer[10] = ' ';
    }
  }
}

/* static */
void ParameterEditor::UpdateLeds() {
  UiPage::UpdateLeds();
  if (info_->index == PAGE_ENV_LFO) {
    uint8_t current_lfo_value = multi.part(ui.active_part()).lfo_value(ui.state().active_env_lfo());
    leds.set_pixel(LED_STATUS, highNibbleUnshifted(current_lfo_value));
  } else {
    if (multi.running() && (multi.step() % 4) == 0) {
      leds.set_pixel(LED_STATUS, 0xf0);
    }
  }
}

}  // namespace ambika
