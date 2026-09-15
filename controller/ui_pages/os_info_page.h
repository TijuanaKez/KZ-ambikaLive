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
// Special UI page for triggering OS updates.

#ifndef CONTROLLER_UI_PAGES_OS_INFO_PAGE_H_
#define CONTROLLER_UI_PAGES_OS_INFO_PAGE_H_

#include "controller/controller.h"
#include "controller/ui_pages/ui_page.h"

namespace ambika {

class OsInfoPage : public UiPage {
 public:
  OsInfoPage() = default;
  
  static void OnInit(PageInfo* info);
  
  static uint8_t OnKey(uint8_t key);
  static uint8_t OnIncrement(int8_t increment);
  static void UpdateScreen();
  static void UpdateLeds();
#ifdef DIAGNOSTIC_BUILD
  // Clicking the encoder switches between the memory/headroom view and the
  // ordinary firmware-update view, so a diagnostic build can still flash voice
  // cards. Without this the update action is unreachable, because the
  // diagnostic screen took its page.
  static uint8_t OnClick();
#endif

 // NOTE: These now must always reflect exactly EventHandlers struct in ui.h
  static constexpr EventHandlers event_handlers_ PROGMEM = {
      OnInit,
      SetActiveControl,
      OnIncrement,
      OnIncrementAndCycle,
      OnClick,  // OsInfoPage::OnClick in diagnostic builds, UiPage's otherwise
      OnPot,
      OnKey,
      nullptr,
      OnIdle,
      UpdateScreen,
      UpdateLeds,
      OnDialogClosed,
  };


private:
  // The ordinary firmware-update implementation. Always compiled; in a
  // diagnostic build the public entry points above dispatch to it or to the
  // Memory* pair below.
  static void FirmwareOnInit(PageInfo* info);
  static uint8_t FirmwareOnKey(uint8_t key);
  static uint8_t FirmwareOnIncrement(int8_t increment);
  static void FirmwareUpdateScreen();
  static void FirmwareUpdateLeds();
#ifdef DIAGNOSTIC_BUILD
  static void MemoryOnInit(PageInfo* info);
  static uint8_t MemoryOnKey(uint8_t key);
  static uint8_t MemoryOnIncrement(int8_t increment);
  static void MemoryUpdateScreen();
  static void MemoryUpdateLeds();
  static uint8_t show_memory_;
#endif

  static void PrintVersionNumber(char* buffer, uint8_t number);
  //static void ReadVoicecardVersion();
  static void FindFirmwareFiles(uint8_t port);
  static void UpdateVoiceCard (uint8_t port);
  
  //static uint8_t voicecard_version_;
  //static uint8_t active_port_;
  static uint8_t found_firmware_files_;
#ifdef DIAGNOSTIC_BUILD
  // KZ MOD: latest audio render headroom reported by each voice card, polled
  // one card per redraw.
  static uint8_t audio_headroom_[kNumVoices];
  static uint8_t audio_headroom_index_;
#endif
  
  DISALLOW_COPY_AND_ASSIGN(OsInfoPage);
};

}  // namespace ambika

#endif  // CONTROLLER_UI_PAGES_OS_INFO_PAGE_H_
