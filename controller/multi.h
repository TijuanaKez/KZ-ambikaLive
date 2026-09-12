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
// "MultiData" is the dumb data structure storing the global settings.
// "Multi" is the root synthesis engine.

#ifndef CONTROLLER_MULTI_H_
#define CONTROLLER_MULTI_H_

#include "avrlib/base.h"

#include "common/features.h"

#include "controller/controller.h"
#include "controller/part.h"

namespace ambika {
  
struct KnobAssignment {
  uint8_t part;
  uint8_t parameter;
  uint8_t instance;
};

struct PartMapping {
  uint8_t midi_channel;
  uint8_t keyrange_low;
  uint8_t keyrange_high;
  uint8_t voice_allocation;

  // channels are 1-indexed; midi_channel == 0 means receive all
  inline uint8_t receive_channel(uint8_t channel) const {
    return midi_channel == 0 || (channel + 1 == midi_channel);
  }
  
  inline uint8_t accept_note(uint8_t note) const {
    if (keyrange_low <= keyrange_high) {
      return (note >= keyrange_low && note <= keyrange_high);
    } else {
      return note <= keyrange_high || note >= keyrange_low;
    }
  }
  
  inline uint8_t accept_channel_note(uint8_t channel, uint8_t note) const {
    return receive_channel(channel) && accept_note(note);
  }
  
  inline uint8_t tx_channel() const {
    return midi_channel == 0 ? 0 : midi_channel - 1;
  }
};

struct MultiData {
  struct Parameters {
    // Offset: 0-24
    PartMapping part_mapping[kNumParts];

    // Offset: 24-28
    uint8_t clock_bpm;
    uint8_t clock_groove_template;
    uint8_t clock_groove_amount;
    uint8_t clock_release;

    // Offset: 28-52
    KnobAssignment knob_assignment[8];

    // Offset: 52-56
    uint8_t padding2[4];
  };

private:
  union Data {
    Parameters params;
    uint8_t bytes[sizeof(Parameters)];

    explicit Data(Parameters p) : params(p) {};
    explicit Data() = default;
  };

  Data data;

public:
  inline Parameters* params_addr() {
    return &data.params;
  }

  inline uint8_t* bytes() {
    return data.bytes;
  }

  inline PartMapping& part_mapping(uint8_t index) {
    return data.params.part_mapping[index];
  }

  inline uint8_t& clock_bpm() {
    return data.params.clock_bpm;
  }
  inline uint8_t& clock_groove_template() {
    return data.params.clock_groove_template;
  }
  inline uint8_t& clock_groove_amount() {
    return data.params.clock_groove_amount;
  }
  inline uint8_t& clock_release() {
    return data.params.clock_release;
  }

  inline KnobAssignment& knobAssignment(uint8_t index) {
    return data.params.knob_assignment[index];
  }
  
  static constexpr inline size_t size() {
    return sizeof(Parameters);
  }
};

// has to match order (and size) of members in MultiData::Parameteters
// TODO this is kind of ugly, can we reduce repetition
enum MultiParameter {
  // 1x partMapping
  PRM_MULTI_MIDI_CHANNEL,
  PRM_MULTI_KEYRANGE_LOW,
  PRM_MULTI_KEYRANGE_HIGH,
  PRM_MULTI_VOICE_ALLOCATION,

  // clock parameters after all part mappings
  PRM_MULTI_CLOCK_BPM = sizeof(PartMapping)*kNumParts,
  PRM_MULTI_CLOCK_GROOVE_TEMPLATE,
  PRM_MULTI_CLOCK_GROOVE_AMOUNT,
  PRM_MULTI_CLOCK_LATCH,
};

static const uint8_t kNumStepsInGroovePattern = 16;
static const uint8_t kNumTicksPerStep = 6;

class Multi {
 public:
  Multi() = default;
  static uint8_t launchkey_play_button_note_;
  static bool launchkey_play_button_note_active_;
  
  static void Init(bool force_reset);
  
  static void InitSettings(InitializationMode mode);
  
  static void StartNote(uint8_t note) {
    if (!running_) {
      Start();
    }
    for (uint8_t i = 0; i < kNumParts; ++i) {
      parts_[i].NoteOn(note, 0x64);
    }
  }
  
  static void StopNote(uint8_t note) {
    for (uint8_t i = 0; i < kNumParts; ++i) {
      parts_[i].NoteOff(note);
    }
  }
  
  static void NoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    // If no note has been played for a while, reset the sequencer and
    // arpeggiator to the first step.
    if (!running_) {
      Start();
    }
    for (uint8_t i = 0; i < kNumParts; ++i) {
      if (data_.part_mapping(i).accept_channel_note(channel, note)) {
        parts_[i].NoteOn(note, velocity);
      }
    }
  }
  static void NoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
    IGNORE_UNUSED(velocity);
    for (uint8_t i = 0; i < kNumParts; ++i) {
      if (data_.part_mapping(i).accept_channel_note(channel, note)) {
        parts_[i].NoteOff(note);
      }
    }
  }
  static void ControlChange(uint8_t channel, uint8_t controller, uint8_t value) {
    for (uint8_t i = 0; i < kNumParts; ++i) {
      if (data_.part_mapping(i).receive_channel(channel)) {
        parts_[i].ControlChange(controller, value);
      }
    }
  }
  static void PitchBend(uint8_t channel, uint16_t pitch_bend) {
    for (uint8_t i = 0; i < kNumParts; ++i) {
      if (data_.part_mapping(i).receive_channel(channel)) {
        parts_[i].PitchBend(pitch_bend);
      }
    }
  }
  static void Aftertouch(uint8_t channel, uint8_t note, uint8_t velocity) {
    for (uint8_t i = 0; i < kNumParts; ++i) {
      if (data_.part_mapping(i).accept_channel_note(channel, note)) {
        parts_[i].Aftertouch(note, velocity);
      }
    }
  }
  static void Aftertouch(uint8_t channel, uint8_t velocity) {
    for (uint8_t i = 0; i < kNumParts; ++i) {
      if (data_.part_mapping(i).receive_channel(channel)) {
        parts_[i].Aftertouch(velocity);
      }
    }
  }
  static void AllSoundOff(uint8_t channel) {
    for (uint8_t i = 0; i < kNumParts; ++i) {
      if (data_.part_mapping(i).receive_channel(channel)) {
        parts_[i].AllSoundOff();
      }
    }
  }
  static void ResetAllControllers(uint8_t channel) {
    for (uint8_t i = 0; i < kNumParts; ++i) {
      if (data_.part_mapping(i).receive_channel(channel)) {
        parts_[i].ResetAllControllers();
      }
    }
  }
  static void AllNotesOff(uint8_t channel) {
    for (uint8_t i = 0; i < kNumParts; ++i) {
      if (data_.part_mapping(i).receive_channel(channel)) {
        parts_[i].AllNotesOff();
      }
    }
  }
  static void OmniModeOff(uint8_t channel) {
    for (uint8_t i = 0; i < kNumParts; ++i) {
      if (data_.part_mapping(i).receive_channel(channel)) {
        data_.part_mapping(i).midi_channel = channel + 1;
      }
    }
  }
  static void OmniModeOn(uint8_t channel) {
    for (uint8_t i = 0; i < kNumParts; ++i) {
      if (data_.part_mapping(i).receive_channel(channel)) {
        data_.part_mapping(i).midi_channel = 0;
      }
    }
  }
  static void MonoModeOn(uint8_t channel, uint8_t num_channels) {
    for (uint8_t i = 0; i < kNumParts; ++i) {
      if (data_.part_mapping(i).receive_channel(channel)) {
        parts_[i].MonoModeOn(num_channels);
      }
    }
  }
  static void PolyModeOn(uint8_t channel) {
    for (uint8_t i = 0; i < kNumParts; ++i) {
      if (data_.part_mapping(i).receive_channel(channel)) {
        parts_[i].PolyModeOn();
      }
    }
  }
  static void Reset() {
    for (uint8_t i = 0; i < kNumParts; ++i) {
      parts_[i].Reset();
    }
  }
  static void Clock();
  static void Start();
  static void Stop();
  static void Continue() {
    Start();
  }
  
  static void Tick() {
    ++clock_counter_;
    ++lfo_refresh_counter_;
    if (clock_counter_ >= tick_duration_) {
      ++num_clock_events_;
      clock_counter_ = 0;
    }
  }
  
  static void SetValue(uint8_t address, uint8_t value);

  static inline uint8_t GetValue(uint8_t address) {
    return data_.bytes()[address];
  }
  
  static void UpdateClocks();
  
  static Part& part(uint8_t i) { return parts_[i]; }
  static MultiData& data() { return data_; }

  static uint8_t internal_clock() { return data_.clock_bpm() >= 40; }

  static uint8_t SolveAllocationConflicts(uint8_t constraint);
  static void AssignVoicesToParts();
  static void SyncPartClocks();
  static void ToggleMute(uint8_t part);
  
  static uint8_t part_channel(Part* part) {
    for (uint8_t i = 0; i < kNumParts; ++i) {
      if (&parts_[i] == part) {
        return data_.part_mapping(i).tx_channel();
      }
    }
    return 0;
  }

  static bool IsPartMuted(uint8_t part) { return parts_[part].isMuted(); }
  static uint8_t step() { return step_count_; }
  static uint8_t running() { return running_; }
  static void Touch();
  
  static inline uint8_t flags() {
    uint8_t result = flags_;
    for (uint8_t i = 0; i < kNumParts; ++i) {
      result |= parts_[i].flags();
    }
    return result;
  }
  
  inline void ClearFlag(uint8_t flag) {
    flags_ &= byteInverse(flag);
    for (uint8_t i = 0; i < kNumParts; ++i) {
      parts_[i].ClearFlag(flag);
    }
  }
  
 private:
  static void ComputeInternalClockOverflowsTable();
  
  // Incremented at 39kHz
  static uint16_t clock_counter_;
  static uint16_t lfo_refresh_counter_;
  // Incremented at 39kHz / 40
  static uint8_t lfo_refresh_cycle_;
  static volatile uint8_t num_clock_events_;

  // Duration of a clock tick, in 39kHz increments.
  static uint16_t tick_duration_;
  // Incremented at each clock tick, reset after 6 ticks.
  static uint8_t tick_count_;
  // Incremented every 6 tick.
  static uint8_t step_count_;
  // Incremented at each clock tick if no key is pressed.
  static uint8_t idle_ticks_;
  // Whether the clock is started.
  static uint8_t running_;
  // This list of tick durations is computed from the BPM and the groove
  // template.
  static uint16_t tick_duration_table_[kNumStepsInGroovePattern];
  

  static MultiData data_;
  static Part parts_[kNumParts];
  static uint8_t flags_;
  
  DISALLOW_COPY_AND_ASSIGN(Multi);
};

extern Multi multi;

}  // namespace ambika

#endif  // CONTROLLER_MULTI_H_
