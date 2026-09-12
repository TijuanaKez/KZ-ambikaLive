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
// Patch storage and SysEx transfer.

#ifndef CONTROLLER_STORAGE_H_
#define CONTROLLER_STORAGE_H_

#include <avr/pgmspace.h>

#include "avrlib/base.h"

#include "avrlib/filesystem/filesystem.h"
#include "avrlib/filesystem/file.h"
#include "controller/voicecard_tx.h"

namespace ambika {

using avrlib::FilesystemStatus;

enum StorageObject {
  STORAGE_OBJECT_PATCH,
  STORAGE_OBJECT_SEQUENCE,
  STORAGE_OBJECT_PROGRAM,
  STORAGE_OBJECT_MULTI, // \/ KZ Not sure why but these 2 are swapped compared to the info in the manual
  STORAGE_OBJECT_PART, // /\ KZ Not sure why but these 2 are swapped compared to the info in the manual
};

enum SysExReceptionState {
  RECEIVING_HEADER = 0,
  RECEIVING_COMMAND = 1,
  RECEIVING_DATA = 2,
  RECEIVING_FOOTER = 3,
  RECEPTION_OK = 4,
  RECEPTION_ERROR = 5,
};

enum SysExCommand {
    SYSEX_RECEIVE_PATCH_DATA = 0x01, // Recieve Patch dump
    SYSEX_RECEIVE_SEQUENCER_DATA = 0x02, // Recieve PartData::sequence_data dump
    SYSEX_RECEIVE_PROGRAM_DATA = 0x03, // According to StorageObject enum. Is this different to Patch + Part Data?
    SYSEX_RECEIVE_PART_DATA = 0x04, // Recieve PartData dump. According to Manual, but 0x04 and 0x05 seem switched around.
    SYSEX_RECEIVE_MULTI_DATA =  0x05, // Recieve MultiData dump
    SYSEX_RECEIVE_PATCH_NAME = 0x06, // Recieve Patch dump
    SYSEX_RECEIVE_SEQUENCE_NAME = 0x07, // Recieve PartData::sequence_data dump
    SYSEX_RECEIVE_PROGRAM_NAME = 0x08, // According to StorageObject enum. Is this different to Patch + Part Data?
    SYSEX_RECEIVE_PART_NAME = 0x09, // Parts dont have names but ......
    SYSEX_RECEIVE_MULTI_NAME =  0x0a, // Recieve MultiData dump
    SYSEX_POKE_COMMAND =  0x0f, // ???
    SYSEX_REQUEST_PATCH_DATA = 0x11, // Request Patch Data
    SYSEX_REQUEST_SEQUENCER_DATA = 0x12, // Request Sequencer Data
    SYSEX_REQUEST_PROGRAM_DATA =  0x13, // Request Patch bytes + PartData
    SYSEX_REQUEST_PART_DATA = 0x14, // Request PartData
    SYSEX_REQUEST_MULTI_DATA = 0x15, // Request MultiData + Patch and PartData
    SYSEX_REQUEST_PATCH_NAME =  0x16, // Request Patch Name
    SYSEX_REQUEST_SEQUENCE_NAME =  0x17, // Request Sequencer Name
    SYSEX_REQUEST_PROGRAM_NAME = 0x18, // Request Progam Name
    SYSEX_REQUEST_PART_NAME = 0x19, // Parts dont have names
    SYSEX_REQUEST_MULTI_NAME = 0x1a, // Request Multi?
    SYSEX_PEEK_COMMAND = 0x1f,
};

enum StorageDir {
  STORAGE_BANK,
  STORAGE_CLIPBOARD,
  STORAGE_PREVIOUS_CLIPBOARD,
  STORAGE_HISTORY
};

struct StorageLocation {
  StorageObject object;
  uint8_t part;
  uint8_t alias;
  uint8_t bank;
  uint8_t slot;
  char* name;

  inline uint16_t bank_slot() const {
    return wordOr(bank << 8u, slot);
  }
  
  inline uint8_t index() const {
    if (object == STORAGE_OBJECT_MULTI) {
      return kNumVoices * 3;
    } else {
      return part * 3 + object;
    }
  }
};

typedef void (*ObjectFn)(const StorageLocation& location);

static const uint16_t kFsInitTimeout = 750;

class Storage {
 public:
  Storage() = default;
  
  static void Init();
  static FilesystemStatus InitFilesystem() {
    scoped_resource<SdCardSession> session;
    return fs_.Init(kFsInitTimeout);
  }

  static inline void Tick() {
    fs_.Tick();
  }

  static uint8_t Checksum(const void* data, uint8_t size);
  
  static void SysExSend(const StorageLocation& location) {
    ForEachObject(location, &SysExSendObject);
  }
  
  static void Snapshot(const StorageLocation& location);
  static FilesystemStatus PreviousVersion(const StorageLocation& location);
  static FilesystemStatus NextVersion(const StorageLocation& location);
  
  static FilesystemStatus Copy(const StorageLocation& location) {
    return Save(STORAGE_CLIPBOARD, location);
  }
  static FilesystemStatus Paste(const StorageLocation location) {
    Snapshot(location);
    return Load(STORAGE_CLIPBOARD, location, 1);
  }
  static FilesystemStatus Swap(const StorageLocation location) {
    Snapshot(location);
    Save(STORAGE_CLIPBOARD, location);
    return Load(STORAGE_PREVIOUS_CLIPBOARD, location, 1);
  }
  static FilesystemStatus Load(const StorageLocation location) {
    if (has_user_changes(location)) {
      Snapshot(location);
    }
    return Load(STORAGE_BANK, location, 1);
  }
  
  static FilesystemStatus LoadName(const StorageLocation location) {
    return Load(STORAGE_BANK, location, 0);
  }
  
  static FilesystemStatus Save(const StorageLocation& location) {
    return Save(STORAGE_BANK, location);
  }
  
  static FilesystemStatus Mkfs() {
    scoped_resource<SdCardSession> session;
    InvalidatePendingSysExTransfer();
    return fs_.Mkfs();
  }
  
  static uint32_t GetFreeSpace() {
    scoped_resource<SdCardSession> session;
    InvalidatePendingSysExTransfer();
    return fs_.GetFreeSpace();
  }
  
  static uint8_t FileExists(const char* name) {
    return FileExists(name, 0);
  }
  static uint8_t FileExists(const char* name, char variable);

  static FilesystemStatus SpiCopy(uint8_t voice_id, const char* name, char variable, uint8_t page_size_nibbles);
  
  static uint16_t GetType() {
    return fs_.GetType();
  }
  
  static void SysExReceive(uint8_t byte);
  
  static inline uint8_t sysex_rx_state() {
    return sysex_rx_state_;
  }
  
  static inline uint8_t version(const StorageLocation& location) {
    return version_[location.index()];
  }

  static uint8_t has_user_changes(const StorageLocation& location);

  static void WriteMultiToEeprom();
  static uint8_t LoadMultiFromEeprom();

 private:
  static void InvalidatePendingSysExTransfer();
  
  static void Expand(const char* name, char variable);
  
  static FilesystemStatus Save(StorageDir type, const StorageLocation& location);

  static FilesystemStatus Load(StorageDir type, const StorageLocation& location, uint8_t load_contents);

  static char* GetFileName(StorageDir type, const StorageLocation& location);

  static uint16_t riff_size(const StorageLocation& location);
  static uint8_t object_size(const StorageLocation& location);
  static const uint8_t* object_data(const StorageLocation& location);
  static uint8_t* mutable_object_data(const StorageLocation& location);

  static void ForEachObject(const StorageLocation& source, ObjectFn object_fn);
  
  static void ReadObject(const StorageLocation& location);
  static void SysExSendObject(const StorageLocation& location);
  static void SysExSendRaw(uint8_t, uint8_t, const uint8_t*, uint8_t, bool);
  static void RIFFWriteObject(const StorageLocation& location);
  static void TouchObject(const StorageLocation& location);

  static void SysExParseCommand();
  static void SysExAcceptCommand();
  
  static void EepromWrite(const void* data, uint8_t size, uint8_t** offset_ptr);
  static uint8_t EepromRead(void* data, uint8_t size, uint8_t** offset_ptr);
  
  static uint8_t* buffer_;
  
  static uint16_t sysex_rx_bytes_received_;
  static uint16_t sysex_rx_expected_size_;
  static uint8_t sysex_rx_state_;
  static uint8_t sysex_rx_checksum_;
  static uint8_t sysex_rx_command_[2];
  
  static char tmp_buffer_[64];
  
  static uint8_t version_[kNumVoices * 3 + 1];
  
  static avrlib::Filesystem fs_;
  static avrlib::File file_;
  
  DISALLOW_COPY_AND_ASSIGN(Storage);
};

extern Storage storage;

}  // namespace ambika

#endif  // CONTROLLER_STORAGE_H_
