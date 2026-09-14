#ifndef AVRLIB_RING_BUFFER_2_H_
#define AVRLIB_RING_BUFFER_2_H_

#include "avrlib/base.h"
#include "avrlib/avrlib.h"

namespace avrlib {

// size must be power of 2
template<typename data_t, uint8_t size>
class RingBuffer2 {
private:
  data_t buffer[size];
  volatile uint8_t read_ptr;
  volatile uint8_t write_ptr;

  DISALLOW_COPY_AND_ASSIGN(RingBuffer2);

public:
  RingBuffer2() = default;


  inline uint8_t capacity() const {
    return size;
  }
  // full is defined to be if the write pointer is one position behind
  // the read pointer (so technically there is still one slot empty)
  inline uint8_t spaceLeft() const {
    return byteAnd(read_ptr - write_ptr - 1, size - 1);
  }

  inline uint8_t isEmpty() const {
    return read_ptr == write_ptr;
  }

  inline uint8_t count() const {
    return byteAnd(write_ptr - read_ptr, size - 1);
  }

  inline uint8_t isReadable() const {
    return count() != 0;
  }

  inline uint8_t isWritable() const {
    return spaceLeft() == 0;
  }

  inline void flush() {
    write_ptr = read_ptr;
  }

  inline void writeBlocking(data_t v) {
    while (!isWritable());
    overwrite(v);
  }

  inline uint8_t writeNonBlocking(data_t v) {
    if (!isWritable()) {
      return 0;
    } else {
      overwrite(v);
      return 1;
    }
  }
  inline void overwrite(data_t v) {
    // cache index because it's marked volatile
    uint8_t p = write_ptr;
    buffer[p] = v;
    write_ptr = byteAnd(p + 1, size - 1);
  }
  inline void overwrite2(data_t v1, data_t v2) {
    // cache index because it's marked volatile
    uint8_t p = write_ptr;
    buffer[p] = v1;
    buffer[p + 1] = v2;
    write_ptr = byteAnd(p + 2, size - 1);
  }

  inline data_t read() {
    while (!isReadable());
    return immediateRead();
  }

  inline data_t readNonBlocking(data_t nullValue) {
    return isReadable() ? immediateRead() : nullValue;
  }

  // non-template solution - returning a value outside the range
  // if no null value is given
  inline int16_t readNonBlocking() {
    return isReadable() ? immediateRead() : -1;
  }

  inline data_t immediateRead() {
    // cache index because it's marked volatile
    uint8_t p = read_ptr;
    data_t result = buffer[p];
    read_ptr = byteAnd(p + 1, size - 1);
    return result;
  }
};


}  // namespace avrlib

#endif   // AVRLIB_RING_BUFFER_2_H_
