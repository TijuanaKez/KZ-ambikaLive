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

}  // namespace ambika

#endif  // CONTROLLER_DIAGNOSTICS_H_
