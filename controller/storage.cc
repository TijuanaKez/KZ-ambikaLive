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

#include "controller/storage.h"

#include <avr/eeprom.h>

#include "avrlib/bitops.h"
#include "avrlib/op.h"
#include "avrlib/string.h"

#include "controller/display.h"
#include "controller/midi_dispatcher.h"
#include "controller/multi.h"
#include "controller/system_settings.h"
#include "controller/ui.h"

namespace ambika {


static constexpr uint32_t kRiffTag = FourCC('R', 'I', 'F', 'F');
static constexpr uint32_t kFormatTag = FourCC('M', 'B', 'K', 'S');
static constexpr uint32_t kNameTag = FourCC('n', 'a', 'm', 'e');
static constexpr uint32_t kObjectTag = FourCC('o', 'b', 'j', ' ');

using namespace avrlib;

static constexpr uint8_t sysex_header[] PROGMEM = {
  0xf0,  // <SysEx>
  0x00, 0x21, 0x02,  // Mutable instruments manufacturer id.
  0x00, 0x04,  // Product ID for Ambika-6.
};

/* static */
Filesystem Storage::fs_;

/* static */
File Storage::file_;

/* static */
uint8_t* Storage::buffer_;

/* static */
uint16_t Storage::sysex_rx_bytes_received_;

/* static */
uint8_t Storage::sysex_rx_checksum_;

/* static */
uint8_t Storage::sysex_rx_state_;

/* static */
uint16_t Storage::sysex_rx_expected_size_;

/* static */
uint8_t Storage::sysex_rx_command_[2];

/* static */
char Storage::tmp_buffer_[64];

/* static */
uint8_t Storage::version_[kNumVoices * 3 + 1];

/* static */
void Storage::Init() {
  buffer_ = fs_.buffer();
}

/* static */
uint8_t Storage::Checksum(const void* data, uint8_t size) {
  uint8_t s = 0;
  auto d = static_cast<const uint8_t*>(data);
  while (size--) {
    s += *d++;
  }
  return s;
}

/* static */
void Storage::WriteMultiToEeprom() {
  uint8_t* address = nullptr;
  for (uint8_t i = 0; i < kNumParts; ++i) {
    EepromWrite(multi.part(i).raw_data_readonly(), PartData::sizeBytes(), &address);
    EepromWrite(multi.part(i).raw_patch_data_readonly(), Patch::sizeBytes(), &address);
  }
  EepromWrite(&multi.data(), MultiData::size(), &address);
}

/* static */
uint8_t Storage::LoadMultiFromEeprom() {
  uint8_t* address = nullptr;
  uint8_t success = 1;
  for (uint8_t i = 0; i < kNumParts; ++i) {
    success = success && Storage::EepromRead(
        multi.part(i).raw_data(), PartData::sizeBytes(), &address);
    success = success && Storage::EepromRead(
        multi.part(i).raw_patch_data(), Patch::sizeBytes(), &address);
  }
  success = success && Storage::EepromRead(&multi.data(), MultiData::size(), &address);
  return success;
}


/* static */
void Storage::EepromWrite(const void* data, uint8_t size, uint8_t** offset_ptr) {
  eeprom_write_block(data, static_cast<void*>(*offset_ptr), size);
  *offset_ptr += size;
  eeprom_write_byte(*offset_ptr, Storage::Checksum(data, size));
  *offset_ptr += 1;
}

/* static */
uint8_t Storage::EepromRead(void* data, uint8_t size, uint8_t** offset_ptr) {
  eeprom_read_block(data, static_cast<void*>(*offset_ptr), size);
  *offset_ptr += size;
  uint8_t expected_checksum = eeprom_read_byte(*offset_ptr);
  *offset_ptr += 1;
  return Storage::Checksum(data, size) == expected_checksum;
}

/* static */
uint8_t Storage::object_size(const StorageLocation& location) {
  switch (location.object) {
    case STORAGE_OBJECT_PATCH:
      return Patch::sizeBytes();
      
    case STORAGE_OBJECT_SEQUENCE:
      return PartData::sequence_data_size;

    case STORAGE_OBJECT_PROGRAM:
      // program is PartData + patch data?
      return Part::sizeBytes();

    case STORAGE_OBJECT_PART:
      return PartData::sizeBytes();
      
    case STORAGE_OBJECT_MULTI:
      return MultiData::size();

    default:
      return 0;
  }
}

/* static */
uint16_t Storage::riff_size(const StorageLocation& location) {
  constexpr uint8_t extra_size = 12; // TODO is this a header?
  switch (location.object) {
    case STORAGE_OBJECT_MULTI:
      {
        uint16_t part_size = (Patch::sizeBytes() + extra_size + PartData::sizeBytes() + extra_size);
        return part_size * kNumParts + MultiData::size() + extra_size;
      }
    case STORAGE_OBJECT_PROGRAM:
      return Patch::sizeBytes() + extra_size + PartData::sizeBytes() + extra_size;
    default:
      return object_size(location) + extra_size;
  }
}

/* static */
const uint8_t* Storage::object_data(const StorageLocation& location) {
  switch (location.object) {
    case STORAGE_OBJECT_PATCH:
      return multi.part(location.part).raw_patch_data_readonly();
    case STORAGE_OBJECT_SEQUENCE:
      return multi.part(location.part).raw_sequence_data_readonly();
    case STORAGE_OBJECT_PART:
      return multi.part(location.part).raw_data_readonly();
    case STORAGE_OBJECT_MULTI:
      return multi.data().bytes();
    case STORAGE_OBJECT_PROGRAM:
      // program is patch + part, but it's not valid to do this
      // fall-through
    default:
      // TODO is this right?
       return nullptr;
  }
}

/* static */
uint8_t* Storage::mutable_object_data(const StorageLocation& location) {
  switch (location.object) {
    case STORAGE_OBJECT_PATCH:
      return multi.part(location.part).raw_patch_data();
    case STORAGE_OBJECT_SEQUENCE:
      return multi.part(location.part).raw_sequence_data();
    case STORAGE_OBJECT_PART:
      return multi.part(location.part).raw_data();
    case STORAGE_OBJECT_MULTI:
      return multi.data().bytes();
    case STORAGE_OBJECT_PROGRAM:
      // program is patch + part, but it's not valid to do this
      // fall-through
    default:
      // TODO is this right?
      return nullptr;
  }
}

/* static */
uint8_t Storage::has_user_changes(const StorageLocation& location) {
  if (location.object == STORAGE_OBJECT_MULTI) {
    return multi.flags() & FLAG_HAS_USER_CHANGE;
  } else {
    return multi.part(location.part).flags() & FLAG_HAS_USER_CHANGE;
  }
}

/* static */
void Storage::Snapshot(const StorageLocation& location) {
  StorageLocation l = location;
  uint8_t version = version_[location.index()];
  l.slot = version;
  Save(STORAGE_HISTORY, l);
  
  // Create a gap in the version number sequence to prevent forwarding.
  ++version;
  l.slot = version;
  char* forward_version_name = GetFileName(STORAGE_HISTORY, l);
  {
    scoped_resource<SdCardSession> session;
    InvalidatePendingSysExTransfer();
    fs_.Unlink(forward_version_name);
  }
  version_[location.index()] = version;
}

/* static */
FilesystemStatus Storage::PreviousVersion(const StorageLocation& location) {
  StorageLocation l = location;
  uint8_t version = version_[location.index()] - 1;
  l.slot = version;
  FilesystemStatus status = Load(STORAGE_HISTORY, l, 1);
  if (status == FS_OK) {
    version_[location.index()] = version;
  }
  return status;
}

/* static */
FilesystemStatus Storage::NextVersion(const StorageLocation& location) {
  StorageLocation l = location;
  uint8_t version = version_[location.index()] + 1;
  l.slot = version;
  FilesystemStatus status = Load(STORAGE_HISTORY, l, 1);
  if (status == FS_OK) {
    version_[location.index()] = version;
  }
  return status;
}

/* static */
void Storage::ForEachObject(const StorageLocation& location, ObjectFn object_fn) {
  StorageLocation destination = location;
  destination.alias = 0;
  switch (destination.object) {
    case STORAGE_OBJECT_PATCH:
    case STORAGE_OBJECT_SEQUENCE:
    case STORAGE_OBJECT_PART:
      (*object_fn)(destination);
      break;
      
    case STORAGE_OBJECT_PROGRAM:
      destination.object = STORAGE_OBJECT_PATCH;
      (*object_fn)(destination);
      destination.object = STORAGE_OBJECT_PART;
      (*object_fn)(destination);
      break;
      
    case STORAGE_OBJECT_MULTI:
      (*object_fn)(destination);
      for (uint8_t i = 0; i < kNumParts; ++i) {
        destination.part = i;
        destination.alias = i + 1;
        destination.object = STORAGE_OBJECT_PATCH;
        (*object_fn)(destination);
        destination.object = STORAGE_OBJECT_PART;
        (*object_fn)(destination);
      }
      break;
  }
}

/* static */
void Storage::ReadObject(const StorageLocation& location) {
  uint8_t* data = mutable_object_data(location);
  uint8_t size = object_size(location);
  if (size) {
    memcpy(data, buffer_, size);
  }
}

/* static */
void Storage::TouchObject(const StorageLocation& location) {
  // Recompute the dependent variables and update the voicecard. Mark that the
  // object is freshly loaded and has received no user changes.
  switch (location.object) {
    case STORAGE_OBJECT_PATCH:
      multi.part(location.part).TouchPatch();
      break;

    case STORAGE_OBJECT_PART:
      /* fall through */
    case STORAGE_OBJECT_SEQUENCE:
      multi.part(location.part).Touch();
      break;
    
    case STORAGE_OBJECT_MULTI:
      multi.Touch();
      break;
    case STORAGE_OBJECT_PROGRAM:
      // program is patch and sequence?
      multi.part(location.part).TouchPatch();
      multi.part(location.part).Touch();
      break;
    default:
      break;
  }
}

/* static */
void Storage::SysExSendObject(const StorageLocation& l) {
  SysExSendRaw(l.object + 1, l.alias, object_data(l), object_size(l), false);
}

/* static */
void Storage::SysExSendRaw(uint8_t command, uint8_t argument, const uint8_t* data, uint8_t size, bool send_address) {
  midi_dispatcher.Flush();
  for (uint8_t i = 0; i < sizeof(sysex_header); ++i) {
    midi_dispatcher.SendBlocking(pgm_read_byte(sysex_header + i));
  }
  midi_dispatcher.SendBlocking(command);
  midi_dispatcher.SendBlocking(argument);
  // Outputs the data.
  uint8_t checksum = 0;
  if (send_address) {
    // OK on AVR (not a narrowing conversion)
    auto address = reinterpret_cast<uint16_t>(data);
    auto addr_low = lowByte(address);
    auto addr_high = highByte(address);
    checksum += addr_low;
    checksum += addr_high;
    midi_dispatcher.SendBlocking(highNibble(addr_low));
    midi_dispatcher.SendBlocking(lowNibble(addr_low));
    midi_dispatcher.SendBlocking(highNibble(addr_high));
    midi_dispatcher.SendBlocking(lowNibble(addr_high));
  }
  for (uint8_t i = 0; i < size; ++i) {
    checksum += data[i];
    midi_dispatcher.SendBlocking(highNibble(data[i]));
    midi_dispatcher.SendBlocking(lowNibble(data[i]));
  }
  // Outputs a checksum.
  midi_dispatcher.SendBlocking(highNibble(checksum));
  midi_dispatcher.SendBlocking(lowNibble(checksum));

  // End of SysEx block.
  midi_dispatcher.SendBlocking(0xf7);
  midi_dispatcher.Flush();
}

/* static */
void Storage::RIFFWriteObject(const StorageLocation& location) {
  const uint8_t* data = object_data(location);
  uint8_t size = object_size(location);
  
  LongWord w;
  uint16_t written;

  // Write the RIFF header.
  w.value = kObjectTag;
  file_.Write(w.bytes, 4, &written);
  w.value = size + 4;
  file_.Write(w.bytes, 4, &written);
  // Write the position words.
  w.value = 0;
  w.bytes[0] = location.object + 1;
  w.bytes[1] = location.alias;
  file_.Write(w.bytes, 4, &written);
  
  // Write the data.
  file_.Write(data, size, &written);
}

/* static */
FilesystemStatus Storage::Load(StorageDir type, const StorageLocation& location, uint8_t load_contents) {
  {
    // Load data from SD

    /*
  All files are stored in RIFF format.

  The first 4 bytes of the file is the ‘RIFF’ string.
  The next 4 bytes are the total size of the file in bytes, little endian format.
  The next 4 bytes is the ‘MBKS’ string.

  This is followed by one or many chunks. Each chunk consists of a 4 chars identifier, a 32-bit integer indicating the size N of the “payload” data, and N bytes of “payload” data. Two types of chunk are present in a file:

    The name chunk is always 16 bytes long and contains the name of the object (multi, patch, program…) stored in the file. The 16th byte must be a null character ; and the name must be padded with spaces.
    The obj chunk contains data that will be copied into one of Ambika’s internal data structures. It consists of:
    A byte indicating the data structure (1: Patch, 2: PartData.sequence_data, 4: MultiData, 5: PartData).
    A byte indicating the part number the data must be loaded into (0: current part or global, 1: part 1, etc.)
    2 null bytes.
    The actual data.
*/
    scoped_resource<SdCardSession> session;

    FilesystemStatus s;
    char* name = GetFileName(type, location);
  
    file_.Close();
    InvalidatePendingSysExTransfer();
  
    s = file_.Open(name, FA_READ | FA_OPEN_EXISTING, kFsInitTimeout);
    if (s != FS_OK) {
      return s;
    }
  
    LongWord id;
    LongWord size;
    uint16_t read;
  
    file_.Read(id.bytes, 4, &read); // Read and check the RIFF header
    if (id.value != kRiffTag) { 
      file_.Close();
      return FS_BAD_FILE_FORMAT;
    }
    // Skip the size.
    file_.Read(size.bytes, 4, &read);
    file_.Read(id.bytes, 4, &read); // Read and check file format tag
    if (id.value != kFormatTag) {
      file_.Close();
      return FS_BAD_FILE_FORMAT;
    }
  
    while (!file_.eof()) {
      // Load a chunk
      file_.Read(id.bytes, 4, &read); // Chunk idenitifer
      file_.Read(size.bytes, 4, &read); // Payload size
      uint8_t skip_data = 1;
    
      if (id.value == kObjectTag && load_contents) { // Object Chunk
        file_.Read(id.bytes, 4, &read);
        StorageLocation destination { // Load the object
            .object = static_cast<StorageObject>(id.bytes[0] - 1),
            .part = U8(id.bytes[1] == 0 ? location.part : (id.bytes[1] - 1)),
            .alias = 0,
            .bank = 0,
            .slot = 0,
            .name = nullptr
        };

        uint8_t expected_size = object_size(destination);
        if (expected_size == size.value - 4) {
          uint8_t* data = mutable_object_data(destination);
          file_.Read(data, expected_size, &read);
          skip_data = 0;
        }
      } else if (id.value == kNameTag && location.name) { // Name Chunk. Size.value should be 16
        file_.Read(location.name, size.value, &read);
        skip_data = 0;
      }
      if (skip_data) {
        file_.Seek(file_.tell() + size.value);
      }
    }

    file_.Close();
  }
  
  // Refresh all datastructures.
  if (load_contents) {
    ForEachObject(location, TouchObject);
  }

  return FS_OK;
}

/* static */
FilesystemStatus Storage::Save(StorageDir type, const StorageLocation& location) {
  scoped_resource<SdCardSession> session;

  FilesystemStatus s;
  char* name = GetFileName(type, location);

  file_.Close();
  InvalidatePendingSysExTransfer();
  
  // Create a backup of the older version.
  if (type == STORAGE_CLIPBOARD || (type == STORAGE_BANK && system_settings.data().autobackup())) {
    char* backup_name = tmp_buffer_ + 32;
    strcpy(backup_name, name);
    backup_name[strlen(backup_name) - 3] = '~';
    
    // Remove the old backup.
    fs_.Unlink(backup_name);
    
    // Rename current version with the backup name.
    fs_.Rename(name, backup_name);
  }
  
  // Attempt to open.
  s = file_.Open(name, FA_WRITE | FA_CREATE_ALWAYS, kFsInitTimeout);
  if (s == FS_PATH_NOT_FOUND) {
    fs_.Mkdirs(name);
    s = file_.Open(name, FA_WRITE | FA_CREATE_ALWAYS, kFsInitTimeout);
  }
  
  // There's nothing we can do if Open failed at this stage.
  if (s != FS_OK) {
    return s;
  }
  
  LongWord w;
  uint16_t written;
  
  // RIFF header.
  w.value = kRiffTag;
  file_.Write(w.bytes, 4, &written);
  w.value = 4 + 24 + riff_size(location);
  file_.Write(w.bytes, 4, &written);
  w.value = kFormatTag;
  file_.Write(w.bytes, 4, &written);
  
  // NAME block.
  w.value = kNameTag;
  file_.Write(w.bytes, 4, &written);
  w.value = 16;
  file_.Write(w.bytes, 4, &written);
  file_.Write(location.name, 16, &written);

  // Write subchunks.
  ForEachObject(location, &RIFFWriteObject);
  
  file_.Close();
  return FS_OK;
}

/* static */
char* Storage::GetFileName(StorageDir type, const StorageLocation& location) {
  // Generate a path string for SD Filesystem
  char* p = tmp_buffer_;

  // Root.
  *p++ = '/';

  // Object name.
  ResourcesManager::LoadStringResource(STR_RES_PATCH + location.object, p, 32);
  p += strlen(p);
  
  // Path.
  if (type == STORAGE_CLIPBOARD || type == STORAGE_PREVIOUS_CLIPBOARD) {
    strcat_P(p, PSTR("/CLIPBRD/CLIPBRD"));
    p += strlen(p);
  } else if (type == STORAGE_HISTORY) {
    strcat_P(p, PSTR("/HISTORY/"));
    p += strlen(p);
    if (location.object != STORAGE_OBJECT_MULTI) {
      *p++ = '0' + (location.part);
      *p++ = '/';
    }
  } else if (type == STORAGE_BANK) {
    strcat_P(p, PSTR("/BANK/"));
    p += strlen(p);
    *p++ = 'A' + (location.bank);
    *p++ = '/';
  }
  
  // Name. (3 digit number starting at 000)
  if (type != STORAGE_CLIPBOARD && type != STORAGE_PREVIOUS_CLIPBOARD) {
    UnsafeItoa<int16_t>(location.slot, 3, p);
    PadRight(p, 3, '0');
    p += 3;
  }
  
  // Extension dot.
  *p++ = '.';
  
  // Extension name.
  ResourcesManager::LoadStringResource(STR_RES_PATCH + location.object, p, 3);
  p += 3;
  
  if (type == STORAGE_PREVIOUS_CLIPBOARD) {
    p[-3] = '~';
  }
  *p = '\0';
  
  return tmp_buffer_;
}

/* static */
void Storage::Expand(const char* name, char variable) {
  strcpy_P(tmp_buffer_, name);
  for (uint8_t i = 0; i < sizeof(tmp_buffer_); ++i) {
    if (tmp_buffer_[i] == '$') {
      tmp_buffer_[i] = variable;
    }
  }
}

/* static */
FilesystemStatus Storage::SpiCopy(uint8_t voice_id, const char* name, char variable, uint8_t page_size_nibbles) {
  scoped_resource<SdCardSession> session;
  file_.Close();
  InvalidatePendingSysExTransfer();
  
  FilesystemStatus s;
  Expand(name, variable);
  s = file_.Open(tmp_buffer_, FA_READ | FA_OPEN_EXISTING, kFsInitTimeout);
  if (s != FS_OK) {
    return s;
  }
  uint8_t bytes[4];
  uint16_t page_count = 0;
  uint16_t page_size = U8U8Mul(page_size_nibbles, 16);
  uint8_t byte_count = 0;
  while (!file_.eof()) {
    uint16_t read;

    // Read a few bytes.
    {
      scoped_resource<SdCardSession> session;
      s = file_.Read(bytes, 4, &read);
    }
    for (uint8_t i = 0; i < read; ++i) {
      if (voicecard_tx.WriteAsNibbles(voice_id, bytes[i]) != byte_count) {
        return FS_COPY_ERROR;
      }
      ++page_count;
      ++byte_count;
    }
    // Pause every time a page is full to let the voicecard write its
    // input buffer to the flash.
    if (page_count == page_size) {
      ConstantDelay(15);
      page_count = 0;
    }
  }
  file_.Close();
  return s;
}

/* static */
uint8_t Storage::FileExists(const char* name, char variable) {
  scoped_resource<SdCardSession> session;

  file_.Close();
  InvalidatePendingSysExTransfer();
  
  FilesystemStatus s;
  Expand(name, variable);
  s = file_.Open(tmp_buffer_, FA_READ | FA_OPEN_EXISTING, kFsInitTimeout);
  file_.Close();
  return (s == FS_OK);
}


/* static */
void Storage::SysExParseCommand() {
  sysex_rx_bytes_received_ = 0;
  sysex_rx_state_ = RECEIVING_DATA;
  switch (static_cast<SysExCommand>(sysex_rx_command_[0])) {
    case SYSEX_RECEIVE_PATCH_DATA: // Recieve Patch dump
    case SYSEX_RECEIVE_SEQUENCER_DATA: // Recieve PartData::sequence_data dump
    case SYSEX_RECEIVE_PROGRAM_DATA: // Program? Not listed in Manual
    case SYSEX_RECEIVE_MULTI_DATA: // Recieve PartData dump. According to Manual, but 0x04 and 0x05 seem switched around.
    case SYSEX_RECEIVE_PART_DATA: // Recieve MultiData dump
      {
        StorageLocation location {
          .object = static_cast<StorageObject>(sysex_rx_command_[0] - 1),
          .part = 0,
          .alias = 0,
          .bank = 0,
          .slot = 0,
          .name = nullptr
        };
        sysex_rx_expected_size_ = object_size(location);
      }
      break;
      
    case SYSEX_POKE_COMMAND:
      // POKE command contains 2 bytes of address + $argument bytes of data.
      {
        sysex_rx_expected_size_ = 2 + sysex_rx_command_[1];
      }
      break;

    case SYSEX_REQUEST_PATCH_DATA: // Request Patch Data
    case SYSEX_REQUEST_SEQUENCER_DATA: // Request Sequencer Data
    case SYSEX_REQUEST_PROGRAM_DATA: // Request Patch bytes + PartData
    case SYSEX_REQUEST_MULTI_DATA: // Request PartData
    case SYSEX_REQUEST_PART_DATA: // Request MultiData + Patch and PartData
      // Request commands have no data.
      sysex_rx_expected_size_ = 0;
      break;
      
    case SYSEX_REQUEST_PATCH_NAME: // 0x16 Request Patch Name
    case SYSEX_REQUEST_SEQUENCE_NAME: // 0x17 Request Sequencer Name
    case SYSEX_REQUEST_PROGRAM_NAME: // 0x18 Request Progam Name
    case SYSEX_REQUEST_MULTI_NAME: // 0x19 Request Mutti Name
      // Request commands have no data.
      sysex_rx_expected_size_ = 1; // Change to 1 | Argument = Bank | Data[0] = Slot
      break;
    case SYSEX_PEEK_COMMAND:
      // PEEK command accepts 2 bytes (address).
      sysex_rx_expected_size_ = 2;
      break;
    
    default:
      sysex_rx_state_ = RECEIVING_FOOTER;
      break;
  }
}

/* static */
void Storage::SysExAcceptCommand() {
  uint8_t success = 1;
  
  StorageLocation location {
      .object = static_cast<StorageObject>(0), // null object | // If argument=0 use active part, otherwise part=argument
      .part = U8(sysex_rx_command_[1] == 0 ? ui.active_part() : sysex_rx_command_[1] - 1), 
      .alias = 0,
      .bank = 0,
      .slot = 0,
      .name = nullptr
  };

    switch (static_cast<SysExCommand>(sysex_rx_command_[0])) {
    case SYSEX_RECEIVE_PATCH_DATA: // Recieve Patch dump
    case SYSEX_RECEIVE_SEQUENCER_DATA: // Recieve PartData::sequence_data dump
    case SYSEX_RECEIVE_PROGRAM_DATA: // Program?
    case SYSEX_RECEIVE_PART_DATA: // Recieve PartData dump |  These are swapped per the manual, no matter here though due to break;
    case SYSEX_RECEIVE_MULTI_DATA: // Recieve MultiData dump
      location.object = static_cast<StorageObject>(sysex_rx_command_[0] - 0x01); // StorageObject type (enum)
      ReadObject(location); // Copy mutable data to buffer
      TouchObject(location); // Recompute the dependent variables and update the voicecard
      break;

    case 0x0f: 
    // POKE sysex_rx_command_[1] = Argument = size in bytes of data to be poked
      {
        Word address {
          .bytes = {buffer_[0], buffer_[1]}
        };
        uint8_t size = sysex_rx_command_[1]; 
        auto p = reinterpret_cast<uint8_t*>(address.value);
        memcpy(p, buffer_ + 2, size);
      }
      break;

    case SYSEX_REQUEST_PATCH_DATA: // Request Patch Data
    case SYSEX_REQUEST_SEQUENCER_DATA: // Request Sequencer Data
    case SYSEX_REQUEST_PROGRAM_DATA: // Request Patch bytes + PartData (Program)
    case SYSEX_REQUEST_PART_DATA: // Request PartData
    case SYSEX_REQUEST_MULTI_DATA: // Request MultiData + Patch and PartData
      location.object = static_cast<StorageObject>(sysex_rx_command_[0] - 0x11);
      SysExSend(location);
      break;
      
    case SYSEX_REQUEST_PATCH_NAME: // Request Patch name
    case SYSEX_REQUEST_SEQUENCE_NAME: // Request Sequence name
    case SYSEX_REQUEST_PROGRAM_NAME: // Request Program name
    case SYSEX_REQUEST_MULTI_NAME: // Request Multi name
      {
      location.object = static_cast<StorageObject>(sysex_rx_command_[0] - 0x16); // (type of object)
      location.bank = sysex_rx_command_[1]; // Bank  = argument . (0x1f for current)
      location.slot = buffer_[0]; // Slot = Data[0]
      Load(STORAGE_BANK, location, 0); // Use this instead of LoadName to get name by reference.
      auto name = reinterpret_cast<const uint8_t*>(location.name);
      SysExSendRaw(
        sysex_rx_command_[0], // Command: Send same command back so we know what we requested.
        sysex_rx_command_[1], // Argument: Bank (0x1f for current )
        name,
        16, // Name is always 16 bytes null terminated and zero-padded
        false);
      }
      break;

    case SYSEX_PEEK_COMMAND:
      // PEEK
      {
        Word address {
          .bytes = {buffer_[0], buffer_[1]}
        };
        uint8_t size = sysex_rx_command_[1];
        auto p = reinterpret_cast<const uint8_t*>(address.value);
        SysExSendRaw(0x0f, size, p, size, true);
      }
      break;
      
    default:
      success = 0;
      break;
  }
  sysex_rx_state_ = success ? RECEPTION_OK : RECEPTION_ERROR;
}

/* static */
void Storage::SysExReceive(uint8_t byte) {
  if (byte == 0xf0) {
    sysex_rx_checksum_ = 0;
    sysex_rx_bytes_received_ = 0;
    sysex_rx_state_ = RECEIVING_HEADER;
  }
  
  switch (sysex_rx_state_) {
    default:
      break;
    case RECEIVING_HEADER:
      if (pgm_read_byte(sysex_header + sysex_rx_bytes_received_) == byte) {
        sysex_rx_bytes_received_++;
        if (sysex_rx_bytes_received_ >= sizeof(sysex_header)) {
          sysex_rx_state_ = RECEIVING_COMMAND;
          sysex_rx_bytes_received_ = 0;
        }
      } else {
        sysex_rx_state_ = RECEIVING_FOOTER;
      }
      break;

    case RECEIVING_COMMAND:
      sysex_rx_command_[sysex_rx_bytes_received_++] = byte;
      if (sysex_rx_bytes_received_ == 2) {
        SysExParseCommand();
      }
      break;

    case RECEIVING_DATA:
      {
        uint16_t i = sysex_rx_bytes_received_ / 2;
        if (sysex_rx_bytes_received_ & 1u) {
          buffer_[i] |= byte & 0xfu;
          if (i < sysex_rx_expected_size_) {
            sysex_rx_checksum_ += buffer_[i];
          }
        } else {
          buffer_[i] = U8ShiftLeft4(byte);
        }
        sysex_rx_bytes_received_++;
        if (sysex_rx_bytes_received_ >= (1 + sysex_rx_expected_size_) * 2) {
          sysex_rx_state_ = RECEIVING_FOOTER;
        }
      }
    break;

  case RECEIVING_FOOTER:
    bool checksum_ok = sysex_rx_checksum_ == buffer_[sysex_rx_expected_size_];
    if (byte == 0xf7 && checksum_ok && system_settings.rx_sysex()) {
      SysExAcceptCommand();
    } else {
      sysex_rx_state_ = RECEPTION_ERROR;
    }
    break;
  }
}

/* static */
void Storage::InvalidatePendingSysExTransfer() {
  sysex_rx_state_ = RECEPTION_ERROR;
}
  
/* extern */
Storage storage;

}  // namespace ambika
