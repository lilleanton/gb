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

        bool getBit(int idx) {
            return (val >> idx) & 1;
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

        // Bit operations, only need to update the Z flag
        uint8_t operator&=(const Reg8& other) { val &= other.val; return (val >> 7) & 0x1; }
        uint8_t operator^=(const Reg8& other) { val ^= other.val; return (val >> 7) & 0x1; }
        uint8_t operator|=(const Reg8& other) { val |= other.val; return (val >> 7) & 0x1; }
        uint8_t operator&=(const uint8_t& other) { val &= other; return (val >> 7) & 0x1; }
        uint8_t operator^=(const uint8_t& other) { val ^= other; return (val >> 7) & 0x1; }
        uint8_t operator|=(const uint8_t& other) { val |= other; return (val >> 7) & 0x1; }

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

        uint16_t operator++() {
            *this = uint16_t(this->getVal() + 1);
            return this->getVal();
        }

        uint16_t operator--() {
            *this = uint16_t(this->getVal() - 1);
            return this->getVal();
        }

        uint16_t operator++(int) {
            uint16_t oldVal = this->getVal();
            *this = uint16_t(this->getVal() + 1);
            return oldVal;
        }

        uint16_t operator--(int) {
            uint16_t oldVal = this->getVal();
            *this = uint16_t(this->getVal() - 1);
            return oldVal;
        }
    };

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

        uint8_t write(uint16_t addr, Reg8& val) {
            this->bus.write(addr, val.getVal());
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

            // TODO: Fix/check all d16 and a16 instructions
            uint8_t opcode = read(PC++);
            switch(opcode) {
                /* 0x00 through 0x3f, misc.*/
                /* 0x00 - 0x0f */
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
                    break;
                }
                case 0x09:  f(HL += BC, 0b0011, 0b0000, 0b0100);    break; /*  */
                case 0x0a:  A = read(BC.getVal());                  break; /*  */
                case 0x0b:  --BC;                                   break; /*  */
                case 0x0c:  f(B += 1, 0b1010, 0b0000, 0b0100);      break; /*  */
                case 0x0d:  f(B -= 1, 0b1010, 0b0100, 0b0000);      break; /*  */
                case 0x0e:  C = read(pc(1));                        break; /*  */
                case 0x0f:  f(A.RRC(), 0b0001, 0b0000, 0b1110);     break; /*  */

                /* 0x10 - 0x1f */
                case 0x10: {                                        break;} /*  */
                case 0x11: {DE = read(pc(2), 2);                    break;} /*  */
                case 0x12: {write(DE, A);                           break;} /*  */
                case 0x13: {++DE;                                   break;} /*  */
                case 0x14: {f(D += 1, 0xA, 0x0, 0x4);               break;} /*  */
                case 0x15: {f(D -= 1, 0xA, 0x4, 0x0);               break;} /*  */
                case 0x16: {D = read(pc(1));                        break;} /*  */
                case 0x17: {f(A.RL(F.getBit(Cidx)), 0x1, 0x0, 0xE); break;} /*  */
                case 0x18: {PC += signed(read(pc(1)));              break;} /*  */
                case 0x19: {f(HL += DE, 0x3, 0x0, 0x4);             break;} /*  */
                case 0x1a: {A = read(DE.getVal());                  break;} /*  */
                case 0x1b: {--DE;                                   break;} /*  */
                case 0x1c: {f(E += 1, 0xA, 0x0, 0x4);               break;} /*  */
                case 0x1d: {f(E -= 1, 0xA, 0x4, 0x0);               break;} /*  */
                case 0x1e: {E = read(pc(1));                        break;} /*  */
                case 0x1f: {f(A.RR(F.getBit(Cidx)), 0x1, 0x0, 0xE); break;} /*  */

                /* 0x20 - 0x2f */
                case 0x20: {
                    if(!F.getBit(Zidx)){
                        PC += signed(read(pc(1)));
                    }
                    break;
                }
                case 0x21: {HL = read(pc(2), 2);                    break;} /*  */
                case 0x22: {write(HL++, A);                         break;} /*  */
                case 0x23: {++HL;                                   break;} /*  */
                case 0x24: {f(H += 1, 0xA, 0x0, 0x4);               break;} /*  */
                case 0x25: {f(H -= 1, 0xA, 0x4, 0x0);               break;} /*  */
                case 0x26: {H = read(pc(1));                        break;} /*  */
                case 0x27: {                                        break;} /* DAA, svår */
                case 0x28: {
                    if(F.getBit(Zidx)){
                        PC += signed(read(pc(1)));
                    }
                    break;
                }
                case 0x29: {f(HL += HL, 0x3, 0x0, 0x4);                 break;} /*  */
                case 0x2a: {A = read(HL++);                             break;} /*  */
                case 0x2b: {--HL;                                       break;} /*  */
                case 0x2c: {f(L += 1, 0xA, 0x0, 0x4);                   break;} /*  */
                case 0x2d: {f(L -= 1, 0xA, 0x4, 0x0);                   break;} /*  */
                case 0x2e: {L = read(pc(1));                            break;} /*  */
                case 0x2f: { A = ~A.getVal(); f(0x0, 0x0, 0x6, 0x0);    break;} /*  */

                /* 0x30 - 0x3f */
                case 0x30: {
                    if(!F.getBit(Cidx)){
                        PC += signed(read(pc(1)));
                    }
                    break;
                }
                case 0x31: {SP = read(pc(2), 2);                    break;} /*  */
                case 0x32: {write(HL--, A);                         break;} /*  */
                case 0x33: {++SP;                                   break;} /*  */
                case 0x34: {
                    dummy8 = read(HL.getVal());
                    write(HL.getVal(), dummy8 + 1);
                    f(dummy8 += 1, 0xA, 0x0, 0x4);
                    break;
                }
                case 0x35: {
                    dummy8 = read(HL.getVal());
                    write(HL.getVal(), dummy8 - 1);
                    f(dummy8 -= 1, 0xA, 0x0, 0x4);
                    break;
                }
                case 0x36: {write(HL.getVal(), read(pc(1)));        break;} /*  */
                case 0x37: {f(0b0000, 0b0000, 0b0001, 0b0110);      break;} /* SCF, -001 */
                case 0x38: {
                    if(F.getBit(Cidx)){
                        PC += signed(read(pc(1)));
                    }
                    break;
                }
                case 0x39: { f(HL += SP, 0x3, 0x0, 0x4);                    break;} /*  */
                case 0x3a: { A = read(HL--);                                break;} /*  */
                case 0x3b: { --HL;                                          break;} /*  */
                case 0x3c: { f(A += 1, 0xA, 0x0, 0x4);                      break;} /*  */
                case 0x3d: { f(A -= 1, 0xA, 0x4, 0x0);                      break;} /*  */
                case 0x3e: { A = read(pc(1));                               break;} /*  */
                case 0x3f: { f(0x0, 0x0, 0x0, 0x6); F = F.getVal() ^ 0x10; break;} /* CCF - toggle the C bit */

                /* 0x40 through 0x7f, predominantly load instructions*/
                /* 0x40 - 0x4f */
                case 0x40: { B = B; break;} /*  */
                case 0x41: { B = C; break;} /*  */
                case 0x42: { B = D; break;} /*  */
                case 0x43: { B = E; break;} /*  */
                case 0x44: { B = H; break;} /*  */
                case 0x45: { B = L; break;} /*  */
                case 0x46: { B = read(HL.getVal()); break;} /*  */
                case 0x47: { B = A; break;} /*  */
                case 0x48: { C = B; break;} /*  */
                case 0x49: { C = C; break;} /*  */
                case 0x4a: { C = D; break;} /*  */
                case 0x4b: { C = E; break;} /*  */
                case 0x4c: { C = H; break;} /*  */
                case 0x4d: { C = L; break;} /*  */
                case 0x4e: { C = read(HL.getVal()); break;} /*  */
                case 0x4f: { C = A; break;} /*  */

                /* 0x50 - 0x5f */
                case 0x50: { D = B; break;} /*  */
                case 0x51: { D = C; break;} /*  */
                case 0x52: { D = D; break;} /*  */
                case 0x53: { D = E; break;} /*  */
                case 0x54: { D = H; break;} /*  */
                case 0x55: { D = L; break;} /*  */
                case 0x56: { D = read(HL.getVal()); break;} /*  */
                case 0x57: { D = A; break;} /*  */
                case 0x58: { E = B; break;} /*  */
                case 0x59: { E = C; break;} /*  */
                case 0x5a: { E = D; break;} /*  */
                case 0x5b: { E = E; break;} /*  */
                case 0x5c: { E = H; break;} /*  */
                case 0x5d: { E = L; break;} /*  */
                case 0x5e: { E = read(HL.getVal()); break;} /*  */
                case 0x5f: { E = A; break;} /*  */

                /* 0x60 - 0x6f */
                case 0x60: { H = B; break;} /*  */
                case 0x61: { H = C; break;} /*  */
                case 0x62: { H = D; break;} /*  */
                case 0x63: { H = E; break;} /*  */
                case 0x64: { H = H; break;} /*  */
                case 0x65: { H = L; break;} /*  */
                case 0x66: { H = read(HL.getVal()); break;} /*  */
                case 0x67: { H = A; break;} /*  */
                case 0x68: { L = B; break;} /*  */
                case 0x69: { L = C; break;} /*  */
                case 0x6a: { L = D; break;} /*  */
                case 0x6b: { L = E; break;} /*  */
                case 0x6c: { L = H; break;} /*  */
                case 0x6d: { L = L; break;} /*  */
                case 0x6e: { L = read(HL.getVal()); break;} /*  */
                case 0x6f: { L = A; break;} /*  */

                /* 0x70 - 0x7f */
                case 0x70: { write(HL, B); break;} /*  */
                case 0x71: { write(HL, C); break;} /*  */
                case 0x72: { write(HL, D); break;} /*  */
                case 0x73: { write(HL, E); break;} /*  */
                case 0x74: { write(HL, H); break;} /*  */
                case 0x75: { write(HL, L); break;} /*  */
                case 0x76: {                                                                        break;} /* HALT */
                case 0x77: { write(HL, A); break;} /*  */
                case 0x78: { A = B; break;} /*  */
                case 0x79: { A = C; break;} /*  */
                case 0x7a: { A = D; break;} /*  */
                case 0x7b: { A = E; break;} /*  */
                case 0x7c: { A = H; break;} /*  */
                case 0x7d: { A = L; break;} /*  */
                case 0x7e: { A = read(HL.getVal()); break;} /*  */
                case 0x7f: { A = A; break;} /*  */

                /* 0x80 through 0xbf, ADD, SUB, AND, OR, ADC, SBC, XOR, CP */
                /* 0x80 - 0x8f, ADD & ADC */
                case 0x80: { f(A += B, 0b1011, 0b0000, 0b0100);                                     break;} /* ADD */
                case 0x81: { f(A += C, 0b1011, 0b0000, 0b0100);                                     break;} /* ... */
                case 0x82: { f(A += D, 0b1011, 0b0000, 0b0100);                                     break;} /*  */
                case 0x83: { f(A += E, 0b1011, 0b0000, 0b0100);                                     break;} /*  */
                case 0x84: { f(A += H, 0b1011, 0b0000, 0b0100);                                     break;} /*  */
                case 0x85: { f(A += L, 0b1011, 0b0000, 0b0100);                                     break;} /*  */
                case 0x86: { f(A += read(HL.getVal()), 0b1011, 0b0000, 0b0100);                     break;} /*  */
                case 0x87: { f(A += A, 0b1011, 0b0000, 0b0100);                                     break;} /*  */
                case 0x88: { f(A += (B + F.getBit(Cidx)), 0b1011, 0b0000, 0b0100);                  break;} /* ADC */
                case 0x89: { f(A += (C + F.getBit(Cidx)), 0b1011, 0b0000, 0b0100);                  break;} /* ... */
                case 0x8a: { f(A += (D + F.getBit(Cidx)), 0b1011, 0b0000, 0b0100);                  break;} /*  */
                case 0x8b: { f(A += (E + F.getBit(Cidx)), 0b1011, 0b0000, 0b0100);                  break;} /*  */
                case 0x8c: { f(A += (H + F.getBit(Cidx)), 0b1011, 0b0000, 0b0100);                  break;} /*  */
                case 0x8d: { f(A += (L + F.getBit(Cidx)), 0b1011, 0b0000, 0b0100);                  break;} /*  */
                case 0x8e: { f(A += (read(HL.getVal()) + F.getBit(Cidx)), 0b1011, 0b0000, 0b0100);  break;} /*  */
                case 0x8f: { f(A += (A + F.getBit(Cidx)), 0b1011, 0b0000, 0b0100);                  break;} /*  */

                /* 0x90 - 0x9f, SUB & SBC */
                case 0x90: { f(A -= B, 0b1011, 0b1000, 0b0000);                                     break;} /* SUB */
                case 0x91: { f(A -= C, 0b1011, 0b1000, 0b0000);                                     break;} /* ... */
                case 0x92: { f(A -= D, 0b1011, 0b1000, 0b0000);                                     break;} /*  */
                case 0x93: { f(A -= E, 0b1011, 0b1000, 0b0000);                                     break;} /*  */
                case 0x94: { f(A -= H, 0b1011, 0b1000, 0b0000);                                     break;} /*  */
                case 0x95: { f(A -= L, 0b1011, 0b1000, 0b0000);                                     break;} /*  */
                case 0x96: { f(A -= read(HL.getVal()), 0b1011, 0b1000, 0b0000);                     break;} /*  */
                case 0x97: { f(A -= A, 0b1011, 0b1000, 0b0000);                                     break;} /*  */
                case 0x98: { f(A -= (B - F.getBit(Cidx)), 0b1011, 0b1000, 0b0000);                  break;} /* SBC */
                case 0x99: { f(A -= (C - F.getBit(Cidx)), 0b1011, 0b1000, 0b0000);                  break;} /* ... */
                case 0x9a: { f(A -= (D - F.getBit(Cidx)), 0b1011, 0b1000, 0b0000);                  break;} /*  */
                case 0x9b: { f(A -= (E - F.getBit(Cidx)), 0b1011, 0b1000, 0b0000);                  break;} /*  */
                case 0x9c: { f(A -= (H - F.getBit(Cidx)), 0b1011, 0b1000, 0b0000);                  break;} /*  */
                case 0x9d: { f(A -= (L - F.getBit(Cidx)), 0b1011, 0b1000, 0b0000);                  break;} /*  */
                case 0x9e: { f(A -= (read(HL.getVal()) - F.getBit(Cidx)), 0b1011, 0b1000, 0b0000);  break;} /*  */
                case 0x9f: { f(A -= (A - F.getBit(Cidx)), 0b1011, 0b1000, 0b0000);                  break;} /*  */

                /* 0xa0 - 0xaf, AND & XOR */
                case 0xa0: { f(A &= B, 0b1000, 0b0010, 0b0101);                 break;} /* AND */
                case 0xa1: { f(A &= C, 0b1000, 0b0010, 0b0101);                 break;} /*  */
                case 0xa2: { f(A &= D, 0b1000, 0b0010, 0b0101);                 break;} /*  */
                case 0xa3: { f(A &= E, 0b1000, 0b0010, 0b0101);                 break;} /*  */
                case 0xa4: { f(A &= H, 0b1000, 0b0010, 0b0101);                 break;} /*  */
                case 0xa5: { f(A &= L, 0b1000, 0b0010, 0b0101);                 break;} /*  */
                case 0xa6: { f(A &= read(HL.getVal()), 0b1000, 0b0010, 0b0101); break;} /*  */
                case 0xa7: { f(A &= A, 0b1000, 0b0010, 0b0101);                 break;} /*  */
                case 0xa8: { f(A ^= B, 0b1000, 0b0000, 0b0111);                 break;} /* XOR */
                case 0xa9: { f(A ^= C, 0b1000, 0b0000, 0b0111);                 break;} /*  */
                case 0xaa: { f(A ^= D, 0b1000, 0b0000, 0b0111);                 break;} /*  */
                case 0xab: { f(A ^= E, 0b1000, 0b0000, 0b0111);                 break;} /*  */
                case 0xac: { f(A ^= H, 0b1000, 0b0000, 0b0111);                 break;} /*  */
                case 0xad: { f(A ^= L, 0b1000, 0b0000, 0b0111);                 break;} /*  */
                case 0xae: { f(A ^= read(HL.getVal()), 0b1000, 0b0000, 0b0111); break;} /*  */
                case 0xaf: { f(A ^= A, 0b1000, 0b0000, 0b0111);                 break;} /*  */

                /* 0xb0 - 0xbf, OR & CP */
                case 0xb0: { f(A |= B, 0b1000, 0b0000, 0b0111);                 break;} /* OR */
                case 0xb1: { f(A |= C, 0b1000, 0b0000, 0b0111);                 break;} /*  */
                case 0xb2: { f(A |= D, 0b1000, 0b0000, 0b0111);                 break;} /*  */
                case 0xb3: { f(A |= E, 0b1000, 0b0000, 0b0111);                 break;} /*  */
                case 0xb4: { f(A |= H, 0b1000, 0b0000, 0b0111);                 break;} /*  */
                case 0xb5: { f(A |= L, 0b1000, 0b0000, 0b0111);                 break;} /*  */
                case 0xb6: { f(A |= read(HL.getVal()), 0b1000, 0b0000, 0b0111); break;} /*  */
                case 0xb7: { f(A |= A, 0b1000, 0b0000, 0b0111);                 break;} /*  */
                case 0xb8: { uint8_t store = A.getVal(); f(A -= B, 0b1011, 0b0100, 0b0000); A = store;                  break;} /* CP */
                case 0xb9: { uint8_t store = A.getVal(); f(A -= C, 0b1011, 0b0100, 0b0000); A = store;                  break;} /*  */
                case 0xba: { uint8_t store = A.getVal(); f(A -= D, 0b1011, 0b0100, 0b0000); A = store;                  break;} /*  */
                case 0xbb: { uint8_t store = A.getVal(); f(A -= E, 0b1011, 0b0100, 0b0000); A = store;                  break;} /*  */
                case 0xbc: { uint8_t store = A.getVal(); f(A -= H, 0b1011, 0b0100, 0b0000); A = store;                  break;} /*  */
                case 0xbd: { uint8_t store = A.getVal(); f(A -= L, 0b1011, 0b0100, 0b0000); A = store;                  break;} /*  */
                case 0xbe: { uint8_t store = A.getVal(); f(A -= read(HL.getVal()), 0b1011, 0b0100, 0b0000); A = store;  break;} /*  */
                case 0xbf: { uint8_t store = A.getVal(); f(A -= A, 0b1011, 0b0100, 0b0000); A = store;                  break;} /*  */

                /* 0xc0 through 0xff, mostly stack management */
                /* 0xc0 - 0xcf */
                case 0xc0: {
                    if(!F.getBit(Zidx)) {
                        PC = (read(SP + 1) << 8) | read(SP);
                        SP += 2;
                    }
                    break;
                }
                case 0xc1: { BC = (read(SP + 1) << 8) | read(SP); SP += 2; break;}
                case 0xc2: {
                    uint8_t lo = read(PC);
                    uint8_t hi = read(PC + 1);
                    if (!F.getBit(Zidx)) {
                        PC = (hi << 8) | lo;
                    } else {
                        PC += 2;
                    }
                    break;
                }
                case 0xc3: {
                    uint8_t lo = read(PC);
                    uint8_t hi = read(PC + 1);
                    PC = (hi << 8) | lo;
                    break;
                }
                case 0xc4: {
                    if (!F.getBit(Zidx)) {
                        // retAddr = PC + 2 (two immediate bytes)
                        uint16_t ret = PC + 2;
                        write(--SP, (ret >> 8) & 0xff);
                        write(--SP, ret & 0xff);

                        uint8_t lo = read(PC);
                        uint8_t hi = read(PC + 1);
                        PC = (hi << 8) | lo;
                    } else {
                        PC += 2;
                    }
                    break;
                }
                case 0xc5: { write(--SP, B); write(--SP, C); break; }
                case 0xc6: { f(A += read(pc(1)), 0b1011, 0b0000, 0b0100); break;} /* ADD */
                case 0xc7: {
                    write(--SP, (PC >> 8) & 0xff);
                    write(--SP, PC & 0xff);
                    PC = 0x0000;
                    break;
                }
                case 0xc8: {
                    if(F.getBit(Zidx)) {
                        PC = (read(SP + 1) << 8) | read(SP);
                        SP += 2;
                    }
                    break;
                }
                case 0xc9: {
                    PC = (read(SP + 1) << 8) | read(SP);
                    SP += 2;
                    break;
                }
                case 0xca: {
                    uint8_t lo = read(PC);
                    uint8_t hi = read(PC + 1);
                    if (F.getBit(Zidx)) {
                        PC = (hi << 8) | lo;
                    } else {
                        PC += 2;
                    }
                    break;
                }
                case 0xcb: { break; } /* TODO - PREFIX CB */
                case 0xcc: {
                    if (F.getBit(Zidx)) {
                        // retAddr = PC + 2 (two immediate bytes)
                        uint16_t ret = PC + 2;
                        write(--SP, (ret >> 8) & 0xff);
                        write(--SP, ret & 0xff);

                        uint8_t lo = read(PC);
                        uint8_t hi = read(PC + 1);
                        PC = (hi << 8) | lo;
                    } else {
                        PC += 2;
                    }
                    break;
                };
                case 0xcd: {
                        // retAddr = PC + 2 (two immediate bytes)
                        uint16_t ret = PC + 2;
                        write(--SP, (ret >> 8) & 0xff);
                        write(--SP, ret & 0xff);

                        uint8_t lo = read(PC);
                        uint8_t hi = read(PC + 1);
                        PC = (hi << 8) | lo;
                }
                case 0xce: { f(A += (read(pc(1)) + F.getBit(Cidx)), 0b1011, 0b0000, 0b0100);                  break;} /* ADC */
                case 0xcf: {
                    write(--SP, (PC >> 8) & 0xff);
                    write(--SP, PC & 0xff);
                    PC = 0x0008;
                    break;
                }

                /* 0xd0 - 0xdf */
                case 0xd0: {
                    if(!F.getBit(Cidx)) {
                        PC = (read(SP + 1) << 8) | read(SP);
                        SP += 2;
                    }
                    break;
                }
                case 0xd1: { DE = (read(SP + 1) << 8) | read(SP); SP += 2; break;}
                case 0xd2: {
                    uint8_t lo = read(PC);
                    uint8_t hi = read(PC + 1);
                    if (!F.getBit(Cidx)) {
                        PC = (hi << 8) | lo;
                    } else {
                        PC += 2;
                    }
                    break;
                }
                case 0xd3: { break; }
                case 0xd4: {
                    if (!F.getBit(Cidx)) {
                        // retAddr = PC + 2 (two immediate bytes)
                        uint16_t ret = PC + 2;
                        write(--SP, (ret >> 8) & 0xff);
                        write(--SP, ret & 0xff);

                        uint8_t lo = read(PC);
                        uint8_t hi = read(PC + 1);
                        PC = (hi << 8) | lo;
                    } else {
                        PC += 2;
                    }
                    break;
                }
                case 0xd5: { write(--SP, D); write(--SP, E); break; }
                case 0xd6: { f(A -= read(pc(1)), 0b1011, 0b0100, 0b0000); break;} /* SUB */
                case 0xd7: {
                    write(--SP, (PC >> 8) & 0xff);
                    write(--SP, PC & 0xff);
                    PC = 0x0010;
                    break;
                }
                case 0xd8: {
                    if(F.getBit(Cidx)) {
                        PC = (read(SP + 1) << 8) | read(SP);
                        SP += 2;
                    }
                    break;
                }
                case 0xd9: { // Don't forget to implement the re-enabling of interrupts here for RETI
                    PC = (read(SP + 1) << 8) | read(SP);
                    SP += 2;
                    break;
                }
                case 0xda: {
                    uint8_t lo = read(PC);
                    uint8_t hi = read(PC + 1);
                    if (F.getBit(Cidx)) {
                        PC = (hi << 8) | lo;
                    } else {
                        PC += 2;
                    }
                    break;
                }
                case 0xdb: { break; }
                case 0xdc: {
                    if (F.getBit(Cidx)) {
                        // retAddr = PC + 2 (two immediate bytes)
                        uint16_t ret = PC + 2;
                        write(--SP, (ret >> 8) & 0xff);
                        write(--SP, ret & 0xff);

                        uint8_t lo = read(PC);
                        uint8_t hi = read(PC + 1);
                        PC = (hi << 8) | lo;
                    } else {
                        PC += 2;
                    }
                    break;
                };
                case 0xdd: { break; }
                case 0xde: { f(A -= (read(pc(1)) - F.getBit(Cidx)), 0b1011, 0b0100, 0b0000);                  break;} /* SBC */
                case 0xdf: {
                    write(--SP, (PC >> 8) & 0xff);
                    write(--SP, PC & 0xff);
                    PC = 0x0018;
                    break;
                }

                /* 0xe0 - 0xef */
                case 0xe0: {                  // LDH (a8), A
                    uint8_t offset = read(PC); // fetch the 8-bit immediate from [PC]
                    PC += 1;                   // consume that byte

                    uint16_t addr = 0xFF00u | offset;
                    write(addr, A);            // store A into 0xFF00 + offset
                    break;
                }
                case 0xe1: { HL = (read(SP + 1) << 8) | read(SP); SP += 2; break;}
                case 0xe2: { write(C.getVal(), A); break; }
                case 0xe3: { break; }
                case 0xe4: { break; }
                case 0xe5: { write(--SP, H); write(--SP, L); break; }
                case 0xe6: { f(A &= read(pc(1)), 0b1000, 0b0010, 0b0101); break;} /* AND */
                case 0xe7: {
                    write(--SP, (PC >> 8) & 0xff);
                    write(--SP, PC & 0xff);
                    PC = 0x0020;
                    break;
                }
                case 0xe8: { f(SP += read(pc(1)), 0b0011, 0b0000, 0b1100); break; }
                case 0xe9: { PC = read(HL.getVal()); break; }
                case 0xea: { write(read(PC) | (read(PC + 1) << 8), A); PC += 2; break; }
                case 0xeb: { break; };
                case 0xec: { break; };
                case 0xed: { break; }
                case 0xee: { f(A ^= read(pc(1)), 0b1000, 0b0000, 0b0111);                  break;} /* XOR */
                case 0xef: {
                    write(--SP, (PC >> 8) & 0xff);
                    write(--SP, PC & 0xff);
                    PC = 0x0028;
                    break;
                }

                /* 0xf0 - 0xff */
                case 0xf0: {                  // LDH A, (a8)
                    uint8_t offset = read(PC);
                    PC += 1;

                    uint16_t addr = 0xFF00u | offset;
                    A = read(addr);           // load A from 0xFF00 + offset
                    break;
                }
                case 0xf1: { AF = (read(SP + 1) << 8) | read(SP); SP += 2; break;}
                case 0xf2: { A = read(C.getVal()); break; }
                case 0xf3: { break; } // DI - disable interrupt handling
                case 0xf4: { break; }
                case 0xf5: { write(--SP, A); write(--SP, F); break; }
                case 0xf6: { f(A |= read(pc(1)), 0b1000, 0b0000, 0b0111);                 break;} /* OR */
                case 0xf7: {
                    write(--SP, (PC >> 8) & 0xff);
                    write(--SP, PC & 0xff);
                    PC = 0x0030;
                    break;
                }
                case 0xf8: {
                    uint16_t store = SP;
                    HL = SP + read(PC + 1);
                    f(SP += read(pc(1)), 0b0011, 0b0000, 0b1100);
                    SP = store;
                    break;
                }
                case 0xf9: { SP = HL.getVal(); break; }
                case 0xfa: { A = read(read(PC) | (read(PC + 1) << 8)); PC += 2; break; }
                case 0xfb: { break; }; /* Enable interrupt, TODO: implement */
                case 0xfc: { break; };
                case 0xfd: { break; }
                case 0xfe: { uint8_t store = A.getVal(); f(A -= read(pc(1)), 0b1011, 0b0100, 0b0000); A = store; break;} /* CP */
                case 0xff: {
                    write(--SP, (PC >> 8) & 0xff);
                    write(--SP, PC & 0xff);
                    PC = 0x0038;
                    break;
                }

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
    core.read(0x3000);
    core.read(0x5000);

    delete ram;
    printf("Memory succesfully freed.");
    return 0;
}
