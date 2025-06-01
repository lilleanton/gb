#include <stdio.h>
#include <cstdint>
#include <array>
#include <stdexcept>
#include <bitset>
#include <iostream>
#include <fstream>
#include <cmath>

#include "memory/memory.h"
#include "Reg8/Reg8.h"
#include "Reg16/Reg16.h"
#include "LR35902/LR35902.h"
#include "testing/testing.h"
#include "json.hpp"
using json = nlohmann::json;

void printBytesHex(const std::vector<uint8_t>& data) {
    for (uint8_t b : data) {
        std::printf("%02X ", b);
    }
    std::printf("\n");
}

std::vector<uint8_t> loadFile(const std::string& src) {
    std::ifstream f{src, std::ios::binary | std::ios::ate};
    if(!f.is_open()) {
        throw std::runtime_error("Failed to open file: " + src);
    }
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if(!f.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("Failed to read file: " + src);
    }

    return buffer;
}

void insertROM(Bus& bus, const std::string& src, int offset = 0) {
    printf("Loading %s...\n", src.c_str());
    auto ROM = loadFile(src);

    printf("Inserting %s into memory through the bus...\n", src.c_str());
    for (int addr = 0; addr < ROM.size(); ++addr) {
        if(addr <= 0xffff)
            bus.write(addr + offset, ROM[addr]);
    }

    printf("Done!\n");
}

int main() {
    Bus bus;
    ROMBlock* ROMBank0              = new ROMBlock(0x0000, 0x4000);
    ROMBlock* ROMBankSwitchable0    = new ROMBlock(0x4000, 0x4000);
    RAMBlock* VRAM                  = new RAMBlock(0x8000, 0x2000);
    RAMBlock* RAMBankSwitchable0    = new RAMBlock(0xa000, 0x2000);
    RAMBlock* RAMInternal           = new RAMBlock(0xc000, 0x2000);
    RAMBlock* loop                  = new RAMBlock(0xe000, 0x1e00); // TODO: Implement proper echo-ram
    REGBlock* RegisterMem           = new REGBlock(0xfe00, 0x01ff);

    bus.mapRange(0, 0x3fff, ROMBank0);
    bus.mapRange(0x4000, 0x7fff, ROMBankSwitchable0);
    bus.mapRange(0x8000, 0x9fff, VRAM);
    bus.mapRange(0xa000, 0xbfff, RAMBankSwitchable0);
    bus.mapRange(0xc000, 0xdfff, RAMInternal);
    bus.mapRange(0xe000, 0xfdff, loop); /* Echo RAM */
    bus.mapRange(0xfe00, 0xffff, RegisterMem); /* Mostly registers */

    CPU::LR35902 core(bus);

    json postBootState = {
        {"a", 0x01},
        {"f", 0xb0}, 
        {"b", 0x00}, 
        {"c", 0x13}, 
        {"d", 0x00}, 
        {"e", 0xd8}, 
        {"h", 0x01},
        {"l", 0x4d},
        {"sp", 0xfffe},
        {"pc", 0x0100}, 
    };
    core.setRegisterStateJSON(postBootState);

    insertROM(bus, "test_roms/02-interrupts.gb");
    //insertROM(bus, "test_roms/dmg_boot.bin");
    std::ofstream logfile("../gameboy-doctor/log.txt");
    core.streamAppendState(logfile);
    bus.read(0xff04); // Divider Register   DIV
    bus.read(0xff05); // Timer Counter      TIMA
    bus.read(0xff06); // Timer Modulo       TMA
    bus.read(0xff07); // Timer Control      TAC

    /*
    static const uint8_t nintendo_logo[48] = {
        0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
        0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
        0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
        0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
        0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
        0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
    };

    for(int i = 0; i < 48; i++)
        bus.write(0x104 + i, nintendo_logo[i]);
    
    printf("%x\n", bus.read(0x0104));
    */

    long long tcycle = 0;
    long long maxtcycles = 1e6 * 16;
    int TACVariants[] = { 1024, 16, 64, 256 };

    while (tcycle < maxtcycles) {
        if ((tcycle & 0b11) == 0) {
            if (core.insCycle()) {
                core.streamAppendState(logfile);
            }
        }
        tcycle++;

        if ((tcycle & 0xFF) == 0xFF) {
            bus.relativeUpdate(0xFF04, true);
        }

        uint8_t tac = bus.read(0xFF07);
        bool timerEnable = (tac & 0x04) != 0;      // Check TAC.bit2
        int clockSelect = (tac & 0x03);            // TAC bits 1–0
        int period = TACVariants[clockSelect];     // 1024, 16, 64, or 256

        if (timerEnable) {
            if ((tcycle & (period - 1)) == (period - 1)) {
                bool noOverflow = bus.relativeUpdate(0xFF05, true);

                if (!noOverflow) {
                    bus.write(0xFF0F, bus.read(0xFF0F) | 0x04); 
                    bus.write(0xFF05, bus.read(0xFF06));
                    core.halt = false;
                }
            }
        }
    }

    delete ROMBank0;
    delete ROMBankSwitchable0;
    delete VRAM;
    delete RAMBankSwitchable0;
    delete RAMInternal;
    delete RegisterMem;
    printf("Memory freed successfully");

    return 0;
}
