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

#include "controller/ui_pages/os_info_page.h"

#include <avr/eeprom.h>

#include "avrlib/string.h"
#include "avrlib/watchdog_timer.h"

#include "controller/display.h"
#include "controller/diagnostics.h"
#include "controller/leds.h"
#include "controller/multi.h"
#include "controller/storage.h"

namespace ambika {

#ifdef DIAGNOSTIC_BUILD

// In diagnostic images this page only reads SRAM. Avoid SD scanning and
// voicecard transactions while observing a possible memory fault.
void OsInfoPage::OnInit(PageInfo* info) {
  UiPage::OnInit(info);
}

uint8_t OsInfoPage::OnIncrement(int8_t increment) {
  IGNORE_UNUSED(increment);
  return 1;
}

uint8_t OsInfoPage::OnKey(uint8_t key) {
  if (key == SWITCH_8) ui.ShowPreviousPage();
  return 1;
}

void OsInfoPage::UpdateScreen() {
  char* buffer = display.line_buffer(0);
  memcpy_P(buffer, PSTR("KZ DIAG3 RAM "), 13);
  UnsafeItoa<int16_t>(FreeSram(), 5, &buffer[13]);
  AlignRight(&buffer[13], 5);
  memcpy_P(&buffer[21], PSTR("LOW "), 4);
  UnsafeItoa<int16_t>(UntouchedSram(), 5, &buffer[25]);
  AlignRight(&buffer[25], 5);
  buffer = display.line_buffer(1);
  memcpy_P(buffer, PSTR("RST "), 4);
  buffer[4] = NibbleToAscii(highNibble(ResetCause()));
  buffer[5] = NibbleToAscii(lowNibble(ResetCause()));
  memcpy_P(&buffer[9], PSTR("LOW=untouched bytes"), 19);
  memcpy_P(&buffer[36], PSTR("exit"), 4);
}

void OsInfoPage::UpdateLeds() {
  leds.set_pixel(LED_8, 0xf0);
}

#else

/* static */
uint8_t OsInfoPage::found_firmware_files_;

/* static */
void OsInfoPage::OnInit(PageInfo* info) {
  IGNORE_UNUSED(info);
  active_control_ = 0;
  FindFirmwareFiles(0);
}

/* static */
void OsInfoPage::FindFirmwareFiles(uint8_t port) {
  found_firmware_files_ = 0;
  if (storage.FileExists(PSTR("/AMBIKA.BIN"))) {
    found_firmware_files_ |= 1;
  }
  
  if (storage.FileExists(PSTR("/VOICE$.BIN"), '1' + port)) {
    found_firmware_files_ |= 2;
  }
}

/* static */
uint8_t OsInfoPage::OnIncrement(int8_t increment) {
  active_control_ = Clip(active_control_ + increment, 0_u8, U8(kNumVoices + 1));
  FindFirmwareFiles(active_control_);
  // TODO figure out what the return value does
  return 1;
}

/* static */
uint8_t OsInfoPage::OnKey(uint8_t key) {
  switch(key) {
    default:
      break;
    case SWITCH_1:
      {
        if (byteAnd(found_firmware_files_, 1)) {
          // Force a reset into the SD card loader.
          eeprom_write_byte(kFirmwareUpdateFlagPtr, 1);
          SystemReset(0);
          while (1) { }
        }
      }
      break;
      
    case SWITCH_4:
      {
        if (active_control_ < kNumVoices){
          UpdateVoiceCard(active_control_);
        } else {
          for (uint8_t p=0; p < kNumVoices; p++){
            FindFirmwareFiles(p);
            UpdateVoiceCard (p);
          }
        }
      }
      break;
      
    case SWITCH_8:
      ui.ShowPreviousPage();
      break;
  }
  return 1;
}

void OsInfoPage::UpdateVoiceCard (uint8_t port){
  if (byteAnd(found_firmware_files_, 2)) {
    // Resets the voicecard into its bootloader.
    voicecard_tx.EnterFirmwareUpdateMode(port);
    // Wait while the voicecard reboots.
    ConstantDelay(100);
    uint8_t page_size_nibbles = 0;
    for (uint8_t i = 0; i < 250; ++i) {
      // Confirms the reset to the bootloader.
      page_size_nibbles = voicecard_tx.EnterFirmwareUpdateMode(port);
    }
    if (page_size_nibbles) {
      // Sends the firmware data in nibblized format.
      storage.SpiCopy(port, PSTR("/VOICE$.BIN"), '1' + port, page_size_nibbles);
      voicecard_tx.EnterFirmwareUpdateMode(port);
    }
  }
}

/* static */
void OsInfoPage::PrintVersionNumber(char* buffer, uint8_t number) {
  *buffer++ = 'v';
  *buffer++ = '0' + highNibble(number);
  *buffer++ = '.';
  *buffer++ = '0' + lowNibble(number);
}

/* static */
void OsInfoPage::UpdateScreen() {
  char* buffer = display.line_buffer(0) + 1;
  memcpy_P(&buffer[0], PSTR("KZambika"), 8);
  PrintVersionNumber(&buffer[10], kSystemVersion);

  memcpy_P(&buffer[15], PSTR("port 1 device ?  "), 17);
  Word version_number = voicecard_tx.GetVersion(active_control_);
  buffer[20] = '1' + active_control_;
  uint8_t valid_device = 0;
  if (version_number.bytes[0] < SLAVE_ID_LAST && version_number.bytes[0] > 0) {
    buffer[29] = '0' + version_number.bytes[0];
    PrintVersionNumber(&buffer[35], version_number.bytes[1]);
    valid_device = 1;
  }
  if (active_control_ == kNumVoices){
    memcpy_P(&buffer[29], PSTR("ALL"), 3);
  }
  buffer[14] = kDelimiter;
  buffer = display.line_buffer(1) + 1;
  if (byteAnd(found_firmware_files_, 1)) {
    strncpy_P(&buffer[0], PSTR("upgrade"), 7);
  }
  buffer[14] = kDelimiter;
  if (byteAnd(found_firmware_files_, 2)) {
    if (valid_device) {
      strncpy_P(&buffer[15], PSTR("upgrade"), 7);
    } else {
      strncpy_P(&buffer[15], PSTR("install"), 7);
    }
  }
  strncpy_P(&buffer[35], PSTR("exit"), 4);
  IGNORE_UNUSED(valid_device);
}

/* static */
void OsInfoPage::UpdateLeds() {
  leds.set_pixel(LED_8, 0xf0);
  if (byteAnd(found_firmware_files_, 1)) {
    leds.set_pixel(LED_1, 0x0f);
  }
  if (byteAnd(found_firmware_files_, 2)) {
    leds.set_pixel(LED_4, 0x0f);
  }
}

#endif  // DIAGNOSTIC_BUILD

}  // namespace ambika
