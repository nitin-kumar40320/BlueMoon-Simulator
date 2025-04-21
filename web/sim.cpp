#include<bits/stdc++.h>
using namespace std;

//global controls
bool forwarding_enable = false, piplining_enable = true;

//global variables which are used to simulate global registers
long long int clock_cycle = 0;
string rz,ry,ra,rb;

// hexadecimal to integer
int hex_to_dec(string hex) 
{
    int dec = 0;
    int base = 1;

    for (int i = hex.size() - 1; i >= 0; i--) 
    {
        char ch = hex[i];

        int val;
        if (ch >= '0' && ch <= '9') val = ch - '0';
        else if (ch >= 'A' && ch <= 'F') val = ch - 'A' + 10;
        else if (ch >= 'a' && ch <= 'f') val = ch - 'a' + 10;
        else 
        {
            throw invalid_argument("Invalid hexadecimal character: " + string(1, ch));
        }

        dec += val * base;
        base *= 16;
    }

    return dec;
}

int hex_to_dec_signed(const std::string& hexStr) {
    // Remove any leading 0x or 0X from the hex string
    std::string cleanHex = hexStr;
    if (hexStr.find("0x") == 0 || hexStr.find("0X") == 0) {
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
    if (value & msbMask) {
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

string hex_to_bin (string hex) 
{
    string binary = "";
    
    for (char ch : hex) {
        int num;
        if (ch >= '0' && ch <= '9') num = ch - '0';        // Convert '0'-'9' to 0-9
        else if (ch >= 'A' && ch <= 'F') num = ch - 'A' + 10; // Convert 'A'-'F' to 10-15
        else if (ch >= 'a' && ch <= 'f') num = ch - 'a' + 10; // Convert 'a'-'f' to 10-15
        else return "Invalid Hex Input"; // Error handling
        
        // Convert num (0-15) to a 4-bit binary string using bitwise operations
        for (int i = 3; i >= 0; --i) {
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
    string pc,next_pc,instr;
    IFID_buffer()
    {
        pc = "ffffffff"; next_pc = "ffffffff"; instr = "";
    }
    void flush()
    {
        pc = "ffffffff"; next_pc = "ffffffff"; instr = "";
    }
};
struct IDEX_buffer
{
    string pc,next_pc,instr,opcode,rd,funct3,rs1,rs2,funct7,imm,instr_type,rs1val,rs2val;
    IDEX_buffer()
    {
        pc = "ffffffff"; next_pc = "ffffffff"; instr = ""; opcode="";rd="";funct3="";rs1="";rs2="";funct7="";imm="";instr_type="";rs1val="";rs2val="";
    }
    void flush()
    {
        pc = "ffffffff"; next_pc = "ffffffff"; instr = ""; opcode="";rd="";funct3="";rs1="";rs2="";funct7="";imm="";instr_type="";rs1val="";rs2val="";
    }
};
struct EXMEM_buffer
{
    string pc,next_pc,instr,opcode,rd,funct3,rs1,rs2,funct7,imm,instr_type,rs1val,rs2val,exe_out;
    EXMEM_buffer()
    {
        pc = "ffffffff"; next_pc = "ffffffff"; instr = ""; opcode="";rd="";funct3="";rs1="";rs2="";funct7="";imm="";instr_type="";rs1val="";rs2val="";exe_out="";
    }
    void flush()
    {
        pc = "ffffffff"; next_pc = "ffffffff"; instr = ""; opcode="";rd="";funct3="";rs1="";rs2="";funct7="";imm="";instr_type="";rs1val="";rs2val="";exe_out="";
    }
};
struct MEMWB_buffer
{
    string pc,next_pc,instr,opcode,rd,funct3,rs1,rs2,funct7,imm,instr_type,rs1val,rs2val,exe_out;
    MEMWB_buffer()
    {
        pc = "ffffffff"; next_pc = "ffffffff"; instr = ""; opcode="";rd="";funct3="";rs1="";rs2="";funct7="";imm="";instr_type="";rs1val="";rs2val="";exe_out="";
    }
    void flush()
    {
        pc = "ffffffff"; next_pc = "ffffffff"; instr = ""; opcode="";rd="";funct3="";rs1="";rs2="";funct7="";imm="";instr_type="";rs1val="";rs2val="";exe_out="";
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
        ifid.flush(); idex.flush(); exmem.flush(); memwb.flush();
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
};

struct ALU 
{
    string operation;

    void perform_op() 
    {
        int aVal = hex_to_dec_signed(ra);
        int bVal = hex_to_dec_signed(rb);
        // cout<<"ra: "<<ra<<" rb: "<<rb<<endl;
        // cout<<"aVal: "<<aVal<<" bVal: "<<bVal<<endl;
        int result = 0;

        if (operation == "add" || operation == "lw" || operation == "lh" || operation == "lb" || operation == "ld" || operation == "sw" || operation == "sh" || operation == "sb" || operation == "sd" || operation == "auipc" || operation == "jalr")  result = aVal + bVal;
        else if (operation == "sub")  result = aVal - bVal;
        else if (operation == "and")  result = aVal & bVal;
        else if (operation == "or")   result = aVal | bVal;
        else if (operation == "xor")  result = aVal ^ bVal;
        else if (operation == "sll")  result = (static_cast<unsigned>(aVal) << (bVal & 31));
        else if (operation == "srl")  result = (static_cast<unsigned>(aVal) >> (bVal & 31));
        else if (operation == "sra")  result = (aVal >> (bVal & 31));
        else if (operation == "slt")  result = (aVal < bVal) ? 1 : 0;
        else if (operation == "mul")  result = aVal * bVal;
        else if (operation == "div")  result = aVal / bVal;
        else if (operation == "rem")  result = aVal % bVal;
        else if (operation == "beq")  result = (aVal == bVal) ? 1 : 0;
        else if (operation == "bne")  result = (aVal != bVal) ? 1 : 0;
        else if (operation == "blt")  result = (aVal < bVal) ? 1 : 0;
        else if (operation == "bge")  result = (aVal >= bVal) ? 1 : 0;
        else if(operation == "lui" || operation == "jal") result = bVal;
        else throw invalid_argument("Invalid ALU operation: " + operation);

        if(operation == "") rz="";
        else rz = dec_to_hex_32bit(result);
        // cout<<"result in rz: "<<rz<<endl;
    }
};


// Memory Structure
struct Memory 
{
    map<int, string> memory;
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

        if (address<268435456)
        { 
            mem.memory[address] = MDR.substr(0, 2); mem.memory[address+1] = MDR.substr(2, 2); mem.memory[address+2] = MDR.substr(4, 2); mem.memory[address+3] = MDR.substr(6, 2);    // Storing word as instruction is always 32 bits
        }
        else 
        {
            throw "This memory is for text segment only and does not have access to static and dynamic data segment.\n";
        }
    }

    void load()
    {
        int address = hex_to_dec(MAR);

        if (address<268435456)
        { 
            MDR = mem.memory[address] + mem.memory[address+1] + mem.memory[address+2] + mem.memory[address+3];     // Loading word as instruction is always 32 bits

            // if (MDR =="") throw "No instruction at this address.\n";
        }
        else 
        {
            throw "This memory is of text segment only and does not have access to static and dynamic data segment.\n";
        }
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

        if (address<268435456)
        { 
            throw "This is data memory only and cannot access the text segment.\n";
        }
        else
        {
            if (type=="011") throw "Loading double is not possible in a 32 bit register.\n"; //double
            else if (type=="000") MDR = mem.memory[address];  // Loading byte 
            else if (type=="001") MDR = mem.memory[address] + mem.memory[address+1];  // Loading half word
            else MDR = mem.memory[address] + mem.memory[address+1] + mem.memory[address+2] + mem.memory[address+3];   // Loading word by default

            if (MDR =="") MDR = "00000000"; // If no data at this address, return 0
        }
        while (MDR.size() < 8) MDR = "0" + MDR;
        // cout<<"loaded data: "<<MDR<<" from "<<MAR<<endl;
    }

    // Store MDR value into mem
    void store(string type) 
    {
        int address = hex_to_dec(MAR);

        if (address<268435456)
        { 
            throw "This is data memory only and cannot access the text segment.\n";
        }
        else
        {
            if (type=="011") throw "Loading double is not possible in a 32 bit register.\n"; // double
            else if (type=="000") mem.memory[address] = MDR.substr(6, 2); // Storing byte
            else if (type=="001") 
            {
                mem.memory[address] = MDR.substr(4, 2); mem.memory[address+1] = MDR.substr(6, 2);  // Storing half word
            }
            else 
            {
                mem.memory[address] = MDR.substr(0, 2); mem.memory[address+1] = MDR.substr(2, 2); mem.memory[address+2] = MDR.substr(4, 2); mem.memory[address+3] = MDR.substr(6, 2);   // Storing word by default
            }
        }
        // cout<<"stored data: "<<MDR<<" at "<<MAR<<endl;        
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
        regs[2] = "7FFFFFDC"; //stack pointer
        rs1 = rs2 = rd = DONT_CARE;
    }

    void setAddresses(int rs1_addr = -1, int rs2_addr = -1)
    {
        if ((rs1_addr != DONT_CARE && (rs1_addr < 0 || rs1_addr >= 32)) ||
            (rs2_addr != DONT_CARE && (rs2_addr < 0 || rs2_addr >= 32))) {
            throw invalid_argument("Invalid register address");
        }
        rs1 = rs1_addr;
        rs2 = rs2_addr;
    }

    void readRS() 
    {
        if (rs1 != DONT_CARE) ra = regs[rs1];
        if (rs2 != DONT_CARE) rb = regs[rs2];

    }

    void writeRD() 
    {
        string value = ry;
        if (rd != DONT_CARE && rd != 0) regs[rd] = value;
    }

    void printRegs() 
    {
        for (int i = 0; i < 32; i++) {
            cout << "x" << i << ": " << regs[i] << endl;
        }
    }
};


struct BranchPredictor
{
    map<string,string> BTB;
    map<string,bool> BHT;

    BranchPredictor() {
        BTB = {};
        BHT = {};
    }
    
    pair<bool,string> predictBranch(string pc)
    {   
        if (pc=="ffffffff") return {false,""};  //stall in the pipeline or when ecall has been encountered

        bool taken = false;
        int intpc = hex_to_dec(pc);
        string target = dec_to_hex_32bit(intpc + 4);
        
        auto it = BHT.find(pc);
        
        if(it != BHT.end())
        {
            // cout << "predicting for pc: " << pc;
            taken = it->second;
            if(taken)
            {
                auto btb_it = BTB.find(pc);
                if(btb_it != BTB.end())target = btb_it->second;
            }
            // cout << " prediction: " << taken << " " << target << endl;
        }
        return {taken, target};
    }

    void update(string pc, bool taken, string target)
    {
        // cout << "updating for: " << pc << " " << taken << " " << target <<endl;
        BHT[pc] = taken;
        if(taken) BTB[pc] = target;
    }
};

class functions
{
    private:    
    string getInstructionType(string opcode, string funct3, string funct7)
    {
        if (opcode == "0110011")
        { // R-type
            if (funct3 == "000" && funct7 == "0000000") return "add";
            if (funct3 == "000" && funct7 == "0100000") return "sub";
            if (funct3 == "111") return "and";
            if (funct3 == "110" && funct7 == "0000000") return "or";
            if (funct3 == "100" && funct7 == "0000000") return "xor";
            if (funct3 == "001" && funct7 == "0000000") return "sll";
            if (funct3 == "101" && funct7 == "0000000") return "srl";
            if (funct3 == "101" && funct7 == "0100000") return "sra";
            if (funct3 == "010") return "slt";
            if (funct3 == "000" && funct7 == "0000001") return "mul";
            if (funct3 == "100" && funct7 == "0000001") return "div";
            if (funct3 == "110" && funct7 == "0000001") return "rem";
        }
        else if (opcode == "0010011")
        { // I-type
            if (funct3 == "000") return "add";
            if (funct3 == "111") return "and";
            if (funct3 == "110") return "or";
            if (funct3 == "100") return "xor";
            if (funct3 == "001") return "sll";
            if (funct3 == "101" && funct7 == "0000000") return "srl";
            if (funct3 == "101" && funct7 == "0100000") return "sra";
            if (funct3 == "010") return "slt";
        }
        else if (opcode == "0000011")
        { // Load instructions
            if (funct3 == "000") return "lb";
            if (funct3 == "001") return "lh";
            if (funct3 == "010") return "lw";
            if (funct3 == "011") return "ld";
        }
        else if (opcode == "0100011")
        { // S-type (Store instructions)
            if (funct3 == "000") return "sb";
            if (funct3 == "001") return "sh";
            if (funct3 == "010") return "sw";
            if (funct3 == "011") return "sd";
        }
        else if (opcode == "1100011")
        { // B-type (Branch instructions)
            if (funct3 == "000") return "beq";
            if (funct3 == "001") return "bne";
            if (funct3 == "100") return "blt";
            if (funct3 == "101") return "bge";
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
        {                                          // I-type
            im_val = extractBits(binaryInstr, 0, 11); // Bits 0-11
        }
        else if (opcode == "0100011")
        {                                                                            // S-type
            im_val = extractBits(binaryInstr, 0, 6) + extractBits(binaryInstr, 20, 24); // Bits 0-6 and 20-24
        }
        else if (opcode == "1100011")
        { // Branch-type
            // since as per logic , this imm value should be multiplied by 2 so add 0 in the end
            im_val = extractBits(binaryInstr, 0, 0) + extractBits(binaryInstr, 24, 24) +
                    extractBits(binaryInstr, 1, 6) + extractBits(binaryInstr, 20, 23) + "0"; // Bits 0, 24, 1-6, 20-23
        }
        else if (opcode == "0110111" || opcode == "0010111")
        {                                          // U-type
            im_val = extractBits(binaryInstr, 0, 19); // Bits 0-19
            im_val.append(12, '0');                  // Append 12 zeros to shift left by 12 bits
        }
        else if (opcode == "1101111")
        { // J-type
            
            im_val = "00000000000000000000";
            im_val[20] = binaryInstr[0]; // Bit 0
            for (int i=1;i<=10;i++) {
                im_val[11-i] = binaryInstr[i]; // Bits 10-1
            }
            im_val[11] = binaryInstr[11];
            for (int i=12;i<=19;i++) {
                im_val[31-i] = binaryInstr[i]; // Bits 19-12
            }
            reverse(im_val.begin(), im_val.end());
        }else if (opcode == "0110011")
        { // R-type (add, and, or, sll, slt, sra, srl, sub, xor, mul, div, rem)
            // R-type instructions do not have an immediate value
            im_val = "0"; // No immediate value for R-type
        }
        if (im_val[0]=='1') {
            int n = 32- im_val.size();
            for (int i=0;i<n;i++) {
                im_val = "1" + im_val;
            }
        }else {
            int n = 32- im_val.size();
            for (int i=0;i<n;i++) {
                im_val = "0" + im_val;
            }
        }
        return im_val;
    }
    
    void appendToConsole(const string &message) {
        cout << message << endl;
    }
    void hazard_detection()
    {
        bool stall = false;
        if (buf.idex.pc == "ffffffff") return;
        //data hazard
        if (buf.exmem.rd!="00000" && (buf.idex.rs1 == buf.exmem.rd || buf.idex.rs2 == buf.exmem.rd)) 
        {
            appendToConsole("!!EXECUTE STAGE DATA HAZARD DETECTED!!");
            if (!forwarding_enable)
            {
                appendToConsole("STALLING THE PIPELINE FOR 1 CYCLE");
                iag.pc = buf.ifid.pc;
                buf.idex.flush();
                stall=true;
            }
            else
            {
                if (buf.exmem.opcode=="0000011") // the instruction in execute is a memory load type
                {
                    appendToConsole("STALLING THE PIPELINE FOR 1 CYCLE");
                    // stall is needed as the value will be avaliable in next cycle only and cant forward in future
                    iag.pc = buf.ifid.pc;
                    buf.idex.flush();
                    stall=true;
                }
                else // do forwarding
                {
                    appendToConsole("DATA FORWARDING");
                    appendToConsole("From Instruction "+buf.exmem.instr+": EXECUTE Stage");
                    appendToConsole("To Instruction "+buf.idex.instr+": DECODE Stage");
                    if (buf.idex.rs1==buf.exmem.rd) // rs1 is dependent on the value in exmem stage
                    {
                        buf.idex.rs1val = buf.exmem.exe_out;
                        ra = buf.idex.rs1val;
                    }
                    if (buf.idex.rs2==buf.exmem.rd) // rs2 is dependent on the value in exmem stage
                    {
                        buf.idex.rs2val = buf.exmem.exe_out;
                        rb = buf.idex.rs2val;
                    }
                }
            }
        }

        if (buf.memwb.rd!="00000" && (buf.idex.rs1 == buf.memwb.rd || buf.idex.rs2 == buf.memwb.rd))
        {
            appendToConsole("!!MEMORY STAGE DATA HAZARD DETECTED!!");
            if (!forwarding_enable)
            {
                if (!stall) appendToConsole("STALLING THE PIPELINE FOR 1 CYCLE");
                else appendToConsole("!!STALLED ALREADY!!");
                iag.pc = buf.ifid.pc;
                buf.idex.flush();
                stall=true;
            }
            else
            {
                appendToConsole("DATA FORWARDING");
                appendToConsole("From Instruction "+buf.memwb.instr+": MEMORY Stage");
                appendToConsole("To Instruction "+buf.idex.instr+": DECODE Stage");
                if (buf.idex.rs1==buf.memwb.rd) 
                {
                    buf.idex.rs1val = ry;
                    ra = buf.idex.rs1val;
                }
                if (buf.idex.rs2==buf.memwb.rd) 
                {
                    buf.idex.rs2val = ry;
                    rb = buf.idex.rs2val;
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
        : data_memory(data_mem), text_memory(text_mem), iag(iagRef), registers(reg), alu(aluRef), buf(buffer), brpre(brpreRef){}


    void fetch()
    {
        // get the instruction from global variable pc
        text_memory.MAR = iag.pc;
        text_memory.load();
        if (text_memory.MDR=="" || iag.use_return_addr)
        {
            buf.ifid.pc = "ffffffff"; buf.ifid.next_pc = "ffffffff"; buf.ifid.instr = "";
        }
        else 
        {
            buf.ifid.pc = iag.pc;
            buf.ifid.next_pc = dec_to_hex_32bit(hex_to_dec(iag.pc) + 4);
            buf.ifid.instr = text_memory.MDR;
        }

        pair<bool,string> prediction = brpre.predictBranch(buf.ifid.pc);

        if(prediction.first)
        {
            iag.update(prediction.second, true);
        }

        iag.compute_nextPC();
    }

    void decode()
    {
        if (buf.ifid.pc  == "ffffffff") //NOP
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

            if (buf.idex.opcode == "0100011" || buf.idex.opcode == "1100011") buf.idex.rd = "00000"; // rd is none in S and SB type instr

            buf.idex.instr_type = getInstructionType(buf.idex.opcode, buf.idex.funct3, buf.idex.funct7);
            
            if (buf.idex.opcode == "0000011" || buf.idex.opcode == "0010011" || buf.idex.opcode == "1100111") buf.idex.rs2 = "00000"; // rs2 is none in I type instr
            
            if (buf.idex.opcode == "1101111" || buf.idex.opcode== "0110111") {buf.idex.rs1 = "00000"; buf.idex.rs2="00000";} // rs1, rs2 is none in U type instr and UJ type

            buf.idex.instr = buf.ifid.instr;
            buf.idex.pc = buf.ifid.pc;
            buf.idex.next_pc = buf.ifid.next_pc;
            
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
        if (buf.idex.opcode!="0110011" && buf.idex.opcode!= "1100011") // if instruction is I or load or store or jalr or auipc
        {
            rb = buf.idex.imm;
        }

        if (buf.idex.opcode=="0010111") ra = buf.idex.pc; //auipc

        if (buf.idex.pc != "ffffffff" && buf.idex.instr != "00000073") alu.perform_op();

        buf.exmem.pc = buf.idex.pc; buf.exmem.next_pc = buf.idex.next_pc; buf.exmem.instr = buf.idex.instr; buf.exmem.opcode = buf.idex.opcode; buf.exmem.rd = buf.idex.rd; buf.exmem.funct3 = buf.idex.funct3; buf.exmem.rs1 = buf.idex.rs1; buf.exmem.rs2 = buf.idex.rs2; buf.exmem.funct7 = buf.idex.funct7; buf.exmem.imm = buf.idex.imm; buf.exmem.instr_type = buf.idex.instr_type; buf.exmem.rs1val = buf.idex.rs1val; buf.exmem.rs2val = buf.idex.rs2val;
        buf.exmem.exe_out = rz;

        string ret_addr;
        if (buf.idex.opcode=="1100011") //branch
        {
            if (buf.exmem.exe_out=="00000001")
            {
                ret_addr = dec_to_hex_32bit(hex_to_dec(buf.idex.pc) + hex_to_dec_signed(buf.idex.imm));
                if (buf.ifid.pc!=ret_addr)
                {
                    iag.update(ret_addr, true);
                    buf.ifid.flush();
                    buf.idex.flush();
                }
                brpre.update(buf.exmem.pc, true, ret_addr);
            }

            else
            {
                ret_addr = buf.exmem.next_pc;
                if(buf.ifid.pc!=ret_addr)
                {
                    iag.update(ret_addr, true);
                    buf.ifid.flush();
                    buf.idex.flush();
                }
                brpre.update(buf.exmem.pc, false, "");
            }
        }

        else if(buf.idex.opcode=="1101111") //jal
        {
            ret_addr = dec_to_hex_32bit(hex_to_dec(buf.idex.pc) + hex_to_dec_signed(buf.idex.imm));
            if(buf.ifid.pc!=ret_addr)
            {
                iag.update(ret_addr, true);
                buf.ifid.flush();
                buf.idex.flush();
            }
            brpre.update(buf.exmem.pc, true, ret_addr);
        }

        else if(buf.idex.opcode=="1100111") //jalr
        {
            ret_addr = buf.exmem.exe_out;
            if(buf.ifid.pc!=ret_addr)
            {
                iag.update(ret_addr, true);
                buf.ifid.flush();
                buf.idex.flush();
            }
            brpre.update(buf.exmem.pc, true, ret_addr);
        }

        if (buf.exmem.opcode=="1101111" || buf.exmem.opcode=="1100111") 
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
        }
        buf.memwb.pc = buf.exmem.pc; buf.memwb.next_pc = buf.exmem.next_pc; buf.memwb.instr = buf.exmem.instr;buf.memwb.opcode=buf.exmem.opcode; buf.memwb.rd=buf.exmem.rd; buf.memwb.funct3=buf.exmem.funct3; buf.memwb.rs1=buf.exmem.rs1; buf.memwb.rs2=buf.exmem.rs2; buf.memwb.funct7=buf.exmem.funct7; buf.memwb.imm=buf.exmem.imm; buf.memwb.instr_type=buf.exmem.instr_type; buf.memwb.rs1val=buf.exmem.rs1val; buf.memwb.rs2val=buf.exmem.rs2val;
        buf.memwb.exe_out = buf.exmem.exe_out;
    }
    void accessMemory(string address, string type) 
    {
        if (buf.exmem.pc != "ffffffff")
        {
            data_memory.MAR = address;
            data_memory.load(type);
            ry = data_memory.MDR;
        }
        buf.memwb.pc = buf.exmem.pc; buf.memwb.next_pc = buf.exmem.next_pc; buf.memwb.instr = buf.exmem.instr;buf.memwb.opcode=buf.exmem.opcode; buf.memwb.rd=buf.exmem.rd; buf.memwb.funct3=buf.exmem.funct3; buf.memwb.rs1=buf.exmem.rs1; buf.memwb.rs2=buf.exmem.rs2; buf.memwb.funct7=buf.exmem.funct7; buf.memwb.imm=buf.exmem.imm; buf.memwb.instr_type=buf.exmem.instr_type; buf.memwb.rs1val=buf.exmem.rs1val; buf.memwb.rs2val=buf.exmem.rs2val;
        buf.memwb.exe_out = buf.exmem.exe_out;
    }
    void accessMemory()
    {
        ry = rz;
        buf.memwb.pc = buf.exmem.pc; buf.memwb.next_pc = buf.exmem.next_pc; buf.memwb.instr = buf.exmem.instr;buf.memwb.opcode=buf.exmem.opcode; buf.memwb.rd=buf.exmem.rd; buf.memwb.funct3=buf.exmem.funct3; buf.memwb.rs1=buf.exmem.rs1; buf.memwb.rs2=buf.exmem.rs2; buf.memwb.funct7=buf.exmem.funct7; buf.memwb.imm=buf.exmem.imm; buf.memwb.instr_type=buf.exmem.instr_type; buf.memwb.rs1val=buf.exmem.rs1val; buf.memwb.rs2val=buf.exmem.rs2val;
        buf.memwb.exe_out = buf.exmem.exe_out;
    }

    void writeBack(bool &flag)
    {
        if (buf.memwb.pc != "ffffffff") 
        {
            registers.rd = stoi(buf.memwb.rd, nullptr, 2);
            registers.writeRD();
        }
        if(buf.memwb.opcode == "1110011") 
        {
            flag = false;
        }
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
    {}

    void appendToConsole(const string &message)
    {
        cout << message << endl;
    }

    void step_cycle(bool &flag)
    {
        appendToConsole("Cycle " + to_string(clock_cycle + 1) + ":");

        if (f.buf.memwb.opcode != "1100011" && f.buf.memwb.opcode != "0100011")
        {
            f.writeBack(flag);
            appendToConsole(
                "  W: rd=x" + to_string(f.registers.rd) +
                " val="   + ry
            );
        }
        else
        {
            appendToConsole("  W: -");
        }

        if (f.buf.exmem.opcode == "0000011")
        {
            f.accessMemory(rz, f.buf.exmem.funct3);
            appendToConsole(
                "  M: LOAD type=" + f.buf.exmem.funct3 +
                " data="       + ry +
                " addr="       + rz
            );
        }
        else if (f.buf.exmem.opcode == "0100011")
        {
            f.accessMemory(rz, f.buf.exmem.rs2val, f.buf.exmem.funct3);
            appendToConsole(
                "  M: STORE type=" + f.buf.exmem.funct3 +
                " data="        + f.buf.exmem.rs2val +
                " addr="        + rz
            );
        }
        else
        {
            f.accessMemory();
            appendToConsole("  M: -");
        }

        f.execute();
        appendToConsole(
            "  E: op="     + f.alu.operation +
            " result="    + rz
        );

        f.decode();
        appendToConsole(
            "  D: opcode=" + f.buf.idex.opcode +
            " rd="     + f.buf.idex.rd    +
            " rs1="    + f.buf.idex.rs1   +
            " rs2="    + f.buf.idex.rs2   +
            " type="   + f.buf.idex.instr_type
        );

        f.fetch();
        appendToConsole(
            "  F: PC="    + f.buf.ifid.pc +
            " Instr="     + f.buf.ifid.instr
        );

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


int main()
{
    PMI_data data_memory; PMI_text text_memory; IAG iag; RegisterFile registers; ALU alu; buffers buffr; BranchPredictor brpre;

    control_circuitry control(data_memory, text_memory, iag, registers, alu, buffr, brpre);

    // string input_filename = "factorial.mc";
    string input_filename = "short_input.mc";

    ifstream inputFile(input_filename);
    if (!inputFile) throw invalid_argument("Unable to open file");exit;

    string line;
    bool memory_flag = false;

    while (getline(inputFile, line)) 
    {  // Read line by line
        stringstream ss(line);
        string code, addr;
        if (line=="") 
        {
            memory_flag = true;
            continue;
        }

        if(memory_flag)
        {
            addr = line.substr(2, 8);
            code = "000000" + line.substr(11, 2);
            data_memory.MAR = addr;
            data_memory.MDR = code;
            data_memory.store("000");
        }
        else
        {
            int temp = line.find_first_of(' ');
            addr = line.substr(2, temp-2);
            line = line.substr(temp + 1);
            code = line.substr(2, line.find_first_of(' ')-2);
            text_memory.MAR = addr;
            text_memory.MDR = code;
            text_memory.store();
        }
    }
    
    inputFile.close();

    control.run_cycles();
    

    cout<<buffr.ifid.pc<<' '<<buffr.ifid.instr<<endl;
    cout<<buffr.idex.pc<<' '<<buffr.idex.instr<<endl;
    cout<<buffr.exmem.pc<<' '<<buffr.exmem.instr<<endl;
    cout<<buffr.memwb.pc<<' '<<buffr.memwb.instr<<endl;
    

    registers.printRegs();

    int count = 0;
    cout<<"Text Segment"<<endl;
    for (auto it : text_memory.mem.memory) 
    {
        string temp;
        if (it.second=="") temp = "00";
        else temp = it.second;
        cout<<dec_to_hex_32bit(it.first)<<':'<<temp<<" ";
        count++;

        if (count % 4 == 0) cout << endl;
    }
    cout<<endl;

    count = 0;
    cout<<"Data Segment"<<endl;
    for (auto it : data_memory.mem.memory) 
    {
        string temp;
        if (it.second=="") temp = "00";
        else temp = it.second;
        cout<<dec_to_hex_32bit(it.first)<<':'<<temp<<" ";
        count++;

        if (count % 4 == 0) cout << endl;
    }
    cout<<endl;

    cout<<"Total clock cycles: "<<clock_cycle<<endl;

    return 0;
}