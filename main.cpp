#include <stdio.h>
#include <cstdint>
#include <array>
#include <stdexcept>
#include <bitset>

enum {
    MEM_TYPE_DNE,
    MEM_TYPE_ROM,
    MEM_TYPE_RAM,
    MEM_TYPE_VRAM
};

class MemoryDevice {
public:
    virtual uint8_t read(uint16_t addr) = 0;
    virtual bool write(uint16_t addr, uint8_t val) = 0;
    virtual int getMemtype() = 0;
    virtual ~MemoryDevice() = default;
};

class ROMBlock : public MemoryDevice {
private:
    uint8_t data[0x4000];
    uint16_t offset;
    uint16_t size;
    const int memtype = MEM_TYPE_ROM;
public:
    ROMBlock(uint16_t offset, uint16_t size) : offset(offset), size(size) {
        if (size > 0x4000)
            throw std::invalid_argument("ROMBlock size exceeds limit");
    }

    uint8_t read(uint16_t addr) override {
        return data[addr - this->offset]; 
    }

    bool write(uint16_t addr, uint8_t val) override {
        return false;   
    }

    int getMemtype() {
        return this->memtype;
    }
};

class RAMBlock : public MemoryDevice {
private:
    uint8_t data[0x2000];
    uint16_t offset;
    uint16_t size;
    const int memtype = MEM_TYPE_RAM;
public:
    RAMBlock(uint16_t offset, uint16_t size) : offset(offset), size(size) {
        if (size > 0x4000)
            throw std::invalid_argument("RAMBlock size exceeds limit");
    }

    uint8_t read(uint16_t addr) override {
        return data[addr - this->offset];
    }

    bool write(uint16_t addr, uint8_t val) override {
        data[addr - this->offset] = val;
        return true;
    }

    int getMemtype() override {
        return this->memtype;
    }
};

class Bus {
private:
    std::array<MemoryDevice*, 0x10000> map = {nullptr};
public:
    Bus() {}

    void mapRange(uint16_t start, uint16_t end, MemoryDevice* dev) {
        for (uint16_t i = start; i <= end; ++i)
            map[i] = dev;
    }

    uint32_t read(uint16_t addr, int n = 1) {
        uint32_t result = 0;
        for (int i = 0; i < n; ++i) {
            uint16_t currentAddr = addr + i;
            uint8_t byte = map[currentAddr] ? map[currentAddr]->read(currentAddr) : 0x00;
            result |= static_cast<uint32_t>(byte) << (8 * i);
        }
        return result;
    }

    void write(uint16_t addr, uint8_t val) {
        MemoryDevice* dev = map[addr];
        if (dev) dev->write(addr, val);
    }

    int getMemtype(uint16_t addr) {
        MemoryDevice* dev = map[addr];
        return dev ? dev->getMemtype() : MEM_TYPE_DNE;
    }
};

namespace CPU {
    class Reg8 {
    private:
        uint8_t val;
    public:
        Reg8(uint8_t v = 0) : val(v) {}

        uint8_t getVal() const {
            return val;
        }

        void operator=(const Reg8& other) { val = other.val; }
        void operator=(const int& other) { val = uint8_t(other); }

        uint8_t operator+(const Reg8& other) const { return uint8_t(val + other.val); }
        uint8_t operator-(const Reg8& other) const { return uint8_t(val - other.val); }

        // 8-bit addition: updates val and returns flag byte (Z=N=H=C flags)
        // Flags: Z=bit7, N=bit6, H=bit5, C=bit4
        uint8_t operator+=(const Reg8& other) {
            uint8_t a = val;
            uint8_t b = other.val;
            uint16_t sum = uint16_t(a) + uint16_t(b);
            uint8_t res = uint8_t(sum);

            uint8_t flags = 0;
            if (res == 0)               flags |= 0x80; // Z
            // N = 0 for addition
            if (((a & 0xF) + (b & 0xF)) > 0xF) flags |= 0x20; // H
            if (sum > 0xFF)                flags |= 0x10; // C

            val = res;
            return flags;
        }

        uint8_t operator+=(const int& other) {
            return *this += Reg8(uint8_t(other));
        }

        // 8-bit subtraction: updates val and returns flag byte
        uint8_t operator-=(const Reg8& other) {
            uint8_t a = val;
            uint8_t b = other.val;
            uint8_t res = uint8_t(a - b);

            uint8_t flags = 0;
            if (res == 0)           flags |= 0x80; // Z
            flags |= 0x40;          // N
            if ((a & 0xF) < (b & 0xF)) flags |= 0x20; // H
            if (a < b)               flags |= 0x10; // C

            val = res;
            return flags;
        }

        uint8_t operator-=(const int& other) {
            return *this -= Reg8(uint8_t(other));
        }

        // Rotation operations
        uint8_t RLC() {
            bool carry = (val & 0x80) != 0;
            val = uint8_t((val << 1) | (carry ? 1 : 0));

            uint8_t flags = 0;
            if (val == 0)    flags |= 0x80; // Z
            if (carry)       flags |= 0x10; // C
            return flags;
        }
        uint8_t RRC() {
            bool carry = (val & 1) != 0;
            val = uint8_t((val >> 1) | (carry ? 0x80 : 0));

            uint8_t flags = 0;
            if (val == 0)    flags |= 0x80; // Z
            if (carry)       flags |= 0x10; // C
            return flags;
        }
        uint8_t RL(bool carry_in) {
            bool carry = (val & 0x80) != 0;
            val = uint8_t((val << 1) | (carry_in ? 1 : 0));

            uint8_t flags = 0;
            if (val == 0)    flags |= 0x80; // Z
            if (carry)       flags |= 0x10; // C
            return flags;
        }

        uint8_t RR(bool carry_in) {
            bool carry = (val & 1) != 0;
            val = uint8_t((val >> 1) | (carry_in ? 0x80 : 0));

            uint8_t flags = 0;
            if (val == 0)    flags |= 0x80; // Z
            if (carry)       flags |= 0x10; // C
            return flags;
        }

        // Shift operations
        uint8_t SRA() {
            bool carry = this->val & 1;
            bool sig = this->val & 0x80;
            this->val = (this->val >> 1) | (sig ? 0x80 : 0x00);
            return ((this->val != 0) << 7) | carry << 4; 
        }
        uint8_t SRL() {
            bool carry = this->val & 1;
            this->val = (this->val >> 1);
            return ((this->val != 0) << 7) | carry << 4; 
        }
        uint8_t SLA() {
            bool carry = (this->val & 0x80) >> 7;
            this->val = (this->val << 1);
            return ((this->val != 0) << 7) | carry << 4; 
        }
    };

    class Reg16 {
    private:
        Reg8* HI;
        Reg8* LO;
    public:
        Reg16(Reg8& H, Reg8& L) : HI(&H), LO(&L) {}

        uint16_t getVal() const {
            return (uint16_t(HI->getVal()) << 8) | LO->getVal();
        }

        void operator=(const Reg16& other) {
            uint16_t otherVal = other.getVal();
            *HI = uint8_t((otherVal & 0xFF00) >> 8);
            *LO = uint8_t(otherVal & 0x00FF);
        }
        void operator=(const uint16_t& other) {
            *HI = uint8_t((other & 0xFF00) >> 8);
            *LO = uint8_t(other & 0x00FF);
        }

        // 16-bit addition (ADD HL, r16): updates and returns flag byte (only H and C)
        // Flags: H=bit5, C=bit4 (Z unaffected, N=0)
        uint8_t operator+=(const Reg16& other) {
            uint16_t a = getVal();
            uint16_t b = other.getVal();
            uint32_t sum = uint32_t(a) + uint32_t(b);

            uint8_t flags = 0;
            // Z flag: unchanged
            // N = 0
            if (((a & 0x0FFF) + (b & 0x0FFF)) > 0x0FFF) flags |= 0x20; // H
            if (sum > 0xFFFF)                       flags |= 0x10; // C

            uint16_t res = uint16_t(sum);
            *HI = uint8_t((res & 0xFF00) >> 8);
            *LO = uint8_t(res & 0x00FF);
            return flags;
        }

        uint8_t operator+=(const uint16_t& other) {
            uint16_t a = getVal();
            uint32_t sum = uint32_t(a) + uint32_t(other);

            uint8_t flags = 0;
            // Z flag: unchanged
            // N = 0
            if (((a & 0x0FFF) + (other & 0x0FFF)) > 0x0FFF) flags |= 0x20; // H
            if (sum > 0xFFFF)            flags |= 0x10; // C

            uint16_t res = uint16_t(sum);
            *HI = uint8_t((res & 0xFF00) >> 8);
            *LO = uint8_t(res & 0x00FF);
            return flags;
        }

        void operator++() {
            *this = uint16_t(this->getVal() + 1);
        }

        void operator--() {
            *this = uint16_t(this->getVal() - 1);
        }
    };

    class LR35902 {
    private:
        Bus& bus;
        Reg8 A, B, C, D, E, F, H, L;
        Reg16 AF, BC, DE, HL;
        uint16_t SP, PC;
        int wait = 0;
    public:
        LR35902(Bus& b) : bus(b), AF(A, F), BC(B, C), DE(D, E), HL(H, L), SP(0), PC(0) {}
        int read(const uint16_t& addr, int n = 1) {
            const int memtype = bus.getMemtype(addr);
            switch(memtype) {
                case MEM_TYPE_RAM:
                    return bus.read(addr, n);
                case MEM_TYPE_ROM:
                    return bus.read(addr, n);
                default:
                printf("Memtype does not exist\n");
            }
            return this->bus.read(addr, n);
        }

        uint8_t write(uint16_t addr, uint8_t val) {
            this->bus.write(addr, val);
        }

        uint8_t write(Reg16& addr, Reg8& val) {
            this->bus.write(addr.getVal(), val.getVal());
        }

        uint16_t pc(int inc) {
            uint16_t old = this->PC;
            this->PC += inc;
            return old;
        }

        // Set flag
        void f(uint8_t ZHNC, uint8_t ZHNCmask, uint8_t on, uint8_t off) {
            uint8_t oldBits = (this->F.getVal() >> 4) & 0xF;
            oldBits = (oldBits | ((ZHNC>>4) & ZHNCmask)) & ~(~(ZHNC>>4) & ZHNCmask);
            F       = (oldBits | on) & ~off;
        }

        void insExecute() {
            if(!wait--)
                return;

            uint8_t opcode = read(PC++);
            switch(opcode) {
                case 0x00:                                          break; /* NOP */
                case 0x01:  BC = read(pc(2), 2);                    break; /* LD BC, n16.  3 12.   ---- */
                case 0x02:  write(BC, A);                           break; /* LD [BC], A.  1 8.    ---- */
                case 0x03:  ++BC;                                   break; /*  */
                case 0x04:  f(B += 1, 0b1010, 0b0000, 0b0100);      break; /* INC B        1 4     Z0H- */
                case 0x05:  f(B -= 1, 0b1010, 0b0100, 0b0000);      break; /*  */
                case 0x06:  B = bus.read(pc(1));                    break; /*  */
                case 0x07:  f(A.RLC(), 0b0001, 0b0000, 0b1110);     break; /*  */
                case 0x08: {
                    uint16_t addr = read(pc(2), 2);
                    write(addr, SP & 0xff);
                    write(addr + 1, (SP >> 8) & 0xff);
                    break; /*  */
                }
                case 0x09:  f(HL += BC, 0b0011, 0b0000, 0b0100);    break; /*  */
                case 0x0a:  A = read(BC.getVal());                  break; /*  */
                case 0x0b:  --BC;                                   break; /*  */
                case 0x0c:  f(B += 1, 0b1010, 0b0000, 0b0100);      break; /*  */
                case 0x0d:  f(B -= 1, 0b1010, 0b0100, 0b0000);      break; /*  */
                case 0x0e:  C = read(pc(1));                        break; /*  */
                case 0x0f:  f(A.RRC(), 0b0001, 0b0000, 0b1110);     break; /*  */

                default: printf("Unknown opcode - %d\n", opcode);   break; /* Unknown opcode */
            }
        }
    };

};

int main() {
    Bus bus;
    RAMBlock* ram = new RAMBlock(0, 0x4000);
    bus.mapRange(0, 0x4000, ram);
    CPU::LR35902 core(bus);
    core.read(0x5000);

    delete ram;
    printf("Memory succesfully freed");
    return 0;
}
