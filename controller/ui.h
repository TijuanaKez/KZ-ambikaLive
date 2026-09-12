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
// User interface.

#ifndef CONTROLLER_UI_H_
#define CONTROLLER_UI_H_

#include "avrlib/base.h"

#include "avrlib/devices/pot_scanner.h"
#include "avrlib/devices/rotary_encoder.h"
#include "avrlib/devices/switch.h"
#include "avrlib/ui/event_queue.h"

#include "common/features.h"

#include "controller/hardware_config.h"
#include "controller/resources.h"

namespace ambika {

using namespace avrlib;

// has to match order of members in UiState::Parameters

enum UiStateParameter : uint8_t {
  PRM_UI_ACTIVE_ENV_LFO,     // uint8_t active_env_lfo;
  PRM_UI_ACTIVE_MODULATION,  // uint8_t active_modulation;
  PRM_UI_ACTIVE_MODIFIER,    // uint8_t active_modifier;
  PRM_UI_ACTIVE_PART,        // uint8_t active_part;
};

struct UiState {
  struct Parameters {
    uint8_t active_env_lfo;
    uint8_t active_modulation;
    uint8_t active_modifier;
    uint8_t active_part;
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
  inline uint8_t getValue(uint8_t offset) const {
    return data.bytes[offset];
  }
  inline uint8_t& active_env_lfo() {
    return data.params.active_env_lfo;
  }
  inline uint8_t& active_modulation() {
    return data.params.active_modulation;
  }
  inline uint8_t& active_modifier() {
    return data.params.active_modifier;
  }
  inline uint8_t& active_part() {
    return data.params.active_part;
  }
};


enum SwitchNumber : uint8_t {
  SWITCH_1,
  SWITCH_2,
  SWITCH_3,
  SWITCH_4,
  SWITCH_5,
  SWITCH_6,
  SWITCH_7,
  SWITCH_8,
  
  SWITCH_SHIFT_1,
  SWITCH_SHIFT_2,
  SWITCH_SHIFT_3,
  SWITCH_SHIFT_4,
  SWITCH_SHIFT_5,
  SWITCH_SHIFT_6,
  SWITCH_SHIFT_7,
  SWITCH_SHIFT_8
};

enum ActiveControl : uint8_t {
  ACTIVE_CONTROL_FIRST,
  ACTIVE_CONTROL_LAST,
};

enum UiPageNumber : uint8_t {
  PAGE_OSCILLATORS,
  PAGE_MIXER,
  
  PAGE_FILTER,
  
  PAGE_ENV_LFO,
  PAGE_VOICE_LFO,
  
  PAGE_MODULATIONS,
  
  PAGE_PART,
  PAGE_PART_ARPEGGIATOR,
  PAGE_PART_SEQUENCER,
  
  PAGE_MULTI,
  PAGE_MULTI_CLOCK,
  
  PAGE_PERFORMANCE,
  PAGE_KNOB_ASSIGN,
  
  PAGE_LIBRARY,
  PAGE_VERSION_MANAGER,
  PAGE_SYSTEM_SETTINGS, 
  PAGE_SYSTEM_SETTINGS_B,
  PAGE_OS_INFO,
  PAGE_CARD_INFO,
  INCREMENT_ENV,
  INCREMENT_MODULATION_SLOT,
};

typedef RotaryEncoder<EncoderALine, EncoderBLine, EncoderClickLine> Encoder;
typedef DebouncedSwitches<IOEnableLine, IOClockLine, IOInputLine, 8> Switches;
typedef HysteresisPotScanner<8, 0, 8, 7> Pots;

struct PageInfo;

struct EventHandlers {
  void (*OnInit)(PageInfo* info);
  void (*SetActiveControl)(ActiveControl);
  uint8_t (*OnIncrement)(int8_t);
  bool (*OnIncrementAndCycle)(int8_t, int8_t);
  uint8_t (*OnClick)();
  uint8_t (*OnPot)(uint8_t, uint8_t);
  uint8_t (*OnKey)(uint8_t);
  uint8_t (*OnNote)(uint8_t, uint8_t);
  uint8_t (*OnIdle)();
  void (*UpdateScreen)();
  void (*UpdateLeds)();
  void (*OnDialogClosed)(uint8_t, uint8_t);
};

enum DialogType : uint8_t {
  DIALOG_ERROR,
  DIALOG_INFO,
  DIALOG_CONFIRM,
  DIALOG_SELECT,
};

struct Dialog {
  DialogType dialog_type;
  uint8_t num_choices;
  ResourceId first_choice;
  const char* text;
  char* user_text;
};

struct PageInfo {
  uint8_t index;
  
  const EventHandlers* event_handlers;
  // For a standard parameter editing page, indices of the parameters to edit ;
  // or 0xf0 -- 0xf7 for the 8 assignable knobs.
  union {
    uint8_t data[8];
    Dialog dialog;
  };

  UiPageNumber next_page;
  uint8_t group;
  uint8_t led_pattern;
};

class Ui {
 public:
  Ui() = default;
  
  static void Init();
  static void Poll();
  static void DoEvents();
  static void FlushEvents() {
    queue_.Flush();
  }
  
  static uint8_t has_events() {
    return queue_.available();
  }
  
  static void ShowPageRelative(int8_t increment);
  static void MoveActiveControl(int8_t increment);
  static void ShowPage(UiPageNumber page, uint8_t initialize);
  static void ShowPage(UiPageNumber page) {
    ShowPage(page, 1);
  }
  static void ShowPreviousPage() {
    ShowPage(most_recent_non_system_page_);
  };

  static void ShowDialogBox(uint8_t dialog_id, Dialog dialog, uint8_t choice);
  static void CloseDialogBox(uint8_t return_value);
  
  static UiState& state() { return state_; }

  static void setStateValue(uint8_t index, uint8_t value) {
    state_.bytes()[index] = value;
  }

  static uint8_t getStateValue(uint8_t address) {
    return state_.bytes()[address];
  }


  static uint8_t& active_part() {
    return state_.active_part();
  }
  
  static uint8_t OnNote(uint8_t note, uint8_t velocity) {
    if (event_handlers_.OnNote && (*event_handlers_.OnNote)(note, velocity)) {
      queue_.AddEvent(0x3, 0x3f, 0xff);
      return 1;
    }
    return 0;
  }
  
  static uint8_t shifted() { return switches_.low(7); }
  
 private:
  static UiPageNumber active_page_;
  static UiPageNumber most_recent_non_system_page_;
   
  static uint8_t cycle_;
  static Encoder encoder_;
  static Switches switches_;
  static uint8_t inhibit_switch_;
  static Pots pots_;
  static UiState state_;
  static EventQueue<8> queue_;
  static uint8_t pot_value_[8];
  
  static uint8_t active_group_;
  static UiPageNumber most_recent_page_in_group_[9];
  static EventHandlers event_handlers_;
  static PageInfo page_info_;

  DISALLOW_COPY_AND_ASSIGN(Ui);
};

extern Ui ui;

}  // namespace ambika

#endif  // CONTROLLER_UI_H_
