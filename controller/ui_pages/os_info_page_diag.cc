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
// Diagnostic view of the OS info page: SRAM, stack watermark, reset cause
// and per-voice-card audio render headroom.
//
// Kept in its own translation unit so the host UI tests can compile it
// without substituting the whole firmware-update path, which needs the SD
// card, the voice card protocol and EEPROM.

#include "controller/ui_pages/os_info_page.h"

#include <avr/eeprom.h>

#include "avrlib/string.h"
#include "avrlib/watchdog_timer.h"

#include "controller/display.h"
#include "controller/midi_dispatcher.h"
#include "controller/diagnostics.h"
#include "controller/leds.h"
#include "controller/multi.h"
#include "controller/storage.h"

namespace ambika {

#ifdef DIAGNOSTIC_BUILD

// The memory view reads SRAM and polls voice cards for their render headroom.
// It deliberately avoids SD scanning, so a suspected memory fault can be
// observed without the filesystem running underneath it.
void OsInfoPage::MemoryOnInit(PageInfo* info) {
  UiPage::OnInit(info);
}

uint8_t OsInfoPage::MemoryOnIncrement(int8_t increment) {
  IGNORE_UNUSED(increment);
  return 1;
}

uint8_t OsInfoPage::MemoryOnKey(uint8_t key) {
  if (key == SWITCH_8) ui.ShowPreviousPage();
  return 1;
}

/* static */
uint8_t OsInfoPage::show_memory_ = 1;

// Both views live on this page in a diagnostic build. The memory view opens
// first; clicking the encoder swaps to the firmware-update view, so voice cards
// can still be flashed. Without that the update action is unreachable, because
// the diagnostic screen took its page.
void OsInfoPage::OnInit(PageInfo* info) {
  show_memory_ = 1;
  FirmwareOnInit(info);
  MemoryOnInit(info);
}

uint8_t OsInfoPage::OnClick() {
  show_memory_ = show_memory_ ? 0 : 1;
  if (!show_memory_) {
    // Re-probe: the card may have been swapped since the page opened.
    uint8_t port = (active_control_ > 0 && active_control_ < static_cast<int8_t>(kNumVoices))
        ? static_cast<uint8_t>(active_control_) : 0;
    FindFirmwareFiles(port);
  }
  return 1;
}

uint8_t OsInfoPage::OnIncrement(int8_t increment) {
  return show_memory_ ? MemoryOnIncrement(increment) : FirmwareOnIncrement(increment);
}

uint8_t OsInfoPage::OnKey(uint8_t key) {
  return show_memory_ ? MemoryOnKey(key) : FirmwareOnKey(key);
}

void OsInfoPage::UpdateScreen() {
  if (show_memory_) {
    MemoryUpdateScreen();
  } else {
    FirmwareUpdateScreen();
  }
}

void OsInfoPage::UpdateLeds() {
  if (show_memory_) {
    MemoryUpdateLeds();
  } else {
    FirmwareUpdateLeds();
  }
}

/* static */
uint8_t OsInfoPage::audio_headroom_[kNumVoices];

/* static */
uint8_t OsInfoPage::audio_headroom_index_;

void OsInfoPage::MemoryUpdateScreen() {
  // Poll one card per redraw. Each query is a blocking SPI transaction with a
  // settling delay, so asking all six at once would stall the UI.
  audio_headroom_[audio_headroom_index_] =
      voicecard_tx.GetAudioHeadroom(audio_headroom_index_);
  ++audio_headroom_index_;
  if (audio_headroom_index_ >= kNumVoices) {
    audio_headroom_index_ = 0;
  }

  char* buffer = display.line_buffer(0);
  memcpy_P(buffer, PSTR("RAM "), 4);
  UnsafeItoa<int16_t>(FreeSram(), 5, &buffer[4]);
  AlignRight(&buffer[4], 5);
  memcpy_P(&buffer[10], PSTR("LOW "), 4);
  UnsafeItoa<int16_t>(UntouchedSram(), 5, &buffer[14]);
  AlignRight(&buffer[14], 5);
  // High-water mark of the MIDI output queue, against its 64-byte size.
  memcpy_P(&buffer[20], PSTR("MID "), 4);
  UnsafeItoa<int16_t>(midi_dispatcher.out_peak(), 3, &buffer[24]);
  AlignRight(&buffer[24], 3);
  memcpy_P(&buffer[28], PSTR("RST "), 4);
  buffer[32] = NibbleToAscii(highNibble(ResetCause()));
  buffer[33] = NibbleToAscii(lowNibble(ResetCause()));
  // Which code path was running when the stack reached its deepest point. LOW
  // on its own is a latch with no context; this says who set it.
  static const char context_names[] PROGMEM =
      "idl" "ui " "lod" "sav" "bak" "sys" "sdt" "mid";
  uint8_t context = stack_low_context < STACK_CTX_LAST ? stack_low_context
                                                       : U8(STACK_CTX_IDLE);
  memcpy_P(&buffer[36], &context_names[context * 3], 3);

  // Audio render headroom per voice card: free space left in the audio buffer
  // just before a block was rendered. Around 40 is healthy, rising means the
  // card is falling behind, 255 means its audio buffer starved.
  buffer = display.line_buffer(1);
  memcpy_P(buffer, PSTR("AUD "), 4);
  for (uint8_t i = 0; i < kNumVoices; ++i) {
    char* cell = &buffer[4 + i * 4];
    if (audio_headroom_[i] == kAudioHeadroomUnsupported) {
      // No counter on that card -- v1.1 firmware, or no card in the slot.
      memcpy_P(cell, PSTR(" --"), 3);
    } else {
      UnsafeItoa<int16_t>(audio_headroom_[i], 3, cell);
      AlignRight(cell, 3);
    }
  }
  memcpy_P(&buffer[29], PSTR("clk:fw"), 6);
  memcpy_P(&buffer[36], PSTR("exit"), 4);
}

void OsInfoPage::MemoryUpdateLeds() {
  leds.set_pixel(LED_8, 0xf0);
}

#endif  // DIAGNOSTIC_BUILD

}  // namespace ambika
