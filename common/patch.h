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
// Patch data structure.

#ifndef COMMON_PATCH_H_
#define COMMON_PATCH_H_

#include "avrlib/base.h"

namespace ambika {



enum LfoWave : uint8_t {
  // For oscillators.
  LFO_WAVEFORM_TRIANGLE,
  LFO_WAVEFORM_SQUARE,
  LFO_WAVEFORM_S_H,
  LFO_WAVEFORM_RAMP,
  LFO_WAVEFORM_WAVE_1,
  LFO_WAVEFORM_WAVE_2,
  LFO_WAVEFORM_WAVE_3,
  LFO_WAVEFORM_WAVE_4,
  LFO_WAVEFORM_WAVE_5,
  LFO_WAVEFORM_WAVE_6,
  LFO_WAVEFORM_WAVE_7,
  LFO_WAVEFORM_WAVE_8,
  LFO_WAVEFORM_WAVE_9,
  LFO_WAVEFORM_WAVE_10,
  LFO_WAVEFORM_WAVE_11,
  LFO_WAVEFORM_WAVE_12,
  LFO_WAVEFORM_WAVE_13,
  LFO_WAVEFORM_WAVE_14,
  LFO_WAVEFORM_WAVE_15,
  LFO_WAVEFORM_WAVE_16,
  LFO_WAVEFORM_COUNT
};

// ordering of the LP/PK/BP/HP is important!!
// offsets of the wave shape from CZ_SAW_LP are used in rendering code
// e.g. in RenderCzResoTri

// ---- MACHFOUR VOICECARD ----
/*
enum OscillatorAlgorithm : uint8_t {
  WAVEFORM_NONE,
  WAVEFORM_SAW,
  WAVEFORM_SQUARE,
  WAVEFORM_TRIANGLE,
  WAVEFORM_SINE,
  WAVEFORM_CZ_SAW,
  WAVEFORM_CZ_SAW_LP, // these rendered by CzResoWave
  WAVEFORM_CZ_SAW_PK,
  WAVEFORM_CZ_SAW_BP,
  WAVEFORM_CZ_SAW_HP,
  WAVEFORM_CZ_PLS_LP,
  WAVEFORM_CZ_PLS_PK,
  WAVEFORM_CZ_PLS_BP,
  WAVEFORM_CZ_PLS_HP,
  WAVEFORM_CZ_TRI_LP,
  WAVEFORM_QUAD_SAW_PAD,
  WAVEFORM_FM,
  WAVEFORM_8BITLAND,
  WAVEFORM_DIRTY_PWM,
  WAVEFORM_FILTERED_NOISE,
  WAVEFORM_VOWEL,
  WAVEFORM_POLYBLEP_SAW, // these three are rendered by PolyblepWave
  WAVEFORM_POLYBLEP_PWM,
  WAVEFORM_POLYBLEP_CSAW,
  WAVEFORM_WAVETABLE_1,
  WAVEFORM_WAVETABLE_16 = WAVEFORM_WAVETABLE_1 - 1 + 16,
  WAVEFORM_WAVEQUENCE,
  WAVEFORM_LAST
};
*/

// ---- YAM VOICECARD ----
enum OscillatorAlgorithm : uint8_t {
  WAVEFORM_NONE,
  WAVEFORM_POLYBLEP_SAW,
  WAVEFORM_POLYBLEP_PWM,
  WAVEFORM_TRIANGLE,
  WAVEFORM_SINE,
  WAVEFORM_CZ_SAW,
  WAVEFORM_CZ_SAW_LP,
  WAVEFORM_CZ_SAW_PK,
  WAVEFORM_CZ_SAW_BP,
  WAVEFORM_CZ_SAW_HP,
  WAVEFORM_CZ_PLS_LP,
  WAVEFORM_CZ_PLS_PK,
  WAVEFORM_CZ_PLS_BP,
  WAVEFORM_CZ_PLS_HP,
  WAVEFORM_CZ_TRI_LP,
  WAVEFORM_QUAD_SAW_PAD,
  WAVEFORM_FM,
  WAVEFORM_8BITLAND,
  WAVEFORM_DIRTY_PWM,
  WAVEFORM_FILTERED_NOISE,
  WAVEFORM_VOWEL,
  WAVEFORM_WAVETABLE_1,
  WAVEFORM_WAVETABLE_16 = WAVEFORM_WAVETABLE_1 - 1 + 16,
  WAVEFORM_WAVEQUENCE,
  WAVEFORM_OLD_SAW,
  WAVEFORM_QUAD_PWM,
  WAVEFORM_FM_FB,
  WAVEFORM_POLYBLEP_CSAW,
  WAVEFORM_VOWEL_2,
  WAVEFORM_LAST
};

enum SubOscillatorAlgorithm : uint8_t {
  WAVEFORM_SUB_OSC_SQUARE_1,
  WAVEFORM_SUB_OSC_TRIANGLE_1,
  WAVEFORM_SUB_OSC_PULSE_1,
  WAVEFORM_SUB_OSC_SQUARE_2,
  WAVEFORM_SUB_OSC_TRIANGLE_2,
  WAVEFORM_SUB_OSC_PULSE_2,
  WAVEFORM_SUB_OSC_CLICK,
  WAVEFORM_SUB_OSC_GLITCH,
  WAVEFORM_SUB_OSC_BLOW,
  WAVEFORM_SUB_OSC_METALLIC,
  WAVEFORM_SUB_OSC_POP,
  WAVEFORM_SUB_OSC_LAST
};

enum Operator : uint8_t {
  OP_SUM,
  OP_SYNC,
  OP_RING_MOD,
  OP_XOR,
  OP_FOLD,
  OP_BITS,
  OP_LAST
};

enum ModifierOp : uint8_t {
  MODIFIER_NONE,
  MODIFIER_SUM,
  MODIFIER_PRODUCT,
  MODIFIER_ATTENUATE,
  MODIFIER_MAX,
  MODIFIER_MIN,
  MODIFIER_XOR,
  MODIFIER_GE,
  MODIFIER_LE,
  MODIFIER_QUANTIZE,
  MODIFIER_LAG_PROCESSOR,
  MODIFIER_COUNT
};

enum LfoSyncMode : uint8_t {
  LFO_SYNC_MODE_FREE,
  LFO_SYNC_MODE_SLAVE,
  LFO_SYNC_MODE_MASTER,
  LFO_SYNC_MODE_COUNT
};

enum ModSource : uint8_t {
  MOD_SRC_ENV_1,
  MOD_SRC_ENV_2,
  MOD_SRC_ENV_3,

  MOD_SRC_LFO_1,
  MOD_SRC_LFO_2,
  MOD_SRC_LFO_3,
  MOD_SRC_LFO_4,

  MOD_SRC_OP_1,
  MOD_SRC_OP_2,
  MOD_SRC_OP_3,
  MOD_SRC_OP_4,

  MOD_SRC_SEQ_1,
  MOD_SRC_SEQ_2,
  MOD_SRC_ARP_STEP,

  MOD_SRC_VELOCITY,
  MOD_SRC_AFTERTOUCH,
  MOD_SRC_PITCH_BEND,
  MOD_SRC_WHEEL,
  MOD_SRC_WHEEL_2,
  MOD_SRC_EXPRESSION,

  MOD_SRC_NOTE,
  MOD_SRC_GATE,
  MOD_SRC_NOISE,
  MOD_SRC_RANDOM,
  MOD_SRC_CONSTANT_256,
  MOD_SRC_CONSTANT_128,
  MOD_SRC_CONSTANT_64,
  MOD_SRC_CONSTANT_32,
  MOD_SRC_CONSTANT_16,
  MOD_SRC_CONSTANT_8,
  MOD_SRC_CONSTANT_4,
  MOD_SRC_COUNT
};

enum ModDestination : uint8_t {
  MOD_DST_PARAMETER_1,
  MOD_DST_PARAMETER_2,
  MOD_DST_OSC_1,
  MOD_DST_OSC_2,
  MOD_DST_OSC_1_2_COARSE,
  MOD_DST_OSC_1_2_FINE,

  MOD_DST_MIX_BALANCE,
  MOD_DST_MIX_PARAM,
  MOD_DST_MIX_NOISE,
  MOD_DST_MIX_SUB_OSC,
  MOD_DST_MIX_FUZZ,
  MOD_DST_MIX_CRUSH,

  MOD_DST_FILTER_CUTOFF,
  MOD_DST_FILTER_RESONANCE,

  MOD_DST_ATTACK,
  MOD_DST_DECAY,
  MOD_DST_RELEASE,
  MOD_DST_LFO_4,

  MOD_DST_VCA,
  MOD_DST_OSC_1_DRUM,
  MOD_DST_OSC_2_DRUM,

  MOD_DST_COUNT
};

enum FilterMode : uint8_t {
  FILTER_MODE_LP,
  FILTER_MODE_BP,
  FILTER_MODE_HP,
  FILTER_MODE_NOTCH,
};

struct OscillatorSettings {
  struct Parameters {
    OscillatorAlgorithm shape;
    uint8_t parameter;
    int8_t range;
    int8_t detune;
  };

  union Data {
    Parameters params;
    uint8_t bytes[sizeof(Parameters)];
  };

  Data data;

  inline OscillatorAlgorithm& shape() {
    return data.params.shape;
  }
  inline uint8_t& parameter() {
    return data.params.parameter;
  }
  inline int8_t& range() {
    return data.params.range;
  }
  inline int8_t& detune() {
    return data.params.detune;
  }
};

struct FilterSettings {
  uint8_t cutoff;
  uint8_t resonance;
  FilterMode mode;
};

struct EnvelopeLfoSettings {
  uint8_t attack;
  uint8_t decay;
  uint8_t sustain;
  uint8_t release;
  LfoWave shape;
  uint8_t rate;
  uint8_t padding;
  LfoSyncMode retrigger_mode;
};



struct Modulation {
  ModSource source;
  ModDestination destination;
  int8_t amount;
};

struct Modifier {
  ModSource operands[2];
  ModifierOp op;
};


static constexpr uint8_t kNumSyncedLfoRates = 15;
static constexpr uint8_t kNumEnvelopes = 3;
static constexpr uint8_t kNumLfos = kNumEnvelopes;
static constexpr uint8_t kNumModulations = 14;
static constexpr uint8_t kNumModifiers = 4;
static constexpr uint8_t kNumOscillators = 2;
static constexpr uint8_t kNumModulationSources = MOD_SRC_COUNT;
static constexpr uint8_t kNumModulationDestinations = MOD_DST_COUNT;

struct Patch {
  struct Parameters {
    // Offset: 0-8
    OscillatorSettings osc[kNumOscillators];
    // Offset: 8-16
    uint8_t mix_balance;
    Operator mix_op;
    uint8_t mix_parameter;
    SubOscillatorAlgorithm mix_sub_osc_shape;
    uint8_t mix_sub_osc;
    uint8_t mix_noise;
    uint8_t mix_fuzz;
    uint8_t mix_crush;
    // Offset: 16-24
    FilterSettings filter[2];
    int8_t filter_env;
    int8_t filter_lfo;
    // Offset: 24-48
    EnvelopeLfoSettings env_lfo[kNumEnvelopes];
    // Offset: 48-50
    LfoWave voice_lfo_shape;
    uint8_t voice_lfo_rate;
    // Offset: 50-92
    Modulation modulation[kNumModulations];
    // Offset: 92-104
    Modifier modifier[kNumModifiers];

    // Offset: 104-106
    // Phase57 filter parameter mods
    int8_t filter_velo;
    int8_t filter_kbt;

    // Offset: 106-112
    uint8_t padding[6];
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
  explicit Patch(Parameters p) : data(p) {};
  explicit Patch() = default;

  constexpr static inline size_t sizeBytes() {
    return sizeof(Parameters);
  }

  inline void setData(uint8_t address, uint8_t value) {
    data.bytes[address] = value;
  }
  inline uint8_t getData(uint8_t address) const {
    return data.bytes[address];
  }

  // raw underlying data
  inline uint8_t* bytes() {
    return data.bytes;
  }

  inline Parameters* params_addr() {
    return &data.params;
  }

  [[nodiscard]] // Clang-Tidy wants this
  inline const uint8_t* bytes_readonly() const {
    return data.bytes;
  }

  // Parameters accessors/mutators
  inline OscillatorSettings& osc(uint8_t index) {
      return data.params.osc[index];
  }
  inline uint8_t& mix_balance() {
      return data.params.mix_balance;
  }
  inline Operator& mix_op() {
      return data.params.mix_op;
  }
  inline uint8_t& mix_parameter() {
      return data.params.mix_parameter;
  }
  inline SubOscillatorAlgorithm & mix_sub_osc_shape() {
      return data.params.mix_sub_osc_shape;
  }
  inline uint8_t& mix_sub_osc() {
      return data.params.mix_sub_osc;
  }
  inline uint8_t& mix_noise() {
      return data.params.mix_noise;
  }
  inline uint8_t& mix_fuzz() {
      return data.params.mix_fuzz;
  }
  inline uint8_t& mix_crush() {
      return data.params.mix_crush;
  }
  inline FilterSettings& filter(uint8_t index) {
      return data.params.filter[index];
  }
  inline int8_t& filter_env() {
      return data.params.filter_env;
  };
  inline int8_t& filter_lfo() {
      return data.params.filter_lfo;
  }
  inline EnvelopeLfoSettings& env_lfo(uint8_t index) {
      return data.params.env_lfo[index];
  }
  inline LfoWave& voice_lfo_shape() {
      return data.params.voice_lfo_shape;
  }
  inline uint8_t& voice_lfo_rate() {
      return data.params.voice_lfo_rate;
  }
  inline Modulation& modulation(uint8_t index) {
      return data.params.modulation[index];
  }
  inline Modifier& modifier(uint8_t index) {
      return data.params.modifier[index];
  }

  inline int8_t& filter_velo() {
    return data.params.filter_velo;
  }

  inline int8_t& filter_kbt() {
    return data.params.filter_kbt;
  }
  // no need to access padding
};

// these offsets have to match the order of members in Patch::Parameters
// TODO this is kind of ugly, can we reduce repetition
enum PatchParameter : uint8_t {
  PRM_PATCH_OSC1_SHAPE,
  PRM_PATCH_OSC1_PWM,
  PRM_PATCH_OSC1_RANGE,
  PRM_PATCH_OSC1_DETUNE,
  PRM_PATCH_OSC2_SHAPE,
  PRM_PATCH_OSC2_PWM,
  PRM_PATCH_OSC2_RANGE,
  PRM_PATCH_OSC2_DETUNE,

  PRM_PATCH_MIX_BALANCE,
  PRM_PATCH_MIX_OPERATOR,
  PRM_PATCH_MIX_PARAMETER,
  PRM_PATCH_MIX_SUB_SHAPE,
  PRM_PATCH_MIX_SUB_LEVEL,
  PRM_PATCH_MIX_NOISE_LEVEL,
  PRM_PATCH_MIX_FUZZ,
  PRM_PATCH_MIX_CRUSH,

  PRM_PATCH_FILTER1_CUTOFF,
  PRM_PATCH_FILTER1_RESONANCE,
  PRM_PATCH_FILTER1_MODE,
  PRM_PATCH_FILTER2_CUTOFF,
  PRM_PATCH_FILTER2_RESONANCE,
  PRM_PATCH_FILTER2_MODE,
  PRM_PATCH_FILTER1_ENV,
  PRM_PATCH_FILTER1_LFO,
  
  PRM_PATCH_ENV_ATTACK,
  PRM_PATCH_ENV_DECAY,
  PRM_PATCH_ENV_SUSTAIN,
  PRM_PATCH_ENV_RELEASE,
  PRM_PATCH_LFO_SHAPE,
  PRM_PATCH_LFO_RATE,
  PRM_PATCH_LFO_ATTACK,
  PRM_PATCH_LFO_SYNC,
  
  PRM_PATCH_VOICE_LFO_SHAPE = 48,
  PRM_PATCH_VOICE_LFO_RATE,
  
  PRM_PATCH_MOD_SOURCE,
  PRM_PATCH_MOD_DESTINATION,
  PRM_PATCH_MOD_AMOUNT,
  
  PRM_PATCH_MOD_OPERAND1 = 92,
  PRM_PATCH_MOD_OPERAND2,
  PRM_PATCH_MOD_OPERATOR,

  // Phase57 filter mods
  PRM_PATCH_FILTER1_VELO = 104,
  PRM_PATCH_FILTER1_KBT
};

}  // namespace ambika

#endif  // COMMON_PATCH_H_
