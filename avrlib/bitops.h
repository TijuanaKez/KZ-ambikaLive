//
// Created by max on 1/3/19.
//

#ifndef AVRLIB_BITOPS_H
#define AVRLIB_BITOPS_H

#include <inttypes.h>
#include "base.h"

// get rid of warnings about unsigned bitwise operations

template <typename T>
inline constexpr uint8_t U8(T x) {
    return static_cast<uint8_t>(x);
}

template <typename T>
inline constexpr uint8_t U7(T x) {
  return static_cast<uint8_t>(x & 0x7f);
}

template <typename T>
inline constexpr uint16_t U16(T x) {
    return static_cast<uint16_t>(x);
}
template <typename T>
inline constexpr int8_t S8(T x) {
    return static_cast<int8_t>(x);
}

template <typename T>
inline constexpr int16_t S16(T x) {
    return static_cast<int16_t>(x);
}

template <typename T>
inline constexpr uint32_t U32(T x) {
  return static_cast<uint32_t>(x);
}
template <typename T>
inline constexpr int32_t S32(T x) {
  return static_cast<int32_t>(x);
}

// easy way to make uint8_t integer literals
inline constexpr uint8_t operator "" _u8(unsigned long long arg) noexcept {
    return U8(arg);

}
// easy way to make uint16_t integer literals
inline constexpr uint16_t operator "" _u16(unsigned long long arg) noexcept {
    return U16(arg);
}


// The outermost casts aren't technically needed in these functions,
// due to the implicit casting to the return type, but they show the intent.
inline constexpr uint8_t lowByte(uint16_t w) {
  return U8(w);
}

inline constexpr uint8_t highByte(uint16_t w) {
    return U8(w >> 8u);
}

inline constexpr uint8_t highByte24(uint24_t d) {
  return U8(d >> 16u);
}

inline constexpr uint16_t highWord24(uint24_t d) {
  return U16(d >> 8u);
}

inline constexpr uint16_t highWord(uint32_t i) {
  return U16(i >> 16u);
}

inline constexpr uint16_t word(uint8_t high, uint8_t low) {
  return U16(high << 8u) | low;
}

// Little endian order!!!!!!
inline constexpr uint32_t FourCC(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
  return (U32(word(b3, b2)) << 16u) | word(b1, b0);
}

template<typename S, typename T>
inline constexpr uint8_t byteOr(S b1, T b2) {
    return U8(U8(b1) | U8(b2));
}

template<typename S, typename T>
inline constexpr uint8_t byteAnd(S b1, T b2) {
    return U8(U8(b1) & U8(b2));
}

// Masks off the low 4 bits
inline constexpr uint8_t highNibbleUnshifted(uint8_t b) {
  return byteAnd(b, 0xf0);
}

inline constexpr uint8_t highNibble(uint8_t b) {
  return b >> 4u;
}


// Masks off the low 4 bits
inline constexpr uint8_t lowNibble(uint8_t b) {
  return byteAnd(b, 0x0f);
}

template<typename S, typename T>
inline constexpr uint16_t wordOr(S w1, T w2) {
  return U16(w1) | U16(w2);
}

template<typename S, typename T>
inline constexpr uint16_t wordAnd(S w1, T w2) {
  return U16(w1) & U16(w2);
}

template<typename S, typename T, typename U>
inline constexpr uint8_t byteOr(S b1, T b2, U b3) {
    return byteOr(byteOr(b1, b2), b3);
}

template<typename S, typename T, typename U>
inline constexpr uint8_t byteAnd(S b1, T b2, U b3) {
    return byteAnd(byteAnd(b1, b2), b3);
}

constexpr uint8_t staticBitFlag(uint8_t bit) {
  switch (bit) {
    case 0: return 1;
    case 1: return 2;
    case 2: return 4;
    case 3: return 8;
    case 4: return 16;
    case 5: return 32;
    case 6: return 64;
    case 7: return 128;
    default: return 0;
  }
}

template<typename T>
inline constexpr uint8_t bitFlag8(T b) {
    //return U8(1u << U8(b));
    return bitFlag8Lookup(U8(b));
}

template<typename T>
inline constexpr uint16_t bitFlag16(T b) {
    return U16(1u << U8(b));
}

template<typename T>
inline constexpr uint8_t byteInverse(T b) {
    return U8(~b);
}

template<typename T>
inline constexpr bool bitTest(uint8_t byte, T bit) {
    return byteAnd(byte, bitFlag8(bit));
}
template<typename T>
inline constexpr bool bitTest(uint16_t word, T bit) {
    return (word & bitFlag16(bit)) != 0;
}

inline constexpr bool msb(uint8_t x) {
  return byteAnd(x, 0x80) != 0;
}


//#define mask1(bit1) bitMask(bit1)
//#define mask2(bit1, bit2) byteOr(bitMask(bit1), bitMask(bit2))
//#define mask3(bit1, bit2, bit3) byteOr3(bitMask(bit1), bitMask(bit2), bitMask(bit3))



#endif //AVRLIB_BITOPS_H
