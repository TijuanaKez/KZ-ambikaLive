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

#ifndef VOICECARD_VOICECARD_RX_H_
#define VOICECARD_VOICECARD_RX_H_


#include "avrlib/bitops.h"
#include "avrlib/ring_buffer.h"
#include "avrlib/spi.h"
#include "avrlib/timer.h"
#include "avrlib/watchdog_timer.h"

#include "common/protocol.h"

#include "voicecard/voice.h"
#include "voicecard/voicecard.h"
#include "voicecard/leds.h"

#include <avr/eeprom.h>

namespace ambika {

using avrlib::RingBuffer;

enum State : uint8_t {
  EXPECTING_COMMAND,
  EXPECTING_ARGUMENTS,
};

struct InputBufferSpecs {
  typedef uint8_t Value;
  enum {
    buffer_size = 256,
    data_size = 8,
  };
};

class VoicecardProtocolRx {
 public:
  VoicecardProtocolRx() = default;
  
  static void Init() {
    spi_.Init();
    state_ = EXPECTING_COMMAND;
    lights_out_ = 0;
  }
  
  static inline void Receive() {
    if (spi_.readable()) {
      rx_led_counter_ = 255;
      uint8_t byte = spi_.ImmediateRead();
      buffer_.Overwrite(byte);
      spi_.Reply(0xff);
    }
  }
  
  static void TickRxLed() {
    if (rx_led_counter_) {
      --rx_led_counter_;
      if (!lights_out_) {
        RxLed::high();
      }
    } else {
      RxLed::low();
    }
  }
  
  static void DoLongCommand() {
    auto commandCode = highNibbleUnshifted(command_);
    switch (commandCode) {
      default:
        break;
      case COMMAND_NOTE_ON:
      {
        auto note = word(arguments_[0], arguments_[1]);
        auto velocity = arguments_[2];
        auto legato = byteAnd(command_, 1);
        voice.Trigger(note, velocity, legato);
        if (!lights_out_) {
          NoteLed::high();
        }
        break;
      }
      case COMMAND_WRITE_PATCH_DATA:
        voice.patch().setData(arguments_[0], arguments_[1]);
        break;
      case COMMAND_WRITE_PART_DATA:
        voice.part().setData(arguments_[0], arguments_[1]);
        break;
      case COMMAND_WRITE_MOD_MATRIX:
      {
        if (arguments_[0] < MOD_SRC_COUNT) {
          auto mod_source = static_cast<ModSource>(arguments_[0]);
          voice.set_mod_source_value(mod_source, arguments_[1]);
        }
        break;
      }
      case COMMAND_WRITE_LFO:
      {
        auto lfo_index = lowNibble(command_);
        if (lfo_index < kNumLfos) {
          auto lfo = static_cast<ModSource>(MOD_SRC_LFO_1 + lfo_index);
          voice.set_mod_source_value(lfo, arguments_[0]);
        }
        break;
      }
    }
  }
  
  static void DoShortCommand() {
    switch (command_) {
      default:
        break;
      case COMMAND_RELEASE:
        voice.Release();
        NoteLed::low();
        break;
      case COMMAND_KILL:
        voice.Kill();
        NoteLed::low();
        break;
      case COMMAND_RETRIGGER_ENVELOPE:
      case COMMAND_RETRIGGER_ENVELOPE + 1:
      case COMMAND_RETRIGGER_ENVELOPE + 2:
        voice.TriggerEnvelope(lowNibble(command_), Envelope::Stage::ATTACK);
        break;
      case COMMAND_RESET_ALL_CONTROLLERS:
        voice.ResetAllControllers();
        break;
      case COMMAND_RESET:
        voice.Init();
        NoteLed::low();
        break;
      case COMMAND_LIGHTS_OUT:
        lights_out_ = 1;
        RxLed::low();
        NoteLed::low();
        break;
      case COMMAND_BULK_SEND:
        {
          // Stop doing anything else
          Timer<2>::Stop();
          uint8_t size = spi_.Read();
          data_ptr_ = voice.patch().bytes();
          for (uint8_t i = 0; i < size; ++i) {
            uint8_t value = spi_.Read();
            if (i < Patch::sizeBytes()) {
              data_ptr_[i] = value;
            }
          }
          Timer<2>::Start();
        }
        break;
      case COMMAND_FIRMWARE_UPDATE_MODE:
        eeprom_write_byte(kFirmwareUpdateFlagPtr, FIRMWARE_UPDATE_REQUESTED);
        SystemReset(0);
        while (1) { }
        break;
      // This prepares SPDR for the next transfer.
      case COMMAND_GET_SLAVE_ID:
        SPDR = SLAVE_ID_SOLO_VOICECARD;
        break;
      case COMMAND_GET_VERSION_ID:  
        SPDR = kSystemVersion;
        break;
      // KZ MOD: report and reset the headroom, so each query returns the peak
      // since the previous one. 255 means the buffer starved at least once.
      case COMMAND_GET_AUDIO_HEADROOM:
        SPDR = audio_starved ? kAudioStarved
                             : (audio_drain_peak < kAudioStarved ? audio_drain_peak
                                                                 : U8(kAudioStarved - 1));
        audio_drain_peak = 0;
        audio_starved = 0;
        break;
    }
  }
  
  static void Process() {
    while (buffer_.readable()) {
      uint8_t byte = buffer_.ImmediateRead();
      if (state_ == EXPECTING_COMMAND) {
        command_ = byte;
        data_ptr_ = arguments_;
        state_ = EXPECTING_ARGUMENTS;
        if (command_ >= COMMAND_NOTE_ON && command_ < COMMAND_WRITE_PATCH_DATA) {
          data_size_ = 3;
        } else if (command_ >= COMMAND_WRITE_PATCH_DATA && command_ < COMMAND_WRITE_LFO) {
          data_size_ = 2;
        } else if (highNibbleUnshifted(command_) == COMMAND_WRITE_LFO) {
          data_size_ = 1;
        } else {
          DoShortCommand();
          state_ = EXPECTING_COMMAND;
        }
      } else {
        *data_ptr_++ = byte;
        if (--data_size_ == 0) {
          DoLongCommand();
          state_ = EXPECTING_COMMAND;
        }
      }
    }
  }
  
  static inline uint8_t writable() { return buffer_.writable(); }

 private:
  static SpiSlave<MSB_FIRST, false> spi_;

  static RingBuffer<InputBufferSpecs> buffer_;
  static uint8_t command_;
  static uint8_t state_;
  static uint8_t data_size_;
  static uint8_t* data_ptr_;
  static uint8_t arguments_[3];
  static uint8_t rx_led_counter_;
  static uint8_t lights_out_;
   
  DISALLOW_COPY_AND_ASSIGN(VoicecardProtocolRx);
};

extern VoicecardProtocolRx voicecard_rx;

}  // namespace ambika

#endif  // VOICECARD_VOICECARD_RX_H_
