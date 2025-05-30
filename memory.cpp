#include "memory.h"

// ROMBlock implementation
ROMBlock::ROMBlock(uint16_t offset, uint16_t size)
    : offset(offset), size(size) {
    if (size > 0x4000)
        throw std::invalid_argument("ROMBlock size exceeds limit");
}

uint8_t ROMBlock::read(uint16_t addr) {
    return data[addr - offset];
}

bool ROMBlock::write(uint16_t addr, uint8_t val) {
    return data[addr - offset] = val;
}

int ROMBlock::getMemtype() {
    return memtype;
}

// RAMBlock implementation
RAMBlock::RAMBlock(uint16_t offset, uint16_t size)
    : offset(offset), size(size) {
    if (size > 0x2000)
        throw std::invalid_argument("RAMBlock size exceeds limit");
}

uint8_t RAMBlock::read(uint16_t addr) {
    return data[addr - offset];
}

bool RAMBlock::write(uint16_t addr, uint8_t val) {
    return data[addr - offset] = val;
}

int RAMBlock::getMemtype() {
    return memtype;
}

// Bus implementation
Bus::Bus() = default;

void Bus::mapRange(uint16_t start, uint16_t end, MemoryDevice* dev) {
    for (int i = start; i <= end; ++i) {
        map[i] = dev;
    }
}

uint32_t Bus::read(uint16_t addr, int n) {
    uint32_t result = 0;
    for (int i = 0; i < n; ++i) {
        uint16_t a = addr + i;
        uint8_t byte = map[a] ? map[a]->read(a) : 0x00;
        result |= uint32_t(byte) << (8 * i);
    }
    return result;
}

void Bus::write(uint16_t addr, uint8_t val) {
    if (auto dev = map[addr])
        dev->write(addr, val);
}

int Bus::getMemtype(uint16_t addr) {
    return map[addr] ? map[addr]->getMemtype() : MEM_TYPE_DNE;
}

bool Bus::isMapFull() {
    for (const auto& dev : map) {
        if (dev == nullptr) return false;
    }
    return true;
}
