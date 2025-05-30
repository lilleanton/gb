// Reg16.cpp
#include "Reg16.h"
#include <stdio.h>

namespace CPU {

Reg16::Reg16(Reg8& H, Reg8& L) : HI(&H), LO(&L) {}

uint16_t Reg16::getVal() const {
    return (uint16_t(HI->getVal()) << 8) | LO->getVal();
}

void Reg16::operator=(const Reg16& other) {
    uint16_t v = other.getVal();
    *HI = uint8_t((v & 0xFF00) >> 8);
    *LO = uint8_t(v & 0x00FF);
}

void Reg16::operator=(uint16_t other) {
    *HI = uint8_t((other & 0xFF00) >> 8);
    *LO = uint8_t(other & 0x00FF);
}

uint8_t Reg16::operator+=(const Reg16& other) {
    uint16_t a = getVal();
    uint16_t b = other.getVal();
    uint32_t sum = uint32_t(a) + uint32_t(b);

    uint8_t flags = 0;
    // H: carry from bit 11
    if (((a & 0x0FFF) + (b & 0x0FFF)) > 0x0FFF) flags |= 0x20;
    // C: carry from bit 15
    if (sum > 0xFFFF)                     flags |= 0x10;

    uint16_t res = uint16_t(sum);
    *HI = uint8_t((res & 0xFF00) >> 8);
    *LO = uint8_t(res & 0x00FF);
    return flags;
}

uint8_t Reg16::operator+=(uint16_t other) {
    uint16_t a = getVal();
    uint32_t sum = uint32_t(a) + uint32_t(other);
    uint8_t flags = 0;
    if (((a & 0x0FFF) + (other & 0x0FFF)) > 0x0FFF) flags |= 0x20;
    if (sum > 0xFFFF)                                flags |= 0x10;

    uint16_t res = uint16_t(sum);
    *HI = uint8_t((res & 0xFF00) >> 8);
    *LO = uint8_t(res & 0x00FF);
    return flags;
}

// Signed 8-bit add (for ADD SP,r8 / opcode 0xE8)
uint8_t Reg16::operator+=(int8_t offset) {
    uint16_t old = getVal();
    uint16_t res = uint16_t(old + offset);  // wraps mod 2¹⁶
    uint8_t flags = 0;
    // half-carry: carry from bit 3 to 4 of low nibble
    if ( ((old & 0xF) + (offset & 0xF)) > 0xF )   flags |= 0x20;
    // carry:    carry from bit 7 to 8 of low byte
    if ( ((old & 0xFF) + (offset & 0xFF)) > 0xFF ) flags |= 0x10;
    *HI = uint8_t(res >> 8);
    *LO = uint8_t(res & 0xFF);
    return flags;  // Z and N are forced 0 by the caller
  }

uint16_t Reg16::operator++() {
    *this = uint16_t(getVal() + 1);
    return getVal();
}

uint16_t Reg16::operator--() {
    *this = uint16_t(getVal() - 1);
    return getVal();
}

uint16_t Reg16::operator++(int) {
    uint16_t old = getVal();
    *this = uint16_t(old + 1);
    return old;
}

uint16_t Reg16::operator--(int) {
    uint16_t old = getVal();
    *this = uint16_t(old - 1);
    return old;
}

} // namespace CPU
