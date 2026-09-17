// Copyright 2012 Emilie Gillet.
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

#include "voicecard/oscillator.h"

#include "voicecard/wavetables.h"

#include "voicecard/voicecard.h"

/*
 * A general note about optimising compares:
 * checking a < b or a >= b where a, b are 16 bits
 * can be achieved simply by checking the most significant 8 bits.
 * However, when checking a <= b or a > b, the previous optimisation
 * will cause some inaccuracy since these comparisons are affected by the lower 8 bits.
 */

namespace ambika {

// This variant has a larger register width, but yields faster code.
// ------- Silence (useful when processing external signals) -----------------
void Oscillator::RenderSilence(uint8_t* buffer) {
  for (uint8_t samples_left = kAudioBlockSize; samples_left > 0; samples_left--) {
    *buffer++ = 128;
  }
}

// ------- Band-limited PWM --------------------------------------------------
void Oscillator::RenderBandlimitedPwm(uint8_t* buffer) {
  uint8_t balance_index = U8Swap4(note /* - 12 play safe with Aliasing */);
  uint8_t gain_2 = highNibbleUnshifted(balance_index);
  uint8_t gain_1 = byteInverse(gain_2);

  const uint8_t wave_index_1 = lowNibble(balance_index);
  const uint8_t wave_index_2 = U8AddClip(wave_index_1, 1, kNumZonesHalfSampleRate);

  const uint8_t* wave_1 = waveform_table[WAV_RES_BANDLIMITED_SAW_1 + wave_index_1];
  const uint8_t* wave_2 = waveform_table[WAV_RES_BANDLIMITED_SAW_1 + wave_index_2];

  uint16_t shift = word(parameter + 128u, 0);

  // For higher pitched notes, simply use 128 instead of 192
  uint8_t scale = 192u - (parameter / 2);

  if (note > 52) {
    scale = U8Mix(scale, 102, U8(note - 52) * 4);
    scale = U8Mix(scale, 102, U8(note - 52) * 4);
  }
  phase_increment <<= 1;

  uint24_t phase_tmp = phase;
  for (uint8_t samples_left = kAudioBlockSize; samples_left > 0; samples_left--) {
    if (*sync_input || *(sync_input + 1)) {
      phase_tmp = phase_increment;
    } else {
      phase_tmp += phase_increment;
    }
    sync_input += 2;
    // TODO sync output?
    
    uint8_t a = InterpolateTwoTables(wave_1, wave_2, highWord24(phase_tmp), gain_1, gain_2);
    uint8_t b = InterpolateTwoTables(wave_1, wave_2, highWord24(phase_tmp) + shift, gain_1, gain_2);
    a = U8U8MulShift8(a, scale);
    b = U8U8MulShift8(b, scale);
    a = a - b + 128;
    *buffer++ = a;
    *buffer++ = a;

    // second decrement
    samples_left--;
  }
    phase = phase_tmp;
}

// ------- Interpolation between two waveforms from two wavetables -----------
// The position is determined by the note pitch, to prevent aliasing.

void Oscillator::RenderSimpleWavetable(uint8_t* buffer) {
  uint8_t balance_index = U8Swap4(note);
  uint8_t gain_2 = highNibbleUnshifted(balance_index);
  uint8_t gain_1 = byteInverse(gain_2);
  uint8_t wave_1_index, wave_2_index;
  if (shape == WAVEFORM_SINE) {
    wave_1_index = WAV_RES_SINE;
    wave_2_index = WAV_RES_SINE;
  } else {
    uint8_t wave_index = lowNibble(balance_index);
    const uint8_t base_resource_id = WAV_RES_BANDLIMITED_SAW_0;
    wave_1_index = base_resource_id + wave_index;
    wave_2_index = base_resource_id + U8AddClip(wave_index, 1, kNumZonesFullSampleRate);
  }
  const uint8_t* wave_1 = waveform_table[wave_1_index];
  const uint8_t* wave_2 = waveform_table[wave_2_index];

  uint24_t phase_tmp = phase;
  bool *sync_input_tmp = sync_input;
  bool *sync_output_tmp = sync_output;
  for (uint8_t samples_left = kAudioBlockSize; samples_left > 0; samples_left--) {
    update_phase_and_sync(phase_tmp, phase_increment, sync_input_tmp, sync_output_tmp);
    uint8_t sample = InterpolateTwoTables(wave_1, wave_2, highWord24(phase_tmp), gain_1, gain_2);

    if (sample < parameter) {
      // The waveshaper for the triangle is different.
      sample = (shape == WAVEFORM_TRIANGLE) ? parameter : sample + parameter / 2;
    }
    *buffer++ = sample;
  }
  phase = phase_tmp;
}

// ------- Naive triangle with a fold ----------------------------------------
// KZ MOD: ported from the YAM voicecard, which is what Carey's v1.1 cards run.
// parameter folds the triangle back on itself, which is where its timbre comes
// from; at 0 it is a plain triangle.
void Oscillator::RenderNewTriangle(uint8_t* buffer) {
  uint24_t phase_tmp = phase;
  bool* sync_input_tmp = sync_input;
  bool* sync_output_tmp = sync_output;
  for (uint8_t samples_left = kAudioBlockSize; samples_left > 0; samples_left--) {
    update_phase_and_sync(phase_tmp, phase_increment, sync_input_tmp, sync_output_tmp);
    uint16_t integral = highWord24(phase_tmp);
    uint8_t ramp = U8(integral >> 7u);
    uint8_t value = byteAnd(highByte(integral), 0x80u) ? ramp : byteInverse(ramp);
    if (value < parameter) {
      value = U8(U8(parameter << 1u) - value);
    }
    *buffer++ = value;
  }
  phase = phase_tmp;
}

// ------- Casio CZ-like synthesis -------------------------------------------
void Oscillator::RenderCzSaw(uint8_t* buffer) {
  uint24_t phase_tmp = phase;
  bool *sync_input_tmp = sync_input;
  bool *sync_output_tmp = sync_output;
  for (uint8_t samples_left = kAudioBlockSize; samples_left > 0; samples_left--) {
    update_phase_and_sync(phase_tmp, phase_increment, sync_input_tmp, sync_output_tmp);
    uint8_t phase_byte = highByte24(phase_tmp);
    uint8_t clipped_phase = highByte(phase_tmp) >= 0x20 ? 0xff : highWord24(phase_tmp) >> 5u;
    // Interpolation causes more aliasing here.
    *buffer++ = ReadSample(wav_res_sine, U8MixU16(phase_byte, clipped_phase, parameter * 2));
  }
  phase = phase_tmp;
}

/*
 * Merges RenderCzResoTri, RenderCzResoPulse, RenderCzResoSaw
 */

void Oscillator::RenderCzResoWave(uint8_t* buffer) {
  using rs = ResourcesManager;
  const uint8_t cz_wave_type = shape - WAVEFORM_CZ_SAW_LP; // == 8 for ztri
  const uint8_t cz_wave_shape = cz_wave_type / 4; // == 0 for saw, 1, for pulse, 2 for tri
  const uint8_t isBPorHP = byteAnd(cz_wave_type, 2); // == (filter_type >= 2) in boolean expressions
  const uint16_t increment = highWord24(phase_increment) + ((highWord24(phase_increment) * U16(parameter)) / 4);
  uint16_t phase_2 = data.secondary_phase;

  uint24_t phase_tmp = phase;
  bool *sync_input_tmp = sync_input;
  bool *sync_output_tmp = sync_output;
  for (uint8_t samples_left = kAudioBlockSize; samples_left > 0; samples_left--) {
    bool phase_reset = update_phase_and_sync(phase_tmp, phase_increment, sync_input_tmp, sync_output_tmp);

    if (phase_reset) {
      // this computation depends on order of waves specified in patch.h
      // 0 corresponds to LP, 1 to PK, 2 to BP, 3 to HP
      const uint8_t filter_type = cz_wave_type % 4; // == byteAnd(cz_wave_type, 3); (optimiser should take care of this)
      phase_2 = rs::Lookup<uint16_t, uint8_t>(lut_res_cz_phase_reset, filter_type);
    }
    phase_2 += increment;
    uint8_t carrier = ReadSample(wav_res_sine, phase_2);
    uint8_t window;
    // TODO is having the switch statement inside the loop incredibad?
    switch (cz_wave_shape) {
      case 0: // saw
        window = byteInverse(highByte24(phase_tmp));
        break;
      case 1: // pulse
        window = 0;
        if (highByte24(phase_tmp) < 0x40) {
          window = 255;
        } else if (highByte24(phase_tmp) < 0x80) {
          window = U16(~(highWord24(phase_tmp) - 0x4000u)) >> 6u;
        }
        if (cz_wave_type == 5) { // WAVEFORM_CZ_PLS_PK
          carrier = carrier / 2 + 128;
        }
        break;
      case 2: // tri
        window = highWord24(phase_tmp) >> 7u;
        if (byteAnd(highByte24(phase_tmp), 0x80)) {
          window = byteInverse(window);
        }
        break;
    }
    if (isBPorHP) {
      *buffer++ = S8U8MulShift8(carrier + 128, window) + 128;
    } else {
      *buffer++ = U8U8MulShift8(carrier, window);
    }
  }
  phase = phase_tmp;
  data.secondary_phase = phase_2;
}

// ------- FM ----------------------------------------------------------------
void Oscillator::RenderFm(uint8_t* buffer) {
  // FM table currently has 64 entries. fn_parameter goes from 0 to 72 = 2*36
  const uint8_t fm_type = byteAnd(fm_parameter, 64 - 1); // == mod 64
  const auto multiplier = ResourcesManager::Lookup<uint16_t, uint8_t>(lut_res_fm_frequency_ratios, fm_type);
  const uint24_t modulator_phase_inc = phase_increment * multiplier >> 8u;


  const uint8_t depth_parameter = parameter * 2;

  uint24_t phase_tmp = phase;
  uint24_t modulator_phase = data.secondary_phase;


  for (uint8_t samples_left = kAudioBlockSize; samples_left > 0; samples_left--) {
    update_phase_and_sync(phase_tmp, phase_increment, sync_input, sync_output);
    modulator_phase += modulator_phase_inc;

    int8_t modulator = InterpolateSample(wav_res_sine, highWord24(modulator_phase)) - 128;
    uint16_t modulation = S8U8Mul(modulator, depth_parameter);
    // sin(phi(n) + m), where m = sin(phi_2(n)) is the modulation
    *buffer++ = InterpolateSample(wav_res_sine, highWord24(phase_tmp) + modulation);
  }
  phase = phase_tmp;
  data.secondary_phase = modulator_phase;
}

// ------- 8-bit land --------------------------------------------------------
void Oscillator::Render8BitLand(uint8_t* buffer) {
  const uint8_t x = parameter;
  uint24_t phase_tmp = phase;
  for (uint8_t samples_left = kAudioBlockSize; samples_left > 0; samples_left--) {
    update_phase_and_sync(phase_tmp, phase_increment, sync_input, sync_output);
    uint8_t basic_saw_sample = highByte24(phase_tmp);
    // the offset by x >> 1 does nothing
    //*buffer++ = byteAnd(basic_saw_sample ^ (x << 1), ~(x)) + (x >> 1);
    *buffer++ = byteAnd(basic_saw_sample ^ U8(x << 1), ~x);
  }
  phase = phase_tmp;
}

void Oscillator::RenderVowel(uint8_t* buffer) {
  using rs = ResourcesManager;
  data.vw.update = byteAnd(data.vw.update + 1, 0x3); // reset to zero every 4th call
  if (data.vw.update != 0) {
    uint8_t offset_1 = highNibble(parameter) * 7;
    uint8_t balance = lowNibble(parameter);
    uint8_t offset_2 = offset_1 + 7; // highNibble(parameter) * 8

    // Interpolate formant frequencies.
    for (uint8_t i = 0; i < 3; ++i) {
      auto freq_a = rs::Lookup<uint8_t, uint8_t>(wav_res_vowel_data, offset_1 + i);
      auto freq_b = rs::Lookup<uint8_t, uint8_t>(wav_res_vowel_data, offset_2 + i);
      data.vw.formant_increment[i] = U8U4MixU12(freq_a, freq_b, balance) << 3u;
    }
    
    // Interpolate formant amplitudes.
    // formant_amplitude[3] is noise_modulation
    for (uint8_t i = 0; i < 4; ++i) {
      const auto index_a = offset_1 + 3 + i;
      const auto index_b = offset_2 + 3 + i;
      auto amplitude_a = rs::Lookup<uint8_t, uint8_t>(wav_res_vowel_data, index_a);
      auto amplitude_b = rs::Lookup<uint8_t, uint8_t>(wav_res_vowel_data, index_b);
      data.vw.formant_amplitude[i] = U8U4MixU8(amplitude_a, amplitude_b, balance);
    }
  }
  const auto noise_modulation = data.vw.formant_amplitude[3];
  const int16_t phase_noise = S8S8Mul(Random::state_msb(), noise_modulation);

  constexpr const uint8_t* formant_wave[] = {wav_res_formant_sine, wav_res_formant_sine, wav_res_formant_square};

  uint24_t phase_tmp = phase;
  for (uint8_t samples_left = kAudioBlockSize; samples_left > 0; samples_left--) {
    int8_t result = 0;
    uint8_t phaselet;

    for (uint8_t i = 0; i < 3; i++) {
      data.vw.formant_phase[i] += data.vw.formant_increment[i];
      phaselet = highNibbleUnshifted(highByte(data.vw.formant_phase[i]));
      result += rs::Lookup<uint8_t, uint8_t>(formant_wave[i], phaselet | data.vw.formant_amplitude[i]);
    }

    result = S8U8MulShift8(result, highByte24(phase_tmp));
    // counts downwards
    phase_tmp -= phase_increment;
    if (phase_tmp + (static_cast<int24_t>(phase_noise) << 8u) < phase_increment) {
      data.vw.formant_phase[0] = 0;
      data.vw.formant_phase[1] = 0;
      data.vw.formant_phase[2] = 0;
    }
    // (-32, 32) -> (0, 255)
    //uint8_t x = S16ClipS8(4 * result) + 128;
    uint8_t x;
    if (result <= -32) {
      x = 0;
    } else if (result >= 32) {
      x = 255;
    } else {
      x = U8(result + 32) * 4;
    }
    *buffer++ = x;
    *buffer++ = x;
    samples_left--; // second decrement
  }
  phase = phase_tmp;
}

// ------- Dirty Pwm (kills kittens) -----------------------------------------
void Oscillator::RenderDirtyPwm(uint8_t* buffer) {
  const uint8_t flip_point = 127u + parameter;
  uint24_t phase_tmp = phase;
  for (uint8_t samples_left = kAudioBlockSize; samples_left > 0; samples_left--) {
    update_phase_and_sync(phase_tmp, phase_increment, sync_input, sync_output);
    *buffer++ = highByte24(phase_tmp) < flip_point ? 0 : 255;
  }
  phase = phase_tmp;
}

// ------- Quad saw (mit aliasing) -------------------------------------------
void Oscillator::RenderQuadSawPad(uint8_t* buffer) {
  uint16_t phase_increment_tmp = highWord24(phase_increment);
  uint16_t phase_spread = U32(phase_increment_tmp * U16(parameter)) >> 13u;
  ++phase_spread;
  uint16_t increments[3];
  for (uint8_t i = 0; i < 3; ++i) {
    phase_increment_tmp += phase_spread;
    increments[i] = phase_increment_tmp;
  }

  uint24_t phase_tmp = phase;
  // KZ MOD: WAVEFORM_QUAD_PWM shares this renderer, as it does on the YAM
  // voicecard. Four detuned pulses rather than four detuned saws.
  if (shape == WAVEFORM_QUAD_PWM) {
    uint8_t pwm_phase = U8(127 + parameter);
    for (uint8_t samples_left = kAudioBlockSize; samples_left > 0; samples_left--) {
      update_phase_and_sync(phase_tmp, phase_increment, sync_input, sync_output);
      data.qs.phase[0] += increments[0];
      data.qs.phase[1] += increments[1];
      data.qs.phase[2] += increments[2];
      uint8_t value = highWord24(phase_tmp) < U16(pwm_phase << 8u) ? 0 : 63;
      if (highByte(data.qs.phase[0]) >= pwm_phase) value += 63;
      if (highByte(data.qs.phase[1]) >= pwm_phase) value += 63;
      if (highByte(data.qs.phase[2]) >= pwm_phase) value += 63;
      *buffer++ = value;
    }
    phase = phase_tmp;
    return;
  }
  for (uint8_t samples_left = kAudioBlockSize; samples_left > 0; samples_left--) {
    update_phase_and_sync(phase_tmp, phase_increment, sync_input, sync_output);

    data.qs.phase[0] += increments[0];
    data.qs.phase[1] += increments[1];
    data.qs.phase[2] += increments[2];
    uint8_t value = (highWord24(phase_tmp) >> 10u);
    value += (data.qs.phase[0] >> 10u);
    value += (data.qs.phase[1] >> 10u);
    value += (data.qs.phase[2] >> 10u);
    *buffer++ = value;
  }
  phase = phase_tmp;
}

// ------- Low-passed, then high-passed white noise --------------------------
void Oscillator::RenderFilteredNoise(uint8_t* buffer) {
  uint16_t rng_state = data.no.rng_state;
  if (rng_state == 0) {
    ++rng_state;
  }
  uint8_t filter_coefficient = parameter * 4 ;
  if (filter_coefficient <= 4) {
    filter_coefficient = 4;
  }
  uint24_t phase_tmp = phase;
  for (uint8_t samples_left = kAudioBlockSize; samples_left > 0; samples_left--) {
    if (*sync_input++) {
      rng_state = data.no.rng_reset_value;
    }
    rng_state = U16(rng_state >> 1u) ^ (-(rng_state & 1u) & 0xb400u);
    uint8_t noise_sample = highByte(rng_state);
    // This trick is used to avoid having a DC component (no innovation) when
    // the parameter is set to its minimal or maximal value.
    data.no.lp_noise_sample = U8Mix(data.no.lp_noise_sample, noise_sample, filter_coefficient);
    if (parameter >= 64) {
      *buffer++ = data.no.lp_noise_sample + 127 - noise_sample;
    } else {
      *buffer++ = data.no.lp_noise_sample + (parameter * 2); // slowly scale up to the -128 offset
    }
  }
  phase = phase_tmp;
  data.no.rng_state = rng_state;
}

// KZ MOD: the wavetable renderers are built only when DISABLE_WAVETABLES is
// undefined. With it set -- the default -- their shapes are remapped to
// WAVEFORM_POLYBLEP_SAW in Oscillator::Render() and the 10,608-byte wave bank
// is left out of the image entirely.
#ifndef DISABLE_WAVETABLES
// The position is freely determined by the parameter
void Oscillator::RenderInterpolatedWavetable(uint8_t* buffer) {
  const uint8_t* which_wavetable = wav_res_wavetables + U8(shape - WAVEFORM_WAVETABLE_1) * U8(18);

  // Get a 8:8 value with the wave index in the first byte, and the
  // balance amount in the second byte.
  auto num_steps = ResourcesManager::Lookup<uint8_t, uint8_t>(which_wavetable, 0);
  uint16_t pointer = U8(parameter * 2) * num_steps;
  auto wave_index_1 = ResourcesManager::Lookup<uint8_t, uint8_t>(which_wavetable, 1 + highByte(pointer));
  auto wave_index_2 = ResourcesManager::Lookup<uint8_t, uint8_t>(which_wavetable, 2 + highByte(pointer));
  uint8_t gain = lowByte(pointer);
  const uint8_t* wave_1 = wav_res_waves + U8U8Mul(wave_index_1, 129);
  const uint8_t* wave_2 = wav_res_waves + U8U8Mul(wave_index_2, 129);

  uint24_t phase_tmp = phase;
  bool *sync_input_tmp = sync_input;
  bool *sync_output_tmp = sync_output;
  for (uint8_t samples_left = kAudioBlockSize; samples_left > 0; samples_left--) {
    update_phase_and_sync(phase_tmp, phase_increment, sync_input_tmp, sync_output_tmp);
    *buffer++ = InterpolateTwoTables(wave_1, wave_2, highWord24(phase_tmp) / 2, ~gain, gain);
  }
  phase = phase_tmp;
}

// The position is freely determined by the parameter
void Oscillator::RenderWavequence(uint8_t* buffer) {
  const uint8_t* wave = wav_res_waves + U8U8Mul(parameter, 129);

  uint24_t phase_tmp = phase;
  for (uint8_t samples_left = kAudioBlockSize; samples_left > 0; samples_left--) {
    update_phase_and_sync(phase_tmp, phase_increment, sync_input, sync_output);
    *buffer++ = InterpolateSample(wave, highWord24(phase_tmp) / 2);
  }
  phase = phase_tmp;
}
#endif  // DISABLE_WAVETABLES


#define CALCULATE_DIVISION_FACTOR(divisor, result_quotient, result_quotient_shifts) \
  uint16_t div_table_index = divisor; \
  int8_t result_quotient_shifts = 0; \
  while (div_table_index > 255) { \
    div_table_index >>= 1; \
    --result_quotient_shifts; \
  } \
  while (div_table_index < 128) { \
    div_table_index <<= 1; \
    ++result_quotient_shifts; \
  } \
  div_table_index -= 128; \
  uint8_t result_quotient = ResourcesManager::Lookup<uint8_t, uint8_t>(wav_res_division_table, div_table_index);

// function version of CALCULATE_BLEP_INDEX from github.com/bjoeri/ambika/voicecard/oscillator.cc
uint8_t calculate_blep_index(uint16_t increment, uint8_t quotient, int8_t quotient_shifts) {
  uint16_t blep_index = increment;
  if (quotient_shifts < 0) {
    blep_index >>= U8(-quotient_shifts);
  } else {
    blep_index <<= U8(quotient_shifts);
  }
  // approximate division by multiplication with inverse
  blep_index = U16U8MulShift8(blep_index, quotient);
  return highByte(blep_index);
}

#if 0

  /* ------- Polyblep Saw ------------------------------------------------------
 * Implementation adapted from https://github.com/bjoeri/ambika
 * His description:
 * Heavily inspired by Oliviers experimental implementation for STM but
 * dumbed down and much less generic (does not do polyblep for sync etc)
 */
void Oscillator::RenderPolyBlepSaw(uint8_t* buffer) {
  using rs = ResourcesManager;

  // calculate (1/increment) for later multiplication with current phase
  CALCULATE_DIVISION_FACTOR(highWord24(phase_increment), quotient, quotient_shifts)

  // Revert to pure saw (=single blep) to avoid cpu overload for high notes
  uint8_t mod_parameter = note > 107 ? 0 : parameter;
  uint8_t phase_exceeds = highByte24(phase) >= 0x80;

  uint8_t next_sample = data.output_sample;
  uint24_t phase_tmp = phase;
  for (uint8_t samples_left = kAudioBlockSize; samples_left > 0; samples_left--) {
    bool phase_reset = update_phase_and_sync(phase_tmp, phase_increment, sync_input, sync_output);

    uint8_t this_sample = next_sample;

    // Compute naive waveform
    next_sample = highByte24(phase_tmp);

    if (next_sample >= 0x80) {
      next_sample -= mod_parameter;
    }

    uint16_t blep_index;
    uint8_t mix_parameter;

    if (phase_reset) {
      phase_exceeds = false;
      blep_index = calculate_blep_index(highWord24(phase_tmp), quotient, quotient_shifts);
      mix_parameter = 255 - mod_parameter;

    } else if (mod_parameter && !phase_exceeds && highWord24(phase_tmp) >= 0x8000) {
      phase_exceeds = true;
      blep_index = calculate_blep_index(highWord24(phase_tmp) - 0x8000, quotient, quotient_shifts);
      mix_parameter = mod_parameter;
    } else {
      // don't update this_sample and next_sample
      *buffer++ = this_sample;
      continue;
    }

    auto blep_lookup = rs::Lookup<uint8_t, uint8_t>(wav_res_square_table, blep_index);
    this_sample -= U8U8MulShift8(blep_lookup, mix_parameter); // scale blep to size of edge

    auto blep_lookup_inv = rs::Lookup<uint8_t, uint8_t>(wav_res_square_table, 127 - blep_index);
    next_sample += U8U8MulShift8(blep_lookup_inv, mix_parameter); // scale blep to size of edge

    *buffer++ = this_sample;
  }
  phase = phase_tmp;

  data.output_sample = next_sample;
}

/* ------- Polyblep Pwm ------------------------------------------------------
 * Implementation adapted from https://github.com/bjoeri/ambika
 * His description:
 * Heavily inspired by Oliviers experimental implementation for STM but
 * dumbed down and much less generic (does not do polyblep for sync etc)
 */
void Oscillator::RenderPolyBlepPwm(uint8_t* buffer) {
  using rs = ResourcesManager;

  // calculate (1/increment) for later multiplication with current phase
  CALCULATE_DIVISION_FACTOR(highWord24(phase_increment), quotient, quotient_shifts)

  // Revert to pure saw (=single blep) to avoid cpu overload for high notes
  bool revert_to_saw = note > 107;

  // PWM modulation (constrained to extend over at least one increment)
  uint8_t pwm_limit = 127 - highByte24(phase_increment);
  // prevent dual bleps at same increment

  uint8_t pwm_phase_offset = (parameter < pwm_limit) ? parameter : pwm_limit; // == min(parameter, pwm_limit)
  uint16_t pwm_phase = word(127 + pwm_phase_offset, 0);
  bool phase_exceeds = highWord24(phase) >= pwm_phase;
  uint8_t next_sample = data.output_sample;

  uint24_t phase_tmp = phase;
  for (uint8_t samples_left = kAudioBlockSize; samples_left > 0; samples_left--) {
    bool phase_reset = update_phase_and_sync(phase_tmp, phase_increment, sync_input, sync_output);
    uint8_t this_sample = next_sample;

    // Compute naive waveform
    if (revert_to_saw) {
      next_sample = highByte(phase_tmp);
    } else if (highWord24(phase_tmp) < pwm_phase) {
      next_sample = 0;
    } else {
      next_sample = 255;
    }

    if (phase_reset) {
      phase_exceeds = false;
      uint8_t blep_index = calculate_blep_index(highWord24(phase_tmp), quotient, quotient_shifts);

      this_sample -= rs::Lookup<uint8_t, uint8_t>(wav_res_square_table, blep_index);
      next_sample += rs::Lookup<uint8_t, uint8_t>(wav_res_square_table, 127 - blep_index);

    } else if (!revert_to_saw && /* no positive edge for pure saw */
             highWord24(phase_tmp) >= pwm_phase && !phase_exceeds) {
      phase_exceeds = true;
      uint8_t blep_index = calculate_blep_index(highWord24(phase_tmp) - pwm_phase, quotient, quotient_shifts);

      this_sample += rs::Lookup<uint8_t, uint8_t>(wav_res_square_table, blep_index);
      next_sample -= rs::Lookup<uint8_t, uint8_t>(wav_res_square_table, 127 - blep_index);
    }

    *buffer++ = this_sample;
  }
  phase = phase_tmp;

  data.output_sample = next_sample;
}

/* ------- Polyblep CS-80 Saw ------------------------------------------------
 * Implementation adapted from https://github.com/bjoeri/ambika
 * His descriptions:
 * Heavily inspired by Emilies experimental implementation for STM but
 * dumbed down and much less generic (does not do polyblep for sync etc)
 */
void Oscillator::RenderPolyBlepCSaw(uint8_t* buffer) {
  using rs = ResourcesManager;

  // calculate (1/increment) for later multiplication with current phase
  CALCULATE_DIVISION_FACTOR(highWord24(phase_increment), quotient, quotient_shifts)

  // Revert to pure saw (=single blep) to avoid cpu overload for high notes
  bool revert_to_saw = note > 107;

  // PWM modulation (constrained to extend over at least one increment)
  uint8_t pwm_limit = highByte24(phase_increment);

  uint8_t pwm_phase_offset = (parameter > 0 && parameter < pwm_limit) ? pwm_limit : parameter;
  uint16_t pwm_phase = word(pwm_phase_offset, 0);

  bool phase_exceeds = highWord24(phase) >= pwm_phase;

  uint8_t next_sample = data.output_sample;
  uint24_t phase_tmp = phase;
  for (uint8_t samples_left = kAudioBlockSize; samples_left > 0; samples_left--) {
    bool phase_reset = update_phase_and_sync(phase_tmp, phase_increment, sync_input, sync_output);
    uint8_t this_sample = next_sample;

    // Compute naive waveform
    next_sample = (revert_to_saw || highWord24(phase_tmp) >= pwm_phase) ? highByte24(phase_tmp) : 0;

    if (phase_reset) {
      phase_exceeds = false;
      uint16_t blep_index = calculate_blep_index(highWord24(phase_tmp), quotient, quotient_shifts);
      this_sample -= rs::Lookup<uint8_t, uint8_t>(wav_res_square_table, blep_index);
      next_sample += rs::Lookup<uint8_t, uint8_t>(wav_res_square_table, 127-blep_index);
    }
    else if (!revert_to_saw && /* no positive edge for pure saw */
             highWord24(phase_tmp) >= pwm_phase && !phase_exceeds) {
      phase_exceeds = true;
      uint16_t blep_index = calculate_blep_index(highWord24(phase_tmp) - pwm_phase, quotient, quotient_shifts);
      // scale bleps to size of edge
      this_sample += U8U8MulShift8(rs::Lookup<uint8_t, uint8_t>(wav_res_square_table, blep_index), parameter);
      next_sample -= U8U8MulShift8(rs::Lookup<uint8_t, uint8_t>(wav_res_square_table, 127-blep_index), parameter);
    }

    *buffer++ = this_sample;
  }
  phase = phase_tmp;

  data.output_sample = next_sample;
}

#endif

// Combined polyblep saw/pwm/csaw
/*
 * Polyblep is implemented with one sample of delay:
 * in order to know if there's a discontinuity between samples, we need to keep track
 * of two samples at once: 'this sample' and 'next' sample'.
 * Hence a 'last output sample' is needed to be stored as part of the oscillator state
 */
void Oscillator::RenderPolyBlepWave(uint8_t *buffer) {
  // calculate (1/increment) for later multiplication with current phase
  //CALCULATE_DIVISION_FACTOR(highWord24(phase_increment), quotient, quotient_shifts)

  // Revert to pure saw (=single blep) for high notes, to avoid cpu overload
  const bool use_simple_saw = (note > 107);

  // Where in the cycle (8 bit precision) does our wave have a step discontinuity?
  // NB for all waves, there is also a step discontinuity at zero,
  // i.e whenever the phase_tmp overflows / phase_reset != 0
  uint8_t step_phase_byte;

  // setup parameters for each wave type
  switch (shape) {
    case WAVEFORM_POLYBLEP_SAW:
      step_phase_byte = 127;
      break;
    case WAVEFORM_POLYBLEP_PWM:
    {
      // For pwm: where to flip from low to high. 127 means (approx) 50% duty cycle

      // Removed:
      // First, limit the pwm range so each half of the cycle lasts at least one increment.
      // I'm assuming that highByte24(phase_increment) is never more than 127?
      //const uint8_t max_pwm_deviation = 127 - highByte24(phase_increment);

      // PWM point is calculated so that parameter value of 0 corresponds to 50% duty cycle
      // Also, this prevents dual bleps at same increment... somehow
      //const uint8_t pwm_step_phase = 127 + ((parameter < max_pwm_deviation) ? parameter : max_pwm_deviation);
      // == 127 + min(parameter, pwm_limit)
      step_phase_byte = 127 + parameter;
    }
    break;
    case WAVEFORM_POLYBLEP_CSAW:
    {
      // The PWM point for Csaw works a bit differently: the parameter value is directly tied to the flip point

      // Removed:
      // the actual duty cycle is constrained to be in the interval [highByte24(phase_increment)/255, 127/255]
      // This also ensures that the PWM modulation does not get smaller than one increment
      //const uint8_t csaw_min_pwm_phase = highByte24(phase_increment);
      //const uint8_t csaw_step_phase = (parameter < csaw_min_pwm_phase) ? csaw_min_pwm_phase : parameter;
      // == max(parameter, pwm_limit)

      step_phase_byte = parameter;
      break;
    }
    default:
      step_phase_byte = 0;
      break;
  }

  uint8_t next_sample = data.output_sample;

  uint24_t phase_tmp = phase;
  for (uint8_t samples_left = kAudioBlockSize; samples_left > 0; samples_left--) {
    update_phase_and_sync(phase_tmp, phase_increment, sync_input, sync_output);

    // move one sample forward ('the future is now')
    uint8_t this_sample = next_sample;

    uint16_t current_phase_byte = highByte24(phase_tmp);
    // small optimisation: 8 bit compare. See note at top of file
    bool past_step_point = current_phase_byte >= step_phase_byte;

    // Naive waveform for next sample
    if (!use_simple_saw) {
      switch (shape) {
        case WAVEFORM_POLYBLEP_SAW:
          // Shift the upper half cycle of the sawtooth wave downwards by the parameter value
          next_sample = (past_step_point) ? current_phase_byte - parameter : current_phase_byte;
          break;
        case WAVEFORM_POLYBLEP_PWM:
          next_sample = (past_step_point) ? 255 : 0;
          break;
        case WAVEFORM_POLYBLEP_CSAW:
          next_sample = (past_step_point) ? current_phase_byte : 0;
          break;
        default:
          next_sample = 0;
          break;
      }
    } else {
      next_sample = current_phase_byte; // nothing else to do
    }
    // Every time the phase resets, all waveforms have a negative step
    // discontinuity.

    // Provided use_simple_saw == false, all waveforms have
    // a step discontinuity the first time that the phase exceeds step_phase,
    // aka when past_step_point == true but already_past_step_point == false.
    // For the basic saw, this is a negative step, for the other two waves it's positive.

    /* Don't blep for now -  just alias!

    uint16_t blep_index;

    // if phase has just reset, current_phase should be small
    if (phase_reset) {
      already_past_step_point = false;
      // phase_offset_from_step = current_phase;
      blep_index = calculate_blep_index(current_phase - 0, quotient, quotient_shifts);
    }
    // else if we've just passed the step point, should have current_phase just over step_phase
    else if (!use_simple_saw && just_reached_step_point) {
      already_past_step_point = true;
      // at this point, we must have current_phase >= step_phase.
      // phase_offset_from_step = current_phase - step_phase;
      blep_index = calculate_blep_index(current_phase - step_phase, quotient, quotient_shifts);
    }
    // else no bleps are needed
    else {
      *buffer++ = this_sample;
      continue;
    }

    // all uint8_t
    auto blep_residual_this = rs::Lookup<uint8_t, uint8_t>(wav_res_square_table, blep_index);
    auto blep_residual_next = rs::Lookup<uint8_t, uint8_t>(wav_res_square_table, 127 - blep_index);

    // Essentially these are the operations, however we have to deal with signedness and bit width:
    //this_sample -= blep_residual_this * (this_sample - next_sample);
    //next_sample += blep_residual_next * (this_sample - next_sample);

    bool step_is_negative = next_sample < this_sample;
    if (step_is_negative) {
      uint8_t step_size = this_sample - next_sample;
      // Scale bleps to size of edge:
      this_sample -= highByte(blep_residual_this * step_size);
      next_sample += highByte(blep_residual_next * step_size);
    } else {
      uint8_t step_size = next_sample - this_sample;
      // Scale bleps to size of edge:
      this_sample += highByte(blep_residual_this * step_size);
      next_sample -= highByte(blep_residual_next * step_size);
    }
    */

    *buffer++ = this_sample;
  }
  phase = phase_tmp;
  data.output_sample = next_sample;
}

// can check phase_reset externally:
// if phase has overflowed, it has to be end up being <= phase_increment after the previous line.
// bool phase_reset = phase <= phase_increment;

inline bool Oscillator::update_phase_and_sync(uint24_t& phase, const uint24_t& phase_increment, bool*& sync_in, bool*& sync_out) {
  bool phase_reset;
  if (*sync_in++) {
    // phase = 0; phase += phase_increment;
    phase = phase_increment;
    phase_reset = true;
  } else {
    phase += phase_increment;
    phase_reset = phase <= phase_increment;
  }
  *sync_out++ = phase_reset;
  return phase_reset;
}

}  // namespace
