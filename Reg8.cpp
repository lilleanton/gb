#include "Reg8.h"

namespace CPU {

// ctor
Reg8::Reg8(uint8_t v) : val(v) { }

// accessors
uint8_t Reg8::getVal() const {
    return val;
}

bool Reg8::getBit(int idx) {
    return (val >> idx) & 1;
}

// assignment
void Reg8::operator=(const Reg8& other) {
    val = other.val;
}

void Reg8::operator=(int other) {
    val = uint8_t(other);
}

// arithmetic
uint8_t Reg8::operator+(const Reg8& other) const {
    return uint8_t(val + other.val);
}

uint8_t Reg8::operator-(const Reg8& other) const {
    return uint8_t(val - other.val);
}

uint8_t Reg8::operator+=(const Reg8& other) {
    uint8_t a = val, b = other.val;
    uint16_t sum = uint16_t(a) + uint16_t(b);
    uint8_t res = uint8_t(sum);

    uint8_t flags = 0;
    if (res == 0)               flags |= 0x80; // Z
    if (((a & 0xF) + (b & 0xF)) > 0xF) flags |= 0x20; // H
    if (sum > 0xFF)             flags |= 0x10; // C

    val = res;
    return flags;
}

uint8_t Reg8::operator+=(int other) {
    return *this += Reg8(uint8_t(other));
}

uint8_t Reg8::operator-=(const Reg8& other) {
    uint8_t a = val, b = other.val;
    uint8_t res = uint8_t(a - b);

    uint8_t flags = 0;
    if (res == 0)           flags |= 0x80; // Z
    flags |= 0x40;          // N
    if ((a & 0xF) < (b & 0xF)) flags |= 0x20; // H
    if (a < b)               flags |= 0x10; // C

    val = res;
    return flags;
}

uint8_t Reg8::operator-=(int other) {
    return *this -= Reg8(uint8_t(other));
}

// bitwise
uint8_t Reg8::operator&=(const Reg8& other) {
    val &= other.val;
    return (val >> 7) & 1; 
}

uint8_t Reg8::operator^=(const Reg8& other) {
    val ^= other.val;
    return (val >> 7) & 1; 
}

uint8_t Reg8::operator|=(const Reg8& other) {
    val |= other.val;
    return (val >> 7) & 1; 
}

uint8_t Reg8::operator&=(uint8_t other) {
    val &= other;
    return (val >> 7) & 1; 
}

uint8_t Reg8::operator^=(uint8_t other) {
    val ^= other;
    return (val >> 7) & 1; 
}

uint8_t Reg8::operator|=(uint8_t other) {
    val |= other;
    return (val >> 7) & 1; 
}

// rotate
uint8_t Reg8::RLC() {
    bool carry = (val & 0x80) != 0;
    val = uint8_t((val << 1) | (carry ? 1 : 0));

    uint8_t flags = 0;
    if (val == 0)      flags |= 0x80; // Z
    if (carry)         flags |= 0x10; // C
    return flags;
}

uint8_t Reg8::RRC() {
    bool carry = (val & 1) != 0;
    val = uint8_t((val >> 1) | (carry ? 0x80 : 0));

    uint8_t flags = 0;
    if (val == 0)      flags |= 0x80; // Z
    if (carry)         flags |= 0x10; // C
    return flags;
}

uint8_t Reg8::RL(bool carry_in) {
    bool carry = (val & 0x80) != 0;
    val = uint8_t((val << 1) | (carry_in ? 1 : 0));

    uint8_t flags = 0;
    if (val == 0)      flags |= 0x80; // Z
    if (carry)         flags |= 0x10; // C
    return flags;
}

uint8_t Reg8::RR(bool carry_in) {
    bool carry = (val & 1) != 0;
    val = uint8_t((val >> 1) | (carry_in ? 0x80 : 0));

    uint8_t flags = 0;
    if (val == 0)      flags |= 0x80; // Z
    if (carry)         flags |= 0x10; // C
    return flags;
}

// shift
uint8_t Reg8::SRA() {
    bool carry = val & 1;
    bool msb   = val & 0x80;
    val = uint8_t((val >> 1) | (msb ? 0x80 : 0));

    return uint8_t((val != 0) << 7) | uint8_t(carry << 4);
}

uint8_t Reg8::SRL() {
    bool carry = val & 1;
    val = uint8_t(val >> 1);

    return uint8_t((val != 0) << 7) | uint8_t(carry << 4);
}

uint8_t Reg8::SLA() {
    bool carry = (val & 0x80) != 0;
    val = uint8_t(val << 1);

    return uint8_t((val != 0) << 7) | uint8_t(carry << 4);
}

} // namespace CPU
