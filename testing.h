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
    //cpu.printState();

    for (auto& ramEdit : state["ram"]) {
        // std::cout << ramEdit << std::endl;
        bus.write(ramEdit[0], ramEdit[1]);
    }
}

void testOpcode(Bus& bus, CPU::LR35902& cpu, std::string opcode, int numToTest = 1) {
    std::fstream f(std::format("V1/{}.json", opcode));
    json data = json::parse(f);

    for(int i = 0; i < numToTest; i++) {
        setMachineStateJSON(bus, cpu, data[i]["initial"]);
        cpu.insExecute();

        if(compareCpuState(cpu, data[i]["final"])){
           // printf("(%s)-test number %d, success\n", opcode.c_str(), i);
        } else {
            printf("Failed at index %d for opcode %s\n", i, opcode.c_str());
            break;
        }
    }
}

void test1byteOpcodes(Bus& bus, CPU::LR35902& cpu) {
    for(int i = 0x00; i <= 0xff; i++) {
        if(
            std::format("{:02x}", i) == "cb"
            || std::format("{:02x}", i) == "d3"
            || std::format("{:02x}", i) == "db"
            || std::format("{:02x}", i) == "dd"
            || std::format("{:02x}", i) == "e3"
            || std::format("{:02x}", i) == "e4"
            || std::format("{:02x}", i) == "eb"
            || std::format("{:02x}", i) == "ec"
            || std::format("{:02x}", i) == "ed"
            || std::format("{:02x}", i) == "f4"
            || std::format("{:02x}", i) == "fc"
            || std::format("{:02x}", i) == "fd"
        )
            continue;
        testOpcode(bus, cpu, std::format("{:02x}", i), 999);
    }
}

void testCBopcodes(Bus& bus, CPU::LR35902& cpu){
    for(int i = 0x00; i <= 0x2f; i++) {
        testOpcode(bus, cpu, std::format("cb {:02x}", i), 999);
    }
}
