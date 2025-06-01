// Reg16.h
#pragma once

#include <cstdint>
#include "../Reg8/Reg8.h"

namespace CPU {

class Reg16 {
private:
    Reg8* HI;
    Reg8* LO;
public:
    // ctor binds to two 8-bit registers
    Reg16(Reg8& H, Reg8& L);

    // read 16-bit value
    uint16_t getVal() const;

    // assignments
    void operator=(const Reg16& other);
    void operator=(uint16_t other);

    // 16-bit add: updates registers and returns flag byte (H and C)
    uint8_t operator+=(const Reg16& other);
    uint8_t operator+=(uint16_t other);
    uint8_t operator+=(int8_t offset);

    // increments/decrements
    uint16_t operator++();    // prefix
    uint16_t operator--();    // prefix
    uint16_t operator++(int); // postfix
    uint16_t operator--(int); // postfix
};

} // namespace CPU