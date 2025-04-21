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
using ll = long long int;
using ld = long double;
using namespace std;

// global controls
bool forwarding_enable = false, piplining_enable = true;
string printPipelineForInstruction = "";
vector<pair<string, string>> forwardingPaths;
vector<vector<string>> hazards;

// global variables which are used to simulate global registers
ll clock_cycle = 0;
ll instructionCt = 0;
ld CPI = 0;
ll DataTransferInstr = 0;
ll ALUInstr = 0;
ll ControlInstr = 0;
ll stalls = 0;
ll data_hazards = 0;
ll control_hazards = 0;
ll mispredictions = 0;
ll data_stalls = 0;
ll control_stalls = 0;

string rz, ry, ra, rb;
string consoleOutput = "";

// Forward declarations of classes
struct RegisterFile;
struct ALU;
struct PMI_data;
struct PMI_text;
struct IAG;
class functions;
class control_circuitry;
struct buffers;
struct BranchPredictor;

// Global instances that will be accessed by exported functions
PMI_data *g_data_memory = nullptr;
PMI_text *g_text_memory = nullptr;
IAG *g_iag = nullptr;
RegisterFile *g_registers = nullptr;
ALU *g_alu = nullptr;
buffers *g_buffers = nullptr;
BranchPredictor *g_brpre = nullptr;
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
            appendToConsole("Invalid hexadecimal character: " + string(1, ch));
            throw invalid_argument("Invalid hexadecimal character: " + string(1, ch));
        }

        dec += val * base;
        base *= 16;
    }

    return dec;
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
        appendToConsole("Input number is more than 32 bits");
        throw out_of_range("Input number is more than 32 bits");
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

// Helper function to extract a specific range of bits
string extractBits(const string &binary, int start, int end)
{
    return binary.substr(start, end - start + 1);
}

// buffers to store intemediate values between stages
struct IFID_buffer
{
    string pc, next_pc, instr;
    IFID_buffer()
    {
        pc = "ffffffff";
        next_pc = "ffffffff";
        instr = "";
    }
    void flush()
    {
        stalls++;
        pc = "ffffffff";
        next_pc = "ffffffff";
        instr = "";
    }
};
struct IDEX_buffer
{
    string pc, next_pc, instr, opcode, rd, funct3, rs1, rs2, funct7, imm, instr_type, rs1val, rs2val;
    bool mem_store_needed, mem_load_needed, wb_needed, branch_needed, jal, jalr;
    IDEX_buffer()
    {
        pc = "ffffffff";
        next_pc = "ffffffff";
        instr = "";
        opcode = "";
        rd = "";
        funct3 = "";
        rs1 = "";
        rs2 = "";
        funct7 = "";
        imm = "";
        instr_type = "";
        rs1val = "";
        rs2val = "";
        mem_store_needed = false;
        mem_load_needed = false;
        wb_needed = false;
        branch_needed = false;
        jal = false;
        jalr = false;
    }
    void flush()
    {
        stalls++;
        pc = "ffffffff";
        next_pc = "ffffffff";
        instr = "";
        opcode = "";
        rd = "";
        funct3 = "";
        rs1 = "";
        rs2 = "";
        funct7 = "";
        imm = "";
        instr_type = "";
        rs1val = "";
        rs2val = "";
        mem_store_needed = false;
        mem_load_needed = false;
        wb_needed = false;
        branch_needed = false;
        jal = false;
        jalr = false;
    }
};
struct EXMEM_buffer
{
    string pc, next_pc, instr, opcode, rd, funct3, rs1, rs2, funct7, imm, instr_type, rs1val, rs2val, exe_out;
    bool mem_store_needed, mem_load_needed, wb_needed;
    EXMEM_buffer()
    {
        pc = "ffffffff";
        next_pc = "ffffffff";
        instr = "";
        opcode = "";
        rd = "";
        funct3 = "";
        rs1 = "";
        rs2 = "";
        funct7 = "";
        imm = "";
        instr_type = "";
        rs1val = "";
        rs2val = "";
        exe_out = "";
        mem_store_needed = false;
        mem_load_needed = false;
        wb_needed = false;
    }
    void flush()
    {
        stalls++;
        pc = "ffffffff";
        next_pc = "ffffffff";
        instr = "";
        opcode = "";
        rd = "";
        funct3 = "";
        rs1 = "";
        rs2 = "";
        funct7 = "";
        imm = "";
        instr_type = "";
        rs1val = "";
        rs2val = "";
        exe_out = "";
        mem_store_needed = false;
        mem_load_needed = false;
        wb_needed = false;
    }
};
struct MEMWB_buffer
{
    string pc, next_pc, instr, opcode, rd, funct3, rs1, rs2, funct7, imm, instr_type, rs1val, rs2val, exe_out;
    bool wb_needed;
    MEMWB_buffer()
    {
        stalls++;
        pc = "ffffffff";
        next_pc = "ffffffff";
        instr = "";
        opcode = "";
        rd = "";
        funct3 = "";
        rs1 = "";
        rs2 = "";
        funct7 = "";
        imm = "";
        instr_type = "";
        rs1val = "";
        rs2val = "";
        exe_out = "";
        wb_needed = false;
    }
    void flush()
    {
        pc = "ffffffff";
        next_pc = "ffffffff";
        instr = "";
        opcode = "";
        rd = "";
        funct3 = "";
        rs1 = "";
        rs2 = "";
        funct7 = "";
        imm = "";
        instr_type = "";
        rs1val = "";
        rs2val = "";
        exe_out = "";
        wb_needed = false;
    }
};
struct buffers
{
    IFID_buffer ifid;
    IDEX_buffer idex;
    EXMEM_buffer exmem;
    MEMWB_buffer memwb;

    buffers()
    {
        ifid = IFID_buffer();
        idex = IDEX_buffer();
        exmem = EXMEM_buffer();
        memwb = MEMWB_buffer();
    }
    void flush()
    {
        ifid.flush();
        idex.flush();
        exmem.flush();
        memwb.flush();
    }
};

struct IAG
{
    string pc;
    string return_addr;
    bool use_return_addr;

    IAG()
    {
        pc = "00000000";
        return_addr = "00000000";
        use_return_addr = false;
    }

    void update(string return_addr, bool use_return_addr)
    {
        this->return_addr = return_addr;
        this->use_return_addr = use_return_addr;
    }

    void compute_nextPC()
    {
        int pc_value = hex_to_dec(pc);
        int next_pc_value;

        next_pc_value = pc_value + 4;

        if (use_return_addr)
        {
            next_pc_value = hex_to_dec(return_addr);
            use_return_addr = false;
            return_addr = "00000000";
        }

        pc = dec_to_hex_32bit(next_pc_value);
    }

    string getPC()
    {
        return pc;
    }
};

struct ALU
{
    string operation;

    void perform_op()
    {
        int aVal = hex_to_dec_signed(ra);
        int bVal = hex_to_dec_signed(rb);
        // appendToConsole("ra: " + ra + " rb: " + rb);
        // appendToConsole("aVal: " + to_string(aVal) + " bVal: " + to_string(bVal));

        int result = 0;

        if (operation == "add" || operation == "lw" || operation == "lh" || operation == "lb" || operation == "ld" || operation == "sw" || operation == "sh" || operation == "sb" || operation == "sd" || operation == "auipc" || operation == "jalr")
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
        else if (operation == "lui" || operation == "jal")
        {
            ALUInstr--;
            result = bVal;
        }
        else
        {
            appendToConsole("Invalid ALU operation: " + operation);
            throw invalid_argument("Invalid ALU operation: " + operation);
        }

        if (operation == "")
            rz = "";
        else
            rz = dec_to_hex_32bit(result);
        // appendToConsole("result in rz: " + rz);
    }
};

// Memory Structure
struct Memory
{
    map<int, string> memory;

    string getMemoryContent(int startAddr, int count)
    {
        string result = "";
        for (int i = 0; i < count; i++)
        {
            int addr = startAddr + i;
            string addrHex = "0x" + dec_to_hex_32bit(addr);

            string value = "";
            if (memory.find(addr) != memory.end())
            {
                value = memory[addr] + memory[addr + 1] +
                        memory[addr + 2] + memory[addr + 3];
            }
            else
            {
                value = "00";
            }

            result += addrHex + ":" + value + ";";
        }
        return result;
    }
};

// PMI (Processor Memory Interface)
struct PMI_text
{
    string MAR; // Memory Address Register
    string MDR; // Memory Data Register
    Memory mem;

    PMI_text() : mem() {}

    // Store MDR value into mem
    void store()
    {
        int address = hex_to_dec(MAR);

        if (address < 268435456)
        {
            mem.memory[address] = MDR.substr(0, 2);
            mem.memory[address + 1] = MDR.substr(2, 2);
            mem.memory[address + 2] = MDR.substr(4, 2);
            mem.memory[address + 3] = MDR.substr(6, 2); // Storing word as instruction is always 32 bits
            // appendToConsole("Stored instruction: " + MDR + " at " + MAR);
        }
        else
        {
            appendToConsole("This memory is for text segment only and does not have access to static and dynamic data segment.");
            throw runtime_error("This memory is for text segment only and does not have access to static and dynamic data segment.\\n");
        }
    }

    void load()
    {
        int address = hex_to_dec(MAR);

        if (address < 268435456)
        {
            MDR = "";
            MDR += mem.memory[address];
            MDR += mem.memory[address + 1];
            MDR += mem.memory[address + 2];
            MDR += mem.memory[address + 3];

            // appendToConsole("Loaded instruction: " + MDR + " from " + MAR);
        }
        else
        {
            appendToConsole("This memory is of text segment only and does not have access to static and dynamic data segment.");
            throw runtime_error("This memory is of text segment only and does not have access to static and dynamic data segment.\\n");
        }
    }

    string getMemoryContent(int startAddr, int count)
    {
        return mem.getMemoryContent(startAddr, count);
    }
};

// PMI (Processor Memory Interface)
struct PMI_data
{
    string MAR; // Memory Address Register
    string MDR; // Memory Data Register
    Memory mem;

    PMI_data() : mem() {}

    // Load from mem into MDR
    void load(string type)
    {
        int address = hex_to_dec(MAR);

        if (address < 268435456)
        {
            appendToConsole("This is data memory only and cannot access the text segment.");
            throw runtime_error("This is data memory only and cannot access the text segment.\\n");
        }
        else
        {
            if (type == "011")
            {
                appendToConsole("Loading double is not possible in a 32 bit register.");
                throw runtime_error("Loading double is not possible in a 32 bit register.\\n");
            } // double
            else if (type == "000")
            {
                MDR = mem.memory[address];
            } // Loading byte
            else if (type == "001")
            {
                MDR = "";
                MDR += mem.memory[address];
                MDR += mem.memory[address + 1];
            } // Loading half word
            else
            {
                MDR = "";
                MDR += mem.memory[address];
                MDR += mem.memory[address + 1];
                MDR += mem.memory[address + 2];
                MDR += mem.memory[address + 3];
            } // Loading word by default

            if (MDR == "")
                MDR = "00000000"; // If no data at this address, return 0
        }
        while (MDR.size() < 8)
            MDR = "0" + MDR;
        //appendToConsole("loaded data: " + MDR + " from " + MAR);
    }

    // Store MDR value into mem
    void store(string type)
    {
        int address = hex_to_dec(MAR);

        if (address < 268435456)
        {
            appendToConsole("This is data memory only and cannot access the text segment.");
            throw runtime_error("This is data memory only and cannot access the text segment.\\n");
        }
        else
        {
            if (type == "011")
            {
                appendToConsole("Loading double is not possible in a 32 bit register.");
                throw runtime_error("Loading double is not possible in a 32 bit register.\\n");
            } // double
            else if (type == "000")
                mem.memory[address] = MDR.substr(6, 2); // Storing byte
            else if (type == "001")
            {
                mem.memory[address] = MDR.substr(4, 2);
                mem.memory[address + 1] = MDR.substr(6, 2); // Storing half word
            }
            else
            {
                mem.memory[address] = MDR.substr(0, 2);
                mem.memory[address + 1] = MDR.substr(2, 2);
                mem.memory[address + 2] = MDR.substr(4, 2);
                mem.memory[address + 3] = MDR.substr(6, 2); // Storing word by default
            }
        }
        // appendToConsole("stored data: " + MDR + " at " + MAR);
    }

    string getMemoryContent(int startAddr, int count)
    {
        return mem.getMemoryContent(startAddr, count);
    }
};

struct RegisterFile
{
    vector<string> regs;
    int rs1, rs2, rd;
    int DONT_CARE = -1;

    RegisterFile()
    {
        regs.resize(32, "00000000");
        regs[2] = "7FFFFFDC";
        rs1 = rs2 = rd = DONT_CARE;
    }

    void setAddresses(int rs1_addr = -1, int rs2_addr = -1)
    {
        if ((rs1_addr != DONT_CARE && (rs1_addr < 0 || rs1_addr >= 32)) ||
            (rs2_addr != DONT_CARE && (rs2_addr < 0 || rs2_addr >= 32)))
        {
            appendToConsole("Invalid register address");
            throw invalid_argument("Invalid register address");
        }
        rs1 = rs1_addr;
        rs2 = rs2_addr;
    }

    void readRS()
    {
        if (rs1 != DONT_CARE)
            ra = regs[rs1];
        if (rs2 != DONT_CARE)
            rb = regs[rs2];
    }

    void writeRD()
    {
        string value = ry;
        if (rd != DONT_CARE && rd != 0)
            regs[rd] = value;
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

struct BranchPredictor
{
    map<string, string> BTB;
    map<string, bool> BHT;

    BranchPredictor()
    {
        BTB = {};
        BHT = {};
    }

    pair<bool, string> predictBranch(string pc)
    {
        if (pc == "ffffffff")
            return {false, ""}; // stall in the pipeline or when ecall has been encountered

        bool taken = false;
        int intpc = hex_to_dec(pc);
        string target = dec_to_hex_32bit(intpc + 4);

        auto it = BHT.find(pc);

        if (it != BHT.end())
        {
            // appendToConsole("predicting for pc: " + pc);
            taken = it->second;
            if (taken)
            {
                auto btb_it = BTB.find(pc);
                if (btb_it != BTB.end())
                    target = btb_it->second;
            }
            // appendToConsole("prediction: " + to_string(taken) + " " + target);
        }
        return {taken, target};
    }

    void update(string pc, bool taken, string target)
    {
        // appendToConsole("updating for: " + pc + " " + to_string(taken) + " " + target);
        BHT[pc] = taken;
        if (taken)
            BTB[pc] = target;
    }
};

class functions
{
private:
    string getInstructionType(string opcode, string funct3, string funct7)
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

    void hazard_detection()
    {
        bool stall = false;
        if (buf.idex.pc == "ffffffff")
            return;
        // data hazard
        if (buf.exmem.rd != "00000" && (buf.idex.rs1 == buf.exmem.rd || buf.idex.rs2 == buf.exmem.rd))
        {
            data_hazards++;
            appendToConsole(" ");
            appendToConsole("!!EXECUTE STAGE DATA HAZARD DETECTED!!");
            hazards.push_back({"Data", "ID/EX", buf.idex.instr, "EX/MEM", buf.exmem.instr});
            if (!forwarding_enable)
            {
                data_stalls++;
                appendToConsole("STALLING THE PIPELINE FOR 1 CYCLE");
                appendToConsole(" ");
                iag.pc = buf.ifid.pc;
                buf.idex.flush();
                stall = true;
            }
            else
            {
                if (buf.exmem.opcode == "0000011") // the instruction in execute is a memory load type
                {
                    data_stalls++;
                    appendToConsole("STALLING THE PIPELINE FOR 1 CYCLE");
                    appendToConsole(" ");
                    // stall is needed as the value will be avaliable in next cycle only and cant forward in future
                    iag.pc = buf.ifid.pc;
                    buf.idex.flush();
                    stall = true;
                }
                else // do forwarding
                {
                    appendToConsole("DATA FORWARDING");
                    appendToConsole("From Instruction " + buf.exmem.instr + ": EXECUTE Stage");
                    appendToConsole("To Instruction " + buf.idex.instr + ": DECODE Stage");
                    appendToConsole(" ");
                    if (buf.idex.rs1 == buf.exmem.rd) // rs1 is dependent on the value in exmem stage
                    {
                        buf.idex.rs1val = buf.exmem.exe_out;
                        ra = buf.idex.rs1val;
                        forwardingPaths.push_back({"EX/MEM", "ID/EX"});
                    }
                    if (buf.idex.rs2 == buf.exmem.rd) // rs2 is dependent on the value in exmem stage
                    {
                        buf.idex.rs2val = buf.exmem.exe_out;
                        rb = buf.idex.rs2val;
                        forwardingPaths.push_back({"EX/MEM", "ID/EX"});
                    }
                }
            }
        }

        if (buf.memwb.rd != "00000" && (buf.idex.rs1 == buf.memwb.rd || buf.idex.rs2 == buf.memwb.rd))
        {
            data_hazards++;
            appendToConsole(" ");
            appendToConsole("!!MEMORY STAGE DATA HAZARD DETECTED!!");
            hazards.push_back({"Data", "ID/EX", buf.idex.instr, "MEM/WB", buf.memwb.instr});
            if (!forwarding_enable)
            {
                data_stalls++;
                if (!stall)
                {
                    appendToConsole("STALLING THE PIPELINE FOR 1 CYCLE");
                    appendToConsole(" ");
                }
                else
                {
                    appendToConsole("!!STALLED ALREADY!!");
                    appendToConsole(" ");
                }
                iag.pc = buf.ifid.pc;
                buf.idex.flush();
                stall = true;
            }
            else
            {
                appendToConsole("DATA FORWARDING");
                appendToConsole("From Instruction " + buf.memwb.instr + ": MEMORY Stage");
                appendToConsole("To Instruction " + buf.idex.instr + ": DECODE Stage");
                appendToConsole(" ");
                if (buf.idex.rs1 == buf.memwb.rd)
                {
                    buf.idex.rs1val = ry;
                    ra = buf.idex.rs1val;
                    forwardingPaths.push_back({"MEM/WB", "ID/EX"});
                }
                if (buf.idex.rs2 == buf.memwb.rd)
                {
                    buf.idex.rs2val = ry;
                    rb = buf.idex.rs2val;
                    forwardingPaths.push_back({"MEM/WB", "ID/EX"});
                }
            }
        }
    }

public:
    PMI_data &data_memory;
    PMI_text &text_memory;
    IAG &iag;
    RegisterFile &registers;
    ALU &alu;
    buffers &buf;
    BranchPredictor &brpre;

    functions(PMI_data &data_mem, PMI_text &text_mem, IAG &iagRef, RegisterFile &reg, ALU &aluRef, buffers &buffer, BranchPredictor &brpreRef)
        : data_memory(data_mem), text_memory(text_mem), iag(iagRef), registers(reg), alu(aluRef), buf(buffer), brpre(brpreRef) {}

    void fetch()
    {
        // get the instruction from global variable pc
        text_memory.MAR = iag.pc;
        text_memory.load();
        if (text_memory.MDR == "" || iag.use_return_addr)
        {
            buf.ifid.pc = "ffffffff";
            buf.ifid.next_pc = "ffffffff";
            buf.ifid.instr = "";
        }
        else
        {
            buf.ifid.pc = iag.pc;
            buf.ifid.next_pc = dec_to_hex_32bit(hex_to_dec(iag.pc) + 4);
            buf.ifid.instr = text_memory.MDR;
        }

        pair<bool, string> prediction = brpre.predictBranch(buf.ifid.pc);

        if (prediction.first)
        {
            iag.update(prediction.second, true);
        }

        iag.compute_nextPC();
    }

    void decode()
    {
        if (buf.ifid.pc == "ffffffff")
        {
            buf.idex.flush();
        }
        else
        {
            // decode the hex instruction to get the opcode, func3, func7, rs1, rs2, rd, imm
            string binaryInstr = hex_to_bin(buf.ifid.instr);

            // Extract fields based on RISC-V instruction format
            buf.idex.opcode = extractBits(binaryInstr, 25, 31); // Bits 25-31
            buf.idex.rd = extractBits(binaryInstr, 20, 24);     // Bits 20-24
            buf.idex.funct3 = extractBits(binaryInstr, 17, 19); // Bits 17-19
            buf.idex.rs1 = extractBits(binaryInstr, 12, 16);    // Bits 12-16
            buf.idex.rs2 = extractBits(binaryInstr, 7, 11);     // Bits 7-11
            buf.idex.funct7 = extractBits(binaryInstr, 0, 6);   // Bits 0-6
            string imm = getImmediate(binaryInstr, buf.idex.opcode);
            buf.idex.imm = bin_to_hex(imm);

            if (buf.idex.opcode == "0100011" || buf.idex.opcode == "1100011")
                buf.idex.rd = "00000"; // rd is none in S and SB type instr

            buf.idex.instr_type = getInstructionType(buf.idex.opcode, buf.idex.funct3, buf.idex.funct7);

            if (buf.idex.opcode == "0000011" || buf.idex.opcode == "0010011" || buf.idex.opcode == "1100111")
                buf.idex.rs2 = "00000"; // rs2 is none in I type instr

            if (buf.idex.opcode == "1101111" || buf.idex.opcode == "0110111")
            {
                buf.idex.rs1 = "00000";
                buf.idex.rs2 = "00000";
            } // rs1, rs2 is none in U type instr and UJ type

            buf.idex.instr = buf.ifid.instr;
            buf.idex.pc = buf.ifid.pc;
            buf.idex.next_pc = buf.ifid.next_pc;

            if (buf.idex.opcode == "0100011")
            {
                buf.idex.mem_store_needed = true;
            }
            else
                buf.idex.mem_store_needed = false;

            if (buf.idex.opcode == "0000011")
            {
                buf.idex.mem_load_needed = true;
            }
            else
                buf.idex.mem_load_needed = false;

            if (buf.idex.opcode != "1100011" && buf.idex.opcode != "0100011") // not branch or store
            {
                buf.idex.wb_needed = true;
            }
            else
                buf.idex.wb_needed = false;

            if (buf.idex.opcode == "1100011")
            {
                buf.idex.branch_needed = true;
            }
            else
                buf.idex.branch_needed = false;

            if (buf.idex.opcode == "1101111")
            {
                buf.idex.jal = true;
            }
            else
                buf.idex.jal = false;

            if (buf.idex.opcode == "1100111")
            {
                buf.idex.jalr = true;
            }
            else
                buf.idex.jalr = false;

            registers.setAddresses(stoi(buf.idex.rs1, nullptr, 2), stoi(buf.idex.rs2, nullptr, 2));
            registers.readRS();
            buf.idex.rs1val = ra;
            buf.idex.rs2val = rb;

            hazard_detection();

            alu.operation = buf.idex.instr_type;
        }
    }

    void execute()
    {
        if (buf.idex.opcode != "0110011" && buf.idex.opcode != "1100011") // if instruction is I or load or store or jalr or auipc
        {
            rb = buf.idex.imm;
        }

        if (buf.idex.opcode == "0010111")
            ra = buf.idex.pc; // auipc

        if (buf.idex.pc != "ffffffff" && buf.idex.instr != "00000073")
        {
            ALUInstr++;
            alu.perform_op();
        }

        buf.exmem.pc = buf.idex.pc;
        buf.exmem.next_pc = buf.idex.next_pc;
        buf.exmem.instr = buf.idex.instr;
        buf.exmem.opcode = buf.idex.opcode;
        buf.exmem.rd = buf.idex.rd;
        buf.exmem.funct3 = buf.idex.funct3;
        buf.exmem.rs1 = buf.idex.rs1;
        buf.exmem.rs2 = buf.idex.rs2;
        buf.exmem.funct7 = buf.idex.funct7;
        buf.exmem.imm = buf.idex.imm;
        buf.exmem.instr_type = buf.idex.instr_type;
        buf.exmem.rs1val = buf.idex.rs1val;
        buf.exmem.rs2val = buf.idex.rs2val;
        buf.exmem.exe_out = rz;
        buf.exmem.mem_store_needed = buf.idex.mem_store_needed;
        buf.exmem.mem_load_needed = buf.idex.mem_load_needed;
        buf.exmem.wb_needed = buf.idex.wb_needed;

        string ret_addr;
        if (buf.idex.branch_needed) // branch
        {
            ControlInstr++;
            if (buf.exmem.exe_out == "00000001")
            {
                ret_addr = dec_to_hex_32bit(hex_to_dec(buf.idex.pc) + hex_to_dec_signed(buf.idex.imm));
                if (buf.ifid.pc != ret_addr)
                {
                    control_stalls += 2;
                    control_hazards++;
                    mispredictions++;
                    appendToConsole(" ");
                    appendToConsole("!!CONTROL HAZARD DETECTED!!");
                    appendToConsole("FLUSHING THE PIPELINE...");
                    appendToConsole(" ");
                    hazards.push_back({"Control", buf.ifid.pc, ret_addr});
                    iag.update(ret_addr, true);
                    buf.ifid.flush();
                    buf.idex.flush();
                }
                brpre.update(buf.exmem.pc, true, ret_addr);
            }

            else
            {
                ret_addr = buf.exmem.next_pc;
                if (buf.ifid.pc != ret_addr)
                {
                    control_stalls += 2;
                    control_hazards++;
                    mispredictions++;
                    appendToConsole(" ");
                    appendToConsole("!!CONTROL HAZARD DETECTED!!");
                    appendToConsole("FLUSHING THE PIPELINE...");
                    appendToConsole(" ");
                    hazards.push_back({"Control", buf.ifid.pc, ret_addr});
                    iag.update(ret_addr, true);
                    buf.ifid.flush();
                    buf.idex.flush();
                }
                brpre.update(buf.exmem.pc, false, "");
            }
        }

        else if (buf.idex.jal) // jal
        {
            ControlInstr++;
            ret_addr = dec_to_hex_32bit(hex_to_dec(buf.idex.pc) + hex_to_dec_signed(buf.idex.imm));
            if (buf.ifid.pc != ret_addr)
            {
                control_stalls += 2;
                control_hazards++;
                mispredictions++;
                appendToConsole(" ");
                appendToConsole("!!CONTROL HAZARD DETECTED!!");
                appendToConsole("FLUSHING THE PIPELINE...");
                appendToConsole(" ");
                hazards.push_back({"Control", buf.ifid.pc, ret_addr});
                iag.update(ret_addr, true);
                buf.ifid.flush();
                buf.idex.flush();
            }
            brpre.update(buf.exmem.pc, true, ret_addr);
        }

        else if (buf.idex.jalr) // jalr
        {
            ControlInstr++;
            ret_addr = buf.exmem.exe_out;
            if (buf.ifid.pc != ret_addr)
            {
                control_stalls += 2;
                control_hazards++;
                mispredictions++;
                appendToConsole(" ");
                appendToConsole("!!CONTROL HAZARD DETECTED!!");
                appendToConsole("FLUSHING THE PIPELINE...");
                appendToConsole(" ");
                hazards.push_back({"Control", buf.ifid.pc, ret_addr});
                iag.update(ret_addr, true);
                buf.ifid.flush();
                buf.idex.flush();
            }
            brpre.update(buf.exmem.pc, true, ret_addr);
        }

        if (buf.exmem.opcode == "1101111" || buf.exmem.opcode == "1100111")
        {
            rz = buf.exmem.next_pc;
            buf.exmem.exe_out = rz;
        }
    }

    void accessMemory(string address, string data, string type)
    {
        if (buf.exmem.pc != "ffffffff")
        {
            data_memory.MAR = address;
            data_memory.MDR = data;
            data_memory.store(type);
            DataTransferInstr++;
        }
        buf.memwb.pc = buf.exmem.pc;
        buf.memwb.next_pc = buf.exmem.next_pc;
        buf.memwb.instr = buf.exmem.instr;
        buf.memwb.opcode = buf.exmem.opcode;
        buf.memwb.rd = buf.exmem.rd;
        buf.memwb.funct3 = buf.exmem.funct3;
        buf.memwb.rs1 = buf.exmem.rs1;
        buf.memwb.rs2 = buf.exmem.rs2;
        buf.memwb.funct7 = buf.exmem.funct7;
        buf.memwb.imm = buf.exmem.imm;
        buf.memwb.instr_type = buf.exmem.instr_type;
        buf.memwb.rs1val = buf.exmem.rs1val;
        buf.memwb.rs2val = buf.exmem.rs2val;
        buf.memwb.exe_out = buf.exmem.exe_out;
        buf.memwb.wb_needed = buf.exmem.wb_needed;
    }
    void accessMemory(string address, string type)
    {
        if (buf.exmem.pc != "ffffffff")
        {
            data_memory.MAR = address;
            data_memory.load(type);
            ry = data_memory.MDR;
            DataTransferInstr++;
        }
        buf.memwb.pc = buf.exmem.pc;
        buf.memwb.next_pc = buf.exmem.next_pc;
        buf.memwb.instr = buf.exmem.instr;
        buf.memwb.opcode = buf.exmem.opcode;
        buf.memwb.rd = buf.exmem.rd;
        buf.memwb.funct3 = buf.exmem.funct3;
        buf.memwb.rs1 = buf.exmem.rs1;
        buf.memwb.rs2 = buf.exmem.rs2;
        buf.memwb.funct7 = buf.exmem.funct7;
        buf.memwb.imm = buf.exmem.imm;
        buf.memwb.instr_type = buf.exmem.instr_type;
        buf.memwb.rs1val = buf.exmem.rs1val;
        buf.memwb.rs2val = buf.exmem.rs2val;
        buf.memwb.exe_out = buf.exmem.exe_out;
        buf.memwb.wb_needed = buf.exmem.wb_needed;
    }
    void accessMemory()
    {
        ry = rz;
        buf.memwb.pc = buf.exmem.pc;
        buf.memwb.next_pc = buf.exmem.next_pc;
        buf.memwb.instr = buf.exmem.instr;
        buf.memwb.opcode = buf.exmem.opcode;
        buf.memwb.rd = buf.exmem.rd;
        buf.memwb.funct3 = buf.exmem.funct3;
        buf.memwb.rs1 = buf.exmem.rs1;
        buf.memwb.rs2 = buf.exmem.rs2;
        buf.memwb.funct7 = buf.exmem.funct7;
        buf.memwb.imm = buf.exmem.imm;
        buf.memwb.instr_type = buf.exmem.instr_type;
        buf.memwb.rs1val = buf.exmem.rs1val;
        buf.memwb.rs2val = buf.exmem.rs2val;
        buf.memwb.exe_out = buf.exmem.exe_out;
        buf.memwb.wb_needed = buf.exmem.wb_needed;
    }

    void writeBack(bool &flag)
    {
        if (buf.memwb.pc != "ffffffff" && buf.memwb.wb_needed)
        {
            registers.rd = stoi(buf.memwb.rd, nullptr, 2);
            registers.writeRD();
        }
        if (buf.memwb.instr == "00000073")
        {
            flag = false;
        }
        instructionCt++;
    }
};

class control_circuitry
{
public:
    functions f;

    control_circuitry(PMI_data &data_memory,
                      PMI_text &text_memory,
                      IAG &iag,
                      RegisterFile &registers,
                      ALU &alu,
                      buffers &vec,
                      BranchPredictor &brpre)
        : f(data_memory, text_memory, iag, registers, alu, vec, brpre)
    {
    }

    void step_cycle(bool &flag)
    {
        forwardingPaths.clear();
        hazards.clear();
        appendToConsole("Cycle " + to_string(clock_cycle + 1) + ":");

        if (f.buf.memwb.wb_needed)
        {
            f.writeBack(flag);
            appendToConsole(
                "  W: PC="+ f.buf.memwb.pc + " rd=x" +to_string(f.registers.rd) +
                " val=" + ry);

            if (f.buf.memwb.pc == printPipelineForInstruction)
            {
                appendToConsole(" ");
                appendToConsole("Instruction at PC " + printPipelineForInstruction + " completes Writeback.");
                appendToConsole("Contents of RegisterFile: ");
                for (int i = 0; i < 32; i++)
                {
                    string result = "x" + to_string(i) + ":" + g_registers->regs[i] + ";";
                    appendToConsole(result);
                }
                appendToConsole(" ");
            }
        }
        else
        {
            appendToConsole("  W: PC=" + f.buf.memwb.pc );
        }

        if (f.buf.exmem.mem_load_needed)
        {
            f.accessMemory(rz, f.buf.exmem.funct3);
            appendToConsole(
                "  M: PC="+ f.buf.memwb.pc  +" LOAD type=" +f.buf.exmem.funct3 +
                " data=" + ry +
                " addr=" + rz);
        }
        else if (f.buf.exmem.mem_store_needed)
        {
            f.accessMemory(rz, f.buf.exmem.rs2val, f.buf.exmem.funct3);
            appendToConsole(
                "  M: PC=" + f.buf.memwb.pc +" STORE type=" + f.buf.exmem.funct3 +
                " data=" + f.buf.exmem.rs2val +
                " addr=" + rz);
        }
        else
        {
            f.accessMemory();
            appendToConsole("  M: PC="+ f.buf.memwb.pc );
        }

        if (f.buf.exmem.pc == printPipelineForInstruction)
        {
            appendToConsole(" ");
            appendToConsole("Instruction at PC " + printPipelineForInstruction + " completes Memory stage.");
            appendToConsole("Contents of Exe/Mem buffer: PC=" + f.buf.exmem.pc + ", Instr=" + f.buf.exmem.instr +
                            ", ALU Result=" + f.buf.exmem.exe_out + ", rs2val=" + f.buf.exmem.rs2val);
            appendToConsole(" ");
        }

        f.execute();
        appendToConsole(
            "  E: PC="+ f.buf.exmem.pc +" op="+ f.alu.operation +
            " result=" + rz);

        if (f.buf.idex.pc == printPipelineForInstruction)
        {
            appendToConsole(" ");
            appendToConsole("Instruction at PC " + printPipelineForInstruction + " completes Execute stage.");
            appendToConsole("Contents of Dec/Exe buffer: PC=" + f.buf.idex.pc + ", Instr=" + f.buf.idex.instr +
                            ", rs1val=" + f.buf.idex.rs1val + ", rs2val=" + f.buf.idex.rs2val +
                            ", Imm=" + f.buf.idex.imm + ", ALU Operation=" + f.alu.operation);
            appendToConsole(" ");
        }

        f.decode();
        appendToConsole(
            "  D: PC=" + f.buf.idex.pc +" opcode=" + f.buf.idex.opcode +
            " rd=" + f.buf.idex.rd +
            " rs1=" + f.buf.idex.rs1 +
            " rs2=" + f.buf.idex.rs2 +
            " type=" + f.buf.idex.instr_type);

        if (f.buf.ifid.pc == printPipelineForInstruction)
        {
            appendToConsole(" ");
            appendToConsole("Instruction at PC " + printPipelineForInstruction + " completes Decode stage.");
            appendToConsole("Contents of F/Dec buffer: PC=" + f.buf.ifid.pc + ", Instr=" + f.buf.ifid.instr +
                            ", Control Instruction=" + (f.buf.idex.branch_needed ? "Yes" : "No") +
                            ", BTB Hit=" + (g_brpre->BTB.find(f.buf.ifid.pc) != g_brpre->BTB.end() ? "Yes" : "No"));
            appendToConsole(" ");
        }

        f.fetch();
        appendToConsole(
            "  F: PC=" + f.buf.ifid.pc +
            " Instr=" + f.buf.ifid.instr);
        appendToConsole(" ");

        if (f.buf.ifid.pc == printPipelineForInstruction)
        {
            appendToConsole(" ");
            appendToConsole("Instruction at PC " + printPipelineForInstruction + " completes Fetch stage.");
            appendToConsole("Contents of F/Dec buffer: PC=" + f.buf.ifid.pc + ", IR=" + f.buf.ifid.instr);
            appendToConsole(" ");
        }

        clock_cycle++;
    }

    void run_cycles()
    {
        bool flag = true;
        while (flag)
            step_cycle(flag);
    }

    bool step()
    {
        bool flag = true;
        step_cycle(flag);
        return flag;
    }
};

// Class to expose to JavaScript
class RiscVPipelinedSimulator
{
public:
    RiscVPipelinedSimulator() : initialized(false) {}

    string assemble(const string &code)
    {
        return ::assemble(code);
    }

    void init()
    {
        if (initialized)
        {
            cleanup();
        }
        clearConsole();

        g_data_memory = new PMI_data();
        g_text_memory = new PMI_text();
        g_iag = new IAG();
        g_registers = new RegisterFile();
        g_alu = new ALU();
        g_buffers = new buffers();
        g_brpre = new BranchPredictor();
        g_control = new control_circuitry(*g_data_memory, *g_text_memory, *g_iag, *g_registers, *g_alu, *g_buffers, *g_brpre);
        g_running = true;
        clock_cycle = 0;
        instructionCt = 0;
        CPI = 0;
        DataTransferInstr = 0;
        ALUInstr = 0;
        ControlInstr = 0;
        stalls = 0;
        data_hazards = 0;
        control_hazards = 0;
        mispredictions = 0;
        data_stalls = 0;
        control_stalls = 0;
        initialized = true;
        forwardingPaths.clear();
        hazards.clear();
        printPipelineForInstruction = "";
        appendToConsole("=> Simulator initialized");
    }

    void cleanup()
    {
        if (initialized)
        {
            delete g_data_memory;
            delete g_text_memory;
            delete g_iag;
            delete g_registers;
            delete g_alu;
            delete g_buffers;
            delete g_brpre;
            delete g_control;

            g_data_memory = nullptr;
            g_text_memory = nullptr;
            g_iag = nullptr;
            g_registers = nullptr;
            g_alu = nullptr;
            g_buffers = nullptr;
            g_brpre = nullptr;
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
            cout << line << endl;
            if (line.empty())
            {
                memory_flag = true;
                continue;
            }

            if (memory_flag)
            {
                string addr = line.substr(2, 8);
                string code = "000000" + line.substr(11, 2);
                g_data_memory->MAR = addr;
                g_data_memory->MDR = code;
                g_data_memory->store("000");
            }
            else
            {
                // Parse address and instruction
                size_t pos = line.find(' ');
                if (pos == string::npos)
                    continue;

                string addrStr = line.substr(2, pos - 2);
                line = line.substr(pos + 1);
                string codeStr = line.substr(2, line.find_first_of(' ') - 2);

                // Store in memory
                g_text_memory->MAR = addrStr;
                g_text_memory->MDR = codeStr;
                g_text_memory->store();
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

        g_control->run_cycles();
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

        if (segment == "text")
            return g_text_memory->getMemoryContent(startAddr, count);
        return g_data_memory->getMemoryContent(startAddr, count);
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

    void toggleForwarding(bool enable)
    {
        forwarding_enable = enable;
    }

    string getPipelineState()
    {
        if (!initialized)
        {
            throw runtime_error("Simulator not initialized");
        }

        string result = "";
        int address = hex_to_dec(g_iag->pc);
        string ins = "";
        if (address < 268435456)
        {
            ins += (g_text_memory->mem).memory[address];
            ins += (g_text_memory->mem).memory[address + 1];
            ins += (g_text_memory->mem).memory[address + 2];
            ins += (g_text_memory->mem).memory[address + 3];
        }

        // IF stage
        result += "IF:";
        result += ins + ",";
        result += g_iag->pc + ";";

        // ID stage
        result += "ID:";
        if (g_buffers->ifid.pc == "ffffffff" || g_buffers->ifid.pc == "")
            result += ";";
        else
        {
            result += g_buffers->ifid.instr + ",";
            result += g_buffers->ifid.pc + ";";
        }

        // EX stage
        result += "EX:";
        if (g_buffers->idex.pc == "ffffffff" || g_buffers->idex.pc == "")
            result += ";";
        else
        {
            result += g_buffers->idex.instr + ",";
            result += g_buffers->idex.rs1 + ",";
            result += g_buffers->idex.rs2 + ",";
            result += g_buffers->idex.imm + ",";
            result += g_buffers->idex.rd + ";";
        }

        // MEM stage
        result += "MEM:";
        if (g_buffers->exmem.pc == "ffffffff" || g_buffers->exmem.pc == "")
            result += ";";
        else
        {
            result += g_buffers->exmem.instr + ",";
            result += g_buffers->exmem.instr_type + ",";
            result += g_buffers->exmem.exe_out + ";";
        }

        // WB stage
        result += "WB:";
        if (g_buffers->memwb.pc == "ffffffff" || g_buffers->memwb.pc == "")
            result += ";";
        else
        {
            result += g_buffers->memwb.instr + ",";
            result += g_buffers->memwb.rd + ",";
            result += ry + ";";
        }

        // IF/ID buffer
        result += "IF/ID:";
        if (g_buffers->ifid.pc == "ffffffff" || g_buffers->ifid.pc == "")
            result += ";";
        else
        {
            result += g_buffers->ifid.instr + ",";
            result += g_buffers->ifid.pc + ";";
        }

        // ID/EX buffer
        result += "ID/EX:";
        if (g_buffers->idex.pc == "ffffffff" || g_buffers->idex.pc == "")
            result += ";";
        else
        {
            result += g_buffers->idex.instr + ",";
            result += g_buffers->idex.rs1 + ",";
            result += g_buffers->idex.rs2 + ",";
            result += g_buffers->idex.imm + ",";
            result += g_buffers->idex.rd + ";";
        }

        // EX/MEM buffer
        result += "EX/MEM:";
        if (g_buffers->exmem.pc == "ffffffff" || g_buffers->exmem.pc == "")
            result += ";";
        else
        {
            result += g_buffers->exmem.instr + ",";
            result += g_buffers->exmem.instr_type + ",";
            result += g_buffers->exmem.exe_out + ";";
        }

        // MEM/WB buffer
        result += "MEM/WB:";
        if (g_buffers->memwb.pc == "ffffffff" || g_buffers->memwb.pc == "")
            result += ";";
        else
        {
            result += g_buffers->memwb.instr + ",";
            result += g_buffers->memwb.rd + ",";
            result += ry + ";";
        }
        // Forwarding paths
        result += "FWD:";
        for (const auto &path : forwardingPaths)
        {
            result += path.first + "-" + path.second + ",";
        }
        result += ";";

        // Hazards
        result += "HAZ:";
        for (const auto &hazard : hazards)
        {
            for (const auto &entry : hazard)
            {
                result += entry + ",";
            }
            result += "-";
        }

        return result;
    }

    string getBP()
    {
        if (!initialized)
        {
            throw runtime_error("Simulator not initialized");
        }

        string result = "";
        for (const auto &entry : g_brpre->BTB)
        {
            result += entry.first + ":" + entry.second + "," + (g_brpre->BHT[entry.first] ? "true" : "false") + ";";
        }
        return result;
    }

    string getBuffers()
    {
        if (!initialized)
        {
            throw runtime_error("Simulator not initialized");
        }

        string result = "";
        result += "IFID: " + g_buffers->ifid.instr + "," + g_buffers->ifid.pc + ";";
        result += "IDEX: " + g_buffers->idex.instr + "," + g_buffers->idex.rs1 + "," + g_buffers->idex.rs2 + "," + g_buffers->idex.rd + "," + g_buffers->idex.imm + ";";
        result += "EXMEM: " + g_buffers->exmem.instr + "," + g_buffers->exmem.exe_out + "," + g_buffers->exmem.rs2val + "," + g_buffers->exmem.rd + ";";
        result += "MEMWB: " + g_buffers->memwb.instr + "," + g_buffers->memwb.exe_out + "," + g_buffers->memwb.rs2val + "," + g_buffers->memwb.rd + ";";
        return result;
    }

    string getStats()
    {
        if (instructionCt > 0)
            CPI = clock_cycle / instructionCt;
        string result = "";
        result += "Cycle count:" + to_string(clock_cycle) + ";";
        result += "Instruction count:" + to_string(instructionCt) + ";";
        result += "CPI:" + to_string(CPI) + ";";
        result += "Data Transfer Instructions:" + to_string(DataTransferInstr) + ";";
        result += "ALU Instructions:" + to_string(ALUInstr) + ";";
        result += "Control Instructions:" + to_string(ControlInstr) + ";";
        result += "Stall Count:" + to_string(stalls) + ";";
        result += "Data Hazards:" + to_string(data_hazards) + ";";
        result += "Control Hazards:" + to_string(control_hazards) + ";";
        result += "Branch Mispredictions:" + to_string(mispredictions) + ";";
        result += "Data Hazard Stalls:" + to_string(data_stalls) + ";";
        result += "Control Hazard Stalls:" + to_string(control_stalls) + ";";
        return result;
    }

    void setPrintPipelineForInstruction(const string &pc)
    {
        printPipelineForInstruction = pc;
    }

private:
    bool initialized;
};

// Binding our C++ class to JavaScript
EMSCRIPTEN_BINDINGS(riscv_pipelined_simulator)
{
    using namespace emscripten;

    class_<RiscVPipelinedSimulator>("RiscVPipelinedSimulator")
        .constructor<>()
        .function("init", &RiscVPipelinedSimulator::init)
        .function("cleanup", &RiscVPipelinedSimulator::cleanup)
        .function("loadCode", &RiscVPipelinedSimulator::loadCode)
        .function("step", &RiscVPipelinedSimulator::step)
        .function("run", &RiscVPipelinedSimulator::run)
        .function("reset", &RiscVPipelinedSimulator::reset)
        .function("showReg", &RiscVPipelinedSimulator::showReg)
        .function("showMem", &RiscVPipelinedSimulator::showMem)
        .function("getPC", &RiscVPipelinedSimulator::getPC)
        .function("getConsoleOutput", &RiscVPipelinedSimulator::getConsoleOutput)
        .function("clearConsoleOutput", &RiscVPipelinedSimulator::clearConsoleOutput)
        .function("getStats", &RiscVPipelinedSimulator::getStats)
        .function("setForwardingEnable", &RiscVPipelinedSimulator::toggleForwarding)
        .function("assemble", &RiscVPipelinedSimulator::assemble)
        .function("getBP", &RiscVPipelinedSimulator::getBP)
        .function("toggleForwarding", &RiscVPipelinedSimulator::toggleForwarding)
        .function("getPipelineState", &RiscVPipelinedSimulator::getPipelineState)
        .function("setPrintPipelineForInstruction", &RiscVPipelinedSimulator::setPrintPipelineForInstruction)
        .function("getBuffers", &RiscVPipelinedSimulator::getBuffers);
};

// int main()
// {
//     // Take R'string input
//     string input = R"ASM(
//     .data
//     n: .word 5

//     .text
//     lui x18, 0x10000
//     result: addi x10, x0, 0
//     temp: addi x9, x0, 0
//     val_n: lw x11, 0(x18)
//     val_1: addi x19, x0, 1
//     val_2: addi x20, x0, 2
//     val_neg1: addi x21, x0, -1
//     jal x1, fibo
//     beq x0, x0, final_exit

//     fibo: addi x2, x2, -12
//         sw x1, 8(x2)
//         sw x11, 4(x2)
//         sw x9, 0(x2)

//         case1: bne x11, x19, case2
//             addi x10, x0, 1
//             sw x21, 8(x2)
//             sw x21, 4(x2)
//             sw x21, 0(x2)
//             addi x2, x2, 12
//             jalr x0, x1, 0

//         case2: bne x11, x20, case3
//             addi x10, x0, 1
//             sw x21, 8(x2)
//             sw x21, 4(x2)
//             sw x21, 0(x2)
//             addi x2, x2, 12
//             jalr x0, x1, 0

//         case3: addi x11, x11, -1
//             jal x1, fibo
//             addi x9, x10, 0
//             addi x11, x11, -1
//             jal x1, fibo
//             add x10, x10, x9
//             lw x9, 0(x2)
//             lw x11, 4(x2)
//             lw x1, 8(x2)
//             sw x21, 8(x2)
//             sw x21, 4(x2)
//             sw x21, 0(x2)
//             addi x2, x2, 12
//             jalr x0, x1, 0

//     final_exit:
//     )ASM";

//     RiscVPipelinedSimulator simulator;
//     simulator.init();
//     cout<<"Simulator initialized\n";
//     input=simulator.assemble(input);
//     cout<<"Code assembled successfully\n";
//     simulator.loadCode(input);
//     cout<<"Code loaded successfully\n";
//     simulator.run();
//     cout<<"Simulation completed\n";
//     simulator.cleanup();
//     return 0;
// }