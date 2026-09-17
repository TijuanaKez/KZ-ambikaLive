// KZ MOD: see wavetables.cc. Declared separately so the wavetable renderers
// can reach the wave bank without the generated resource tables referencing it.

#ifndef VOICECARD_WAVETABLES_H_
#define VOICECARD_WAVETABLES_H_

#include "common/features.h"

#ifndef DISABLE_WAVETABLES

#include <avr/pgmspace.h>
#include "avrlib/base.h"

namespace ambika {

extern const uint8_t wav_res_waves[] PROGMEM;
extern const uint8_t wav_res_wavetables[] PROGMEM;

}  // namespace ambika

#endif  // DISABLE_WAVETABLES
#endif  // VOICECARD_WAVETABLES_H_
