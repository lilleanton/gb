#pragma once
#include <cstdint>
#include <stdio.h>
#include "memory.h"
#include "Reg8.h"
#include "Reg16.h"
#include "json.hpp"
using json = nlohmann::json;

namespace CPU {
class LR35902 {
private:
    Bus& bus;
    Reg8 A, B, C, D, E, F, H, L, dummy8;
    Reg16 AF, BC, DE, HL;
    uint16_t SP, PC;
    int wait = 0;
    const int Zidx = 7;
    const int Nidx = 6;
    const int Hidx = 5;
    const int Cidx = 4;

public:
    LR35902(Bus& b);
    int read(const uint16_t& addr, int n = 1) ;
    uint8_t write(uint16_t addr, uint8_t val);
    uint8_t write(uint16_t addr, Reg8& val);
    uint8_t write(Reg16& addr, Reg8& val);
    uint16_t pc(int inc);
    void f(uint8_t ZHNC, uint8_t ZHNCmask, uint8_t on, uint8_t off);
    void setRegisterStateJSON(json& data);
    bool compareRegisterStateJSON(json& final);
    void insExecute();
};
}