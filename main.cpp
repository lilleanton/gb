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
#include "json.hpp"
using json = nlohmann::json;

bool compareCpuState(CPU::LR35902& cpu, json& state) {
    bool match = true;

    auto check = [&](const std::string& name, uint16_t actual, uint16_t expected) {
        if (actual != expected) {
            std::cout << name << " mismatch! Actual: " << actual << ", Expected: " << expected << "\n";
            match = false;
        }
    };

    if(!cpu.compareRegisterStateJSON(state))
        return false;

    for (const auto& ramEntry : state["ram"]) {
        uint16_t addr = ramEntry[0].get<uint16_t>();
        uint8_t expectedVal = ramEntry[1].get<uint8_t>();
        uint8_t actualVal = cpu.read(addr);
        if (actualVal != expectedVal) {
            std::cout << "RAM[" << addr << "] mismatch! Actual: " << +actualVal << ", Expected: " << +expectedVal << "\n";
            match = false;
        }
    }

    return match;
}

void setMachineStateJSON(Bus& bus, CPU::LR35902& cpu, json& state) {
    cpu.setRegisterStateJSON(state);

    for (auto& ramEdit : state["ram"]) {
        cpu.write(ramEdit[0], ramEdit[1]);
    }
}

void testOpcode(Bus& bus, CPU::LR35902& cpu, std::string opcode, int numToTest = 1) {
    std::fstream f(std::format("V1/{}.json", opcode));
    json data = json::parse(f);

    for(int i = 0; i < numToTest; i++) {
        setMachineStateJSON(bus, cpu, data[i]["initial"]);
        cpu.insExecute();

        if(compareCpuState(cpu, data[i]["final"])){
            printf("(%s)-test number %d, success\n", opcode.c_str(), i);
        } else {
            printf("Failed at index %d\n", i);
            break;
        }
    }
}

int main() {
    Bus bus;
    ROMBlock* ROMBank0              = new ROMBlock(0, 0x4000);
    ROMBlock* ROMBankSwitchable0    = new ROMBlock(0, 0x4000);
    RAMBlock* VRAM                  = new RAMBlock(0, 0x2000);
    RAMBlock* RAMBankSwitchable0    = new RAMBlock(0, 0x2000);
    RAMBlock* RAMInternal           = new RAMBlock(0, 0x2000);

    bus.mapRange(0, 0x3fff, ROMBank0);
    bus.mapRange(0x4000, 0x7fff, ROMBankSwitchable0);
    bus.mapRange(0x8000, 0x9fff, VRAM);
    bus.mapRange(0xa000, 0xbfff, RAMBankSwitchable0);
    bus.mapRange(0xc000, 0xdfff, RAMInternal);
    bus.mapRange(0xe000, 0xfd99, RAMInternal); /* Echo RAM */

    CPU::LR35902 core(bus);
    core.read(0x3000);
    core.read(0x5000);

    testOpcode(bus, core, "19", 100);

    delete ROMBank0;
    delete ROMBankSwitchable0;
    delete VRAM;
    delete RAMBankSwitchable0;
    delete RAMInternal;
    printf("Memory succesfully freed.");
    return 0;
}
