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
// Oscillators. Note that the code of each oscillator is duplicated/specialized,
// for a noticeable performance boost.

#ifndef VOICECARD_OSCILLATOR_H_
#define VOICECARD_OSCILLATOR_H_

#include "avrlib/op.h"
#include "avrlib/random.h"

#include "common/patch.h"

#include "voicecard/resources.h"

using namespace avrlib;

namespace ambika {

//__attribute__((always_inline))
static inline uint8_t ReadSample(const uint8_t* table, uint16_t phase) {
  return ResourcesManager::Lookup<uint8_t, uint8_t>(table, highByte(phase));
}

static inline uint8_t InterpolateTwoTables(const uint8_t* table_a, const uint8_t* table_b,
      uint16_t phase, uint8_t gain_a, uint8_t gain_b) {
  auto a = InterpolateSample(table_a, phase);
  auto b = InterpolateSample(table_b, phase);
  return U8Mix(a, b, gain_a, gain_b);
}

static const uint8_t kNumZonesFullSampleRate = 6;
static const uint8_t kNumZonesHalfSampleRate = 5;

struct VowelSynthesizerState {
  uint16_t formant_increment[3];
  uint16_t formant_phase[3];
  uint8_t formant_amplitude[4]; // last entry represents noise_modulation
  //uint8_t noise_modulation;
  uint8_t update;  // Update only every kVowelControlRateDecimation-th call.
};

struct FilteredNoiseState {
  uint8_t lp_noise_sample;
  uint16_t rng_state;
  uint16_t rng_reset_value;
};

struct QuadSawPadState {
  uint16_t phase[3];
};

union OscillatorState {
  VowelSynthesizerState vw;
  FilteredNoiseState no;
  QuadSawPadState qs;
  // for FM synthesis
  uint24_t secondary_phase;
  // used in polyblep algorithms
  uint8_t output_sample;
};

class Oscillator {
 public:
  using RenderFn = void (Oscillator::*)(uint8_t*);
   
  Oscillator() = default;

  inline void Reset() {
    data.no.rng_reset_value = Random::GetByte() + 1;
  }

  inline void Render(OscillatorAlgorithm new_shape, uint8_t new_note, uint24_t new_phase_increment,
                     bool* new_sync_input, bool* new_sync_output, uint8_t* buffer) {
    shape = new_shape;
    note = new_note;
    phase_increment = new_phase_increment;
    sync_input = new_sync_input;
    sync_output = new_sync_output;
    // KZ MOD: fn_table is indexed by OscillatorAlgorithm. The 16 wavetable
    // shapes and WAVEQUENCE share one slot, so every shape above them has to be
    // shifted down by that block's width. Without this the table was being read
    // with MachFour's original-enum indices while common/patch.h carries YAM's,
    // which rendered saw for POLYBLEP_PWM and TRIANGLE and silence for
    // everything from WAVETABLE_1 upwards.
    uint8_t index;
    if (new_shape >= WAVEFORM_WAVETABLE_1) {
      index = new_shape <= WAVEFORM_WAVEQUENCE
          ? U8(WAVEFORM_WAVETABLE_1)
          : U8(new_shape - WAVEFORM_WAVEQUENCE + WAVEFORM_WAVETABLE_1);
    } else {
      index = U8(new_shape);
    }
    // Patch files written by other firmware do contain out-of-range shape
    // bytes; a real SD card surveyed here held 59, 96 and 229. Render silence
    // rather than reading past the table.
    if (index >= kNumRenderFns) {
      index = U8(WAVEFORM_NONE);
    }
    RenderFn fn;
    ResourcesManager::Load(fn_table, index, &fn);
    if (new_shape == WAVEFORM_WAVEQUENCE) {
      fn = &Oscillator::RenderWavequence;
    }
    (this->*fn)(buffer);
  }
  
  inline void set_parameter(uint8_t new_parameter) {
    parameter = new_parameter;
  }
  
  inline void set_fm_parameter(uint8_t new_fm_parameter) {
    fm_parameter = new_fm_parameter;
  }
  
 private:
  // Current phase of the oscillator.
  uint24_t phase;

  // Phase increment (and phase increment x 2, for low-sr oscillators).
  uint24_t phase_increment;

  // Copy of the shape used by this oscillator. When changing this, you
  // should also update the Update/Render pointers.
  OscillatorAlgorithm shape;

  // Current value of the oscillator parameter.
  uint8_t parameter;
  uint8_t fm_parameter;
  uint8_t note;

  // Union of state data used by each algorithm.
  OscillatorState data;
  // A flag set to true when sync is enabled ; and a table to record the
  // position of phrase wraps
  bool* sync_input;
  bool* sync_output;

  inline static bool update_phase_and_sync(uint24_t& phase, const uint24_t& phase_increment, bool*& sync_in, bool*& sync_out);

  void RenderSilence(uint8_t* buffer);
  void RenderBandlimitedPwm(uint8_t* buffer);
  void RenderSimpleWavetable(uint8_t* buffer);
  void RenderCzSaw(uint8_t* buffer);
  //void RenderCzResoSaw(uint8_t* buffer);
  //void RenderCzResoPulse(uint8_t* buffer);
  //void RenderCzResoTri(uint8_t* buffer);
  void RenderCzResoWave(uint8_t* buffer);
  void RenderFm(uint8_t* buffer);
  void Render8BitLand(uint8_t* buffer);
  void RenderVowel(uint8_t* buffer);
  void RenderDirtyPwm(uint8_t* buffer);
  void RenderQuadSawPad(uint8_t* buffer);
  void RenderFilteredNoise(uint8_t* buffer);
  void RenderInterpolatedWavetable(uint8_t* buffer);
  void RenderWavequence(uint8_t* buffer);
  // polyblep synthesis methods by Bjarne (bjoeri on github)
  //void RenderPolyBlepSaw(uint8_t* buffer);
  //void RenderPolyBlepPwm(uint8_t* buffer);
  //void RenderPolyBlepCSaw(uint8_t* buffer);
  // combines previous three functions
  void RenderPolyBlepWave(uint8_t* buffer);
  void RenderNewTriangle(uint8_t* buffer);

  // Pointer to the render function. Indexed by OscillatorAlgorithm, with the
  // wavetable block collapsed to a single slot -- see Render() above. This
  // ordering must match common/patch.h; the static_assert below pins the
  // boundaries that have actually drifted before.
  static constexpr RenderFn fn_table[] PROGMEM {
      &Oscillator::RenderSilence,             // WAVEFORM_NONE

      &Oscillator::RenderPolyBlepWave,        // POLYBLEP_SAW
      &Oscillator::RenderPolyBlepWave,        // POLYBLEP_PWM
      &Oscillator::RenderNewTriangle,         // TRIANGLE
      &Oscillator::RenderSimpleWavetable,     // SINE

      &Oscillator::RenderCzSaw,               // CZ_SAW
      &Oscillator::RenderCzResoWave,          // CZ_SAW_LP
      &Oscillator::RenderCzResoWave,          // CZ_SAW_PK
      &Oscillator::RenderCzResoWave,          // CZ_SAW_BP
      &Oscillator::RenderCzResoWave,          // CZ_SAW_HP
      &Oscillator::RenderCzResoWave,          // CZ_PLS_LP
      &Oscillator::RenderCzResoWave,          // CZ_PLS_PK
      &Oscillator::RenderCzResoWave,          // CZ_PLS_BP
      &Oscillator::RenderCzResoWave,          // CZ_PLS_HP
      &Oscillator::RenderCzResoWave,          // CZ_TRI_LP

      &Oscillator::RenderQuadSawPad,          // QUAD_SAW_PAD
      &Oscillator::RenderFm,                  // FM
      &Oscillator::Render8BitLand,            // 8BITLAND
      &Oscillator::RenderDirtyPwm,            // DIRTY_PWM
      &Oscillator::RenderFilteredNoise,       // FILTERED_NOISE
      &Oscillator::RenderVowel,               // VOWEL

      &Oscillator::RenderInterpolatedWavetable, // WAVETABLE_1..16 and WAVEQUENCE

      &Oscillator::RenderSimpleWavetable,     // OLD_SAW
      &Oscillator::RenderQuadSawPad,          // QUAD_PWM
      &Oscillator::RenderFm,                  // FM_FB
      &Oscillator::RenderPolyBlepWave,        // POLYBLEP_CSAW
      &Oscillator::RenderVowel,               // VOWEL_2
  };

  static constexpr uint8_t kNumRenderFns = sizeof(fn_table) / sizeof(fn_table[0]);

  static_assert(kNumRenderFns ==
                    WAVEFORM_LAST - (WAVEFORM_WAVEQUENCE - WAVEFORM_WAVETABLE_1),
                "fn_table must cover every shape, with the wavetable block "
                "collapsed to one slot");
  static_assert(WAVEFORM_POLYBLEP_SAW == 1 && WAVEFORM_POLYBLEP_PWM == 2 &&
                    WAVEFORM_TRIANGLE == 3 && WAVEFORM_SINE == 4,
                "fn_table's low entries assume this OscillatorAlgorithm order");


  DISALLOW_COPY_AND_ASSIGN(Oscillator);
};

}  // namespace ambika

#endif  // VOICECARD_OSCILLATOR_H_
