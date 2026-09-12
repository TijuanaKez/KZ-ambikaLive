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
// Base UI page class.

#ifndef CONTROLLER_UI_PAGES_UI_PAGE_H_
#define CONTROLLER_UI_PAGES_UI_PAGE_H_

#include "controller/ui.h"

namespace ambika {

enum EditMode : uint8_t {
  EDIT_IDLE,
  EDIT_STARTED_BY_ENCODER,
  EDIT_STARTED_BY_POT,
};

class UiPage {
 public:
  UiPage() = default;

  static void OnInit(PageInfo* info);
  static void SetActiveControl(ActiveControl active_control);

  static uint8_t OnIncrement(int8_t increment);
  static bool OnIncrementAndCycle(int8_t parameter_index, int8_t part);
  static uint8_t OnClick();
  static uint8_t OnPot(uint8_t index, uint8_t value);
  static uint8_t OnKey(uint8_t key);
  static uint8_t OnIdle();
  static void OnDialogClosed(uint8_t dialog_id, uint8_t return_value);

  static void UpdateScreen();
  static void UpdateLeds();

  // Function pointer table needs to be done like this, rather than the usual
  // polymorphic way, because AVR-GCC puts vtables in SRAM.
  // In subclasses, make sure that this table comes after the overriding
  // function declarations in the subclass's definition, otherwise the function names
  // will point to those here.

  /*
  --- More info about EventHandlers ---
  All these tables are basically a big old hack (credit to Emilie) to allow a limited form of object oriented programming (polymorphism) in C++ without using the actual C++ classes. 
  When you use actual classes, the compiler generates these function pointer tables for you, and stores them in the program code. 
  So that makes things much simpler. Normally they are put into RAM when the program is loaded, which on AVR is the (limited) SRAM. 
  However since they take up space and don’t change throughout program execution, they should be put in AVR PROGMEM instead. 
  But a limitation in the AVR-G++ compiler, prevents that from happening. It’s actually an open issue against the GCC AVR backend to do this, but compiler work for AVR is going a bit stale at the moment…
  */

 // NOTE: These now must always reflect exactly EventHandlers struct in ui.h
  static constexpr EventHandlers event_handlers_ PROGMEM = {
      OnInit,
      SetActiveControl,
      OnIncrement,
      OnIncrementAndCycle,
      OnClick,
      OnPot,
      OnKey,
      nullptr,
      OnIdle,
      UpdateScreen,
      UpdateLeds,
      OnDialogClosed,
  };

protected:
  static EditMode edit_mode_;
  static int8_t active_control_;
  static PageInfo* info_;
  
  DISALLOW_COPY_AND_ASSIGN(UiPage);
};

}  // namespace ambika

#endif  // CONTROLLER_UI_PAGES_PARAMETER_EDITOR_H_
