#include "controller/diagnostics.h"

#ifdef DIAGNOSTIC_BUILD

#include <avr/io.h>
#include <util/atomic.h>

extern char __heap_start;

namespace ambika {

static uint8_t reset_cause;
static constexpr uint8_t kStackPaint = 0xa5;

void __attribute__((noinline)) InitDiagnostics() {
  reset_cause = MCUSR;
  // Interrupts are disabled by the caller. Stay below this function's
  // live frame, with an extra guard for compiler temporaries. The volatile
  // writes must stay here rather than becoming a library memset call.
  uintptr_t limit = SP - 16;
  for (uintptr_t address = reinterpret_cast<uintptr_t>(&::__heap_start);
       address < limit; ++address) {
    *reinterpret_cast<volatile uint8_t*>(address) = kStackPaint;
  }
}

uint8_t ResetCause() {
  return reset_cause;
}

uint16_t FreeSram() {
  uintptr_t stack;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    stack = SP;
  }
  uintptr_t start = reinterpret_cast<uintptr_t>(&::__heap_start);
  return stack > start ? stack - start : 0;
}

volatile uint8_t stack_context = STACK_CTX_IDLE;
volatile uint8_t stack_low_context = STACK_CTX_IDLE;
static uint16_t lowest_stack_pointer = 0xffff;

// Sampled from the TIMER1 interrupt: a compare against the running minimum,
// and a byte store only when a new low is actually reached.
void SampleStackDepth() {
  uint16_t stack = SP;
  if (stack < lowest_stack_pointer) {
    lowest_stack_pointer = stack;
    stack_low_context = stack_context;
  }
}

uint16_t UntouchedSram() {
  uintptr_t start = reinterpret_cast<uintptr_t>(&::__heap_start);
  uintptr_t address = start;
  uintptr_t limit = start + FreeSram();
  while (address < limit &&
         *reinterpret_cast<volatile uint8_t*>(address) == kStackPaint) {
    ++address;
  }
  return address - start;
}

}  // namespace ambika

#else

namespace ambika {

void InitDiagnostics() {}
uint8_t ResetCause() { return 0; }
uint16_t FreeSram() { return 0; }
uint16_t UntouchedSram() { return 0; }

}  // namespace ambika

#endif
