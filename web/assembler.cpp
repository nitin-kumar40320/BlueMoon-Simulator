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
#include <functional>
using namespace std;

map<string, int> label_address_map = {};
vector<pair<int, string>> input_file_instr;
vector<string> input_data_file_instr;
vector<string> memory;
vector<string> machine_codes;

vector<string> convert_to_fixed_hex(const string &input, const string &directive) 
{
    // Normalize the directive (remove any leading '.' and convert to lowercase).
    string type = directive;
    if (!type.empty() && type[0] == '.')
        type.erase(type.begin());
    transform(type.begin(), type.end(), type.begin(), ::tolower);

    int bits = 0;
    bool isDouble = false;  // true for 64-bit directives that split into two 32-bit parts
    if (type == "byte") {
        bits = 8;
    } else if (type == "half" || type == "halfword") {
        bits = 16;
    } else if (type == "word") {
        bits = 32;
    } else if (type == "double" || type == "doubleword" || type == "dword") {
        bits = 64;
        isDouble = true;
    } else {
        throw invalid_argument("Unknown data directive: " + directive);
    }

    // Check for a single character literal enclosed in single quotes.
    if (input.size() >= 2 && input.front() == '\'' && input.back() == '\'') {
        if (input.size() != 3) {
            throw invalid_argument("Character literal must be a single character enclosed by single quotes: '" + input + "'");
        }
        char ch = input[1];
        unsigned long long charVal = static_cast<unsigned char>(ch);
        unsigned long long mask = (bits < 64) ? ((1ULL << bits) - 1ULL) : 0xFFFFFFFFFFFFFFFFULL;
        unsigned long long finalVal = charVal & mask;
        vector<string> result;
        if (!isDouble) {
            ostringstream ss;
            ss << "0x" << uppercase << setw(bits / 4) << setfill('0') << hex << finalVal;
            result.push_back(ss.str());
        } else {
            unsigned int high = static_cast<unsigned int>((finalVal >> 32) & 0xFFFFFFFF);
            unsigned int low  = static_cast<unsigned int>(finalVal & 0xFFFFFFFF);
            ostringstream ssHigh, ssLow;
            ssHigh << "0x" << uppercase << setw(8) << setfill('0') << hex << high;
            ssLow  << "0x" << uppercase << setw(8) << setfill('0') << hex << low;
            result.push_back(ssHigh.str());
            result.push_back(ssLow.str());
        }
        return result;
    }

    // Validate that if a '-' is present it appears only at the very beginning.
    {
        size_t firstMinusPos = input.find('-');
        if (firstMinusPos != string::npos) {
            if (firstMinusPos != 0 || input.find('-', firstMinusPos + 1) != string::npos) {
                throw invalid_argument("NumberFormatException: Invalid number format (misplaced '-' sign): '" + input + "'");
            }
        }
    }

    // Determine if the value is negative and get the number part.
    bool isNegative = (!input.empty() && input[0] == '-');
    string numStr = isNegative ? input.substr(1) : input;

    // Determine numeric base from prefixes.
    int base = 10;
    if (numStr.size() >= 2 && numStr[0] == '0') {
        if (numStr[1] == 'x' || numStr[1] == 'X') {
            base = 16;
            numStr.erase(0, 2); // remove "0x"
        } else if (numStr[1] == 'b' || numStr[1] == 'B') {
            base = 2;
            numStr.erase(0, 2); // remove "0b"
        }
    }

    // If base is 10 but any hex letter appears, switch to hex.
    if (base == 10) {
        bool hasHexAlpha = false;
        for (char c : numStr) {
            if ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
                hasHexAlpha = true;
                break;
            }
        }
        if (hasHexAlpha)
            base = 16;
    }

    if (numStr.empty()) {
        throw invalid_argument("NumberFormatException: No digits found in input: '" + input + "'");
    }
    for (char c : numStr) {
        if (base == 10) {
            if (!isdigit(c)) {
                throw invalid_argument("NumberFormatException: Invalid character in input: '" + input + "'");
            }
        } else if (base == 16) {
            if (!isxdigit(c)) {
                throw invalid_argument("NumberFormatException: Invalid character in input: '" + input + "'");
            }
        } else if (base == 2) {
            if (c != '0' && c != '1') {
                throw invalid_argument("NumberFormatException: Invalid character in input: '" + input + "'");
            }
        }
    }

    // Parse the magnitude.
    unsigned long long magnitude = 0ULL;
    try {
        magnitude = stoull(numStr, nullptr, base);
    } catch (const invalid_argument &) {
        throw invalid_argument("NumberFormatException: Invalid number format: '" + input + "'");
    } catch (const out_of_range &) {
        throw invalid_argument("NumberFormatException: Invalid number format (too large): '" + input + "'");
    }

    // Enforce range and determine final bit pattern.
    unsigned long long mask = (bits < 64) ? ((1ULL << bits) - 1ULL) : 0xFFFFFFFFFFFFFFFFULL;
    unsigned long long finalVal = 0ULL;
    if (isNegative) {
        unsigned long long maxNeg = (1ULL << (bits - 1));
        if (magnitude > maxNeg) {
            throw out_of_range("Value " + input + " is out of signed range for " + directive);
        }
        long long signedVal = -static_cast<long long>(magnitude);
        finalVal = static_cast<unsigned long long>(signedVal) & mask;
    } else {
        if (base == 10) {
            unsigned long long maxPos = ((1ULL << (bits - 1)) - 1ULL);
            if (magnitude > maxPos) {
                throw out_of_range("Value " + input + " is out of signed range for " + directive);
            }
            finalVal = magnitude & mask;
        } else {
            unsigned long long maxUnsigned = (bits < 64) ? ((1ULL << bits) - 1ULL) : 0xFFFFFFFFFFFFFFFFULL;
            if (magnitude > maxUnsigned) {
                throw out_of_range("Value " + input + " is out of range for " + directive);
            }
            finalVal = magnitude & mask;
        }
    }

    // Format the final value as hex.
    vector<string> result;
    if (!isDouble) {
        ostringstream ss;
        ss << "0x" << uppercase << setw(bits / 4) << setfill('0') << hex << finalVal;
        result.push_back(ss.str());
    } else {
        unsigned int high = static_cast<unsigned int>((finalVal >> 32) & 0xFFFFFFFF);
        unsigned int low  = static_cast<unsigned int>(finalVal & 0xFFFFFFFF);
        ostringstream ssHigh, ssLow;
        ssHigh << "0x" << uppercase << setw(8) << setfill('0') << hex << high;
        ssLow  << "0x" << uppercase << setw(8) << setfill('0') << hex << low;
        result.push_back(ssHigh.str());
        result.push_back(ssLow.str());
    }
    return result;
}

// Us while writing this code- https://i.pinimg.com/originals/26/b2/50/26b250a738ea4abc7a5af4d42ad93af0.jpg

map<string, string> opcode_map = 
{
    // R-format
    {"add", "0110011"}, {"and", "0110011"}, {"or", "0110011"}, {"sll", "0110011"},
    {"slt", "0110011"}, {"sra", "0110011"}, {"srl", "0110011"}, {"sub", "0110011"},
    {"xor", "0110011"}, {"mul", "0110011"}, {"div", "0110011"}, {"rem", "0110011"},
    
    // I-format
    {"addi", "0010011"}, {"andi", "0010011"}, {"ori", "0010011"},
    {"lb", "0000011"}, {"ld", "0000011"}, {"lh", "0000011"}, {"lw", "0000011"},
    {"jalr", "1100111"},
    
    // S-format
    {"sb", "0100011"}, {"sw", "0100011"}, {"sd", "0100011"}, {"sh", "0100011"},
    
    // SB-format
    {"beq", "1100011"}, {"bne", "1100011"}, {"bge", "1100011"}, {"blt", "1100011"},
    
    // U-format
    {"auipc", "0010111"}, {"lui", "0110111"},
    
    // UJ-format
    {"jal", "1101111"}
};

map<string, string> func3_map  = 
{
    // R-format
    {"add", "000"}, {"and", "111"}, {"or", "110"}, {"sll", "001"},
    {"slt", "010"}, {"sra", "101"}, {"srl", "101"}, {"sub", "000"},
    {"xor", "100"}, {"mul", "000"}, {"div", "100"}, {"rem", "110"},

    // I-format
    {"addi", "000"}, {"andi", "111"}, {"ori", "110"},
    {"lb", "000"}, {"ld", "011"}, {"lh", "001"}, {"lw", "010"},
    {"jalr", "000"},

    // S-format
    {"sb", "000"}, {"sw", "010"}, {"sd", "011"}, {"sh", "001"},

    // SB-format
    {"beq", "000"}, {"bne", "001"}, {"bge", "101"}, {"blt", "100"}
    
};

map<string, string> func7_map = 
{
    {"add", "0000000"}, {"and", "0000000"}, {"or", "0000000"}, {"sll", "0000000"},
    {"slt", "0000000"}, {"sra", "0100000"}, {"srl", "0000000"}, {"sub", "0100000"},
    {"xor", "0000000"}, {"mul", "0000001"}, {"div", "0000001"}, {"rem", "0000001"}
};


int label_to_offset(int address, string label)
{
    int label_address = label_address_map[label];
    return label_address-address;
}

vector<string>input_parse(string instr)
{
    vector<string>input;
    string cur;
    for(int i=0;i<instr.size();i++)
    {
        if(instr[i]==' '||instr[i]==',' || instr[i]=='(' || instr[i]==')')
        {
            if(cur!="")input.push_back(cur);
            cur="";
        }
        else cur+=instr[i];
    }
    if(cur!="")input.push_back(cur);
    return input;
}

string bth(string bin)
{
    bin = bin.substr(bin.find_first_not_of('0'));

    if (bin.size() > 32) 
    {
        throw out_of_range("Input number is more than 32 bits");
    }

    int gap = 32 - bin.size();
    for (int i = 0; i < gap; i++) bin = '0' + bin;
    string hex="0x";
    for(int i=0;i<bin.size();i+=4)
    {
        int dec=0;
        for(int j=0;j<4;j++)
        {
            dec*=2;
            dec+=bin[i+j]-'0';
        }
        if(dec<10)hex+=dec+'0';
        else hex+=dec-10+'A';
    }
    return hex;
}

int htd(string hex) 
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
            throw invalid_argument("Invalid hexadecimal character: " + ch);
        }

        dec += val * base;
        base *= 16;
    }

    return dec;
}

string dec_to_hex(int num) 
{
    string res = "0x";
    stringstream ss;
    ss << uppercase << setfill('0') << hex << num;
    return res+ss.str();
}



string reg_to_bin(string reg)
{
    if(reg[0]!='x')
    {
        throw invalid_argument(" You used the word " + reg + ". This is not good hun!  Don't do this to me! I will write a mail to your FA and your life will be destroyed");
    }
    int reg_num=stoi(reg.substr(1));
    if(reg_num<0||reg_num>31)
    {
        throw out_of_range("The register number is out of range");
    }
    return bitset<5>(reg_num).to_string();
}



string UJ (int address, string instr) //jal x1,label
{
    //immediate[20,10:1,11,19:12] (20 bits) | rd (5 bits) | opcode (7 bits)
    string mc_code_bin, mc_code_hex;
    string res;
    vector<string>input=input_parse(instr);
    string opcode = opcode_map[input[0]];
    string rd=input[1];
    string label=input[2];
    int imm=label_to_offset(address, label);
    // imm=imm>>1;
    string imm_bin=bitset<21>(imm).to_string();
    string rd_bin=reg_to_bin(rd);

    mc_code_bin+=imm_bin[0];
    mc_code_bin+=imm_bin.substr(10,10);
    mc_code_bin+=imm_bin[9];
    mc_code_bin+=imm_bin.substr(1,8);
    mc_code_bin+=rd_bin;
    mc_code_bin+=opcode;

    mc_code_hex=bth(mc_code_bin);
    res+=mc_code_hex;
    res+=" , ";
    res+=instr;
    res+=" # ";
    res+=opcode;   
    res+="-";
    res+="NULL";
    res+="-";
    res+="NULL";
    res+="-";
    res+=rd_bin;
    res+="-";
    res+="NULL";
    res+="-";
    res+="NULL";
    res+="-";
    res+=imm_bin;
    return res;
}

string U(int address, string instr) //lui x1,100
{
    //immediate[31:12] (20 bits) | rd (5 bits) | opcode (7 bits) 
    string mc_code_bin, mc_code_hex;
    string res;
    vector<string>input=input_parse(instr);
    string opcode = opcode_map[input[0]];
    string rd=input[1];
    string imm=input[2];
    int imm_dec;
    if(imm.substr(0,2)=="0x") imm_dec=htd(imm.substr(2));
    else imm_dec=stoi(imm);

    if(imm_dec<0||imm_dec>1048575)
    {
        throw out_of_range("Immediate value " + imm + " out of range in instruction- '" + instr + "'");
    }

    string imm_bin=bitset<20>(imm_dec).to_string();
    string rd_bin=reg_to_bin(rd);

    mc_code_bin+=imm_bin;
    mc_code_bin+=rd_bin;
    mc_code_bin+=opcode;
    mc_code_hex=bth(mc_code_bin);
    res+=mc_code_hex;
    res+=" , ";
    res+=instr;
    res+=" # ";
    res+=opcode;
    res+="-";
    res+="NULL";
    res+="-";
    res+="NULL";
    res+="-";
    res+=rd_bin;
    res+="-";
    res+="NULL";
    res+="-";
    res+="NULL";
    res+="-";
    res+=imm_bin.substr(0,20);
    return res;
}

string S(int address, string instr) 
{
    // instr = "x11, 1(x5)"
    //fetch the resistor number
    vector<string> obj = input_parse(instr);

    string opcode = opcode_map[obj[0]];

    string func3 = func3_map[obj[0]];
    string func7 = "NULL";

    string rd_val = reg_to_bin(obj[1]);
    string rs1_val = reg_to_bin(obj[3]);
    string rs2_val = "NULL";

    string imm = bitset<12>(stoi(obj[2])).to_string();
    //do error handeling for imm value
    int imm_val = stoi(obj[2]);
    if (imm_val < -2048 || imm_val > 2047) 
    {
        throw out_of_range("Immediate value out of range in instruction- '" + instr + "'");
    }
    
    string machine_code = imm.substr(0,7) + rd_val + rs1_val + func3 + imm.substr(7,5) + opcode;
    
    string machine_hex =  bth(machine_code);
    
    string answer = machine_hex + " , " + instr + " # " + opcode + "-" + func3 + "-" + func7 + "-" + rd_val + "-" + rs1_val +"-"+ rs2_val +"-"+ imm;
    return answer;
}


string SB(int address, string instr) 
{ 
    // <opcode-func3-func7-rd-rs1-rs2-immediate>
    // imm[12]+imm[10:5]+rs2+rs1+func3+imm[4:1]+imm[11]+opcode
    vector<string> obj = input_parse(instr);

    string opcode = opcode_map[obj[0]];

    string func3 = func3_map[obj[0]];
    string func7 = "NULL";

    string rd_val = "NULL";
    string rs1_val = reg_to_bin(obj[1]);
    string rs2_val = reg_to_bin(obj[2]);

    int imm_val = label_to_offset(address, obj[3]);

    if (imm_val < -4096 || imm_val > 4095) 
    {
        throw out_of_range("Immediate value out of range in instruction- '" + instr + "'");
    }

    string imm = bitset<13>(imm_val).to_string();
    // cout<<imm<<' '<<imm_val<<endl;
    string machine_code = "";
    machine_code += imm[0] + imm.substr(2,6);
    machine_code += rs2_val + rs1_val + func3;
    machine_code += imm.substr(8,4) + imm[1];
    machine_code += opcode;
    // cout << machine_code << ' ' << machine_code.size() << endl;
    
    string machine_hex =  bth(machine_code);

    // cout << machine_hex << ' ' << machine_hex.size() << endl;
  
    string answer = machine_hex + " , " + instr + " # " + opcode + "-" + func3 + "-" + func7 + "-" + rd_val + "-" + rs1_val +"-"+ rs2_val +"-"+ imm;
  
    return answer;

}


string R(int address, string instr)
{
    // <opcode-func3-func7-rd-rs1-rs2-immediate>
    // func7+rs2+rs1+func3+rd+opcode

    string res="";
    
    vector<string> strings = input_parse(instr);

    string opcode =opcode_map[strings[0]] , func3= func3_map[strings[0]], func7 = func7_map[strings[0]];
    

    string rd =  reg_to_bin(strings[1]),
    
    rs1 =  reg_to_bin(strings[2]),
    rs2 = reg_to_bin(strings[3]);

    string immediate = "NULL";

    string machine_code = bth(func7 + rs2 + rs1 + func3 + rd + opcode);

    res += machine_code + " , " + instr + " # ";
    res += opcode + '-' + func3 + '-' + func7 + '-' + rd + '-' + rs1 + '-' + rs2 + '-' + immediate;
    return res;

}

string I(int address, string instr)
{
    // <opcode-func3-func7-rd-rs1-rs2-immediate>
    // immediate+rs1+func3+rd+opcode
    string res="";
    
    vector<string> strings = input_parse(instr);
    string opcode= opcode_map[strings[0]];
    string func3 = func3_map[strings[0]];

    if (strings[3][0]=='x') {
        //it is of format lb x1, 0(x2)
        swap(strings[2], strings[3]);
    }
    string func7 = "NULL";

    string rd= reg_to_bin(strings[1]), rs1= reg_to_bin(strings[2]);

    string rs2 = "NULL";

    int immedec;
    if (strings[3].substr(0,2) == "0x") immedec = htd(strings[3].substr(2));
    else immedec = stoi(strings[3]);

    if (immedec > 2047 || immedec < -2048)
    {
        throw out_of_range("Immediate value is out of bounds in instruction- '" + instr + "'");
    }

    string immediate = bitset<12>(immedec).to_string();

    string machine_code = immediate + rs1 + func3 + rd + opcode;
    string machine_hex = bth(machine_code);
    res += machine_hex + " , " + instr + " # ";
    res += opcode + '-' + func3 + '-' + func7 + '-' + rd + '-' + rs1 + '-' + rs2 + '-' + immediate;
    return res;
}


map<string, function<string(int, string)>> instructionType =
{
    //R-format
    {"add", R}, {"and", R}, {"or", R}, {"sll", R},
    {"slt", R}, {"sra", R}, {"srl", R}, {"sub", R},
    {"xor", R}, {"mul", R}, {"div", R}, {"rem", R},

    //I-format
    {"addi", I}, {"andi", I}, {"ori", I},
    {"lb", I}, {"ld", I}, {"lh", I}, {"lw", I},
    {"jalr", I},

    //S-format
    {"sb", S}, {"sw", S}, {"sd", S}, {"sh", S},

    //SB-format
    {"beq", SB}, {"bne", SB}, {"bge", SB}, {"blt", SB},

    //U-format
    {"auipc", U}, {"lui", U},

    //UJ-format
    {"jal", UJ}
};


void first_parse_stream(istream &input_stream)
{
    bool data_flag = false;
    int address = -4;
    string line;

    while (getline(input_stream, line))
    {
        if (line.empty())
            continue; // skip empty lines

        string instr = line;
        string first_word;
        bool quotes_flag = false;
        int colon_end = 0;
        int instr_start = 0;

        for (int i = 0; i < (int)line.size(); i++)
        {
            if (line[i] == ' ' && !first_word.empty())
                break;
            else if (line[i] != ' ')
                first_word += line[i];
            colon_end = i + 1;
        }

        if (!first_word.empty() && first_word.front() == '#')
            continue; // skip comment lines
        if (first_word == ".data")
        {
            data_flag = true;
            continue;
        }
        if (first_word == ".text")
        {
            data_flag = false;
            continue;
        }

        if (!data_flag)
            address += 4;

        // Check labels
        for (int i = 0; i < (int)line.size(); i++)
        {
            if (line[i] == ':' && !quotes_flag)
            {
                instr_start = i + 1;
                string temp = line.substr(0, i);
                temp.erase(0, temp.find_first_not_of(' '));
                if (label_address_map.find(temp) != label_address_map.end())
                {
                    throw invalid_argument("Label " + temp + " already used above");
                }
                label_address_map[temp] = address;
                break;
            }
            if (line[i] == '"' || line[i] == '\'' || line[i] == '#')
                quotes_flag = true;
        }

        instr = instr.substr(instr_start);
        instr.erase(0, instr.find_first_not_of(' ')); // strip leading spaces

        if (instr.empty())
        {
            address -= 4;
            continue; // line with only label
        }

        // Store the instruction
        if (data_flag)
            input_data_file_instr.push_back(instr);
        else
            input_file_instr.push_back({address, instr});
    }
}

void handle_text_segment()
{
    int address;
    string instr;

    for (auto instruction : input_file_instr) 
    {
        address = instruction.first;
        instr = instruction.second;

        string name=input_parse(instr)[0];
        string res = instructionType[name](address, instr);

        machine_codes.push_back(dec_to_hex(address) + " " + res);
    }
    machine_codes.push_back(dec_to_hex(address+4) + " 0x00000073 , end of file # exit");
}


void handle_data_segment()
{
    for (auto instr: input_data_file_instr)
    {
        vector<string> input = input_parse(instr);
        string dtype = input[0];

        if (dtype == ".asciz")
        {
            string str;
            int i = 0;

            // Locate the starting quote
            while (instr[i] != '\"') i++;
            i++; // Move past the opening quote

            // Process the string while handling special characters
            while (instr[i] != '\"')
            {
                if (instr[i] == '\\') // Handle escaped characters
                {
                    i++;
                    switch (instr[i])
                    {
                    case 'n': str += '\n'; break;
                    case 't': str += '\t'; break;
                    case '\\': str += '\\'; break;
                    case '\"': str += '\"'; break;
                    case 'r': str += '\r'; break;
                    default:
                        throw invalid_argument("Invalid escape sequence in .asciz directive: \\" + string(1, instr[i]));
                    }
                }
                else
                {
                    str += instr[i];
                }
                i++;
            }

            // Convert the string to ASCII and push to memory as hex
            for (int j = 0; j < (int)str.size(); j++)
            {
                int ascii = int(str[j]);
                vector<string> hexVec = convert_to_fixed_hex(to_string(ascii), ".byte");
                memory.push_back(hexVec[0].substr(2));
            }

            // Ensure valid syntax after the closing quote
            for (int j = i + 1; j < instr.size(); j++)
            {
                if (instr[j] == ' ') continue;
                if (instr[j] == '#') break; // Comment detected, ignore the rest
                throw invalid_argument("Invalid .asciz directive: " + instr);
            }
        }

        if (dtype == ".byte" || dtype == ".half" || dtype == ".word" ||
            dtype == ".dword")
        {
            for (int i = 1; i < (int)input.size(); i++)
            {
                if (input[i][0] == '#') { break; }
                vector<string> hexParts = convert_to_fixed_hex(input[i], dtype);
                
                for (auto hexStr : hexParts)
                {
                    
                    for (int j=2;j<hexStr.size()-1;j+=2) {
                        memory.push_back(hexStr.substr(j,2));
                        
                    }
                    
                }

            }
        }
    }
}

string assemble(const string &asmCode)
{
    // Clear global data to avoid state conflicts across calls
    input_file_instr.clear();
    input_data_file_instr.clear();
    memory.clear();
    machine_codes.clear();
    label_address_map.clear();

    // Feed the string into first_parse_stream
    stringstream ss(asmCode);
    first_parse_stream(ss); // Reuse the same parsing logic

    handle_data_segment();
    handle_text_segment();

    // Collect results in a string
    ostringstream output;
    for (auto &it : machine_codes)
    {
        output << it << "\n";
    }
    output << "\n";

    int data_address = 268435456;
    for (auto &it : memory)
    {
        output << dec_to_hex(data_address) << " " << it << "\n";
        data_address += 1;
    }
    return output.str();
}

int main()
{
    string asmCode = R"(
        .data
        num: .word 8

        .text
        lui x5, 0x10000
        lui x6, 0x10000
        ori x6, x6, 0x100

        n: lw x9, 0(x5)
        result: addi x10, x0, 1
        jal x1, fact
        sw x10, 0(x6)
        jal x0, code_end

        fact: 
            addi x2, x2, -8
            sw x1, 4(x2)
            sw x9, 8(x2)
            
            bne x9, x0, skip
                addi x10, x0, 1
                beq x0, x0, case_end
            
            skip:
                addi x9, x9, -1
                jal x1, fact
                addi x9, x9, 1
                mul x10, x9, x10
                
            case_end:
                lw x1, 4(x2)
                lw x9, 8(x2)
                addi x31, x0, -1
                sw x31, 8(x2)
                sw x31, 4(x2)
                addi x2, x2, 8
                jalr x0, x1, 0

        code_end:
        )";

    string result = assemble(asmCode);
    cout << result << endl;

    return 0;
}