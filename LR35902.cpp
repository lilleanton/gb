#include "LR35902.h"

namespace CPU {

LR35902::LR35902(Bus& b)
    : bus(b)
    , AF(A, F)
    , BC(B, C)
    , DE(D, E)
    , HL(H, L)
    , SP(0)
    , PC(0)
{}

int LR35902::read(const uint16_t& addr, int n) {
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

uint8_t LR35902::write(uint16_t addr, uint8_t val) {
    this->bus.write(addr, val);
    return 0;
}

uint8_t LR35902::write(uint16_t addr, Reg8& val) {
    this->bus.write(addr, val.getVal());
    return 0;
}

uint8_t LR35902::write(Reg16& addr, Reg8& val) {
    this->bus.write(addr.getVal(), val.getVal());
    return 0;
}

uint16_t LR35902::pc(int inc) {
    uint16_t old = this->PC;
    this->PC += inc;
    return old;
}

// Set flag
void LR35902::f(uint8_t ZHNC, uint8_t ZHNCmask, uint8_t on, uint8_t off) {
    uint8_t oldBits = (this->F.getVal() >> 4) & 0xF;
    oldBits = (oldBits | ((ZHNC>>4) & ZHNCmask)) & ~(~(ZHNC>>4) & ZHNCmask);
    //printf("0x%02x, Old flag\n", F);
    F       = ((oldBits | on) & ~off) << 4;
    //printf("0x%02x, New flag\n", F);
}

void LR35902::setRegisterStateJSON(json& data) {
    A = data["a"].get<uint8_t>();
    B = data["b"].get<uint8_t>();
    C = data["c"].get<uint8_t>();
    D = data["d"].get<uint8_t>();
    E = data["e"].get<uint8_t>();
    F = data["f"].get<uint8_t>();
    H = data["h"].get<uint8_t>();
    L = data["l"].get<uint8_t>();
    PC = data["pc"].get<uint16_t>();
    SP = data["sp"].get<uint16_t>();
}

bool LR35902::compareRegisterStateJSON(json& state) {
    if(A.getVal() != state["a"].get<uint8_t>()) { printf("%d != %d, comparison failed in register A\n", A.getVal(), state["a"].get<uint8_t>()); }
    if(B.getVal() != state["b"].get<uint8_t>()) { printf("%d != %d, comparison failed in register B\n", B.getVal(), state["b"].get<uint8_t>()); }
    if(C.getVal() != state["c"].get<uint8_t>()) { printf("%d != %d, comparison failed in register C\n", C.getVal(), state["c"].get<uint8_t>()); }
    if(D.getVal() != state["d"].get<uint8_t>()) { printf("%d != %d, comparison failed in register D\n", D.getVal(), state["d"].get<uint8_t>()); }
    if(E.getVal() != state["e"].get<uint8_t>()) { printf("%d != %d, comparison failed in register E\n", E.getVal(), state["e"].get<uint8_t>()); }
    if(F.getVal() != state["f"].get<uint8_t>()) { printf("%d != %d, comparison failed in register F\n", F.getVal(), state["f"].get<uint8_t>()); }
    if(H.getVal() != state["h"].get<uint8_t>()) { printf("%d != %d, comparison failed in register H\n", H.getVal(), state["h"].get<uint8_t>()); }
    if(L.getVal() != state["l"].get<uint8_t>()) { printf("%d != %d, comparison failed in register L\n", L.getVal(), state["l"].get<uint8_t>()); }
    if(SP != state["sp"].get<uint16_t>()) { printf("%d != %d, comparison failed in register SP\n", SP, state["sp"].get<uint16_t>()); }
    if(PC != state["pc"].get<uint16_t>()) { printf("%d != %d, comparison failed in register PC\n", PC, state["pc"].get<uint16_t>()); }

    return A.getVal() == state["a"].get<uint8_t>()
        && B.getVal() == state["b"].get<uint8_t>()
        && C.getVal() == state["c"].get<uint8_t>()
        && D.getVal() == state["d"].get<uint8_t>()
        && E.getVal() == state["e"].get<uint8_t>()
        && F.getVal() == state["f"].get<uint8_t>()
        && H.getVal() == state["h"].get<uint8_t>()
        && L.getVal() == state["l"].get<uint8_t>()
        && PC == state["pc"].get<uint16_t>()
        && SP == state["sp"].get<uint16_t>();
    return false;
}

void LR35902::printState() {
    printf("A(0x%02x) B(0x%02x) C(0x%02x) D(0x%02x) E(0x%02x) F(0x%02x) H(0x%02x) L(0x%02x) PC(0x%04x) SP(0x%04x)\n",
    A.getVal(), B.getVal(), C.getVal(), D.getVal(), E.getVal(), F.getVal(), H.getVal(), L.getVal(), PC, SP);
}

}
