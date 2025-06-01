#pragma once
#include <cstdint>

namespace CPU {

class Reg8 {
private:
    uint8_t val;

public:
    // ctor
    Reg8(uint8_t v = 0);

    // accessors
    uint8_t getVal() const;
    bool    getBit(int idx);

    // assignment
    void operator=(const Reg8& other);
    void operator=(int other);

    // arithmetic
    uint8_t operator+(const Reg8& other) const;
    uint8_t operator-(const Reg8& other) const;

    uint8_t operator+=(const Reg8& other);
    uint8_t operator+=(int other);
    uint8_t operator-=(const Reg8& other);
    uint8_t operator-=(int other);

    uint8_t adc(const Reg8& other, bool carry_in);
    uint8_t sbc(const Reg8& other, bool carry_in);

    // bitwise and, xor, or (with Reg8 and uint8_t)
    uint8_t operator&=(const Reg8& other);
    uint8_t operator^=(const Reg8& other);
    uint8_t operator|=(const Reg8& other);
    uint8_t operator&=(uint8_t other);
    uint8_t operator^=(uint8_t other);
    uint8_t operator|=(uint8_t other);
    void    setBit(uint8_t idx);
    void    resetBit(uint8_t idx);

    // rotate
    uint8_t RLC();
    uint8_t RRC();
    uint8_t RL (bool carry_in);
    uint8_t RR (bool carry_in);

    // shift
    uint8_t SRA();
    uint8_t SRL();
    uint8_t SLA();
};

} // namespace CPU