// C++ code for RISC-V simulator
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <bitset>
#include <emscripten/bind.h>
#include "assembler.cpp"
using namespace std;

// Global variables for simulator state
string rz, ry, ra, rb;
long long int cycles = 0;
long long int instructions = 0;
string consoleOutput = "";

// Forward declarations of classes
struct RegisterFile;
struct ALU;
struct PMI;
struct IAG;
class functions;
class control_circuitry;

// Global instances that will be accessed by exported functions
ALU *g_alu = nullptr;
RegisterFile *g_registers = nullptr;
PMI *g_memory = nullptr;
IAG *g_iag = nullptr;
control_circuitry *g_control = nullptr;
bool g_running = true;

// Helper function to append to console output
void appendToConsole(const string &text)
{
    consoleOutput += text + "\n";
}

// Clear console output
void clearConsole()
{
    consoleOutput = "";
}

// hexadecimal to integer
int hex_to_dec(string hex)
{
    int dec = 0;
    int base = 1;

    for (int i = hex.size() - 1; i >= 0; i--)
    {
        char ch = hex[i];

        int val;
        if (ch >= '0' && ch <= '9')
            val = ch - '0';
        else if (ch >= 'A' && ch <= 'F')
            val = ch - 'A' + 10;
        else if (ch >= 'a' && ch <= 'f')
            val = ch - 'a' + 10;
        else
        {
            appendToConsole("=> Invalid hexadecimal character: " + string(1, ch));
            throw invalid_argument("=> Invalid hexadecimal character: " + string(1, ch));
        }

        dec += val * base;
        base *= 16;
    }

    return dec;
}

string dec_to_hex_32bit(int num)
{
    stringstream ss;
    ss << uppercase << setw(8) << setfill('0') << hex << (num & 0xFFFFFFFF);
    return ss.str();
}

string bin_to_hex(string bin)
{
    if (bin.find_first_not_of('0') == string::npos)
    {
        return "0";
    }
    bin = bin.substr(bin.find_first_not_of('0'));

    if (bin.size() > 32)
    {
        appendToConsole("=> Input number is more than 32 bits");
        throw out_of_range("=> Input number is more than 32 bits");
    }

    int gap = 32 - bin.size();
    for (int i = 0; i < gap; i++)
        bin = '0' + bin;
    string hex = "";
    for (int i = 0; i < bin.size(); i += 4)
    {
        int dec = 0;
        for (int j = 0; j < 4; j++)
        {
            dec *= 2;
            dec += bin[i + j] - '0';
        }
        if (dec < 10)
            hex += dec + '0';
        else
            hex += dec - 10 + 'A';
    }
    return hex;
}

string hex_to_bin(string hex)
{
    string binary = "";

    for (char ch : hex)
    {
        int num;
        if (ch >= '0' && ch <= '9')
            num = ch - '0'; // Convert '0'-'9' to 0-9
        else if (ch >= 'A' && ch <= 'F')
            num = ch - 'A' + 10; // Convert 'A'-'F' to 10-15
        else if (ch >= 'a' && ch <= 'f')
            num = ch - 'a' + 10; // Convert 'a'-'f' to 10-15
        else
            return "Invalid Hex Input"; // Error handling

        // Convert num (0-15) to a 4-bit binary string using bitwise operations
        for (int i = 3; i >= 0; --i)
        {
            binary += ((num >> i) & 1) ? '1' : '0';
        }
    }

    return binary;
}

int hex_to_dec_signed(const std::string &hexStr)
{
    // Remove any leading 0x or 0X from the hex string
    std::string cleanHex = hexStr;
    if (hexStr.find("0x") == 0 || hexStr.find("0X") == 0)
    {
        cleanHex = hexStr.substr(2);
    }

    // Determine the number of bits in the input
    size_t bitSize = cleanHex.size() * 4; // Each hex digit represents 4 bits

    // Convert the hexadecimal string to an unsigned long long value
    unsigned long long value = 0;
    std::stringstream ss;
    ss << std::hex << cleanHex;
    ss >> value;

    // Check the most significant bit (MSB) to determine if the number is negative
    unsigned long long msbMask = 1ULL << (bitSize - 1);
    if (value & msbMask)
    {
        // If negative, calculate the two's complement
        long long negativeValue = static_cast<long long>(value - (1ULL << bitSize));
        return negativeValue;
    }

    // If positive, return the value as-is
    return static_cast<int>(value);
}

// Helper function to extract a specific range of bits
string extractBits(const string &binary, int start, int end)
{
    return binary.substr(start, end - start + 1);
}

struct RegisterFile
{
    vector<string> regs;
    int rs1, rs2, rd;
    int DONT_CARE = -1;

    RegisterFile()
    {
        regs.resize(32, "00000000");
        regs[2] = "7FFFFFDC"; // stack pointer
        rs1 = rs2 = rd = DONT_CARE;
    }

    void setAddresses(int rs1_addr = -1, int rs2_addr = -1, int rd_addr = -1)
    {
        if ((rs1_addr != DONT_CARE && (rs1_addr < 0 || rs1_addr >= 32)) ||
            (rs2_addr != DONT_CARE && (rs2_addr < 0 || rs2_addr >= 32)) ||
            (rd_addr != DONT_CARE && (rd_addr < 0 || rd_addr >= 32)))
        {
            appendToConsole("=> Invalid register address");
            throw invalid_argument("=> Invalid register address");
        }
        rs1 = rs1_addr;
        rs2 = rs2_addr;
        rd = rd_addr;
    }

    void readRS()
    {
        if (rs1 != DONT_CARE)
            ra = regs[rs1];
        if (rs2 != DONT_CARE)
            rb = regs[rs2];
        appendToConsole("=> RA: " + ra + " RB: " + rb);
    }

    void writeRD()
    {
        appendToConsole(" ");
        appendToConsole("WRITE BACK STAGE");
        cycles++;
        string value = ry;
        if (rd != DONT_CARE && rd != 0)
            regs[rd] = value;
        appendToConsole("=> RD: x" + to_string(rd) + " Value: " + value);
    }

    string getRegisterValue(int index)
    {
        if (index >= 0 && index < 32)
        {
            return regs[index];
        }
        return "00000000";
    }

    string getAllRegisters()
    {
        string result = "";
        for (int i = 0; i < 32; i++)
        {
            result += "x" + to_string(i) + ":" + regs[i] + ";";
        }
        return result;
    }
};

struct ALU
{
    void perform_op(string operation)
    {
        int aVal = hex_to_dec_signed(ra);
        int bVal = hex_to_dec_signed(rb);
        appendToConsole("=> RA: " + ra + " RB: " + rb);
        appendToConsole("=> Aval: " + to_string(aVal) + " Bval: " + to_string(bVal));
        int result = 0;

        if (operation == "add")
            result = aVal + bVal;
        else if (operation == "sub")
            result = aVal - bVal;
        else if (operation == "and")
            result = aVal & bVal;
        else if (operation == "or")
            result = aVal | bVal;
        else if (operation == "xor")
            result = aVal ^ bVal;
        else if (operation == "sll")
            result = (static_cast<unsigned>(aVal) << (bVal & 31));
        else if (operation == "srl")
            result = (static_cast<unsigned>(aVal) >> (bVal & 31));
        else if (operation == "sra")
            result = (aVal >> (bVal & 31));
        else if (operation == "slt")
            result = (aVal < bVal) ? 1 : 0;
        else if (operation == "mul")
            result = aVal * bVal;
        else if (operation == "div")
            result = aVal / bVal;
        else if (operation == "rem")
            result = aVal % bVal;
        else if (operation == "beq")
            result = (aVal == bVal) ? 1 : 0;
        else if (operation == "bne")
            result = (aVal != bVal) ? 1 : 0;
        else if (operation == "blt")
            result = (aVal < bVal) ? 1 : 0;
        else if (operation == "bge")
            result = (aVal >= bVal) ? 1 : 0;
        else
        {
            appendToConsole("=> Invalid ALU operation: " + operation);
            throw invalid_argument("=> Invalid ALU operation: " + operation);
        }

        rz = dec_to_hex_32bit(result);
    }
};

// Memory Structure
struct Memory
{
    map<int, string> text;         // Text Segment
    map<int, string> static_data;  // Static Data Segment
    map<int, string> dynamic_data; // Dynamic Data Segment
};

// PMI (Processor Memory Interface)
struct PMI
{
    string MAR; // Memory Address Register
    string MDR; // Memory Data Register
    Memory mem;

    PMI() : mem() {}

    // Load from mem into MDR
    void load(char type)
    {
        int address = hex_to_dec(MAR);

        if (address < 268435456)
        {
            MDR = mem.text[address] + mem.text[address + 1] + mem.text[address + 2] + mem.text[address + 3]; // Loading word as instruction is always 32 bits
            if (MDR == "")
            {
                appendToConsole("=> No instruction at this address." + MAR);
                throw runtime_error("No instruction at this address.\n");
            }
        }
        else if (address < 268468224)
        {
            // address-=10; // Adjusting address to start from 0
            switch (type)
            {
            case 'd':
                appendToConsole("=> Loading double is not possible in a 32 bit register.");
                throw runtime_error("Loading double is not possible in a 32 bit register.\n");
                break;
            case 'b':
                MDR = mem.static_data[address];
                break; // Loading byte
            case 'h':
                MDR = mem.static_data[address] + mem.static_data[address+1];
                break; // Loading half word
            default:
                MDR = mem.static_data[address] + mem.static_data[address + 1] + mem.static_data[address + 2] + mem.static_data[address+3];
                break; // Loading word by default
            }
            if (MDR == "")
                MDR = "00000000"; // If no data at this address, return 0
        }
        else
        {
            // address-=20; // Adjusting address to start from 0
            switch (type)
            {
            case 'd':
                appendToConsole("=> Loading double is not possible in a 32 bit register.");
                throw runtime_error("Loading double is not possible in a 32 bit register.\n");
                break;
            case 'b':
                MDR = mem.dynamic_data[address];
                break; // Loading byte
            case 'h':
                MDR = mem.dynamic_data[address] + mem.dynamic_data[address+1];
                break; // Loading half word
            default:
                MDR = mem.dynamic_data[address] + mem.dynamic_data[address + 1] + mem.dynamic_data[address + 2] + mem.dynamic_data[address+3];
                break; // Loading word by default
            }
            if (MDR == "")
                MDR = "00000000"; // If no data at this address, return 0
        }
        while (MDR.size() < 8)
            MDR = "0" + MDR;
        appendToConsole("=> Loaded Data: " + MDR + " from " + MAR);
    }

    // Store MDR value into mem
    void store(char type)
    {
        int address = hex_to_dec(MAR);

        if (address < 268435456)
        {
            mem.text[address] = MDR.substr(0, 2);
            mem.text[address + 1] = MDR.substr(2, 2);
            mem.text[address + 2] = MDR.substr(4, 2);
            mem.text[address + 3] = MDR.substr(6, 2); // Storing word as instruction is always 32 bits
        }
        else if (address < 268468224)
        {
            // address-=10; // Adjusting address to start from 0
            switch (type)
            {
            case 'd':
                appendToConsole("=> Loading double is not possible in a 32 bit register.");
                throw runtime_error("Loading double is not possible in a 32 bit register.\n");
                break;
            case 'b':
                mem.static_data[address] = MDR.substr(6, 2);
                break; // Storing byte
            case 'h':
                mem.static_data[address] = MDR.substr(4, 2);
                mem.static_data[address+1] = MDR.substr(6, 2);
                break; // Storing half word
            default:
                mem.static_data[address] = MDR.substr(0, 2);
                mem.static_data[address + 1] = MDR.substr(2, 2);
                mem.static_data[address + 2] = MDR.substr(4, 2);
                mem.static_data[address + 3] = MDR.substr(6, 2);
                break; // Storing word by default
            }
        }
        else
        {
            // address-=20; // Adjusting address to start from 0
            switch (type)
            {
            case 'd':
                appendToConsole("=> Loading double is not possible in a 32 bit register.");
                throw runtime_error("Loading double is not possible in a 32 bit register.\n");
                break;
            case 'b':
                mem.dynamic_data[address] = MDR.substr(6, 2);
                break; // Storing byte
            case 'h':
                mem.dynamic_data[address] = MDR.substr(4, 2);
                mem.dynamic_data[address+1] = MDR.substr(6, 2);
                break; // Storing half word
            default:
                mem.dynamic_data[address] = MDR.substr(0, 2);
                mem.dynamic_data[address + 1] = MDR.substr(2, 2);
                mem.dynamic_data[address + 2] = MDR.substr(4, 2);
                mem.dynamic_data[address + 3] = MDR.substr(6, 2);
                break; // Storing word by default
            }
        }
        appendToConsole("=> Stored Data: " + MDR + " at " + MAR);
    }

    // Get memory content for a specific segment
    string getMemoryContent(string segment, int startAddr, int count)
    {
        map<int, string> *memSegment;

        if (segment == "text")
        {
            memSegment = &mem.text;
        }
        else if (segment == "static")
        {
            memSegment = &mem.static_data;
        }
        else if (segment == "dynamic")
        {
            memSegment = &mem.dynamic_data;
        }
        else
        {
            return "Invalid segment";
        }

        string result = "";
        for (int i = 0; i < count; i++)
        {
            int addr = startAddr + i ;
            string addrHex = "0x" + dec_to_hex_32bit(addr);

            string value = "";
            if (addr < 268435456 && segment == "text")
            {
                value = (*memSegment)[addr] + (*memSegment)[addr+1] +
                        (*memSegment)[addr + 2] + (*memSegment)[addr+3];
            }
            else
            {
                value = (*memSegment)[addr ] + (*memSegment)[addr + 1] +
                        (*memSegment)[addr + 2] + (*memSegment)[addr + 3];
            }

            if (value.empty())
                value = "00000000";
            result += addrHex + ":" + value + ";";
        }

        return result;
    }
};

struct IAG
{
    string pc;
    string branch_offset;
    string return_addr;
    bool use_return_addr;
    bool branch_taken;

    IAG()
    {
        pc = "00000000";
    }

    void compute_nextPC(string branch_offset, string return_addr, bool use_return_addr, bool branch_taken)
    {
        int pc_value = hex_to_dec(pc);
        int next_pc_value;

        if (branch_taken)
            next_pc_value = pc_value + hex_to_dec(branch_offset);
        else
            next_pc_value = pc_value + 4;

        if (use_return_addr)
            next_pc_value = hex_to_dec(return_addr);

        pc = dec_to_hex_32bit(next_pc_value);
    }

    string getPC()
    {
        return pc;
    }
};

class functions
{
public:
    string instr, opcode, rd, funct3, rs1, rs2, funct7, imm, instr_type;

    PMI &memory;
    IAG &iag;
    RegisterFile &registers;
    ALU &alu;

    functions(PMI &mem, IAG &iagRef, RegisterFile &reg, ALU &aluRef)
        : memory(mem), iag(iagRef), registers(reg), alu(aluRef) {}

private:
    string getInstructionType()
    {
        if (opcode == "0110011")
        { // R-type
            if (funct3 == "000" && funct7 == "0000000")
                return "add";
            if (funct3 == "000" && funct7 == "0100000")
                return "sub";
            if (funct3 == "111")
                return "and";
            if (funct3 == "110" && funct7 == "0000000")
                return "or";
            if (funct3 == "100" && funct7 == "0000000")
                return "xor";
            if (funct3 == "001" && funct7 == "0000000")
                return "sll";
            if (funct3 == "101" && funct7 == "0000000")
                return "srl";
            if (funct3 == "101" && funct7 == "0100000")
                return "sra";
            if (funct3 == "010")
                return "slt";
            if (funct3 == "000" && funct7 == "0000001")
                return "mul";
            if (funct3 == "100" && funct7 == "0000001")
                return "div";
            if (funct3 == "110" && funct7 == "0000001")
                return "rem";
        }
        else if (opcode == "0010011")
        { // I-type
            if (funct3 == "000")
                return "add";
            if (funct3 == "111")
                return "and";
            if (funct3 == "110")
                return "or";
            if (funct3 == "100")
                return "xor";
            if (funct3 == "001")
                return "sll";
            if (funct3 == "101" && funct7 == "0000000")
                return "srl";
            if (funct3 == "101" && funct7 == "0100000")
                return "sra";
            if (funct3 == "010")
                return "slt";
        }
        else if (opcode == "0000011")
        { // Load instructions
            if (funct3 == "000")
                return "lb";
            if (funct3 == "001")
                return "lh";
            if (funct3 == "010")
                return "lw";
            if (funct3 == "011")
                return "ld";
        }
        else if (opcode == "0100011")
        { // S-type (Store instructions)
            if (funct3 == "000")
                return "sb";
            if (funct3 == "001")
                return "sh";
            if (funct3 == "010")
                return "sw";
            if (funct3 == "011")
                return "sd";
        }
        else if (opcode == "1100011")
        { // B-type (Branch instructions)
            if (funct3 == "000")
                return "beq";
            if (funct3 == "001")
                return "bne";
            if (funct3 == "100")
                return "blt";
            if (funct3 == "101")
                return "bge";
        }
        else if (opcode == "0110111")
        { // U-type
            return "lui";
        }
        else if (opcode == "0010111")
        { // U-type
            return "auipc";
        }
        else if (opcode == "1101111")
        { // UJ-type
            return "jal";
        }
        else if (opcode == "1100111")
        { // I-type (JALR)
            return "jalr";
        }
        else if (instr == "00000073")
        { // ECALL
            return "ecall";
        }

        return "unknown"; // If no match is found
    }

    string getImmediate(const string &binaryInstr, const string &opcode)
    {
        string im_val = "";

        if (opcode == "0010011" || opcode == "0000011" || opcode == "1100111")
        {                                             // I-type
            im_val = extractBits(binaryInstr, 0, 11); // Bits 0-11
        }
        else if (opcode == "0100011")
        {                                                                               // S-type
            im_val = extractBits(binaryInstr, 0, 6) + extractBits(binaryInstr, 20, 24); // Bits 0-6 and 20-24
        }
        else if (opcode == "1100011")
        { // Branch-type
            // since as per logic , this imm value should be multiplied by 2 so add 0 in the end
            im_val = extractBits(binaryInstr, 0, 0) + extractBits(binaryInstr, 24, 24) +
                     extractBits(binaryInstr, 1, 6) + extractBits(binaryInstr, 20, 23) + "0"; // Bits 0, 24, 1-6, 20-23
        }
        else if (opcode == "0110111" || opcode == "0010111")
        {                                             // U-type
            im_val = extractBits(binaryInstr, 0, 19); // Bits 0-19
            im_val.append(12, '0');                   // Append 12 zeros to shift left by 12 bits
        }
        else if (opcode == "1101111")
        { // J-type
            im_val = "00000000000000000000";

            im_val[20] = binaryInstr[0]; // Bit 0

            for (int i = 1; i <= 10; i++)
            {

                im_val[11 - i] = binaryInstr[i]; // Bits 10-1
            }

            im_val[11] = binaryInstr[11];

            for (int i = 12; i <= 19; i++)
            {

                im_val[31 - i] = binaryInstr[i]; // Bits 19-12
            }

            reverse(im_val.begin(), im_val.end());
        }
        else if (opcode == "0110011")
        { // R-type (add, and, or, sll, slt, sra, srl, sub, xor, mul, div, rem)
            // R-type instructions do not have an immediate value
            im_val = "0"; // No immediate value for R-type
        }

        if (im_val[0] == '1')
        {

            int n = 32 - im_val.size();

            for (int i = 0; i < n; i++)
            {

                im_val = "1" + im_val;
            }
        }
        else
        {

            int n = 32 - im_val.size();

            for (int i = 0; i < n; i++)
            {

                im_val = "0" + im_val;
            }
        }

        return im_val;
    }

public:
    void fetch()
    {
        appendToConsole(" ");
        appendToConsole("FETCH STAGE");
        // get the instruction from global variable pc
        memory.MAR = iag.pc;
        memory.load('I');
        instr = memory.MDR;
        cycles++;

        appendToConsole("=> Fetched Instruction: " + instr + " from " + memory.MAR);
    }

    void decode()
    {
        appendToConsole(" ");
        appendToConsole("DECODE STAGE");
        // decode the hex instruction to get the opcode, func3, func7, rs1, rs2, rd, imm
        string binaryInstr = hex_to_bin(instr);

        // Extract fields based on RISC-V instruction format
        opcode = extractBits(binaryInstr, 25, 31); // Bits 25-31
        rd = extractBits(binaryInstr, 20, 24);     // Bits 20-24
        funct3 = extractBits(binaryInstr, 17, 19); // Bits 17-19
        rs1 = extractBits(binaryInstr, 12, 16);    // Bits 12-16
        rs2 = extractBits(binaryInstr, 7, 11);     // Bits 7-11
        funct7 = extractBits(binaryInstr, 0, 6);   // Bits 0-6
        imm = getImmediate(binaryInstr, opcode);
        imm = bin_to_hex(imm);

        instr_type = getInstructionType();

        cycles++;

        appendToConsole("=> Instruction Decoded Sucessfullly");
        appendToConsole("=> Machine Code: " + memory.MDR + ", Instruction type: " + instr_type + ", Opcode: " + opcode +
                        ", Func3 :" + funct3 + ", Func7 :" + funct7 + ", RD :" + rd + ", RS1 :" + rs1 +
                        ", RS2 :" + rs2 + ", Imm :" + imm);
    }

    void execute(string op)
    {
        appendToConsole(" ");
        appendToConsole("EXECUTE STAGE");
        appendToConsole(op);
        alu.perform_op(op);
        cycles++;

        appendToConsole("=> Executed Instruction: " + op);
        appendToConsole("=> Result in RZ is " + rz);
    }

    void accessMemory(string address, string data, char type, bool isWrite)
    {
        appendToConsole(" ");
        appendToConsole("MEMORY STAGE");
        if (isWrite)
        {
            memory.MAR = address;
            memory.MDR = data;
            memory.store(type);
        }
        else
        {
            memory.MAR = address;
            memory.load(type);
        }
        cycles++;
    }
    void accessMemory(string address, char type, bool isWrite)
    {

        accessMemory(address, "", type, isWrite);
    }
};

class control_circuitry
{
public:
    functions f;

    control_circuitry(ALU &alu, RegisterFile &registers, PMI &memory, IAG &iag)
        : f(memory, iag, registers, alu)
    {
    }

    bool step()
    {
        bool flag = true;
        instructions++;
        try
        {
            f.fetch();
            f.decode();
            string opcode = f.opcode;

            // appendToConsole("fetch and decode have been done and now the control circuitry will work action specific");

            if (opcode == "1110011")
            {
                flag = false;
                appendToConsole("=> End of the Program Encountered");
                g_running = false;
            }
            else if (opcode == "0110011")
            {
                // R type execution instructions
                f.registers.setAddresses(stoi(f.rs1, nullptr, 2), stoi(f.rs2, nullptr, 2), stoi(f.rd, nullptr, 2));
                f.registers.readRS();
                f.execute(f.instr_type);
                f.iag.compute_nextPC("", "", false, false);
                ry = rz;
                f.registers.writeRD();
            }
            else if (opcode == "0010011")
            {
                // I type execution instructions
                f.registers.setAddresses(stoi(f.rs1, nullptr, 2), stoi(f.rs2, nullptr, 2), stoi(f.rd, nullptr, 2));
                f.registers.readRS();
                rb = f.imm;
                f.execute(f.instr_type);
                f.iag.compute_nextPC("", "", false, false);
                ry = rz;
                f.registers.writeRD();
            }
            else if (opcode == "0000011")
            {
                // load instructions
                f.registers.setAddresses(stoi(f.rs1, nullptr, 2), stoi(f.rs2, nullptr, 2), stoi(f.rd, nullptr, 2));
                f.registers.readRS();
                rb = f.imm;
                f.execute("add");
                f.iag.compute_nextPC("", "", false, false);
                f.accessMemory(rz, f.instr_type.back(), false);
                ry = f.memory.MDR;
                f.registers.writeRD();
            }
            else if (opcode == "0100011")
            {
                // store instructions
                f.registers.setAddresses(stoi(f.rs1, nullptr, 2), stoi(f.rs2, nullptr, 2), stoi(f.rd, nullptr, 2));
                f.registers.readRS();
                string temp = rb;
                rb = f.imm;
                f.execute("add");
                f.iag.compute_nextPC("", "", false, false);
                f.accessMemory(rz, temp, f.instr_type.back(), true);
            }
            else if (opcode == "1100011")
            {
                // branch instructions
                f.registers.setAddresses(stoi(f.rs1, nullptr, 2), stoi(f.rs2, nullptr, 2), stoi(f.rd, nullptr, 2));
                f.registers.readRS();
                f.execute(f.instr_type);
                appendToConsole("=> Branch Offset is: " + f.imm);

                if (rz == "00000001")
                    f.iag.compute_nextPC(f.imm, "", false, true);
                else
                    f.iag.compute_nextPC("", "", false, false);
                appendToConsole("=> The new PC is: " + f.iag.pc);
            }
            else if (opcode == "0110111")
            {
                // lui
                f.registers.setAddresses(stoi(f.rs1, nullptr, 2), stoi(f.rs2, nullptr, 2), stoi(f.rd, nullptr, 2));
                rz = f.imm;
                f.iag.compute_nextPC("", "", false, false);
                ry = rz;
                f.registers.writeRD();
            }
            else if (opcode == "0010111")
            {
                // auipc
                f.registers.setAddresses(stoi(f.rs1, nullptr, 2), stoi(f.rs2, nullptr, 2), stoi(f.rd, nullptr, 2));
                ra = f.iag.pc;
                rb = f.imm;
                f.execute("add");
                f.iag.compute_nextPC("", "", false, false);
                ry = rz;
                f.registers.writeRD();
            }
            else if (opcode == "1101111")
            {
                // jal
                f.registers.setAddresses(stoi(f.rs1, nullptr, 2), stoi(f.rs2, nullptr, 2), stoi(f.rd, nullptr, 2));

                ry = dec_to_hex_32bit(hex_to_dec(f.iag.pc) + 4);
                f.iag.compute_nextPC(f.imm, "", false, true);
                f.registers.writeRD();
            }
            else if (opcode == "1100111")
            {
                // jalr
                f.registers.setAddresses(stoi(f.rs1, nullptr, 2), stoi(f.rs2, nullptr, 2), stoi(f.rd, nullptr, 2));
                f.registers.readRS();

                string temp = dec_to_hex_32bit(hex_to_dec(f.iag.pc) + 4);

                rb = f.imm;
                f.execute("add");
                f.iag.compute_nextPC("", rz, true, false);
                appendToConsole("=> RA, RB, Imm, RD -> " + ra + " " + rb + " " + f.imm + " " + f.rd);
                appendToConsole("=> Imm value " + f.imm + " is added to RS2 " + rb + " to get Return Address " + rz + " and the Next Address " + temp + " is stored to " + ra);

                rz = temp;
                f.registers.writeRD();
            }
        }
        catch (const exception &e)
        {
            appendToConsole("=> Error: " + string(e.what()));
            flag = false;
        }
        return flag;
    }

    void run()
    {
        bool running = true;
        while (running)
        {
            running = step();
            appendToConsole("");
        }
    }
};

// Class to expose to JavaScript
class RiscVSimulator
{
public:
    RiscVSimulator() : initialized(false) {}
    
    string assemble(const string &code) {
        appendToConsole("=> Assembling code...");
        return ::assemble(code);
    }

    void init()
    {
        if (initialized)
        {
            cleanup();
        }
        clearConsole();

        g_alu = new ALU();
        g_registers = new RegisterFile();
        g_memory = new PMI();
        g_iag = new IAG();
        g_control = new control_circuitry(*g_alu, *g_registers, *g_memory, *g_iag);
        g_running = true;
        cycles = 0;
        instructions = 0;
        initialized = true;

        appendToConsole("=> Simulator initialized");
    }

    void cleanup()
    {
        if (initialized)
        {
            delete g_alu;
            delete g_registers;
            delete g_memory;
            delete g_iag;
            delete g_control;

            g_alu = nullptr;
            g_registers = nullptr;
            g_memory = nullptr;
            g_iag = nullptr;
            g_control = nullptr;

            initialized = false;
        }
    }

    void loadCode(const string &codeStr)
    {
        if (!initialized)
        {
            throw runtime_error("Simulator not initialized");
        }

        clearConsole();

        stringstream ss(codeStr);
        string line;
        bool memory_flag = false;

        while (getline(ss, line))
        {
            if (line.empty())
            {
                memory_flag = true;
                continue;
            }

            if (memory_flag)
            {
                string addr = line.substr(2, 8);
                string code = "000000" + line.substr(11, 2);
                g_memory->MAR = addr;
                g_memory->MDR = code;
                g_memory->store('b');
            }
            else
            {
                // Parse address and instruction
                size_t pos = line.find(' ');
                if (pos == string::npos)
                    continue;

                string addrStr = line.substr(2, pos - 2);
                string instrStr = line.substr(pos + 1);
                instrStr = instrStr.substr(2, instrStr.find_first_of(' ') - 2);

                // Store in memory
                g_memory->MAR = addrStr;
                g_memory->MDR = instrStr;
                g_memory->store('w');
            }
        }

        appendToConsole("=> Code loaded successfully");
    }

    bool step()
    {
        if (!initialized)
        {
            throw runtime_error("Simulator not initialized");
        }

        if (!g_running)
        {
            appendToConsole("=> Simulation has ended");
            return false;
        }

        return g_control->step();
    }

    void run()
    {
        if (!initialized)
        {
            throw runtime_error("Simulator not initialized");
        }

        if (!g_running)
        {
            appendToConsole("=> Simulation has ended");
            return;
        }

        g_control->run();
    }

    void reset()
    {
        cleanup();
        init();
        appendToConsole("=> Simulator reset");
    }

    string showReg()
    {
        if (!initialized)
        {
            throw runtime_error("Simulator not initialized");
        }

        return g_registers->getAllRegisters();
    }

    string showMem(const string &segment, int startAddr, int count)
    {
        if (!initialized)
        {
            throw runtime_error("Simulator not initialized");
        }

        return g_memory->getMemoryContent(segment, startAddr, count);
    }

    string getPC()
    {
        if (!initialized)
        {
            throw runtime_error("Simulator not initialized");
        }

        return g_iag->getPC();
    }

    string getConsoleOutput()
    {
        return consoleOutput;
    }

    void clearConsoleOutput()
    {
        clearConsole();
    }

    int getCycleCount()
    {
        return cycles;
    }

    int getInstructionCount()
    {
        return instructions;
    }

private:
    bool initialized;
};

// Binding our C++ class to JavaScript
EMSCRIPTEN_BINDINGS(riscv_simulator)
{
    using namespace emscripten;

    class_<RiscVSimulator>("RiscVSimulator")
        .constructor<>()
        .function("init", &RiscVSimulator::init)
        .function("cleanup", &RiscVSimulator::cleanup)
        .function("loadCode", &RiscVSimulator::loadCode)
        .function("step", &RiscVSimulator::step)
        .function("run", &RiscVSimulator::run)
        .function("reset", &RiscVSimulator::reset)
        .function("showReg", &RiscVSimulator::showReg)
        .function("showMem", &RiscVSimulator::showMem)
        .function("getPC", &RiscVSimulator::getPC)
        .function("getConsoleOutput", &RiscVSimulator::getConsoleOutput)
        .function("clearConsoleOutput", &RiscVSimulator::clearConsoleOutput)
        .function("getCycleCount", &RiscVSimulator::getCycleCount)
        .function("assemble", &RiscVSimulator::assemble)
        .function("getInstructionCount", &RiscVSimulator::getInstructionCount);
}