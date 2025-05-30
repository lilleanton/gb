#pragma once

#include <cstdint>
#include <array>
#include <stdexcept>
#include <iostream>

// memory types
enum MemType {
    MEM_TYPE_DNE,
    MEM_TYPE_ROM,
    MEM_TYPE_RAM,
    MEM_TYPE_VRAM
};

// abstract memory device interface
class MemoryDevice {
public:
    virtual uint8_t read(uint16_t addr) = 0;
    virtual bool    write(uint16_t addr, uint8_t val) = 0;
    virtual int     getMemtype() = 0;
    virtual ~MemoryDevice() = default;
};

// ROM block
class ROMBlock : public MemoryDevice {
private:
    uint8_t  data[0x4000];
    uint16_t offset;
    uint16_t size;
    const int memtype = MEM_TYPE_ROM;

public:
    ROMBlock(uint16_t offset, uint16_t size);

    uint8_t read(uint16_t addr) override;
    bool    write(uint16_t addr, uint8_t val) override;
    int     getMemtype() override;
};

// RAM block
class RAMBlock : public MemoryDevice {
private:
    uint8_t  data[0x2000];
    uint16_t offset;
    uint16_t size;
    const int memtype = MEM_TYPE_RAM;

public:
    RAMBlock(uint16_t offset, uint16_t size);

    uint8_t read(uint16_t addr) override;
    bool    write(uint16_t addr, uint8_t val) override;
    int     getMemtype() override;
};

// address bus
class Bus {
private:
    std::array<MemoryDevice*, 0x10000> map{};

public:
    Bus();

    void        mapRange(uint16_t start, uint16_t end, MemoryDevice* dev);
    uint32_t    read(uint16_t addr, int n = 1);
    void        write(uint16_t addr, uint8_t val);
    int         getMemtype(uint16_t addr);
    bool        isMapFull();
};
