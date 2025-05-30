#include <stdio.h>
#include <cstdint>
#include <array>
#include <stdexcept>
#include <bitset>
#include <fstream>
#include <format>
#include <iostream>

#include "memory.h"
#include "Reg8.h"
#include "Reg16.h"
#include "LR35902.h"
#include "testing.h"
#include "json.hpp"
using json = nlohmann::json;

int main() {
    Bus bus;
    ROMBlock* ROMBank0              = new ROMBlock(0x0000, 0x4000);
    ROMBlock* ROMBankSwitchable0    = new ROMBlock(0x4000, 0x4000);
    RAMBlock* VRAM                  = new RAMBlock(0x8000, 0x2000);
    RAMBlock* RAMBankSwitchable0    = new RAMBlock(0xa000, 0x2000);
    RAMBlock* RAMInternal           = new RAMBlock(0xc000, 0x2000);
    RAMBlock* loop                  = new RAMBlock(0xe000, 0x1e00); // TODO: Implement proper echo-ram
    RAMBlock* RegisterMem           = new RAMBlock(0xfe00, 0x01ff);

    bus.mapRange(0, 0x3fff, ROMBank0);
    bus.mapRange(0x4000, 0x7fff, ROMBankSwitchable0);
    bus.mapRange(0x8000, 0x9fff, VRAM);
    bus.mapRange(0xa000, 0xbfff, RAMBankSwitchable0);
    bus.mapRange(0xc000, 0xdfff, RAMInternal);
    bus.mapRange(0xe000, 0xfdff, loop); /* Echo RAM */
    bus.mapRange(0xfe00, 0xffff, RegisterMem); /* Mostly registers */

    CPU::LR35902 core(bus);

    // test1byteOpcodes(bus, core);
    testCBopcodes(bus, core);

    delete ROMBank0;
    delete ROMBankSwitchable0;
    delete VRAM;
    delete RAMBankSwitchable0;
    delete RAMInternal;
    delete RegisterMem;
    printf("Memory succesfully freed.");

    return 0;
}
