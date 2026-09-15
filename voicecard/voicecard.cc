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


#include <avr/interrupt.h>

#include "voicecard/voicecard.h"

#include "avrlib/boot.h"
#include "avrlib/gpio.h"
#include "avrlib/parallel_io.h"
#include "avrlib/timer.h"

#include "voicecard/audio_out.h"
#include "voicecard/leds.h"
#include "voicecard/resources.h"
#include "voicecard/voice.h"
#include "voicecard/voicecard_rx.h"

using namespace avrlib;
using namespace ambika;

static constexpr uint8_t kPinVcaOut = 3;
static constexpr uint8_t kPinVcfResonanceOut = 5;
static constexpr uint8_t kPinVcfCutoffOut = 6;

PwmOutput<kPinVcfCutoffOut> vcf_cutoff_out;
PwmOutput<kPinVcfResonanceOut> vcf_resonance_out;
PwmOutput<kPinVcaOut> vca_out;

ParallelPort<PortC, PARALLEL_TRIPLE_LOW> vcf_mode;

using log_vca = PortBPin<0>;

using UartSpiSS = PortDPin<2>;
UartSpiMaster<UartSpiPort0, UartSpiSS, 2> dac_interface;

//#define TIMING_CODE

#ifdef TIMING_CODE
using timing_signal1 = PortCPin<0>;
using timing_signal2 = PortCPin<1>;
using timing_signal3 = PortCPin<2>;

static volatile uint8_t interrupt_counter;
#endif


static constexpr uint8_t dac_scale = 16;
static volatile uint8_t update_vca;

// KZ MOD: audio CPU load, for measuring the budget without a scope.
//
// The ISR drains exactly one sample per 39.2 kHz tick, so counting ticks during
// one ProcessBlock() says how many of the 40 sample-times that block was allowed
// were actually used. 40 means the render took as long as the audio it produced,
// i.e. 100% of budget; half that is 50%. Unlike free-space-in-the-buffer, this
// does not saturate while the renderer is keeping up, so it can measure headroom
// rather than only detect its absence.
//
// The cost is one increment per audio interrupt, about 1% of the cycle budget.
// That is the price of being able to measure every new oscillator in Phase 4.
namespace ambika {
volatile uint8_t audio_tick_counter;
volatile uint8_t audio_load_peak;
volatile uint8_t audio_starved;
}  // namespace ambika

ISR(TIMER2_OVF_vect) {
  static uint8_t sample_counter = 0;
  static Word vca_12bits;
  if (update_vca) {
    dac_interface.Strobe();
    update_vca = 0;
    dac_interface.Overwrite(vca_12bits.bytes[1]);
    dac_interface.Overwrite(vca_12bits.bytes[0]);
    dac_interface.Wait();
    auto voice_vca = voice.vca();
    uint16_t next_vca_value = log_vca::isLow()
        ? ambika::ResourcesManager::Lookup<uint16_t, uint8_t>(lut_res_vca_linearization, voice_vca)
        : voice_vca * dac_scale;
    vca_12bits.value = next_vca_value | 0x1000u;
  }

  if (audio_tick_counter < 255) {
    ++audio_tick_counter;
  }
  if (!audio_buffer.isReadable()) {
    // The renderer did not keep up: this sample period has no audio for it.
    audio_starved = 1;
  }
  if (audio_buffer.isReadable()) {
    uint8_t sample = audio_buffer.immediateRead();
    sample_counter++;
    if (sample_counter >= voice.crush()) {
      dac_interface.Strobe();
      sample_counter = 0;
      uint16_t sample_12bits = U16(U16(sample * dac_scale) | 0x9000u);
      dac_interface.Overwrite(highByte(sample_12bits));
      dac_interface.Overwrite(lowByte(sample_12bits));
    }
  }
  voicecard_rx.Receive();

#ifdef TIMING_CODE
  interrupt_counter++;
#endif
}


inline void Init() {
  sei();
  UCSR0B = 0;
  ResetWatchdog();
  
  dac_interface.Init();

  RxLed::outputMode();
  NoteLed::outputMode();
  RxLed::low();
  NoteLed::low();

  vcf_cutoff_out.Init();
  vcf_resonance_out.Init();
  vca_out.Init();
  vcf_mode.set_mode(DIGITAL_OUTPUT);

  voicecard_rx.Init();
  voice.Init();

  dac_interface.Strobe();
  dac_interface.Overwrite(0x1f); // 0x10 | 0x0f
  dac_interface.Overwrite(0xff);

  log_vca::inputMode();
  log_vca::high();

  // Successful boot!
  if (eeprom_read_byte(kFirmwareUpdateFlagPtr) != FIRMWARE_UPDATE_DONE) {
    eeprom_write_byte(kFirmwareUpdateFlagPtr, FIRMWARE_UPDATE_DONE);
  }
  
  // Initialize all the PWM outputs to 39kHz, phase correct mode.
  Timer<0>::set_prescaler(1);
  Timer<0>::set_mode(TIMER_PWM_PHASE_CORRECT);
  Timer<1>::set_prescaler(1);
  Timer<1>::set_mode(TIMER_PWM_PHASE_CORRECT);
  Timer<2>::set_prescaler(1);
  Timer<2>::set_mode(TIMER_PWM_PHASE_CORRECT);
  Timer<2>::Start();

#ifdef TIMING_CODE
  timing_signal1::outputMode();
  timing_signal2::outputMode();
  timing_signal3::outputMode();
  timing_signal1::low();
  timing_signal2::low();
  timing_signal3::low();
#endif
}

static uint8_t filter_mode_bytes[] = { 0, 1, 2, 3 };
//static uint8_t leds_timeout = 0;

int main() {
  Init();
  // For testing only...
  //voice.Trigger(60 * 128, 100, 0);
  while (1) {
    // Check if there's a block of samples to fill.

#ifdef TIMING_CODE
    interrupt_counter = 0;
#endif
    if (audio_buffer.spaceLeft() >= kAudioBlockSize) {
      audio_tick_counter = 0;
      voicecard_rx.TickRxLed();
#ifdef TIMING_CODE
      timing_signal1::high();
      voice.ProcessBlock();
      timing_signal1::low();
#else
      voice.ProcessBlock();
#endif
      // Ticks elapsed during ProcessBlock, out of the kAudioBlockSize the block
      // was worth. Read before anything else can consume time.
      uint8_t load = audio_tick_counter;
      if (load > audio_load_peak) {
        audio_load_peak = load;
      }
      vcf_cutoff_out.Write(voice.cutoff());
      vcf_resonance_out.Write(voice.resonance());
      vcf_mode.Write(filter_mode_bytes[voice.patch().filter(0).mode]);
      update_vca = 1;
    }
    voicecard_rx.Process();

#ifdef TIMING_CODE
    if (interrupt_counter > kAudioBlockSize) {
      // interrupts going faster than audio prcessing
      timing_signal3::high();
    } else {
      timing_signal3::low();
    }
#endif
  }
}
