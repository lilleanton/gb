#pragma once
#include <format>
#include <fstream>

#include "../memory/memory.h"
#include "../Reg8/Reg8.h"
#include "../Reg16/Reg16.h"
#include "../LR35902/LR35902.h"

namespace Testing {

bool compareCpuState(CPU::LR35902& cpu, json& state);
void setMachineStateJSON(Bus& bus, CPU::LR35902& cpu, json& state);
void testOpcode(Bus& bus, CPU::LR35902& cpu, std::string opcode, int numToTest = 1);
void test1byteOpcodes(Bus& bus, CPU::LR35902& cpu);
void testCBopcodes(Bus& bus, CPU::LR35902& cpu);

};
