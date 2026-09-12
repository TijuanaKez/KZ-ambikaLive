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

#include "controller/part.h"

#include "avrlib/op.h"
#include "controller/midi_dispatcher.h"
#include "controller/parameter.h"
#include "controller/resources.h"
#include "controller/voicecard_tx.h"
#include "controller/system_settings.h"
#include "midi/midi.h"

using namespace avrlib;

namespace ambika {

// ------ KZ MODs - Launchkey Stuff ----------
#ifndef DISABLE_LAUNCHKEY_MODE
static const prog_uint8_t launchkey_rgb_pad_notes_top[8] PROGMEM = {
  40, 41, 42, 43, 48, 49, 50, 51
};

static const prog_uint8_t launchkey_rgb_pad_notes_bottom[8] PROGMEM = {
  36, 37, 38, 39, 44, 45, 46, 47
};
#endif
// --------------------------------

static constexpr uint8_t midi_clock_tick_per_step[15] PROGMEM = {
  96, 72, 64, 48, 36, 32, 24, 16, 12, 8, 6, 4, 3, 2, 1
};

static constexpr uint16_t lfo_phase_increment_per_clock_tick[15] PROGMEM = {
  683, 910, 1024, 1365, 1820, 2048, 2731,
  4096, 5461, 8192, 10923, 16384, 21845, 32768, 65535
};

static constexpr Patch::Parameters init_patch_params PROGMEM {
  // Oscillators
  .osc = {
    // MZ MOD: Still using YAM Voicecard
    {WAVEFORM_POLYBLEP_SAW, 0, 0, 0},
    {WAVEFORM_POLYBLEP_PWM, 32, -12, 12}
  },
  // Mixer
  .mix_balance = 32,
  .mix_op = OP_SUM,
  .mix_parameter = 31,
  .mix_sub_osc_shape = WAVEFORM_SUB_OSC_SQUARE_1,
  .mix_sub_osc = 0,
  .mix_noise = 0,
  .mix_fuzz = 0,
  .mix_crush = 0,
  // Filter
  .filter = {
    {96, 0, FILTER_MODE_LP},
    {0, 0, FILTER_MODE_LP}
  },
  .filter_env = 24,
  .filter_lfo = 0,
  // ADSR
  .env_lfo = {
    {0, 40, 20,  60, LFO_WAVEFORM_TRIANGLE, kNumSyncedLfoRates + 24, 0, LFO_SYNC_MODE_FREE},
    {0, 40, 0,   40, LFO_WAVEFORM_TRIANGLE, kNumSyncedLfoRates + 32, 0, LFO_SYNC_MODE_FREE},
    {0, 40, 100, 40, LFO_WAVEFORM_TRIANGLE, kNumSyncedLfoRates + 48, 0, LFO_SYNC_MODE_FREE}
  },

  .voice_lfo_shape = LFO_WAVEFORM_TRIANGLE,
  .voice_lfo_rate = 72,
  // Routing
  .modulation = {
    {MOD_SRC_ENV_1, MOD_DST_PARAMETER_1, 0},
    {MOD_SRC_ENV_1, MOD_DST_PARAMETER_2, 0},
    {MOD_SRC_LFO_1, MOD_DST_OSC_1, 0},
    {MOD_SRC_LFO_1, MOD_DST_OSC_2, 0},
    {MOD_SRC_LFO_2, MOD_DST_PARAMETER_1, 0},
    {MOD_SRC_LFO_2, MOD_DST_PARAMETER_2, 0},
    {MOD_SRC_LFO_3, MOD_DST_MIX_BALANCE, 0},
    {MOD_SRC_LFO_4, MOD_DST_FILTER_CUTOFF, 0},
    {MOD_SRC_SEQ_1, MOD_DST_FILTER_CUTOFF, 0},
    {MOD_SRC_SEQ_2, MOD_DST_MIX_BALANCE, 0},

    {MOD_SRC_ENV_3, MOD_DST_VCA, 63},
    {MOD_SRC_VELOCITY, MOD_DST_VCA, 16},
    {MOD_SRC_PITCH_BEND, MOD_DST_OSC_1_2_COARSE, 32},
    {MOD_SRC_LFO_4, MOD_DST_OSC_1_2_COARSE, 16}
  },
  
  .modifier = {
    {MOD_SRC_LFO_1, MOD_SRC_LFO_2, MODIFIER_NONE},
    {MOD_SRC_LFO_2, MOD_SRC_LFO_3, MODIFIER_NONE},
    {MOD_SRC_LFO_3, MOD_SRC_SEQ_1, MODIFIER_NONE},
    {MOD_SRC_SEQ_1, MOD_SRC_SEQ_2, MODIFIER_NONE}
  },

  .filter_velo = 0,
  .filter_kbt = 0,

  .padding = {0}
};

static constexpr PartData::Parameters init_part_params PROGMEM {
  .volume = 120,
  .octave = 0,
  .tuning = 0,
  .spread = 0,
  .raga = 0,
  .legato = 0,
  .portamento_time = 0,
  .arp_sequencer_mode = ARP_SEQUENCER_MODE_STEP,
  // Arp data
  .arp_direction = ARPEGGIO_DIRECTION_UP,
  .arp_octave = 1,
  .arp_pattern = 0,
  .arp_divider = 10,
  // Sequence length
  .sequence_length = {16, 16, 16},
  .polyphony_mode = POLY,
  // Sequence data
  .sequence_data = {
      //  0..15: step sequence 1
      0xff, 0xff, 0x80, 0x80, 0xcc, 0xcc, 0x20, 0x20,
      0x00, 0x20, 0x40, 0x60, 0x80, 0xa0, 0xc0, 0xff,
      // 16..31: step sequence 2
      0x00, 0x10, 0x20, 0x40, 0x80, 0xff, 0x80, 0x40,
      0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x80, 0x40,
      // 32..63: (note value | 0x80 if gate), (note velocity | 0x80 if legato)
      60u | 0x80u, 100u,
      60u        , 100u,
      48u | 0x80u, 100u,
      48u        , 100u,
      60u | 0x80u, 100u,
      60u        , 100u,
      48u | 0x80u, 100u,
      48u        , 100u,
      60u | 0x80u, 100u,
      60u        , 100u,
      48u | 0x80u, 100u,
      48u | 0x80u, 100u | 0x80u,
      60u | 0x80u, 100u,
      60u        , 100u,
      48u | 0x80u, 100u,
      48u        , 100u
  },
  .padding = {0, 0, 0, 0}
};

void Part::Touch() {
  TouchClock();
  TouchLfos();
  flags_ = FLAG_HAS_CHANGE;
  
  uint8_t* bytes = patch_.bytes();
  for (uint8_t address = PRM_PART_VOLUME; address <= PRM_PART_PORTAMENTO_TIME; ++address) {
    WriteToAllVoices(VOICECARD_DATA_PART, address - Patch::sizeBytes(), bytes[address]);
  }
  
  if (data_.polyphony_mode() != polyphony_mode_) {
    AllSoundOff();
    InitializeAllocators();
  }
}

void Part::TouchPatch() {
  flags_ = FLAG_HAS_CHANGE;
  // Send an "enter block write" command to all voicecards.
  for (uint8_t i = 0; i < num_allocated_voices_; ++i) {
    voicecard_tx.PrepareForBlockWrite(allocated_voices_[i]);
  }
  // Wait for the voicecard to process the "enter block write" command.
  ConstantDelay(5);
  
  // At this stage, all the voicecards are sitting in a tight SPI receive loop.
  const uint8_t* patch_data = patch_.bytes();
  for (uint8_t i = 0; i < num_allocated_voices_; ++i) {
    voicecard_tx.WriteBlock(allocated_voices_[i], patch_data, Patch::sizeBytes());
  }
}

void Part::Init() {
  ignore_note_off_messages_ = 0;
  pressed_keys_.Init();
  mono_allocator_.Init();
}

void Part::InitPatch(InitializationMode mode) {
  if (mode == INITIALIZATION_DEFAULT) {
    ResourcesManager::Load(&init_patch_params, 0, patch_.params_addr());
  } else {
    RandomizeRange(PRM_PATCH, Patch::sizeBytes());
  }
  TouchPatch();
}

void Part::InitSettings(InitializationMode mode) {
  InitPatch(mode);
  if (mode == INITIALIZATION_DEFAULT) {
    ResourcesManager::Load(&init_part_params, 0, data_.params_addr());
  } else {
    RandomizeRange(PRM_PART_VOLUME, PartData::sizeBytes());
  }
  Touch();
}

void Part::InitSequence(InitializationMode mode) {
  if (mode == INITIALIZATION_DEFAULT) {
    // see comments to PartData::sequence_data_size and PartData::sequence_data()
    // for why arp_direction is the start
    memcpy_P(raw_sequence_data(), &init_part_params.arp_direction, PartData::sequence_data_size);
  } else {
    // TODO this is really bad
    RandomizeRange(PRM_PART_ARP_DIRECTION, PartData::sequence_data_size);
  }
}

void Part::RandomizeRange(uint8_t start, uint8_t size) {
  for (uint8_t i = start; i < start + size; ++i) {
    uint8_t parameter_id = parameter_manager.AddressToParameterId(i);
    if (parameter_id != 0xff) {
      const Parameter& parameter = parameter_manager.parameter(parameter_id);
      SetValue(i, parameter.RandomValue(), 0);
    }
  }
}

void Part::TouchClock() {
  midi_clock_prescaler_ = ResourcesManager::Lookup<uint8_t, uint8_t>(
      midi_clock_tick_per_step, data_.arp_divider());
}

void Part::AssignVoices(uint8_t allocation) {
  AllSoundOff();
  
  uint8_t mask = 1;
  num_allocated_voices_ = 0;
  for (uint8_t i = 0; i < kNumVoices; ++i) {
    if (allocation & mask) {
      allocated_voices_[num_allocated_voices_++] = i;
    }
    mask <<= 1;
  }
  InitializeAllocators();
  TouchPatch();
  Touch();
}

void Part::InitializeAllocators() {
  if (data_.polyphony_mode() == MONO || data_.polyphony_mode() == SOLO) {
    mono_allocator_.Init();
  } else {
    uint8_t size = num_allocated_voices_;
    if (data_.polyphony_mode() == UNISON_2X) {
      size = (num_allocated_voices_ + 1) / 2u;
    } else if (data_.polyphony_mode() == CHAIN) {
      size *= 2;
    }
    // We reuse the storage for the monophonic allocator (note stack) for the
    // polyphonic allocator - since these two datastructures are never used
    // at the same time.
    poly_allocator_.Init(size, data_.polyphony_mode() == CYCLIC,
        mono_allocator_.bytes(), mono_allocator_.bytes() + 12);
  }
  polyphony_mode_ = data_.polyphony_mode();
}

void Part::TouchLfos() {
  for (uint8_t i = 0; i < kNumLfos; ++i) {
    if (patch_.env_lfo(i).rate < kNumSyncedLfoRates) {
      lfo_cycle_length_[i] = ResourcesManager::Lookup<uint8_t, uint8_t>(
          midi_clock_tick_per_step, patch_.env_lfo(i).rate);
    } else {
      lfo_[i].set_phase_increment(ResourcesManager::Lookup<uint16_t, uint8_t>(
          lut_res_lfo_increments, patch_.env_lfo(i).rate - kNumSyncedLfoRates));
    }
  }
}

void Part::SetValue(uint8_t address, uint8_t value, uint8_t user_initiated) {
  uint8_t old_value = patch_.getData(address);
  patch_.setData(address, value);

  flags_ |= FLAG_HAS_CHANGE;
  if (user_initiated) {
    midi_dispatcher.OnEdit(this, address, value);
    flags_ |= FLAG_HAS_USER_CHANGE;
  }
  
  // Some parameter changes need to be propagated to the voicecard.
  if (address < PRM_PART_VOLUME) {
    // We have modified a patch parameter. Notify the voicecards.
    WriteToAllVoices(VOICECARD_DATA_PATCH, address, value);
  } else if (address <= PRM_PART_PORTAMENTO_TIME) {
    // We have modified a part parameter a copy of which is needed by the
    // voicecard. Notify.
    WriteToAllVoices(VOICECARD_DATA_PART, address - Patch::sizeBytes(), value);
  }
  
  if (address == PRM_PART_POLYPHONY_MODE && old_value != value) {
    AllSoundOff();
    InitializeAllocators();
  }
  
  // Some parameter changes requires an update of some internal book-keeping
  // variables.
  if (address == PRM_PART_ARP_RESOLUTION) {
    TouchClock();
  } else if (address >= PRM_PATCH_ENV_ATTACK && 
             address < PRM_PATCH_VOICE_LFO_SHAPE) {
    TouchLfos();
#ifndef DISABLE_ARPEGGIO
  } else if (address == PRM_PART_ARP_DIRECTION) {
    arp_direction_ = (data_.arp_direction() == ARPEGGIO_DIRECTION_DOWN ? -1 : 1);
    StartArpeggio();
#endif
  }
}


void Part::NoteOn(uint8_t note, uint8_t velocity) {
  if (!AcceptNote(note)) { 
    return;
  }
  if (velocity == 0) {
    NoteOff(note);
    // KZ MOD: Arp and Seq Latch modes
  } else if (data_.arp_sequencer_mode() == ARP_SEQUENCER_MODE_ARPEGGIATOR_LATCH){
    // Toggle notes on NoteOn and ignore NoteOff.
    bool is_playing = false;
    for (uint8_t i = 1; i <= pressed_keys_.max_size(); ++i) {
      NoteEntry* e = pressed_keys_.mutable_note(i);
      if (e->note == note) {
        is_playing = true;
        break;
      }
    }
    if (is_playing){
      pressed_keys_.NoteOff(note);
  } else {
    pressed_keys_.NoteOn(note, velocity);
     }
  } else if (data_.arp_sequencer_mode() == ARP_SEQUENCER_MODE_CHORD){
    // Do nothing
  } else {
    pressed_keys_.NoteOn(note, velocity);
    if (data_.arp_sequencer_mode() == ARP_SEQUENCER_MODE_STEP) {      
      // Sequencer and arpeggiator are off, we directly trigger the note.
      InternalNoteOn(note, velocity);
    }
  }
}

void Part::NoteOff(uint8_t note) {
  if (!AcceptNote(note)) { 
    return;
  }
  if (ignore_note_off_messages_ || data_.arp_sequencer_mode() >= ARP_SEQUENCER_MODE_ARPEGGIATOR_LATCH) {
    for (uint8_t i = 1; i <= pressed_keys_.max_size(); ++i) {
      // Flag the note so that it is removed once the sustain pedal is released.
      NoteEntry* e = pressed_keys_.mutable_note(i);
      if (e->note == note && e->velocity) {
        e->velocity |= 0x80;
      }
    }
    return;
  }
  pressed_keys_.NoteOff(note);
  if (data_.arp_sequencer_mode() == ARP_SEQUENCER_MODE_STEP ||
      (data_.arp_sequencer_mode() == ARP_SEQUENCER_MODE_ARPEGGIATOR &&
       data_.arp_direction() == ARPEGGIO_DIRECTION_CHORD)) {
    // Sequencer and arpeggiator are off, we directly kill the note.
    // We also kill the note in chord trigger mode, to avoid stuck notes, since
    // the chord trigger mode doesn't really clean after itself.
    InternalNoteOff(note);
  } else {
    // The sequencer and arpeggiator might still have pending notes. Release
    // them.
    if (pressed_keys_.size() == 0) {
      ignore_note_off_messages_ = 0;
      AllNotesOff();
    }
  }
}

void Part::ControlChange(uint8_t controller, uint8_t value) {
  switch (controller) {
    case midi::kModulationWheelMsb:
      WriteToAllVoices(VOICECARD_DATA_MODULATION, MOD_SRC_WHEEL, value << 1);
      break;
    case midi::kBreathController:
      WriteToAllVoices(VOICECARD_DATA_MODULATION, MOD_SRC_WHEEL_2, value << 1);
      break;
    case midi::kFootPedalMsb:
      WriteToAllVoices(VOICECARD_DATA_MODULATION, MOD_SRC_EXPRESSION, value << 1);
      break;
    case midi::kHoldPedal:
      if (value >= 64) {
        ignore_note_off_messages_ = 1;
      } else {
        // The pedal was released. Kill all the sustained notes.
        ignore_note_off_messages_ = 0;
        for (uint8_t i = 1; i <= pressed_keys_.max_size(); ++i) {
          NoteEntry* e = pressed_keys_.mutable_note(i);
          if (e->velocity & 0x80) {
            NoteOff(e->note);
          }
        }
      }
      break;
    case midi::kDataEntryMsb:
      data_entry_msb_ = value ? 128 : 0;
      break;
    case midi::kNrpnLsb:
      nrpn_ = value | nrpn_msb_;
      nrpn_msb_ = 0;
      data_entry_msb_ = 0;
      break;
    case midi::kNrpnMsb:
      nrpn_msb_ = value ? 128 : 0;
      data_entry_msb_ = 0;
      break;
    case midi::kDataEntryLsb:
      value |= data_entry_msb_;
      // fall through
    case midi::kDataIncrement:
    case midi::kDataDecrement:
      {
        uint8_t address = nrpn_;
        uint8_t parameter_id = parameter_manager.AddressToParameterId(address);
        if (parameter_id == 0xff) {
          return;
        }
        const Parameter& parameter = parameter_manager.parameter(parameter_id);
        uint8_t new_value = GetValue(address);
        if (controller == midi::kDataEntryLsb) {
          new_value = parameter.Clamp(value);
        } else {
          new_value = parameter.Increment(new_value,
              controller == midi::kDataIncrement ? 1 : -1, false);
        }
        if (system_settings.rx_nrpn()) {
          SetValue(address, new_value, 0);
        }
      }
      break;
    default:
      {
        // Check if there is a mapping from this MIDI CC to a parameter.
        uint8_t parameter_id = parameter_manager.ControlChangeToParameterId(controller);
        if (parameter_id == 0xff) {
          return;
        }
        // KZ MOD - TODO 
        //uint8_t cc_map = system_settings.data().midi_cc_map();

        const Parameter& parameter = parameter_manager.parameter(parameter_id);
        // Some ranges of MIDI CC might point to the same parameter ID, for
        // different instances of the same object (for example a LFO).
        uint8_t instance_index = 0;
        for (instance_index = 0;
             instance_index < parameter.num_instances;
             ++instance_index) {
          if (parameter.midi_cc == controller) {
            break;
          }
          controller -= parameter.stride;
        }
        uint8_t new_value = parameter.Scale(value);
        if (system_settings.rx_cc()) {
          if (parameter.level <= PARAMETER_LEVEL_PART) {
            uint8_t address = parameter.offset + parameter.stride * instance_index;
            SetValue(address, new_value, 0);
          } else {
            // Should not happen, but this is a graceful degradation...
            parameter_manager.SetValue(parameter, 0, instance_index, value, 0 /* Not user initiated */);
          }
        }
      }
  }
}

void Part::PitchBend(uint16_t pitch_bend) {
  WriteToAllVoices(VOICECARD_DATA_MODULATION, MOD_SRC_PITCH_BEND, U14ShiftRight6(pitch_bend));
}

uint8_t Part::GetNextVoice(uint8_t index) const {
  if (index + 1 < num_allocated_voices_) {
    return index + 1;
  } else {
    return 0;
  }
}

void Part::Aftertouch(uint8_t note, uint8_t velocity) {
  const PolyphonyMode polyMode = data().polyphony_mode();
  switch (polyMode) {
    case POLY:
    case CYCLIC:
    case CHAIN:
    case UNISON_2X:
    {
      // Send the aftertouch change to the voicecard playing the affected note.
      uint8_t voice_index = poly_allocator_.Find(note);
      uint8_t size = poly_allocator_.size();
      if (polyMode == CYCLIC) {
        size >>= 1u;
      }
      if (voice_index < size) {
        if (polyMode == UNISON_2X) {
          voice_index <<= 1;
          voicecard_tx.WriteData(allocated_voices_[voice_index],
                                 VOICECARD_DATA_MODULATION, MOD_SRC_AFTERTOUCH, velocity);
          voicecard_tx.WriteData(allocated_voices_[GetNextVoice(voice_index)],
                                 VOICECARD_DATA_MODULATION, MOD_SRC_AFTERTOUCH, velocity);

        } else {
          voicecard_tx.WriteData(allocated_voices_[voice_index],
                                 VOICECARD_DATA_MODULATION, MOD_SRC_AFTERTOUCH, velocity);
        }
      }
      break;
    }
    default:
      // Process the message as if it were a global aftertouch message.
      Aftertouch(velocity);
      break;
  }
}

void Part::Aftertouch(uint8_t velocity) {
  WriteToAllVoices(VOICECARD_DATA_MODULATION, MOD_SRC_AFTERTOUCH, velocity);
}

void Part::AllSoundOff() {
  if (data_.polyphony_mode() == MONO || data_.polyphony_mode() == SOLO) {
    mono_allocator_.Clear();
  } else {
    poly_allocator_.Clear();
  }
  pressed_keys_.Clear();
  for (uint8_t i = 0; i < num_allocated_voices_; ++i) {
    voicecard_tx.Kill(allocated_voices_[i]);
  }
}

void Part::AllNotesOff() {
  if (ignore_note_off_messages_) {
    return;
  }
  if (data_.polyphony_mode() == MONO || data_.polyphony_mode() == SOLO) {
    mono_allocator_.Clear();
  } else {
    poly_allocator_.ClearNotes();
  }
  pressed_keys_.Clear();
  for (uint8_t i = 0; i < num_allocated_voices_; ++i) {
    voicecard_tx.Release(allocated_voices_[i]);
  }
  
  if (previous_generated_note_ != 0xff) {
    if (data_.arp_direction() != ARPEGGIO_DIRECTION_CHORD) {
      midi_dispatcher.OnNote(this, previous_generated_note_, 0);
    } else {
      for (uint8_t i = 0; i < pressed_keys_.size(); ++i) {
        const NoteEntry* retriggered_note = &pressed_keys_.sorted_note(i);
        midi_dispatcher.OnNote(this, retriggered_note->note, 0);
      }
    }
    previous_generated_note_ = 0xff;
  }
}

void Part::ResetAllControllers() {
  ignore_note_off_messages_ = 0;
  for (uint8_t i = 0; i < num_allocated_voices_; ++i) {
    voicecard_tx.ResetAllControllers(allocated_voices_[i]);
  }
}

void Part::MonoModeOn(uint8_t num_channels) {
  IGNORE_UNUSED(num_channels);
  data_.polyphony_mode() = MONO;
  //TouchVoiceAllocation();
}

void Part::PolyModeOn() {
  data_.polyphony_mode() = POLY;
  //TouchVoiceAllocation();
}

void Part::Reset() {
  for (uint8_t i = 0; i < num_allocated_voices_; ++i) {
    voicecard_tx.Reset(allocated_voices_[i]);
  }
}

void Part::Clock() {
  ++midi_clock_counter_;
  if (midi_clock_counter_ >= midi_clock_prescaler_) {
    midi_clock_counter_ = 0;
    if (arp_previous_mode_ != data_.arp_sequencer_mode()){
      arp_previous_mode_ = data_.arp_sequencer_mode();
      Stop();
      Start();
      return;
    }
#ifndef DISABLE_SEQUENCER
    ClockSequencer();
#endif
#ifndef DISABLE_ARPEGGIO
    ClockArpeggiator();
#endif
  }
  
  for (uint8_t i = 0; i < kNumLfos; ++i) {
    if (patch_.env_lfo(i).rate < kNumSyncedLfoRates) {
      ++lfo_step_[i];
      if (lfo_step_[i] >= lfo_cycle_length_[i]) {
        lfo_step_[i] = 0;
      }
      uint16_t increment = ResourcesManager::Lookup<uint16_t, uint8_t>(
          lfo_phase_increment_per_clock_tick, patch_.env_lfo(i).rate);
      uint16_t lfo_phase = increment * lfo_step_[i];
      // Force the phase of the LFO to match the MIDI clock.
      lfo_[i].set_phase(lfo_phase);
      // Set the LFO increment so that, if the MIDI clock is regular, we will
      // have reached the expected phase at the next clock tick.
      lfo_[i].set_phase_increment(increment / midi_clock_tick_duration_);
      midi_clock_tick_duration_ = 0;
    }
  }
}

  // KZ MOD: Allow muting parts with SWITCH1-6 on the performance page.
#ifndef DISABLE_PART_MUTES
void Part::ToggleMute(){
  if (is_muted_){
    is_muted_ = false;
    SetValue(PRM_PART_VOLUME, part_volume_, 0);
  } else {
    is_muted_ = true;
    part_volume_ = GetValue(PRM_PART_VOLUME);
    SetValue(PRM_PART_VOLUME, 0, 0);
  }
}
#endif
void Part::Start() {
  memset(sequencer_step_, 0, kNumSequences);
  memset(lfo_step_, 0, kNumLfos);
  midi_clock_counter_ = midi_clock_prescaler_;
  chord_step_counter_ = 0;
  previous_generated_note_ = 0xff;
#ifndef DISABLE_ARPEGGIO
  arp_pattern_mask_ = 0x1;
  arp_direction_ = (data_.arp_direction() == ARPEGGIO_DIRECTION_DOWN ? -1 : 1);
  arp_previous_mode_ = data_.arp_sequencer_mode();
  StartArpeggio();
#endif
}

void Part::Stop() {
  ignore_note_off_messages_ = 0;
  AllNotesOff();
}



uint16_t Part::TuneNote(uint8_t midi_note) {
  int16_t n = Clip(S16(midi_note) + S8U8Mul(data_.octave(), 12), 0, 127);
  int16_t note = U8U8Mul(n, 128);
  // Apply microtuning.
  if (data_.raga()) {
    note += ResourcesManager::Lookup<int16_t, uint8_t>(
        ResourceId(LUT_RES_SCALE_JUST + data_.raga() - 1),
        midi_note % 12);
  }
  // Apply part tuning.
  note += data_.tuning();
  return note;
}

uint8_t Part::AcceptNote(uint8_t midi_note) {
  if (data_.raga()) {
    auto pitch_shift = ResourcesManager::Lookup<int16_t, uint8_t>(
        ResourceId(LUT_RES_SCALE_JUST + data_.raga() - 1),
        midi_note % 12);
    if (pitch_shift == 32767) {
      return 0;
    }
  }
  return 1;
}

void Part::InternalNoteOn(uint8_t note, uint8_t velocity) {
  if (!AcceptNote(note)) { 
    return;
  }
  midi_dispatcher.OnNote(this, note, velocity);

  uint8_t retrigger_lfos = 0;
  if (data_.polyphony_mode() == MONO || data_.polyphony_mode() == SOLO) {
    mono_allocator_.NoteOn(note, velocity);
    uint16_t tuned_note = TuneNote(note);
    uint8_t legato = mono_allocator_.size() > 1;
    uint8_t pitch_drift = 0;
    if (data_.polyphony_mode() == MONO){
      // Trigger all allocated voices for MONO.
      for (uint8_t i = 0; i < num_allocated_voices_; ++i) {
        voicecard_tx.Trigger(
            allocated_voices_[i],
            tuned_note + pitch_drift,
            velocity,
            legato);
        pitch_drift += data_.spread();
      }
    } else {
      // Just Trigger first allocated voice for SOLO (single voice mono).
      voicecard_tx.Trigger(
                            allocated_voices_[0],
                            tuned_note + pitch_drift,
                            velocity,
                            legato);
    }
    retrigger_lfos = !legato || !data_.legato();
  } else {
    // Prevent the same note to be allocated twice on two different voices.
    uint8_t voice_index = poly_allocator_.FindActive(note);
    if (voice_index == 0xff) {
      voice_index = poly_allocator_.NoteOn(note);
    }
    if (data_.polyphony_mode() == UNISON_2X) {
      if (voice_index < poly_allocator_.size()) {
        voice_index <<= 1;
        uint16_t tuned_note = TuneNote(note);
        voicecard_tx.Trigger(allocated_voices_[voice_index], tuned_note, velocity, 0);
        uint16_t spread_note = tuned_note + data_.spread();
        voicecard_tx.Trigger(allocated_voices_[GetNextVoice(voice_index)], spread_note, velocity, 0);
        retrigger_lfos = 1;
      }
    } else if (data_.polyphony_mode() == CHAIN) {
      if (voice_index < (poly_allocator_.size() / 2)) {
        uint16_t tuned_note = TuneNote(note);
        uint16_t spread_note = tuned_note + voice_index * data_.spread();
        voicecard_tx.Trigger(allocated_voices_[voice_index], spread_note, velocity, 0);
      } else {
        midi_dispatcher.ForwardNote(this, note, velocity);
      }
      retrigger_lfos = 1;
    } else {
      if (voice_index < poly_allocator_.size()) {
        uint16_t tuned_note = TuneNote(note);
        uint16_t spread_note = tuned_note + voice_index * data_.spread();
        voicecard_tx.Trigger(allocated_voices_[voice_index], spread_note, velocity, 0);
        retrigger_lfos = 1;
      }
    }
  }
  
  // A note was played, we can retrigger the LFOs which are synced to the
  // envelopes.
  if (retrigger_lfos) {
    RetriggerLfos();
  }
}

void Part::InternalNoteOff(uint8_t note) {
  if (note == 0xff) {
    return;
  }
  midi_dispatcher.OnNote(this, note, 0);
  
  uint8_t retrigger_lfos = 0;
  if (data_.polyphony_mode() == MONO || data_.polyphony_mode() == SOLO) {
    // Only target first allocated voice in SOLO mode
    uint8_t num_target_voices = (data_.polyphony_mode() == SOLO) ? 1 : num_allocated_voices_;
    uint8_t top_note = mono_allocator_.most_recent_note().note;
    mono_allocator_.NoteOff(note);
    if (mono_allocator_.size() == 0) {
      // No key is pressed, we trigger the release segment.
      for (uint8_t i = 0; i < num_target_voices; ++i) {
        voicecard_tx.Release(allocated_voices_[i]);
      }
    } else {
      // We have released the most recent pressed key, but some other keys
      // are still pressed. Retrigger the most recent of them.
      if (top_note == note) {
        uint16_t tuned_note = TuneNote(mono_allocator_.most_recent_note().note);
        uint8_t pitch_drift = 0;
        for (uint8_t i = 0; i < num_target_voices; ++i) {
          uint8_t velocity = byteAnd(mono_allocator_.most_recent_note().velocity, 0x7f);
          voicecard_tx.Trigger(allocated_voices_[i], tuned_note + pitch_drift, velocity, 1);
          pitch_drift += data_.spread();
        }
        retrigger_lfos = !data_.legato();
      }
    }
  } else {
    uint8_t voice_index = poly_allocator_.NoteOff(note);
    if (data_.polyphony_mode() == UNISON_2X) {
      if (voice_index < poly_allocator_.size()) {
        voice_index <<= 1;
        voicecard_tx.Release(allocated_voices_[voice_index]);
        voicecard_tx.Release(allocated_voices_[GetNextVoice(voice_index)]);
      }
    } else if (data_.polyphony_mode() == CHAIN) {
      if (voice_index < (poly_allocator_.size() / 2)) {
        voicecard_tx.Release(allocated_voices_[voice_index]);
      } else {
        midi_dispatcher.ForwardNote(this, note, 0);
      }
    } else {
      if (voice_index < poly_allocator_.size()) {
        voicecard_tx.Release(allocated_voices_[voice_index]);
      }
    }
  }
  if (retrigger_lfos) {
    RetriggerLfos();
  }
}

void Part::UpdateLfos(uint8_t refresh_cycle) {
  ++midi_clock_tick_duration_;
  // No need to bother if there's no voicecard listening.
  if (num_allocated_voices_ == 0) {
    return;
  }
  for (uint8_t i = 0; i < kNumLfos; ++i) {
    uint8_t new_lfo_value = lfo_[i].Render(patch_.env_lfo(i).shape);
    // In order to avoid flooding the voicecard with too many LFO messages,
    // LFO2 and LFO3 are refreshed at half the control rate. This makes LFO1
    // better for pseudo-audio rate modulation!
    if ((i == 0) || refresh_cycle) {
      if (new_lfo_value != lfo_previous_values_[i]) {
        for (uint8_t j = 0; j < num_allocated_voices_; ++j) {
          voicecard_tx.WriteLfo(allocated_voices_[j], i, new_lfo_value);
        }
      }
      lfo_previous_values_[i] = new_lfo_value;
    }
    if (patch_.env_lfo(i).retrigger_mode == LFO_SYNC_MODE_MASTER) {
      // Detect LFO cycle completion - by comparing the phase to the increment
      // if the LFO is free running, or by looking the step index if the LFO
      // is tied to the MIDI clock.
      uint8_t lfo_looped = lfo_[i].looped();
      if (patch_.env_lfo(i).rate < kNumSyncedLfoRates) {
        lfo_looped = lfo_step_[i] == 0;
      }
      if (lfo_looped) {
        for (uint8_t j = 0; j < num_allocated_voices_; ++j) {
          voicecard_tx.RetriggerEnvelope(allocated_voices_[j], i);
        }
      }
    }
  }
}

void Part::RetriggerLfos() {
  for (uint8_t i = 0; i < kNumLfos; ++i) {
    if (patch_.env_lfo(i).retrigger_mode == LFO_SYNC_MODE_SLAVE) {
      lfo_[i].set_phase(0);
      lfo_step_[i] = 0;
    }
  }
}

void Part::ClockSequencer() {
  // Update the value of the sequencer in the modulation matrix.
#ifndef DISABLE_SEQUENCER
  for (uint8_t i = 0; i < 2; ++i) {
    uint8_t value = data_.step_value(i, sequencer_step_[i]);
    if (data_.sequence_length(i)) {
      WriteToAllVoices(VOICECARD_DATA_MODULATION, MOD_SRC_SEQ_1 + i, value);
    }
  }
  
  // Trigger notes if there's a note sequence, and if a key is pressed on the
  // keyboard.
  if (data_.arp_sequencer_mode() == ARP_SEQUENCER_MODE_NOTE &&
      pressed_keys_.size() && data_.sequence_length(2)) {
    NoteStep n = data_.note_step(sequencer_step_[2]);
    uint8_t recent_note = pressed_keys_.most_recent_note().note;
    uint8_t note = Clip(S16(n.note) + recent_note - 60, 0, 127);
    if (!n.gate) {
      // Just kill the previous note.
      InternalNoteOff(previous_generated_note_);
      previous_generated_note_ = 0xff;
    } else {
      if (!n.legato) {
        // Kill the previous note and move to the new note.
        InternalNoteOff(previous_generated_note_);
        InternalNoteOn(note, U7(n.velocity));
      } else {
        // Kill the previous note, but only after having started playing the
        // new one.
        if (previous_generated_note_ != note) {
          InternalNoteOn(note, U7(n.velocity));
          InternalNoteOff(previous_generated_note_);
        } else {
          // Do nothing, this is just a note being held.
        }
      }
      previous_generated_note_ = note;
    }
        // KZ MOD: -------  Chord Sequencer ------------
  } else if (data_.arp_sequencer_mode() == ARP_SEQUENCER_MODE_CHORD){
    // Step the chord every nth steps. Note sequence Length parameter is now used for n and note sequence is locked at 16.
    if (chord_step_counter_ % data_.sequence_length(2) == 0){
      AllNotesOff();
      // Play the next 4 notes in the sequencer together as a chord
      uint8_t chord_step = chord_step_counter_ / data_.sequence_length(2);
      for (uint8_t i = 0; i < 4; ++i) {
        NoteStep n = data_.note_step(chord_step * 4 + i);
        if (n.gate) {
          pressed_keys_.NoteOn(n.note, n.velocity & 0x7f);
        }
      }
    }
    ++chord_step_counter_;
    if (chord_step_counter_ >= 4 * data_.sequence_length(2)){
      chord_step_counter_ = 0;
    }

  }
#endif


#ifndef DISABLE_LAUNCHKEY_MODE
  // KZ MOD ---- LaunchKeyStuff ------------
  // Dispatch note to Lanuchkey here?
  // RGB LED Midi Notes
  // CH16, Notes 40 - 51, but will need arrays as they arent sequential
  // Color Set by Velocity, (see lookup table)
  // THIS IS IN THE WRONG SPOT - SHOULD BE IN MULTI
  if (system_settings.data().launchkey_mode){
    uint8_t onPad = sequencer_step_[0] & 7;
    uint8_t offPad = (sequencer_step_[0] + 7) & 7;
//    midi_dispatcher.SetLaunchKeyPadColor(pgm_read_byte(launchkey_rgb_pad_notes_top + onPad), 100); // ON
//    midi_dispatcher.SetLaunchKeyPadColor(pgm_read_byte(launchkey_rgb_pad_notes_top + offPad), 0); // OFF
  }
#endif
  
  // Jump to the next step in the sequencer.
  for (uint8_t i = 0; i < kNumSequences; ++i) {
    ++sequencer_step_[i];
    if (i == 2
        && data_.arp_sequencer_mode() == ARP_SEQUENCER_MODE_CHORD
        && sequencer_step_[i] >= 16){
      // Lock the note sequence length to 16 in Chord Sequencer mode
      // As the Length parameter is now used to determine the chord duration.
      sequencer_step_[i] = 0;
    } else if (sequencer_step_[i] >= data_.sequence_length(i)) {
      sequencer_step_[i] = 0;
    }
  }
}

#ifndef DISABLE_ARPEGGIO
void Part::ClockArpeggiator() {
  using rs = ResourcesManager;
  uint16_t pattern = rs::Lookup<uint16_t, uint8_t>(lut_res_arpeggiator_patterns, data_.arp_pattern());
  uint8_t has_arpeggiator_note = (arp_pattern_mask_ & pattern) ? 255 : 0;
  // Update the gate value in the modulation matrix.
  for (uint8_t i = 0; i < num_allocated_voices_; ++i) {
    WriteToAllVoices(VOICECARD_DATA_MODULATION, MOD_SRC_ARP_STEP, has_arpeggiator_note);
  }
  
  // Trigger notes only if the arp is on, and if keys are pressed.
  if (data_.arp_sequencer_mode() == ARP_SEQUENCER_MODE_ARPEGGIATOR || data_.arp_sequencer_mode() >= ARP_SEQUENCER_MODE_ARPEGGIATOR_LATCH) {
    if (pressed_keys_.size() && has_arpeggiator_note) {
      if (data_.arp_direction() != ARPEGGIO_DIRECTION_CHORD) {
        InternalNoteOff(previous_generated_note_);
        StepArpeggio();
        const NoteEntry* arpeggio_note = &pressed_keys_.sorted_note(arp_step_);
        if (data_.arp_direction() == ARPEGGIO_DIRECTION_AS_PLAYED) {
          arpeggio_note = &pressed_keys_.played_note(arp_step_);
        }
        uint8_t note = arpeggio_note->note;
        uint8_t velocity = U7(arpeggio_note->velocity);
        note += 12 * arp_octave_;
        while (note > 127) {
          note -= 12;
        }
        InternalNoteOn(note, velocity);
        previous_generated_note_ = note;
      } else {
        for (uint8_t i = 0; i < pressed_keys_.size(); ++i) {
          const NoteEntry* retriggered_note = &pressed_keys_.sorted_note(i);
          InternalNoteOn(retriggered_note->note, U7(retriggered_note->velocity));
        }
        // This is arbitrary, and used only to know at the next blank step in
        // the sequence that we have to send a note off event.
        previous_generated_note_ = 60;
      }
    } else {
      if (data_.arp_direction() != ARPEGGIO_DIRECTION_CHORD) {
        InternalNoteOff(previous_generated_note_);
      } else {
        if (previous_generated_note_ != 0xff) {
          for (uint8_t i = 0; i < pressed_keys_.size(); ++i) {
            const NoteEntry* retriggered_note = &pressed_keys_.sorted_note(i);
            InternalNoteOff(retriggered_note->note);
          }
        }
      }
      previous_generated_note_ = 0xff;
    }    
  }
  
  arp_pattern_mask_ <<= 1;
  if (!arp_pattern_mask_) {
    arp_pattern_mask_ = 1;
  }
}

void Part::StartArpeggio() {
  if (arp_direction_ == 1) {
    arp_octave_ = 0;
    arp_step_ = 0;
  } else {
    arp_step_ = pressed_keys_.size() - 1;
    arp_octave_ = data_.arp_octave() - 1;
  }
}

void Part::StepArpeggio() {
  uint8_t num_notes = pressed_keys_.size();
  if (data_.arp_direction() == ARPEGGIO_DIRECTION_RANDOM) {
    uint8_t random_byte = Random::GetByte();
    arp_octave_ = lowNibble(random_byte);
    arp_step_ = highNibble(random_byte);
    while (arp_octave_ >= data_.arp_octave()) {
      arp_octave_ -= data_.arp_octave();
    }
    while (arp_step_ >= num_notes) {
      arp_step_ -= num_notes;
    }
  } else {
    arp_step_ += arp_direction_;
    uint8_t change_octave = 0;
    if (arp_step_ >= num_notes) {
      arp_step_ = 0;
      change_octave = 1;
    } else if (arp_step_ < 0) {
      arp_step_ = num_notes - 1;
      change_octave = 1;
    }
    if (change_octave) {
      arp_octave_ += arp_direction_;
      if (arp_octave_ >= data_.arp_octave() || arp_octave_ < 0) {
        if (data_.arp_direction() == ARPEGGIO_DIRECTION_UP_DOWN) {
          arp_direction_ = -arp_direction_;
          StartArpeggio();
          if (num_notes > 1 || data_.arp_octave() > 1) {
            StepArpeggio();
          }
        } else {
          StartArpeggio();
        }
      }
    }
  }
}
#endif

void Part::WriteToAllVoices(uint8_t data_type, uint8_t address, uint8_t value) {
  for (uint8_t i = 0; i < num_allocated_voices_; ++i) {
    voicecard_tx.WriteData(allocated_voices_[i], data_type, address, value);
  }
}

}  // namespace ambika
