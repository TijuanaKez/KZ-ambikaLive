#ifndef CONTROLLER_DIAGNOSTICS_H_
#define CONTROLLER_DIAGNOSTICS_H_

#include "avrlib/base.h"
#include "common/features.h"

namespace ambika {

// Call before enabling interrupts or starting timers. No dynamic allocation
// is permitted after painting the unused SRAM in a diagnostic build.
void InitDiagnostics();
uint8_t ResetCause();
uint16_t FreeSram();
uint16_t UntouchedSram();

#ifdef DIAGNOSTIC_BUILD
// KZ MOD: stack context marking.
//
// LOW is a latch with no context: one rare deep excursion sets it and there is
// no way to tell which code path did it. These let the deepest observed stack
// position be attributed to whatever was running at the time.
//
// Set the marker around anything suspected of being deep; the sampler in the
// TIMER1 interrupt records which marker was active when a new low was seen.
enum StackContext : uint8_t {
  STACK_CTX_IDLE,
  STACK_CTX_UI,          // UI event dispatch and page rendering
  STACK_CTX_LOAD,        // Storage::Load, including the snapshot it triggers
  STACK_CTX_SAVE,        // Storage::Save
  STACK_CTX_SYSEX,       // SysEx receive/reply
  STACK_CTX_SD_TICK,     // the periodic filesystem tick
  STACK_CTX_MIDI,        // MIDI input dispatch
  STACK_CTX_LAST
};

extern volatile uint8_t stack_context;
extern volatile uint8_t stack_low_context;

// Called from the TIMER1 interrupt. Cheap: a compare against the running
// minimum, and a byte store only when a new low is actually reached.
void SampleStackDepth();

// Scoped marker. Restores the previous context, so nesting behaves.
class ScopedStackContext {
 public:
  explicit ScopedStackContext(uint8_t context) : previous_(stack_context) {
    stack_context = context;
  }
  ~ScopedStackContext() { stack_context = previous_; }
 private:
  uint8_t previous_;
};
#define STACK_CONTEXT(name) ScopedStackContext scoped_stack_context_(name)
#else
#define STACK_CONTEXT(name) do {} while (0)
#endif

}  // namespace ambika

#endif  // CONTROLLER_DIAGNOSTICS_H_
