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

#include "avrlib/boot.h"
#include "avrlib/serial.h"
#include "avrlib/watchdog_timer.h"

#include "controller/diagnostics.h"
#include "controller/midi_dispatcher.h"
#include "controller/multi.h"
#include "controller/resources.h"
#include "controller/storage.h"
#include "controller/system_settings.h"
#include "controller/ui.h"
#include "controller/voicecard_tx.h"

#include "midi/midi.h"

using namespace ambika;
using namespace avrlib;
using namespace midi;

// Midi input.
MidiIO midi_io;
MidiBuffer midi_in_buffer;
MidiStreamParser<MidiDispatcher> midi_parser;

inline void FlushMidiOut() {
  // Try to flush the high priority buffer first.
  if (midi_dispatcher.readable_high_priority()) {
    if (midi_io.writable()) {
      midi_io.Overwrite(midi_dispatcher.ImmediateReadHighPriority());
    }
  } else {
    if (midi_dispatcher.readable_low_priority()) {
      if (midi_io.writable()) {
        midi_io.Overwrite(midi_dispatcher.ImmediateReadLowPriority());
      }
    }
  }
}

inline void PollMidiIn() {
  STACK_CONTEXT(STACK_CTX_MIDI);
  if (midi_io.readable()) {
    midi_in_buffer.NonBlockingWrite(midi_io.ImmediateRead());
  }
}

// This timer is responsible for:
// - Flushing the MIDI out data, at a rate of 4.882kHz
// - Debouncing the switches and refreshing the LCD at 4.882kHz
// - Ticking the ms sys clock at 4.882kHz / 4 = 1.221 kHz
// KZ MOD: this used to be ISR_NOBLOCK, which re-enables interrupts in the
// prologue so the handler can interrupt itself if a pass runs past its 205 us
// period -- likely under a MIDI controller flood, since PollMidiIn,
// FlushMidiOut and ui.Poll all live here. Each re-entry costs another frame of
// the deepest interrupt in the firmware, so unbounded nesting walks the stack
// into static data.
//
// Entering with interrupts disabled makes the test-and-set atomic; an earlier
// attempt kept ISR_NOBLOCK and raced against the very interrupt it excluded,
// because the flag was set after the prologue had already re-enabled them.
// sei() then restores the nesting this ISR needs for the audio and voice card
// timers, with re-entry of *this* handler bounded to one.
//
// A tick arriving while a pass is still running is dropped. At 4.882 kHz that
// is invisible, and it is strictly better than running out of SRAM.
ISR(TIMER1_OVF_vect) {
  static volatile uint8_t in_progress = 0;
  if (in_progress) {
    return;
  }
  in_progress = 1;
  sei();

#ifdef DIAGNOSTIC_BUILD
  SampleStackDepth();
#endif

  static uint8_t cycle = 0;
  PollMidiIn();
  FlushMidiOut();
  ui.Poll();
  if ((cycle & 3) == 0) {
    TickSystemClock();
  }
  ++cycle;
  if (cycle == 48) {
    cycle = 0;
    STACK_CONTEXT(STACK_CTX_SD_TICK);
    storage.Tick();
  }
  cli();
  in_progress = 0;
}

// This timer is responsible for keeping track of time for the internal clock,
// and for sending data to the voicecards.
ISR(TIMER2_OVF_vect) {
  multi.Tick();
  voicecard_tx.SendBytes();
}

void Init() {
#ifdef DIAGNOSTIC_BUILD
  cli();
  InitDiagnostics();
#endif
  sei();
  UCSR0B = 0;
  UCSR1B = 0;
  ResetWatchdog();
  Gpio<PortC, 0>::set_mode(DIGITAL_OUTPUT);
  system_settings.Init(false);
  midi_io.Init();
  
  Timer<1>::set_prescaler(2);
  Timer<1>::set_mode(TIMER_PWM_PHASE_CORRECT);
  Timer<2>::set_prescaler(1);
  Timer<2>::set_mode(TIMER_PWM_PHASE_CORRECT);
  Timer<2>::Start();
  
  ui.Init();
  Timer<1>::Start();

  voicecard_tx.Init();
  voicecard_tx.SyncAllVoices();
  if (!system_settings.data().voicecard_leds()) {
    voicecard_tx.LightsOut();
  }
  if (ui.shifted()) {
    system_settings.Init(true);
  }
  multi.Init(ui.shifted());

  storage.Init();
}

int main() {
  Init();
#ifdef DIAGNOSTIC_BUILD
  ui.ShowPage(PAGE_OS_INFO);
#endif
  ui.FlushEvents();
  while (1) {
    // Do some MIDI.
    while (midi_in_buffer.readable()) {
      midi_parser.PushByte(midi_in_buffer.ImmediateRead());
    }
    // Do some LFOs and clocks.
    multi.UpdateClocks();
    // Do some display.
    ui.DoEvents();
  }
}
