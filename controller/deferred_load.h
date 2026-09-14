// KZ MOD: deferral timer for library browsing.
//
// Browsing used to run a full patch load on every encoder detent, which parses
// the whole RIFF file, pushes parameters to the voice cards and can trigger a
// snapshot write to the SD card. This tracks a load that has been postponed
// until the user stops scrolling.

#ifndef CONTROLLER_DEFERRED_LOAD_H_
#define CONTROLLER_DEFERRED_LOAD_H_

#include "avrlib/base.h"

namespace ambika {

// Times are 16-bit snapshots of the millisecond clock. The subtraction below
// wraps correctly, so an armed load stays correct across the 65.536 s rollover
// for any delay shorter than that; the one-byte setting cannot exceed 2.55 s.
class DeferredLoad {
 public:
  inline void Arm(uint16_t now) {
    armed_at_ = now;
    armed_ = 1;
  }
  inline void Disarm() {
    armed_ = 0;
  }
  inline uint8_t armed() const {
    return armed_;
  }
  inline uint8_t Due(uint16_t now, uint16_t delay_ms) const {
    return armed_ && static_cast<uint16_t>(now - armed_at_) >= delay_ms;
  }

 private:
  uint16_t armed_at_ = 0;
  uint8_t armed_ = 0;
};

}  // namespace ambika

#endif  // CONTROLLER_DEFERRED_LOAD_H_
