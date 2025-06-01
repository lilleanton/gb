#include "LR35902.h"

bool CPU::LR35902::insCycle() {
    // Cooldown to simulate machine cycles
    wait -= 1;
    if(wait > 0){
        return false;
    }

    // Perform interrupt handling
    uint8_t IF = read(0xff0f);
    uint8_t IE = read(0xffff);
    uint8_t pending = IF & IE;

    if(IME && (pending != 0)) {
        IME = false;

        // Highest priority bit
        int bit = __builtin_ctz(pending);
        printf("Interrupt handling begins for bit %d at 0xff0f\n", bit);

        // Clear the current IF bit
        write(0xff0f, IF & ~(1 << bit));

        // Push PC
        write(--SP, (PC >> 8) & 0xff);
        write(--SP, PC & 0xff);

        // Jump to ISR
        PC = 0x40 + 8*bit;

        wait = 5; // This takes 5 cycles
        return false;
    }

    if(halt) {
        return false;
    }

    // Load one opcode from memory. It's important to keep in mind that the PC has incremented when implementing/editing opcodes.
    uint8_t opcode;
    if(haltBug) {
        haltBug = false;
        opcode = read(PC - 1); // Re-read previous byte, but PC already incremented
    } else {
        opcode = read(PC++);
    }

    // Precompiled jumptable for all instructions
    wait = 1; // Most 1-byte instructions only need 1 m-cycle
    switch(opcode) {
        /* 0x00 through 0x3f, misc.*/
        /* 0x00 - 0x0f */
        case 0x00: { break; } /* NOP */
        case 0x01: { wait = 3; BC = read(pc(2), 2);                    break; } /* LD BC, n16.  3 12.   ---- */
        case 0x02: { wait = 2; write(BC, A);                           break; } /* LD [BC], A.  1 8.    ---- */
        case 0x03: { wait = 2; ++BC;                                   break; } 
        case 0x04: { f(B += 1, 0b1010, 0b00000, 0b0100);      break; } /* INC B        1 4     Z0H- */
        case 0x05: { f(B -= 1, 0b1010, 0b0100, 0b00000);      break; } 
        case 0x06: { wait = 2; B = bus.read(pc(1));                    break; } 
        case 0x07: { f(A.RLC(), 0b00001, 0b00000, 0b1110);     break; } 
        case 0x08: {
            uint16_t addr = read(pc(2), 2);
            write(addr, SP & 0xff);
            write(addr + 1, (SP >> 8) & 0xff);
            wait = 5;
            break;
        }
        case 0x09: { wait = 2; f(HL += BC, 0b0011, 0b00000, 0b0100);    break; } 
        case 0x0a: { wait = 2; A = read(BC.getVal());                  break; } 
        case 0x0b: { wait = 2; --BC;                                   break; } 
        case 0x0c: { f(C += 1, 0b1010, 0b00000, 0b0100);      break; } 
        case 0x0d: { f(C -= 1, 0b1010, 0b0100, 0b00000);      break; }
        case 0x0e: { wait = 2; C = read(pc(1));                        break; } 
        case 0x0f: { f(A.RRC(), 0b00001, 0b00000, 0b1110);     break; } 

        /* 0x10 - 0x1f */
        case 0x10: {                                        break;} 
        case 0x11: {wait = 3; DE = read(pc(2), 2);                    break;} 
        case 0x12: {wait = 2; write(DE, A);                           break;} 
        case 0x13: {wait = 2; ++DE;                                   break;} 
        case 0x14: {f(D += 1, 0xA, 0x0, 0x4);               break;} 
        case 0x15: {f(D -= 1, 0xA, 0x4, 0x0);               break;} 
        case 0x16: {wait = 2; D = read(pc(1));                        break;} 
        case 0x17: {f(A.RL(F.getBit(Cidx)), 0x1, 0x0, 0xE); break;} 
        case 0x18: {wait = 3; PC += int8_t(read(PC)); PC++;           break;} 
        case 0x19: {wait = 2; f(HL += DE, 0x3, 0x0, 0x4);             break;} 
        case 0x1a: {wait = 2; A = read(DE.getVal());                  break;} 
        case 0x1b: {wait = 2; --DE;                                   break;} 
        case 0x1c: {f(E += 1, 0xA, 0x0, 0x4);               break;} 
        case 0x1d: {f(E -= 1, 0xA, 0x4, 0x0);               break;} 
        case 0x1e: {wait = 2; E = read(pc(1));                        break;} 
        case 0x1f: {f(A.RR(F.getBit(Cidx)), 0x1, 0x0, 0xE); break;} 

        /* 0x20 - 0x2f */
        case 0x20: {
            if(!F.getBit(Zidx)){
                PC += int8_t(read(PC));
                wait = 3;
            } else {
                wait = 2;
            }
            PC++;
            break;
        }
        case 0x21: {wait = 3; HL = read(pc(2), 2);                    break;} 
        case 0x22: {wait = 2; write(HL++, A);                         break;} 
        case 0x23: {wait = 2; ++HL;                                   break;} 
        case 0x24: {f(H += 1, 0xA, 0x0, 0x4);               break;} 
        case 0x25: {f(H -= 1, 0xA, 0x4, 0x0);               break;} 
        case 0x26: {wait = 2; H = read(pc(1));                        break;} 
        case 0x27: {
            // Flags bits in F: Z=0x80, N=0x40, H=0x20, C=0x10
            uint8_t a = A.getVal();
            uint8_t f = F.getVal();
            bool n =  f & 0x40;   // previous operation was subtraction?
            bool h =  f & 0x20;   // half-carry flag
            bool c =  f & 0x10;   // carry flag
            uint8_t correction = 0;

            if (!n) {
                // after an addition
                if (h || (a & 0x0F) > 9)         correction |= 0x06;
                if (c || a > 0x99) { correction |= 0x60;  c = true; }
                a += correction;
            } else {
                // after a subtraction
                if (h) a -= 0x06;
                if (c) a -= 0x60;
            }

            // rebuild F:
            // Z: set if A == 0
            // N: preserved
            // H: always reset
            // C: maybe set above
            uint8_t newF = 0;
            if (a == 0)          newF |= 0x80;  // Z
            if (n)               newF |= 0x40;  // N
            if (c)               newF |= 0x10;  // C
            // H stays 0

            A = a;
            F = newF;
            break;
        } /* DAA */
        case 0x28: {
            if(F.getBit(Zidx)){
                PC += int8_t(read(PC));
                wait = 3;
            } else {
                wait = 2;
            }
            PC++;
            break;
        }
        case 0x29: {wait = 2; f(HL += HL, 0x3, 0x0, 0x4);                 break;} 
        case 0x2a: {wait = 2; A = read(HL++);                             break;} 
        case 0x2b: {wait = 2; --HL;                                       break;} 
        case 0x2c: {f(L += 1, 0xA, 0x0, 0x4);                   break;} 
        case 0x2d: {f(L -= 1, 0xA, 0x4, 0x0);                   break;} 
        case 0x2e: {wait = 2; L = read(pc(1));                            break;} 
        case 0x2f: { A = ~A.getVal(); f(0x0, 0x0, 0x6, 0x0);    break;} 

        /* 0x30 - 0x3f */
        case 0x30: {
            if(!F.getBit(Cidx)){
                PC += int8_t(read(PC));
                wait = 3;
            } else {
                wait = 2;
            }
            PC++;
            break;
        }
        case 0x31: {wait = 3; SP = read(pc(2), 2);                    break;} 
        case 0x32: {wait = 2; write(HL--, A);                         break;} 
        case 0x33: {wait = 2; ++SP;                                   break;} 
        case 0x34: {
            dummy8 = read(HL.getVal());
            write(HL.getVal(), dummy8 + 1);
            f(dummy8 += 1, 0xA, 0x0, 0x4);
            wait = 3;
            break;
        }
        case 0x35: {
            dummy8 = read(HL.getVal());
            write(HL.getVal(), dummy8 - 1);
            f(dummy8 -= 1, 0xA, 0x4, 0x0);
            wait = 3;
            break;
        }
        case 0x36: {wait = 3; write(HL.getVal(), read(pc(1)));        break;} 
        case 0x37: {f(0b00000, 0b00000, 0b00001, 0b0110);      break;} /* SCF, -001 */
        case 0x38: {
            if(F.getBit(Cidx)){
                wait = 3;
                PC += int8_t(read(PC));
            } else {
                wait = 2;
            }
            PC++;
            break;
        }
        case 0x39: { wait = 2; f(HL += SP, 0x3, 0x0, 0x4);                    break;} 
        case 0x3a: { wait = 2; A = read(HL--);                                break;} 
        case 0x3b: { wait = 2; --SP;                                          break;} 
        case 0x3c: { f(A += 1, 0xA, 0x0, 0x4);                      break;} 
        case 0x3d: { f(A -= 1, 0xA, 0x4, 0x0);                      break;} 
        case 0x3e: { wait = 2; A = read(pc(1));                               break;} 
        case 0x3f: { f(0x0, 0x0, 0x0, 0x6); F = F.getVal() ^ 0x10; break;} /* CCF - toggle the C bit */

        /* 0x40 through 0x7f, predominantly load instructions*/
        /* 0x40 - 0x4f */
        case 0x40: { B = B; break;} 
        case 0x41: { B = C; break;} 
        case 0x42: { B = D; break;} 
        case 0x43: { B = E; break;} 
        case 0x44: { B = H; break;} 
        case 0x45: { B = L; break;} 
        case 0x46: { wait = 2; B = read(HL.getVal()); break;} 
        case 0x47: { B = A; break;} 
        case 0x48: { C = B; break;} 
        case 0x49: { C = C; break;} 
        case 0x4a: { C = D; break;} 
        case 0x4b: { C = E; break;} 
        case 0x4c: { C = H; break;} 
        case 0x4d: { C = L; break;} 
        case 0x4e: { wait = 2; C = read(HL.getVal()); break;} 
        case 0x4f: { C = A; break;} 

        /* 0x50 - 0x5f */
        case 0x50: { D = B; break;} 
        case 0x51: { D = C; break;} 
        case 0x52: { D = D; break;} 
        case 0x53: { D = E; break;} 
        case 0x54: { D = H; break;} 
        case 0x55: { D = L; break;} 
        case 0x56: { wait = 2; D = read(HL.getVal()); break;} 
        case 0x57: { D = A; break;} 
        case 0x58: { E = B; break;} 
        case 0x59: { E = C; break;} 
        case 0x5a: { E = D; break;} 
        case 0x5b: { E = E; break;} 
        case 0x5c: { E = H; break;} 
        case 0x5d: { E = L; break;} 
        case 0x5e: { wait = 2; E = read(HL.getVal()); break;} 
        case 0x5f: { E = A; break;} 

        /* 0x60 - 0x6f */
        case 0x60: { H = B; break;} 
        case 0x61: { H = C; break;} 
        case 0x62: { H = D; break;} 
        case 0x63: { H = E; break;} 
        case 0x64: { H = H; break;} 
        case 0x65: { H = L; break;} 
        case 0x66: { wait = 2; H = read(HL.getVal()); break;} 
        case 0x67: { H = A; break;} 
        case 0x68: { L = B; break;} 
        case 0x69: { L = C; break;} 
        case 0x6a: { L = D; break;} 
        case 0x6b: { L = E; break;} 
        case 0x6c: { L = H; break;} 
        case 0x6d: { L = L; break;} 
        case 0x6e: { wait = 2; L = read(HL.getVal()); break;} 
        case 0x6f: { L = A; break;} 

        /* 0x70 - 0x7f */
        case 0x70: { wait = 2; write(HL, B); break;} 
        case 0x71: { wait = 2; write(HL, C); break;} 
        case 0x72: { wait = 2; write(HL, D); break;} 
        case 0x73: { wait = 2; write(HL, E); break;} 
        case 0x74: { wait = 2; write(HL, H); break;} 
        case 0x75: { wait = 2; write(HL, L); break;} 
        case 0x76: {
            uint8_t IF = read(0xFF0F);
            uint8_t IE = read(0xFFFF);
            bool pendingInterrupt = (IF & IE) != 0;
            printf("HALT\n");

            if (!IME && pendingInterrupt) {
                // HALT bug triggers
                halt = false;
                haltBug = true;
            } else {
                halt = true;
            }
 
            break;
        } /* HALT, TODO: Implement the "HALT bug" */
        case 0x77: { wait = 2; write(HL, A); break;} 
        case 0x78: { A = B; break;} 
        case 0x79: { A = C; break;} 
        case 0x7a: { A = D; break;} 
        case 0x7b: { A = E; break;} 
        case 0x7c: { A = H; break;} 
        case 0x7d: { A = L; break;} 
        case 0x7e: { wait = 2; A = read(HL.getVal()); break;} 
        case 0x7f: { A = A; break;} 

        /* 0x80 through 0xbf, ADD, SUB, AND, OR, ADC, SBC, XOR, CP */
        /* 0x80 - 0x8f, ADD & ADC */
        case 0x80: { f(A += B, 0b1011, 0b00000, 0b0100);                                     break; } /* ADD */
        case 0x81: { f(A += C, 0b1011, 0b00000, 0b0100);                                     break; } /* ... */
        case 0x82: { f(A += D, 0b1011, 0b00000, 0b0100);                                     break; } 
        case 0x83: { f(A += E, 0b1011, 0b00000, 0b0100);                                     break; } 
        case 0x84: { f(A += H, 0b1011, 0b00000, 0b0100);                                     break; } 
        case 0x85: { f(A += L, 0b1011, 0b00000, 0b0100);                                     break; } 
        case 0x86: { wait = 2; f(A += read(HL.getVal()), 0b1011, 0b00000, 0b0100);                     break; } 
        case 0x87: { f(A += A, 0b1011, 0b00000, 0b0100);                                     break; } 
        case 0x88: { bool c = F.getBit(Cidx); uint8_t flags = A.adc(B, c); F = flags;       break; } /* ADC */
        case 0x89: { bool c = F.getBit(Cidx); uint8_t flags = A.adc(C, c); F = flags;       break; }
        case 0x8a: { bool c = F.getBit(Cidx); uint8_t flags = A.adc(D, c); F = flags;       break; }
        case 0x8b: { bool c = F.getBit(Cidx); uint8_t flags = A.adc(E, c); F = flags;       break; }
        case 0x8c: { bool c = F.getBit(Cidx); uint8_t flags = A.adc(H, c); F = flags;       break; }
        case 0x8d: { bool c = F.getBit(Cidx); uint8_t flags = A.adc(L, c); F = flags;       break; }
        case 0x8e: { wait = 2; bool c = F.getBit(Cidx); Reg8 tmp(read(HL.getVal())); uint8_t flags = A.adc(tmp, c); F = flags; break; }
        case 0x8f: { bool c = F.getBit(Cidx); uint8_t flags = A.adc(A, c); F = flags;       break; }

        /* 0x90 - 0x9f, SUB & SBC */
        case 0x90: { f(A -= B, 0b1011, 0b0100, 0b00000);                                     break; } /* SUB */
        case 0x91: { f(A -= C, 0b1011, 0b0100, 0b00000);                                     break; } /* ... */
        case 0x92: { f(A -= D, 0b1011, 0b0100, 0b00000);                                     break; } 
        case 0x93: { f(A -= E, 0b1011, 0b0100, 0b00000);                                     break; } 
        case 0x94: { f(A -= H, 0b1011, 0b0100, 0b00000);                                     break; } 
        case 0x95: { f(A -= L, 0b1011, 0b0100, 0b00000);                                     break; } 
        case 0x96: { wait = 2; f(A -= read(HL.getVal()), 0b1011, 0b0100, 0b00000);                     break; } 
        case 0x97: { f(A -= A, 0b1011, 0b0100, 0b00000);                                     break; } 
        case 0x98: { bool c = F.getBit(Cidx); uint8_t flags = A.sbc(B, c); F = flags;       break; } /* SBC */
        case 0x99: { bool c = F.getBit(Cidx); uint8_t flags = A.sbc(C, c); F = flags;       break; }
        case 0x9a: { bool c = F.getBit(Cidx); uint8_t flags = A.sbc(D, c); F = flags;       break; }
        case 0x9b: { bool c = F.getBit(Cidx); uint8_t flags = A.sbc(E, c); F = flags;       break; }
        case 0x9c: { bool c = F.getBit(Cidx); uint8_t flags = A.sbc(H, c); F = flags;       break; }
        case 0x9d: { bool c = F.getBit(Cidx); uint8_t flags = A.sbc(L, c); F = flags;       break; }
        case 0x9e: { wait = 2; bool c = F.getBit(Cidx); Reg8 tmp(read(HL.getVal())); uint8_t flags = A.sbc(tmp, c); F = flags; break; }
        case 0x9f: { bool c = F.getBit(Cidx); uint8_t flags = A.sbc(A, c); F = flags;       break; }

        /* 0xa0 - 0xaf, AND & XOR */
        case 0xa0: { f(A &= B, 0b1000, 0b0010, 0b0101);                 break;} /* AND */
        case 0xa1: { f(A &= C, 0b1000, 0b0010, 0b0101);                 break;} 
        case 0xa2: { f(A &= D, 0b1000, 0b0010, 0b0101);                 break;} 
        case 0xa3: { f(A &= E, 0b1000, 0b0010, 0b0101);                 break;} 
        case 0xa4: { f(A &= H, 0b1000, 0b0010, 0b0101);                 break;} 
        case 0xa5: { f(A &= L, 0b1000, 0b0010, 0b0101);                 break;} 
        case 0xa6: { wait = 2; f(A &= read(HL.getVal()), 0b1000, 0b0010, 0b0101); break;} 
        case 0xa7: { f(A &= A, 0b1000, 0b0010, 0b0101);                 break;} 
        case 0xa8: { f(A ^= B, 0b1000, 0b00000, 0b0111);                 break;} /* XOR */
        case 0xa9: { f(A ^= C, 0b1000, 0b00000, 0b0111);                 break;} 
        case 0xaa: { f(A ^= D, 0b1000, 0b00000, 0b0111);                 break;} 
        case 0xab: { f(A ^= E, 0b1000, 0b00000, 0b0111);                 break;} 
        case 0xac: { f(A ^= H, 0b1000, 0b00000, 0b0111);                 break;} 
        case 0xad: { f(A ^= L, 0b1000, 0b00000, 0b0111);                 break;} 
        case 0xae: { wait = 2; f(A ^= read(HL.getVal()), 0b1000, 0b00000, 0b0111); break;} 
        case 0xaf: { f(A ^= A, 0b1000, 0b00000, 0b0111);                 break;} 

        /* 0xb0 - 0xbf, OR & CP */
        case 0xb0: { f(A |= B, 0b1000, 0b00000, 0b0111);                 break;} /* OR */
        case 0xb1: { f(A |= C, 0b1000, 0b00000, 0b0111);                 break;} 
        case 0xb2: { f(A |= D, 0b1000, 0b00000, 0b0111);                 break;} 
        case 0xb3: { f(A |= E, 0b1000, 0b00000, 0b0111);                 break;} 
        case 0xb4: { f(A |= H, 0b1000, 0b00000, 0b0111);                 break;} 
        case 0xb5: { f(A |= L, 0b1000, 0b00000, 0b0111);                 break;} 
        case 0xb6: { wait = 2; f(A |= read(HL.getVal()), 0b1000, 0b00000, 0b0111); break;} 
        case 0xb7: { f(A |= A, 0b1000, 0b00000, 0b0111);                 break;} 
        case 0xb8: { uint8_t store = A.getVal(); f(A -= B, 0b1011, 0b0100, 0b00000); A = store;                  break;} /* CP */
        case 0xb9: { uint8_t store = A.getVal(); f(A -= C, 0b1011, 0b0100, 0b00000); A = store;                  break;} 
        case 0xba: { uint8_t store = A.getVal(); f(A -= D, 0b1011, 0b0100, 0b00000); A = store;                  break;} 
        case 0xbb: { uint8_t store = A.getVal(); f(A -= E, 0b1011, 0b0100, 0b00000); A = store;                  break;} 
        case 0xbc: { uint8_t store = A.getVal(); f(A -= H, 0b1011, 0b0100, 0b00000); A = store;                  break;} 
        case 0xbd: { uint8_t store = A.getVal(); f(A -= L, 0b1011, 0b0100, 0b00000); A = store;                  break;} 
        case 0xbe: { wait = 2; uint8_t store = A.getVal(); f(A -= read(HL.getVal()), 0b1011, 0b0100, 0b00000); A = store;  break;} 
        case 0xbf: { uint8_t store = A.getVal(); f(A -= A, 0b1011, 0b0100, 0b00000); A = store;                  break;} 

        /* 0xc0 through 0xff, mostly stack management */
        /* 0xc0 - 0xcf */
        case 0xc0: {
            if(!F.getBit(Zidx)) {
                PC = (read(SP + 1) << 8) | read(SP);
                SP += 2;
                wait = 5;
            } else {
                wait = 2;
            }
            break;
        }
        case 0xc1: { wait = 3; BC = (read(SP + 1) << 8) | read(SP); SP += 2; break;}
        case 0xc2: {
            uint8_t lo = read(PC);
            uint8_t hi = read(PC + 1);
            if (!F.getBit(Zidx)) {
                PC = (hi << 8) | lo;
                wait = 4;
            } else {
                PC += 2;
                wait = 3;
            }
            break;
        }
        case 0xc3: {
            uint8_t lo = read(PC);
            uint8_t hi = read(PC + 1);
            PC = (hi << 8) | lo;
            wait = 4;
            break;
        }
        case 0xc4: {
            if (!F.getBit(Zidx)) {
                uint16_t ret = PC + 2;
                write(--SP, (ret >> 8) & 0xff);
                write(--SP, ret & 0xff);

                uint8_t lo = read(PC);
                uint8_t hi = read(PC + 1);
                PC = (hi << 8) | lo;
                wait = 6;
            } else {
                PC += 2;
                wait = 3;
            }
            break;
        }
        case 0xc5: { wait = 4; write(--SP, B); write(--SP, C); break; }
        case 0xc6: { wait = 2; f(A += read(pc(1)), 0b1011, 0b00000, 0b0100); break; } /* ADD */
        case 0xc7: {
            write(--SP, (PC >> 8) & 0xff);
            write(--SP, PC & 0xff);
            PC = 0x0000;
            wait = 4;
            break;
        }
        case 0xc8: {
            if(F.getBit(Zidx)) {
                PC = (read(SP + 1) << 8) | read(SP);
                SP += 2;
                wait = 5;
            } else {
                wait = 2;
            }
            break;
        }
        case 0xc9: {
            PC = (read(SP + 1) << 8) | read(SP);
            SP += 2;
            wait = 4;
            break;
        }
        case 0xca: {
            uint8_t lo = read(PC);
            uint8_t hi = read(PC + 1);
            if (F.getBit(Zidx)) {
                PC = (hi << 8) | lo;
                wait = 4;
            } else {
                PC += 2;
                wait = 3;
            }
            break;
        }
        case 0xcb: { CBExtension(); break; } /* TODO - PREFIX CB */
        case 0xcc: {
            if (F.getBit(Zidx)) {
                uint16_t ret = PC + 2;
                write(--SP, (ret >> 8) & 0xff);
                write(--SP, ret & 0xff);

                uint8_t lo = read(PC);
                uint8_t hi = read(PC + 1);
                PC = (hi << 8) | lo;
                wait = 6;
            } else {
                PC += 2;
                wait = 3;
            }
            break;
        }
        case 0xcd: {
            uint16_t ret = PC + 2;
            write(--SP, (ret >> 8) & 0xff);
            write(--SP, ret & 0xff);

            uint8_t lo = read(PC);
            uint8_t hi = read(PC + 1);
            PC = (hi << 8) | lo;
            wait = 6;
            break;
        }
        case 0xce: { wait = 2; f(A.adc(Reg8(read(pc(1))), F.getBit(Cidx)), 0b1011, 0b00000, 0b0100); break; } /* ADC */
        case 0xcf: {
            write(--SP, (PC >> 8) & 0xff);
            write(--SP, PC & 0xff);
            PC = 0x0008;
            wait = 4;
            break;
        }

        /* 0xd0 - 0xdf */
        case 0xd0: {
            if(!F.getBit(Cidx)) {
                PC = (read(SP + 1) << 8) | read(SP);
                SP += 2;
                wait = 5;
            } else {
                wait = 2;
            }
            break;
        }
        case 0xd1: { wait = 3; DE = (read(SP + 1) << 8) | read(SP); SP += 2; break;}
        case 0xd2: {
            uint8_t lo = read(PC);
            uint8_t hi = read(PC + 1);
            if (!F.getBit(Cidx)) {
                PC = (hi << 8) | lo;
                wait = 4;
            } else {
                PC += 2;
                wait = 3;
            }
            break;
        }
        case 0xd3: { f(B.RLC(), 0b1001, 0b0000, 0b0110); break; }
        case 0xd4: {
            if (!F.getBit(Cidx)) {
                uint16_t ret = PC + 2;
                write(--SP, (ret >> 8) & 0xff);
                write(--SP, ret & 0xff);

                uint8_t lo = read(PC);
                uint8_t hi = read(PC + 1);
                PC = (hi << 8) | lo;
                wait = 6;
            } else {
                PC += 2;
                wait = 3;
            }
            break;
        }
        case 0xd5: { wait = 4; write(--SP, D); write(--SP, E); break; }
        case 0xd6: { wait = 2; f(A -= read(pc(1)), 0b1011, 0b0100, 0b00000); break;} /* SUB */
        case 0xd7: {
            write(--SP, (PC >> 8) & 0xff);
            write(--SP, PC & 0xff);
            PC = 0x0010;
            wait = 4;
            break;
        }
        case 0xd8: {
            if(F.getBit(Cidx)) {
                PC = (read(SP + 1) << 8) | read(SP);
                SP += 2;
                wait = 5;
            } else {
                wait = 2;
            }
            break;
        }
        case 0xd9: {
            PC = (read(SP + 1) << 8) | read(SP);
            SP += 2;
            IME = true;
            wait = 4;
            break;
        }
        case 0xda: {
            uint8_t lo = read(PC);
            uint8_t hi = read(PC + 1);
            if (F.getBit(Cidx)) {
                PC = (hi << 8) | lo;
                wait = 4;
            } else {
                PC += 2;
                wait = 3;
            }
            break;
        }
        case 0xdb: { f(B.RLC(), 0b1001, 0b0000, 0b0110); break; }
        case 0xdc: {
            if (F.getBit(Cidx)) {
                uint16_t ret = PC + 2;
                write(--SP, (ret >> 8) & 0xff);
                write(--SP, ret & 0xff);

                uint8_t lo = read(PC);
                uint8_t hi = read(PC + 1);
                PC = (hi << 8) | lo;
                wait = 6;
            } else {
                PC += 2;
                wait = 3;
            }
            break;
        }
        case 0xdd: { f(B.RLC(), 0b1001, 0b0000, 0b0110); break; }
        case 0xde: { wait = 2; f(A.sbc(Reg8(read(pc(1))), F.getBit(Cidx)), 0b1011, 0b0100, 0b00000); break; } /* SBC */
        case 0xdf: {
            write(--SP, (PC >> 8) & 0xff);
            write(--SP, PC & 0xff);
            PC = 0x0018;
            wait = 4;
            break;
        }

        /* 0xe0 - 0xef */
        case 0xe0: {                  // LDH (a8), A
            uint8_t offset = read(PC); // fetch the 8-bit immediate from [PC]
            PC += 1;                   // consume that byte

            uint16_t addr = 0xFF00u | offset;
            write(addr, A);            // store A into 0xFF00 + offset
            wait = 3;
            break;
        }
        case 0xe1: { wait = 3; HL = (read(SP + 1) << 8) | read(SP); SP += 2; break;}
        case 0xe2: { wait = 2; write(0xff00 + C.getVal(), A); break; }
        case 0xe3: { f(B.RLC(), 0b1001, 0b0000, 0b0110); break; }
        case 0xe4: { f(B.RLC(), 0b1001, 0b0000, 0b0110); break; }
        case 0xe5: { wait = 4; write(--SP, H); write(--SP, L); break; }
        case 0xe6: { wait = 2; f(A &= read(pc(1)), 0b1000, 0b0010, 0b0101); break;} /* AND */
        case 0xe7: {
            write(--SP, (PC >> 8) & 0xff);
            write(--SP, PC & 0xff);
            PC = 0x0020;
            wait = 4;
            break;
        }
        case 0xe8: {
            int8_t offset = read(pc(1));
            Reg8 HI((SP >> 8) & 0xff);
            Reg8 LO(SP & 0xff);
            Reg16 temp(HI, LO);
            f(temp += offset, 0b0011, 0b00000, 0b1100);
            SP += offset;
            wait = 4;
            break;
        }
        case 0xe9: { PC = HL.getVal(); break; }
        case 0xea: { wait = 4; write(read(PC) | (read(PC + 1) << 8), A); PC += 2; break; }
        case 0xeb: { f(B.RLC(), 0b1001, 0b0000, 0b0110); break; }
        case 0xec: { f(B.RLC(), 0b1001, 0b0000, 0b0110); break; }
        case 0xed: { f(B.RLC(), 0b1001, 0b0000, 0b0110); break; }
        case 0xee: { wait = 2; f(A ^= read(pc(1)), 0b1000, 0b00000, 0b0111);                  break;} /* XOR */
        case 0xef: {
            write(--SP, (PC >> 8) & 0xff);
            write(--SP, PC & 0xff);
            PC = 0x0028;
            wait = 4;
            break;
        }

        /* 0xf0 - 0xff */
        case 0xf0: {                  // LDH A, (a8)
            uint8_t offset = read(PC);
            PC += 1;

            uint16_t addr = 0xFF00u | offset;
            A = read(addr);           // load A from 0xFF00 + offset
            wait = 3;
            break;
        }
        case 0xf1: { wait = 3; AF = (read(SP + 1) << 8) | read(SP); F = F.getVal() & 0xf0; SP += 2; break; }
        case 0xf2: { A = read(0xff00 + C.getVal()); break; }
        case 0xf3: { IME = false; pendingEnable = false; return true; }
        case 0xf4: { f(B.RLC(), 0b1001, 0b0000, 0b0110); break; }
        case 0xf5: { wait = 4; write(--SP, A); write(--SP, F); break; }
        case 0xf6: { wait = 2; f(A |= read(pc(1)), 0b1000, 0b00000, 0b0111);                 break;} /* OR */
        case 0xf7: {
            write(--SP, (PC >> 8) & 0xff);
            write(--SP, PC & 0xff);
            PC = 0x0030;
            wait = 4;
            break;
        }
        case 0xf8: {
            int8_t offset = read(PC++);
            HL = SP + offset;
            Reg8 HI((SP >> 8) & 0xff);
            Reg8 LO(SP & 0xff);
            Reg16 temp(HI, LO);
            f(temp += offset, 0b0011, 0b00000, 0b1100);
            wait = 3;
            break;
        }
        case 0xf9: { wait = 2; SP = HL.getVal(); break; }
        case 0xfa: { wait = 4; A = read(read(PC) | (read(PC + 1) << 8)); PC += 2; break; }
        case 0xfb: { pendingEnable = true; return true; }
        case 0xfc: { f(B.RLC(), 0b1001, 0b0000, 0b0110); break; }
        case 0xfd: { f(B.RLC(), 0b1001, 0b0000, 0b0110); break; }
        case 0xfe: { wait = 2; uint8_t store = A.getVal(); f(A -= read(pc(1)), 0b1011, 0b0100, 0b00000); A = store; break;} /* CP */
        case 0xff: {
            write(--SP, (PC >> 8) & 0xff);
            write(--SP, PC & 0xff);
            PC = 0x0038;
            wait = 4;
            break;
        }

        default: { printf("Unknown opcode - %d\n", opcode);   break; } /* Unknown opcode */
    }

    if(pendingEnable) {
        pendingEnable = false;
        IME = true;
    }

    return true;
};

void CPU::LR35902::CBExtension() {
    uint8_t postfix = read(PC++);
    wait = 2; // In almost all cases we need 2 m-cycles

    switch(postfix) {
        case 0x00: { f(B.RLC(), 0b1001, 0b0000, 0b0110); break; }
        case 0x01: { f(C.RLC(), 0b1001, 0b0000, 0b0110); break; }
        case 0x02: { f(D.RLC(), 0b1001, 0b0000, 0b0110); break; }
        case 0x03: { f(E.RLC(), 0b1001, 0b0000, 0b0110); break; }
        case 0x04: { f(H.RLC(), 0b1001, 0b0000, 0b0110); break; }
        case 0x05: { f(L.RLC(), 0b1001, 0b0000, 0b0110); break; }
        case 0x06: { wait = 4;
            Reg8 temp(read(HL.getVal()));
            f(temp.RLC(), 0b1001, 0b0000, 0b0110);
            write(HL, temp);
            break;
        }
        case 0x07: { f(A.RLC(), 0b1001, 0b0000, 0b0110); break; }
        case 0x08: { f(B.RRC(), 0b1001, 0b0000, 0b0110); break; }
        case 0x09: { f(C.RRC(), 0b1001, 0b0000, 0b0110); break; }
        case 0x0a: { f(D.RRC(), 0b1001, 0b0000, 0b0110); break; }
        case 0x0b: { f(E.RRC(), 0b1001, 0b0000, 0b0110); break; }
        case 0x0c: { f(H.RRC(), 0b1001, 0b0000, 0b0110); break; }
        case 0x0d: { f(L.RRC(), 0b1001, 0b0000, 0b0110); break; }
        case 0x0e: { wait = 4;
            Reg8 temp(read(HL.getVal()));
            f(temp.RRC(), 0b1001, 0b0000, 0b0110);
            write(HL, temp);
            break;
        }
        case 0x0f: { f(A.RRC(), 0b1001, 0b0000, 0b0110); break; }

        case 0x10: { f(B.RL(F.getBit(Cidx)), 0b1001, 0b0000, 0b0110); break; }
        case 0x11: { f(C.RL(F.getBit(Cidx)), 0b1001, 0b0000, 0b0110); break; }
        case 0x12: { f(D.RL(F.getBit(Cidx)), 0b1001, 0b0000, 0b0110); break; }
        case 0x13: { f(E.RL(F.getBit(Cidx)), 0b1001, 0b0000, 0b0110); break; }
        case 0x14: { f(H.RL(F.getBit(Cidx)), 0b1001, 0b0000, 0b0110); break; }
        case 0x15: { f(L.RL(F.getBit(Cidx)), 0b1001, 0b0000, 0b0110); break; }
        case 0x16: { wait = 4;
            Reg8 temp(read(HL.getVal()));
            f(temp.RL(F.getBit(Cidx)), 0b1001, 0b0000, 0b0110);
            write(HL, temp);
            break;
        }
        case 0x17: { f(A.RL(F.getBit(Cidx)), 0b1001, 0b0000, 0b0110); break; }
        case 0x18: { f(B.RR(F.getBit(Cidx)), 0b1001, 0b0000, 0b0110); break; }
        case 0x19: { f(C.RR(F.getBit(Cidx)), 0b1001, 0b0000, 0b0110); break; }
        case 0x1a: { f(D.RR(F.getBit(Cidx)), 0b1001, 0b0000, 0b0110); break; }
        case 0x1b: { f(E.RR(F.getBit(Cidx)), 0b1001, 0b0000, 0b0110); break; }
        case 0x1c: { f(H.RR(F.getBit(Cidx)), 0b1001, 0b0000, 0b0110); break; }
        case 0x1d: { f(L.RR(F.getBit(Cidx)), 0b1001, 0b0000, 0b0110); break; }
        case 0x1e: { wait = 4;
            Reg8 temp(read(HL.getVal()));
            f(temp.RR(F.getBit(Cidx)), 0b1001, 0b0000, 0b0110);
            write(HL, temp);
            break;
        }
        case 0x1f: { f(A.RR(F.getBit(Cidx)), 0b1001, 0b0000, 0b0110); break; }

        case 0x20: { f(B.SLA(), 0b1001, 0b0000, 0b0110); break; }
        case 0x21: { f(C.SLA(), 0b1001, 0b0000, 0b0110); break; }
        case 0x22: { f(D.SLA(), 0b1001, 0b0000, 0b0110); break; }
        case 0x23: { f(E.SLA(), 0b1001, 0b0000, 0b0110); break; }
        case 0x24: { f(H.SLA(), 0b1001, 0b0000, 0b0110); break; }
        case 0x25: { f(L.SLA(), 0b1001, 0b0000, 0b0110); break; }
        case 0x26: { wait = 4;
            Reg8 temp(read(HL.getVal()));
            f(temp.SLA(), 0b1001, 0b0000, 0b0110);
            write(HL, temp);
            break;
        }
        case 0x27: { f(A.SLA(), 0b1001, 0b0000, 0b0110); break; }
        case 0x28: { f(B.SRA(), 0b1001, 0b0000, 0b0110); break; }
        case 0x29: { f(C.SRA(), 0b1001, 0b0000, 0b0110); break; }
        case 0x2a: { f(D.SRA(), 0b1001, 0b0000, 0b0110); break; }
        case 0x2b: { f(E.SRA(), 0b1001, 0b0000, 0b0110); break; }
        case 0x2c: { f(H.SRA(), 0b1001, 0b0000, 0b0110); break; }
        case 0x2d: { f(L.SRA(), 0b1001, 0b0000, 0b0110); break; }
        case 0x2e: { wait = 4;
            Reg8 temp(read(HL.getVal()));
            f(temp.SRA(), 0b1001, 0b0000, 0b0110);
            write(HL, temp);
            break;
        }
        case 0x2f: { f(A.SRA(), 0b1001, 0b0000, 0b0110); break; }

        case 0x30: { B = (B.getVal() >> 4) | (B.getVal() << 4); F = (B.getVal() == 0) << 7; break; }
        case 0x31: { C = (C.getVal() >> 4) | (C.getVal() << 4); F = (C.getVal() == 0) << 7; break; }
        case 0x32: { D = (D.getVal() >> 4) | (D.getVal() << 4); F = (D.getVal() == 0) << 7; break; }
        case 0x33: { E = (E.getVal() >> 4) | (E.getVal() << 4); F = (E.getVal() == 0) << 7; break; }
        case 0x34: { H = (H.getVal() >> 4) | (H.getVal() << 4); F = (H.getVal() == 0) << 7; break; }
        case 0x35: { L = (L.getVal() >> 4) | (L.getVal() << 4); F = (L.getVal() == 0) << 7; break; }
        case 0x36: { wait = 4;
            uint8_t val = read(HL.getVal());
            uint8_t result = (val << 4) | (val >> 4);
            write(HL.getVal(), result);

            uint8_t flags = 0;
            if (result == 0) flags |= 0x80;

            F = flags;
            break;
        }
        case 0x37: { A = (A.getVal() >> 4) | (A.getVal() << 4); F = (A.getVal() == 0) << 7; break; }
        case 0x38: { f(B.SRL(), 0b1001, 0b0000, 0b0110); break; }
        case 0x39: { f(C.SRL(), 0b1001, 0b0000, 0b0110); break; }
        case 0x3a: { f(D.SRL(), 0b1001, 0b0000, 0b0110); break; }
        case 0x3b: { f(E.SRL(), 0b1001, 0b0000, 0b0110); break; }
        case 0x3c: { f(H.SRL(), 0b1001, 0b0000, 0b0110); break; }
        case 0x3d: { f(L.SRL(), 0b1001, 0b0000, 0b0110); break; }
        case 0x3e: { wait = 4;
            Reg8 temp(read(HL.getVal()));
            f(temp.SRL(), 0b1001, 0b0000, 0b0110);
            write(HL, temp);
            break;
        }
        case 0x3f: { f(A.SRL(), 0b1001, 0b0000, 0b0110); break; }

        case 0x40: { F = (F.getVal() & 0x10) | (((B.getVal() >> 0 & 1) == 0) << 7) | 0x20; break; }
        case 0x41: { F = (F.getVal() & 0x10) | (((C.getVal() >> 0 & 1) == 0) << 7) | 0x20; break; }
        case 0x42: { F = (F.getVal() & 0x10) | (((D.getVal() >> 0 & 1) == 0) << 7) | 0x20; break; }
        case 0x43: { F = (F.getVal() & 0x10) | (((E.getVal() >> 0 & 1) == 0) << 7) | 0x20; break; }
        case 0x44: { F = (F.getVal() & 0x10) | (((H.getVal() >> 0 & 1) == 0) << 7) | 0x20; break; }
        case 0x45: { F = (F.getVal() & 0x10) | (((L.getVal() >> 0 & 1) == 0) << 7) | 0x20; break; }
        case 0x46: { wait = 3; F = (F.getVal() & 0x10) | (((read(HL.getVal()) >> 0 & 1) == 0) << 7) | 0x20; break; }
        case 0x47: { F = (F.getVal() & 0x10) | (((A.getVal() >> 0 & 1) == 0) << 7) | 0x20; break; }
        case 0x48: { F = (F.getVal() & 0x10) | (((B.getVal() >> 1 & 1) == 0) << 7) | 0x20; break; }
        case 0x49: { F = (F.getVal() & 0x10) | (((C.getVal() >> 1 & 1) == 0) << 7) | 0x20; break; }
        case 0x4a: { F = (F.getVal() & 0x10) | (((D.getVal() >> 1 & 1) == 0) << 7) | 0x20; break; }
        case 0x4b: { F = (F.getVal() & 0x10) | (((E.getVal() >> 1 & 1) == 0) << 7) | 0x20; break; }
        case 0x4c: { F = (F.getVal() & 0x10) | (((H.getVal() >> 1 & 1) == 0) << 7) | 0x20; break; }
        case 0x4d: { F = (F.getVal() & 0x10) | (((L.getVal() >> 1 & 1) == 0) << 7) | 0x20; break; }
        case 0x4e: { wait = 3; F = (F.getVal() & 0x10) | (((read(HL.getVal()) >> 1 & 1) == 0) << 7) | 0x20; break; }
        case 0x4f: { F = (F.getVal() & 0x10) | (((A.getVal() >> 1 & 1) == 0) << 7) | 0x20; break; }

        case 0x50: { F = (F.getVal() & 0x10) | (((B.getVal() >> 2 & 1) == 0) << 7) | 0x20; break; }
        case 0x51: { F = (F.getVal() & 0x10) | (((C.getVal() >> 2 & 1) == 0) << 7) | 0x20; break; }
        case 0x52: { F = (F.getVal() & 0x10) | (((D.getVal() >> 2 & 1) == 0) << 7) | 0x20; break; }
        case 0x53: { F = (F.getVal() & 0x10) | (((E.getVal() >> 2 & 1) == 0) << 7) | 0x20; break; }
        case 0x54: { F = (F.getVal() & 0x10) | (((H.getVal() >> 2 & 1) == 0) << 7) | 0x20; break; }
        case 0x55: { F = (F.getVal() & 0x10) | (((L.getVal() >> 2 & 1) == 0) << 7) | 0x20; break; }
        case 0x56: { wait = 3; F = (F.getVal() & 0x10) | (((read(HL.getVal()) >> 2 & 1) == 0) << 7) | 0x20; break; }
        case 0x57: { F = (F.getVal() & 0x10) | (((A.getVal() >> 2 & 1) == 0) << 7) | 0x20; break; }
        case 0x58: { F = (F.getVal() & 0x10) | (((B.getVal() >> 3 & 1) == 0) << 7) | 0x20; break; }
        case 0x59: { F = (F.getVal() & 0x10) | (((C.getVal() >> 3 & 1) == 0) << 7) | 0x20; break; }
        case 0x5a: { F = (F.getVal() & 0x10) | (((D.getVal() >> 3 & 1) == 0) << 7) | 0x20; break; }
        case 0x5b: { F = (F.getVal() & 0x10) | (((E.getVal() >> 3 & 1) == 0) << 7) | 0x20; break; }
        case 0x5c: { F = (F.getVal() & 0x10) | (((H.getVal() >> 3 & 1) == 0) << 7) | 0x20; break; }
        case 0x5d: { F = (F.getVal() & 0x10) | (((L.getVal() >> 3 & 1) == 0) << 7) | 0x20; break; }
        case 0x5e: { wait = 3; F = (F.getVal() & 0x10) | (((read(HL.getVal()) >> 3 & 1) == 0) << 7) | 0x20; break; }
        case 0x5f: { F = (F.getVal() & 0x10) | (((A.getVal() >> 3 & 1) == 0) << 7) | 0x20; break; }

        case 0x60: { F = (F.getVal() & 0x10) | (((B.getVal() >> 4 & 1) == 0) << 7) | 0x20; break; }
        case 0x61: { F = (F.getVal() & 0x10) | (((C.getVal() >> 4 & 1) == 0) << 7) | 0x20; break; }
        case 0x62: { F = (F.getVal() & 0x10) | (((D.getVal() >> 4 & 1) == 0) << 7) | 0x20; break; }
        case 0x63: { F = (F.getVal() & 0x10) | (((E.getVal() >> 4 & 1) == 0) << 7) | 0x20; break; }
        case 0x64: { F = (F.getVal() & 0x10) | (((H.getVal() >> 4 & 1) == 0) << 7) | 0x20; break; }
        case 0x65: { F = (F.getVal() & 0x10) | (((L.getVal() >> 4 & 1) == 0) << 7) | 0x20; break; }
        case 0x66: { wait = 3; F = (F.getVal() & 0x10) | (((read(HL.getVal()) >> 4 & 1) == 0) << 7) | 0x20; break; }
        case 0x67: { F = (F.getVal() & 0x10) | (((A.getVal() >> 4 & 1) == 0) << 7) | 0x20; break; }
        case 0x68: { F = (F.getVal() & 0x10) | (((B.getVal() >> 5 & 1) == 0) << 7) | 0x20; break; }
        case 0x69: { F = (F.getVal() & 0x10) | (((C.getVal() >> 5 & 1) == 0) << 7) | 0x20; break; }
        case 0x6a: { F = (F.getVal() & 0x10) | (((D.getVal() >> 5 & 1) == 0) << 7) | 0x20; break; }
        case 0x6b: { F = (F.getVal() & 0x10) | (((E.getVal() >> 5 & 1) == 0) << 7) | 0x20; break; }
        case 0x6c: { F = (F.getVal() & 0x10) | (((H.getVal() >> 5 & 1) == 0) << 7) | 0x20; break; }
        case 0x6d: { F = (F.getVal() & 0x10) | (((L.getVal() >> 5 & 1) == 0) << 7) | 0x20; break; }
        case 0x6e: { wait = 3; F = (F.getVal() & 0x10) | (((read(HL.getVal()) >> 5 & 1) == 0) << 7) | 0x20; break; }
        case 0x6f: { F = (F.getVal() & 0x10) | (((A.getVal() >> 5 & 1) == 0) << 7) | 0x20; break; }

        case 0x70: { F = (F.getVal() & 0x10) | (((B.getVal() >> 6 & 1) == 0) << 7) | 0x20; break; }
        case 0x71: { F = (F.getVal() & 0x10) | (((C.getVal() >> 6 & 1) == 0) << 7) | 0x20; break; }
        case 0x72: { F = (F.getVal() & 0x10) | (((D.getVal() >> 6 & 1) == 0) << 7) | 0x20; break; }
        case 0x73: { F = (F.getVal() & 0x10) | (((E.getVal() >> 6 & 1) == 0) << 7) | 0x20; break; }
        case 0x74: { F = (F.getVal() & 0x10) | (((H.getVal() >> 6 & 1) == 0) << 7) | 0x20; break; }
        case 0x75: { F = (F.getVal() & 0x10) | (((L.getVal() >> 6 & 1) == 0) << 7) | 0x20; break; }
        case 0x76: { wait = 3; F = (F.getVal() & 0x10) | (((read(HL.getVal()) >> 6 & 1) == 0) << 7) | 0x20; break; }
        case 0x77: { F = (F.getVal() & 0x10) | (((A.getVal() >> 6 & 1) == 0) << 7) | 0x20; break; }
        case 0x78: { F = (F.getVal() & 0x10) | (((B.getVal() >> 7 & 1) == 0) << 7) | 0x20; break; }
        case 0x79: { F = (F.getVal() & 0x10) | (((C.getVal() >> 7 & 1) == 0) << 7) | 0x20; break; }
        case 0x7a: { F = (F.getVal() & 0x10) | (((D.getVal() >> 7 & 1) == 0) << 7) | 0x20; break; }
        case 0x7b: { F = (F.getVal() & 0x10) | (((E.getVal() >> 7 & 1) == 0) << 7) | 0x20; break; }
        case 0x7c: { F = (F.getVal() & 0x10) | (((H.getVal() >> 7 & 1) == 0) << 7) | 0x20; break; }
        case 0x7d: { F = (F.getVal() & 0x10) | (((L.getVal() >> 7 & 1) == 0) << 7) | 0x20; break; }
        case 0x7e: { wait = 3; F = (F.getVal() & 0x10) | (((read(HL.getVal()) >> 7 & 1) == 0) << 7) | 0x20; break; }
        case 0x7f: { F = (F.getVal() & 0x10) | (((A.getVal() >> 7 & 1) == 0) << 7) | 0x20; break; }

        case 0x80: { B.resetBit(0); break; }
        case 0x81: { C.resetBit(0); break; }
        case 0x82: { D.resetBit(0); break; }
        case 0x83: { E.resetBit(0); break; }
        case 0x84: { H.resetBit(0); break; }
        case 0x85: { L.resetBit(0); break; }
        case 0x86: { wait = 4; write(HL.getVal(), read(HL.getVal())  & ~(1 << 0)); break; }
        case 0x87: { A.resetBit(0); break; }
        case 0x88: { B.resetBit(1); break; }
        case 0x89: { C.resetBit(1); break; }
        case 0x8a: { D.resetBit(1); break; }
        case 0x8b: { E.resetBit(1); break; }
        case 0x8c: { H.resetBit(1); break; }
        case 0x8d: { L.resetBit(1); break; }
        case 0x8e: { wait = 4; write(HL.getVal(), read(HL.getVal()) & ~(1 << 1)); break; }
        case 0x8f: { A.resetBit(1); break; }

        case 0x90: { B.resetBit(2); break; }
        case 0x91: { C.resetBit(2); break; }
        case 0x92: { D.resetBit(2); break; }
        case 0x93: { E.resetBit(2); break; }
        case 0x94: { H.resetBit(2); break; }
        case 0x95: { L.resetBit(2); break; }
        case 0x96: { wait = 4; write(HL.getVal(), read(HL.getVal())  & ~(1 << 2)); break; }
        case 0x97: { A.resetBit(2); break; }
        case 0x98: { B.resetBit(3); break; }
        case 0x99: { C.resetBit(3); break; }
        case 0x9a: { D.resetBit(3); break; }
        case 0x9b: { E.resetBit(3); break; }
        case 0x9c: { H.resetBit(3); break; }
        case 0x9d: { L.resetBit(3); break; }
        case 0x9e: { wait = 4; write(HL.getVal(), read(HL.getVal()) & ~(1 << 3)); break; }
        case 0x9f: { A.resetBit(3); break; }

        case 0xa0: { B.resetBit(4); break; }
        case 0xa1: { C.resetBit(4); break; }
        case 0xa2: { D.resetBit(4); break; }
        case 0xa3: { E.resetBit(4); break; }
        case 0xa4: { H.resetBit(4); break; }
        case 0xa5: { L.resetBit(4); break; }
        case 0xa6: { wait = 4; write(HL.getVal(), read(HL.getVal())  & ~(1 << 4)); break; }
        case 0xa7: { A.resetBit(4); break; }
        case 0xa8: { B.resetBit(5); break; }
        case 0xa9: { C.resetBit(5); break; }
        case 0xaa: { D.resetBit(5); break; }
        case 0xab: { E.resetBit(5); break; }
        case 0xac: { H.resetBit(5); break; }
        case 0xad: { L.resetBit(5); break; }
        case 0xae: { wait = 4; write(HL.getVal(), read(HL.getVal()) & ~(1 << 5)); break; }
        case 0xaf: { A.resetBit(5); break; }

        case 0xb0: { B.resetBit(6); break; }
        case 0xb1: { C.resetBit(6); break; }
        case 0xb2: { D.resetBit(6); break; }
        case 0xb3: { E.resetBit(6); break; }
        case 0xb4: { H.resetBit(6); break; }
        case 0xb5: { L.resetBit(6); break; }
        case 0xb6: { wait = 4; write(HL.getVal(), read(HL.getVal())  & ~(1 << 6)); break; }
        case 0xb7: { A.resetBit(6); break; }
        case 0xb8: { B.resetBit(7); break; }
        case 0xb9: { C.resetBit(7); break; }
        case 0xba: { D.resetBit(7); break; }
        case 0xbb: { E.resetBit(7); break; }
        case 0xbc: { H.resetBit(7); break; }
        case 0xbd: { L.resetBit(7); break; }
        case 0xbe: { wait = 4; write(HL.getVal(), read(HL.getVal()) & ~(1 << 7)); break; }
        case 0xbf: { A.resetBit(7); break; }

        case 0xc0: { B.setBit(0); break; }
        case 0xc1: { C.setBit(0); break; }
        case 0xc2: { D.setBit(0); break; }
        case 0xc3: { E.setBit(0); break; }
        case 0xc4: { H.setBit(0); break; }
        case 0xc5: { L.setBit(0); break; }
        case 0xc6: { wait = 4; write(HL.getVal(), read(HL.getVal()) | (1 << 0)); break; }
        case 0xc7: { A.setBit(0); break; }
        case 0xc8: { B.setBit(1); break; }
        case 0xc9: { C.setBit(1); break; }
        case 0xca: { D.setBit(1); break; }
        case 0xcb: { E.setBit(1); break; }
        case 0xcc: { H.setBit(1); break; }
        case 0xcd: { L.setBit(1); break; }
        case 0xce: { wait = 4; write(HL.getVal(), read(HL.getVal()) | (1 << 1)); break; }
        case 0xcf: { A.setBit(1); break; }

        case 0xd0: { B.setBit(2); break; }
        case 0xd1: { C.setBit(2); break; }
        case 0xd2: { D.setBit(2); break; }
        case 0xd3: { E.setBit(2); break; }
        case 0xd4: { H.setBit(2); break; }
        case 0xd5: { L.setBit(2); break; }
        case 0xd6: { wait = 4; write(HL.getVal(), read(HL.getVal()) | (1 << 2)); break; }
        case 0xd7: { A.setBit(2); break; }
        case 0xd8: { B.setBit(3); break; }
        case 0xd9: { C.setBit(3); break; }
        case 0xda: { D.setBit(3); break; }
        case 0xdb: { E.setBit(3); break; }
        case 0xdc: { H.setBit(3); break; }
        case 0xdd: { L.setBit(3); break; }
        case 0xde: { wait = 4; write(HL.getVal(), read(HL.getVal()) | (1 << 3)); break; }
        case 0xdf: { A.setBit(3); break; }

        case 0xe0: { B.setBit(4); break; }
        case 0xe1: { C.setBit(4); break; }
        case 0xe2: { D.setBit(4); break; }
        case 0xe3: { E.setBit(4); break; }
        case 0xe4: { H.setBit(4); break; }
        case 0xe5: { L.setBit(4); break; }
        case 0xe6: { wait = 4; write(HL.getVal(), read(HL.getVal()) | (1 << 4)); break; }
        case 0xe7: { A.setBit(4); break; }
        case 0xe8: { B.setBit(5); break; }
        case 0xe9: { C.setBit(5); break; }
        case 0xea: { D.setBit(5); break; }
        case 0xeb: { E.setBit(5); break; }
        case 0xec: { H.setBit(5); break; }
        case 0xed: { L.setBit(5); break; }
        case 0xee: { wait = 4; write(HL.getVal(), read(HL.getVal()) | (1 << 5)); break; }
        case 0xef: { A.setBit(5); break; }

        case 0xf0: { B.setBit(6); break; }
        case 0xf1: { C.setBit(6); break; }
        case 0xf2: { D.setBit(6); break; }
        case 0xf3: { E.setBit(6); break; }
        case 0xf4: { H.setBit(6); break; }
        case 0xf5: { L.setBit(6); break; }
        case 0xf6: { wait = 4; write(HL.getVal(), read(HL.getVal()) | (1 << 6)); break; }
        case 0xf7: { A.setBit(6); break; }
        case 0xf8: { B.setBit(7); break; }
        case 0xf9: { C.setBit(7); break; }
        case 0xfa: { D.setBit(7); break; }
        case 0xfb: { E.setBit(7); break; }
        case 0xfc: { H.setBit(7); break; }
        case 0xfd: { L.setBit(7); break; }
        case 0xfe: { wait = 4; write(HL.getVal(), read(HL.getVal()) | (1 << 7)); break; }
        case 0xff: { A.setBit(7); break; }

        default: { printf("Unknown CB postfix\n"); break; }
    }
}
