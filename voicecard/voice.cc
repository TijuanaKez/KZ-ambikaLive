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
// Main synthesis engine.

#include "voicecard/voice.h"

#include "common/features.h"

#include "voicecard/audio_out.h"
#include "voicecard/oscillator.h"
#include "voicecard/sub_oscillator.h"
#include "voicecard/transient_generator.h"

using namespace avrlib;

namespace ambika {

/* extern */
Voice voice;

Oscillator osc_1;
Oscillator osc_2;
SubOscillator sub_osc;
TransientGenerator transient_generator;

/* <static> */

Patch Voice::patch_object;
VoicePart Voice::part_object;

Lfo Voice::voice_lfo;
Envelope Voice::envelope[kNumEnvelopes];
uint8_t Voice::gate;
int16_t Voice::pitch_increment;
int16_t Voice::pitch_target;
int16_t Voice::pitch_value;
uint8_t Voice::mod_source_value[kNumModulationSources];
int8_t Voice::modulation_destinations[kNumModulationDestinations];
int16_t Voice::dst[kNumModulationDestinations];
uint8_t Voice::buffer[kAudioBlockSize];
uint8_t Voice::osc2_buffer[kAudioBlockSize];
bool Voice::sync_state[kAudioBlockSize];
bool Voice::no_sync[kAudioBlockSize];
bool Voice::dummy_sync_state[kAudioBlockSize];
/* </static> */


// for use in init patch
static constexpr ModSource NULL_MOD_ENV_SRC = static_cast<ModSource>(0);

static const Patch::Parameters init_patch_params PROGMEM {
  // Oscillators
  //WAVEFORM_WAVETABLE_1 + 1, 63, -24, 0,
  .osc = {
      {WAVEFORM_NONE, 0, 0, 0},
      {WAVEFORM_NONE, 0, 0, 0}
  },
  // Mixer
  .mix_balance = 32,
  .mix_op = OP_SUM,
  .mix_parameter = 0,
  .mix_sub_osc_shape = WAVEFORM_SUB_OSC_SQUARE_1,
  .mix_sub_osc = 0,
  .mix_noise = 0,
  .mix_fuzz = 0,
  .mix_crush = 0,
  // Filter
  .filter = {
    {127, 0, FILTER_MODE_LP},
    {0, 0, FILTER_MODE_LP}
  },
  .filter_env = 63,
  .filter_lfo = 0,
  // ADSR
  .env_lfo = {
      {0, 40, 20, 60, LFO_WAVEFORM_TRIANGLE, 0, 0, LFO_SYNC_MODE_FREE},
      {0, 40, 20, 60, LFO_WAVEFORM_TRIANGLE, 0, 0, LFO_SYNC_MODE_FREE},
      {0, 40, 20, 60, LFO_WAVEFORM_TRIANGLE, 0, 0, LFO_SYNC_MODE_FREE}
  },
  .voice_lfo_shape = LFO_WAVEFORM_TRIANGLE,
  .voice_lfo_rate = 16,

  // Routing
  .modulation = {
      {MOD_SRC_LFO_1, MOD_DST_OSC_1, 0},
      {MOD_SRC_ENV_1, MOD_DST_OSC_2, 0},
      {MOD_SRC_LFO_1, MOD_DST_OSC_1, 0},
      {MOD_SRC_ENV_1, MOD_DST_OSC_2, 0},
      {MOD_SRC_ENV_1, MOD_DST_PARAMETER_1, 0},
      {MOD_SRC_LFO_1, MOD_DST_PARAMETER_2, 0},
      {MOD_SRC_LFO_2, MOD_DST_MIX_BALANCE, 0},
      {MOD_SRC_LFO_4, MOD_DST_PARAMETER_1, 63},
      {MOD_SRC_SEQ_1, MOD_DST_PARAMETER_1, 0},
      {MOD_SRC_SEQ_2, MOD_DST_PARAMETER_2, 0},
      {MOD_SRC_ENV_2, MOD_DST_VCA, 32},
      {MOD_SRC_VELOCITY, MOD_DST_VCA, 0},
      {MOD_SRC_PITCH_BEND, MOD_DST_OSC_1_2_COARSE, 0},
      {MOD_SRC_LFO_1, MOD_DST_OSC_1_2_COARSE, 0}
  },

  // Modifiers
  // this will be whatever modulation source corresponds to '0'
  .modifier = {
      {.operands = {NULL_MOD_ENV_SRC, NULL_MOD_ENV_SRC}, .op = MODIFIER_NONE},
      {.operands = {NULL_MOD_ENV_SRC, NULL_MOD_ENV_SRC}, .op = MODIFIER_NONE},
      {.operands = {NULL_MOD_ENV_SRC, NULL_MOD_ENV_SRC}, .op = MODIFIER_NONE},
      {.operands = {NULL_MOD_ENV_SRC, NULL_MOD_ENV_SRC}, .op = MODIFIER_NONE},
  },

  .filter_velo = 0,
  .filter_kbt = 0,

  // Padding
  .padding = {0}
};

/* static */
void Voice::Init() {
    pitch_value = 0;
  for (uint8_t i = 0; i < kNumEnvelopes; ++i) {
    envelope[i].Init();
  }
  for (uint8_t i = 0; i < kAudioBlockSize; ++i) {
    no_sync[i] = 0;
    sync_state[i] = 0;
    dummy_sync_state[i] = 0;
  }
  Patch::Parameters p;
  ResourcesManager::Load(&init_patch_params, 0, &p);
  patch() = Patch(p);
  ResetAllControllers();
  part().volume() = 127;
  part().portamento_time() = 0;
  part().legato() = 0;
  Kill();
}

/* static */
void Voice::ResetAllControllers() {
    mod_source_value[MOD_SRC_PITCH_BEND] = 128;
    mod_source_value[MOD_SRC_AFTERTOUCH] = 0;
    mod_source_value[MOD_SRC_WHEEL] = 0;
    mod_source_value[MOD_SRC_WHEEL_2] = 0;
    mod_source_value[MOD_SRC_EXPRESSION] = 0;
    mod_source_value[MOD_SRC_CONSTANT_4] = 4;
    mod_source_value[MOD_SRC_CONSTANT_8] = 8;
    mod_source_value[MOD_SRC_CONSTANT_16] = 16;
    mod_source_value[MOD_SRC_CONSTANT_32] = 32;
    mod_source_value[MOD_SRC_CONSTANT_64] = 64;
    mod_source_value[MOD_SRC_CONSTANT_128] = 128;
    mod_source_value[MOD_SRC_CONSTANT_256] = 255;
}

/* static */
void Voice::TriggerEnvelope(Envelope::Stage stage) {
  for (uint8_t i = 0; i < kNumEnvelopes; ++i) {
    TriggerEnvelope(i, stage);
  }
}

/* static */
void Voice::TriggerEnvelope(uint8_t index, Envelope::Stage stage) {
  envelope[index].Trigger(stage);
}

/* static */
void Voice::Trigger(uint16_t note, uint8_t velocity, uint8_t legato) {
    pitch_target = note;
  if (!part().legato() || !legato) {
    gate = 255;
    TriggerEnvelope(Envelope::Stage::ATTACK);
    transient_generator.Trigger();
    mod_source_value[MOD_SRC_VELOCITY] = velocity;
    mod_source_value[MOD_SRC_RANDOM] = Random::state_msb();
    osc_2.Reset();
  }
  if (pitch_value == 0 || (part().legato() && !legato)) {
    pitch_value = pitch_target;
    // don't bother calculating delta or looking up the portamento
    // TODO should this be zero?
    pitch_increment = 1;
  } else {
    int16_t delta = pitch_target - pitch_value;
    int32_t increment = ResourcesManager::Lookup<uint16_t, uint8_t>(
        lut_res_env_portamento_increments, part().portamento_time());
    // TODO WHY does this not work if increment is uint16_t??
    pitch_increment = highWord(S32(delta * increment));
    if (pitch_increment == 0) {
      if (delta < 0) {
        pitch_increment = -1;
      } else {
        pitch_increment = 1;
      }
    }
  }
}

/* static */
void Voice::Release() {
    gate = 0;
  TriggerEnvelope(Envelope::Stage::RELEASE);
}

/* static */
inline void Voice::LoadSources() {
  // Rescale the value of each modulation sources. Envelopes are in the
  // 0-16383 range ; just like pitch. All are scaled to 0-255.
  mod_source_value[MOD_SRC_NOISE] = Random::GetByte();
    mod_source_value[MOD_SRC_ENV_1] = envelope[0].Render();
    mod_source_value[MOD_SRC_ENV_2] = envelope[1].Render();
    mod_source_value[MOD_SRC_ENV_3] = envelope[2].Render();
    mod_source_value[MOD_SRC_NOTE] = U14ShiftRight6(pitch_value);
    mod_source_value[MOD_SRC_GATE] = gate;
    mod_source_value[MOD_SRC_LFO_4] = voice_lfo.Render(patch().voice_lfo_shape());

  // Apply the modulation operators
  uint8_t ops[9] {0};
  for (uint8_t i = 0; i < kNumModifiers; ++i) {
    if (patch().modifier(i).op == MODIFIER_NONE) {
      continue;
    }
    const Modifier& mod = patch().modifier(i);
    const auto mod_src_op_i = static_cast<ModSource>(MOD_SRC_OP_1 + i);

    uint8_t x = get_mod_source_value(mod.operands[0]);
    uint8_t y = get_mod_source_value(mod.operands[1]);
    ModifierOp op = patch().modifier(i).op;
    if (op <= MODIFIER_LE) {
      ops[1] = (x / 2u) + (y / 2u);
      ops[2] = U8U8MulShift8(x, y);
      ops[3] = S8U8MulShift8(x + 128, y) + 128;
      if (x > y) {
        ops[4] = x; // max
        ops[5] = y; // min
        ops[7] = 255;
        ops[8] = 0;
      } else {
        ops[4] = y; // max
        ops[5] = x; // min
        ops[7] = 0;
        ops[8] = 255;
      }
      ops[6] = x ^ y;
      set_mod_source_value(mod_src_op_i, ops[op]);
    } else if (op == MODIFIER_QUANTIZE) {
      uint8_t mask = 0;
      while (y /= 2) {
        mask /= 2;
        mask = byteOr(mask, 0x80);
      }
      mod_source_value[MOD_SRC_OP_1 + i] = byteAnd(x, mask);
    } else if (op == MODIFIER_LAG_PROCESSOR) {
      y /= 4;
      ++y;
      uint16_t v = U8U8Mul(256-y, get_mod_source_value(mod_src_op_i));
      v += y*x;
      set_mod_source_value(mod_src_op_i, highByte(v));
    }
  }

  set_mod_dest_value(MOD_DST_VCA, part().volume() * 2);
  
  // Load and scale to 0-16383 the initial value of each modulated parameter.
  constexpr int16_t uint_14_midrange = 8192;
  dst[MOD_DST_OSC_1] = uint_14_midrange;
  dst[MOD_DST_OSC_2] = uint_14_midrange;
  dst[MOD_DST_OSC_1_2_COARSE] = uint_14_midrange;
  dst[MOD_DST_OSC_1_2_FINE] = uint_14_midrange;

  dst[MOD_DST_PARAMETER_1] = U8U8Mul(patch().osc(0).parameter(), 128);
  dst[MOD_DST_PARAMETER_2] = U8U8Mul(patch().osc(1).parameter(), 128);

  dst[MOD_DST_MIX_BALANCE] = patch().mix_balance() << 8u;
  dst[MOD_DST_MIX_PARAM] = patch().mix_parameter() << 8u;
  dst[MOD_DST_MIX_FUZZ] = patch().mix_fuzz() << 8u;
  dst[MOD_DST_MIX_CRUSH] = patch().mix_crush() << 8u;
  dst[MOD_DST_MIX_NOISE] = patch().mix_noise() << 8u;
  dst[MOD_DST_MIX_SUB_OSC] = patch().mix_sub_osc() << 8u;

  uint16_t cutoff = U8U8Mul(patch().filter(0).cutoff, 128);
  dst[MOD_DST_FILTER_CUTOFF] = S16ClipU14(cutoff + pitch_value - 8192);
  dst[MOD_DST_FILTER_RESONANCE] = patch().filter(0).resonance << 8u;

  dst[MOD_DST_ATTACK] = uint_14_midrange;
  dst[MOD_DST_DECAY] = uint_14_midrange;
  dst[MOD_DST_RELEASE] = uint_14_midrange;
  dst[MOD_DST_LFO_4] = U8U8Mul(patch().voice_lfo_rate(), 128);
}


/* static */
inline void Voice::ProcessModulationMatrix() {
  // Apply the modulations in the modulation matrix.
  for (uint8_t i = 0; i < kNumModulations; ++i) {
    int8_t amount = patch().modulation(i).amount;

    // The rate of the last modulation is adjusted by the wheel.
    if (i == kNumModulations - 1) {
      amount = S8U8MulShift8(amount, mod_source_value[MOD_SRC_WHEEL]);
    }
    const auto source = static_cast<ModSource>(patch().modulation(i).source);
    const auto dest = static_cast<ModDestination>(patch().modulation(i).destination);
    uint8_t source_value = mod_source_value[source];
    if (dest == MOD_DST_VCA) {
      // The VCA modulation is multiplicative, not additive.
      // Yet another special case :(.
      if (amount < 0) {
        amount = -amount;
        source_value = 255 - source_value;
      }
      if (amount != 63) {
        source_value = U8Mix(255, source_value, amount * 4);
      }
      modulation_destinations[MOD_DST_VCA] = U8U8MulShift8(modulation_destinations[MOD_DST_VCA], source_value);
    } else {
      int16_t current_mod_value = dst[dest];
      if ((source >= MOD_SRC_LFO_1 && source <= MOD_SRC_LFO_4) ||
          source == MOD_SRC_PITCH_BEND || source == MOD_SRC_NOTE) {
        // These sources are "AC-coupled" (128 = no modulation).
        current_mod_value += S8S8Mul(amount, source_value + 128);
      } else {
        current_mod_value += S8U8Mul(amount, source_value);
      }
      dst[dest] = S16ClipU14(current_mod_value);
    }
  }
}

/* static */
inline void Voice::UpdateDestinations() {
  // Hardcoded filter modulations.
  // Pichenettes: By default, the resonance tracks the note. Tracking works best when the
  // transistors are thermically coupled. You can disable tracking by applying
  // a negative modulation from NOTE to CUTOFF. (Phase57 mod below exposes this parameter explicitly)
  uint16_t cutoff = dst[MOD_DST_FILTER_CUTOFF];
  cutoff = S16ClipU14(cutoff + S8U8Mul(patch().filter_env(), mod_source_value[MOD_SRC_ENV_2]));
  cutoff = S16ClipU14(cutoff + S8S8Mul(patch().filter_lfo(), mod_source_value[MOD_SRC_LFO_2] + 128));

  // Phase57 mods: velocity to cutoff & keyboard tracking to cutoff.
  // velocity to filter freq
  cutoff = S16ClipU14(cutoff + S8U8Mul(patch().filter_velo(), mod_source_value[MOD_SRC_VELOCITY]));
  // keyb tracking (note) to filter freq
  cutoff = S16ClipU14(cutoff + S8S8Mul(patch().filter_kbt(), mod_source_value[MOD_SRC_NOTE] + 128));
  
  // Store in memory all the updated parameters.
  modulation_destinations[MOD_DST_FILTER_CUTOFF] = U14ShiftRight6(cutoff);
#ifdef POLIVOKS_FILTERBOARD
  // KZ MOD: restored from the YAM voicecard. The Polivoks filter board's
  // resonance CV is inverted, so the control has to be flipped here or the
  // resonance knob works backwards. POLIVOKS_FILTERBOARD is enabled in
  // common/features.h, but the MachFour-derived voice card in this tree had no
  // consumer for it at all -- the switch was silently doing nothing.
  modulation_destinations[MOD_DST_FILTER_RESONANCE] =
      U14ShiftRight6(16383 - dst[MOD_DST_FILTER_RESONANCE]);
#else
  modulation_destinations[MOD_DST_FILTER_RESONANCE] = U14ShiftRight6(dst[MOD_DST_FILTER_RESONANCE]);
#endif
  modulation_destinations[MOD_DST_MIX_CRUSH] = S8(highByte(U16(dst[MOD_DST_MIX_CRUSH]))) + 1;

  osc_1.set_parameter(U15ShiftRight7(dst[MOD_DST_PARAMETER_1]));
  osc_1.set_fm_parameter(patch().osc(0).range() + 36); // minimum zero
  osc_2.set_parameter(U15ShiftRight7(dst[MOD_DST_PARAMETER_2]));
  osc_2.set_fm_parameter(patch().osc(1).range() + 36); // minimum zero
  
  int8_t attack_mod = U15ShiftRight7(dst[MOD_DST_ATTACK]) - 64;
  int8_t decay_mod = U15ShiftRight7(dst[MOD_DST_DECAY]) - 64;
  int8_t release_mod = U15ShiftRight7(dst[MOD_DST_RELEASE]) - 64;
  for (int i = 0; i < kNumEnvelopes; ++i) {
    auto env_lfo = patch().env_lfo(i);
    // integer addition is promoted to 16 bits so no worries
    uint8_t attack = Clip(S16(env_lfo.attack + attack_mod), 0, 127);
    uint8_t decay = Clip(S16(env_lfo.decay + decay_mod), 0, 127);
    uint8_t release = Clip(S16(env_lfo.release + release_mod), 0, 127);
    uint8_t sustain = patch().env_lfo(i).sustain;
    envelope[i].Update(attack, decay, sustain, release);
  }
  uint8_t lfo_increment_index = U14ShiftRight6(dst[MOD_DST_LFO_4]) / 2;
  voice_lfo.set_phase_increment(ResourcesManager::Lookup<uint16_t, uint8_t>(
      lut_res_lfo_increments, lfo_increment_index));
}

/* static */
inline void Voice::RenderOscillators() {
  // Apply portamento.
  int16_t base_pitch = pitch_value + pitch_increment;
  if ((pitch_increment > 0) ^ (base_pitch < pitch_target)) {
    base_pitch = pitch_target;
    pitch_increment = 0;
  }
    pitch_value = base_pitch;

  // -4 / +4 semitones by the vibrato and pitch bend (coarse).
  // -0.5 / +0.5 semitones by the vibrato and pitch bend (fine).
  base_pitch += (dst[MOD_DST_OSC_1_2_COARSE] - 8192) >> 4u;
  base_pitch += (dst[MOD_DST_OSC_1_2_FINE] - 8192) >> 7u;

  // Update the oscillator parameters.
  for (uint8_t i = 0; i < kNumOscillators; ++i) {
    int16_t pitch = base_pitch;
    // -36 / +36 semitones by the range controller.
    //
    // KZ MOD: WAVEFORM_FM_FB was missing from this test. Both FM shapes use
    // `range` as the modulator frequency ratio rather than as a coarse tune --
    // see set_fm_parameter above -- so adding it to the pitch as well detunes
    // the carrier. YAM excludes both; this tree excluded only WAVEFORM_FM.
    if (patch().osc(i).shape() != WAVEFORM_FM &&
        patch().osc(i).shape() != WAVEFORM_FM_FB) {
      pitch += S16(patch().osc(i).range() * 128);
    }
    // -1 / +1 semitones by the detune controller.
    pitch += patch().osc(i).detune();
    // -16 / +16 semitones by the routed modulations.
    pitch += (dst[MOD_DST_OSC_1 + i] - 8192) / 4;

    if (pitch >= kHighestNote) {
      pitch = kHighestNote;
    }
    // Extract the pitch increment from the pitch table.
    int16_t ref_pitch = pitch - kPitchTableStart;
    uint8_t num_shifts = 0;
    while (ref_pitch < 0) {
      ref_pitch += kOctave;
      ++num_shifts;
    }
    using rs = ResourcesManager;
    uint24_t increment = U32(rs::Lookup<uint16_t, uint16_t>(lut_res_oscillator_increments, ref_pitch / 2)) << 8;

    // Divide the pitch increment by the number of octaves we had to transpose
    // to get a value in the lookup table.
    while (num_shifts--) {
      increment >>= 1;
    }

    // Now the oscillators can recompute all their internal variables!
    int8_t midi_note = U15ShiftRight7(pitch) - 12;
    if (midi_note < 0) {
      midi_note = 0;
    }
    if (i == 0) {
      // Seems like sub osc is fixed 1 octave below osc1
      sub_osc.set_increment(increment / 2);

      // OSC1's sync input is a null array (no_sync is never written to), and outputs its sync state to sync_state
      osc_1.Render(patch().osc(0).shape(), midi_note, increment, no_sync, sync_state, buffer);
    } else {
      // OSC2's sync input is set to OSC1's sync output if mix_op is OP_SYNC, else it gets no_sync.
      // dummy sync state is there just to fill the argument, it's never read from
      osc_2.Render(patch().osc(1).shape(), midi_note, increment,
                   patch().mix_op() == OP_SYNC ? sync_state : no_sync, dummy_sync_state, osc2_buffer);
    }
  }
}

/* static */
void Voice::ProcessBlock() {
  LoadSources();
  ProcessModulationMatrix();
  UpdateDestinations();

  // Skip the oscillator rendering code if the VCA output has converged to
  // a small value.
  if (vca() < 2) {
    for (uint8_t i = 0; i < kAudioBlockSize; i += 2) {
      audio_buffer.overwrite2(128, 128);
    }
    return;
  }

  RenderOscillators();

  uint8_t osc_2_gain = U14ShiftRight6(dst[MOD_DST_MIX_BALANCE]);
  uint8_t wet_gain = U14ShiftRight6(dst[MOD_DST_MIX_PARAM]);
  uint8_t osc_1_gain = ~osc_2_gain;
  uint8_t dry_gain = ~wet_gain;

  // Mix oscillators.
  switch (patch().mix_op()) {
    case OP_RING_MOD:
      for (uint8_t i = 0; i < kAudioBlockSize; ++i) {
        uint8_t mix = U8Mix(buffer[i], osc2_buffer[i], osc_1_gain, osc_2_gain);
        uint8_t ring = S8S8MulShift8(buffer[i] + 128, osc2_buffer[i] + 128) + 128;
        buffer[i] = U8Mix(mix, ring, dry_gain, wet_gain);
      }
      break;
    case OP_XOR:
      for (uint8_t i = 0; i < kAudioBlockSize; ++i) {
        uint8_t mix = U8Mix(buffer[i], osc2_buffer[i], osc_1_gain, osc_2_gain);
        uint8_t xord = buffer[i] ^osc2_buffer[i];
        buffer[i] = U8Mix(mix, xord, dry_gain, wet_gain);
      }
      break;
    case OP_FOLD:
      for (uint8_t i = 0; i < kAudioBlockSize; ++i) {
        uint8_t mix = U8Mix(buffer[i], osc2_buffer[i], osc_1_gain, osc_2_gain);
        buffer[i] = U8Mix(mix, mix + 128, dry_gain, wet_gain);
      }
      break;
    case OP_BITS:
      {
        wet_gain >>= 5u;
        wet_gain = 255 - ((1u << wet_gain) - 1);
        for (uint8_t i = 0; i < kAudioBlockSize; ++i) {
          buffer[i] = U8Mix(buffer[i], osc2_buffer[i], osc_1_gain, osc_2_gain) & wet_gain;
        }
        break;
      }
    default:
      for (uint8_t i = 0; i < kAudioBlockSize; ++i) {
        buffer[i] = U8Mix(buffer[i], osc2_buffer[i], osc_1_gain, osc_2_gain);
      }
      break;
  }

    // Mix-in sub oscillator or transient generator.
  uint8_t sub_gain = U15ShiftRight7(dst[MOD_DST_MIX_SUB_OSC]);
  if (patch().mix_sub_osc_shape() < WAVEFORM_SUB_OSC_CLICK) {
    sub_osc.Render(patch().mix_sub_osc_shape(), buffer, sub_gain);
  } else {
    sub_gain *= 2;
    transient_generator.Render(patch().mix_sub_osc_shape(), buffer, sub_gain);
  }

  uint8_t noise = Random::state_msb();
  uint8_t noise_gain = U15ShiftRight7(dst[MOD_DST_MIX_NOISE]);
  uint8_t signal_gain = byteInverse(noise_gain);
  wet_gain = U14ShiftRight6(dst[MOD_DST_MIX_FUZZ]);
  dry_gain = byteInverse(wet_gain);


  // Mix with noise, and apply distortion. The loop processes samples by 2 to
  // avoid some of the overhead of audio_buffer.overwrite()
  for (uint8_t i = 0; i < kAudioBlockSize; i += 2) {
    noise = U8(noise * 73) + 1;
    uint8_t signal_noise_a = U8Mix(buffer[i], noise, signal_gain, noise_gain);
    auto distortion_a = ResourcesManager::Lookup<uint8_t, uint8_t>(wav_res_distortion, signal_noise_a);
    uint8_t a = U8Mix(signal_noise_a, distortion_a, dry_gain, wet_gain);

    noise = U8(noise * 73) + 1;
    uint8_t signal_noise_b = U8Mix(buffer[i + 1], noise, signal_gain, noise_gain);
    auto distortion_b = ResourcesManager::Lookup<uint8_t, uint8_t>(wav_res_distortion, signal_noise_b);
    uint8_t b = U8Mix(signal_noise_b, distortion_b, dry_gain, wet_gain);
    audio_buffer.overwrite2(a, b);
  }
}

}  // namespace ambika
