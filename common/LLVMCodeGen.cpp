/////////////////////////////////////////////////////////////////
//
//    LLVMCodeGen - LLVM IR generation for the Asl programming language
//
//    Copyright (C) 2017-2023  Universitat Politecnica de Catalunya
//
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU General Public License
//    as published by the Free Software Foundation; either version 3
//    of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Affero General Public License for more details.
//
//    You should have received a copy of the GNU Affero General Public
//    License along with this library; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
//
//    contact: José Miguel Rivero (rivero@cs.upc.edu)
//             Computer Science Department
//             Universitat Politecnica de Catalunya
//             despatx Omega.110 - Campus Nord UPC
//             08034 Barcelona.  SPAIN
//
////////////////////////////////////////////////////////////////


#include "LLVMCodeGen.h"
#include "SymTable.h"
#include "TypesMgr.h"
#include "code.h"

#include <string>
#include <cctype>
// uncomment to disable assert()
// #define NDEBUG
#include <cassert>
#include <algorithm>     // find

// using namespace std;


const bool LLVMCodeGen::COMMENTS_ENABLED = false;

const std::string LLVMCodeGen::INDENT_INSTR     = "    ";
const std::string LLVMCodeGen::INDENT_LABEL     = "  ";

const std::string LLVMCodeGen::LLVM_INT         = "i32";
const std::string LLVMCodeGen::LLVM_FLOAT       = "float";
const std::string LLVMCodeGen::LLVM_CHAR        = "i8";
const std::string LLVMCodeGen::LLVM_BOOL        = "i1";
const std::string LLVMCodeGen::LLVM_VOID        = "void";
const std::string LLVMCodeGen::LLVM_LABEL       = "label";
const std::string LLVMCodeGen::LLVM_TYERR       = "tErr";
const std::string LLVMCodeGen::LLVM_TYMISS      = "tMiss";
const std::string LLVMCodeGen::LLVM_INT_BOOL    = "tIntBool";

const std::string LLVMCodeGen::LLVM_INT_PTR     = "i32*";
const std::string LLVMCodeGen::LLVM_FLOAT_PTR   = "float*";
const std::string LLVMCodeGen::LLVM_CHAR_PTR    = "i8*";
const std::string LLVMCodeGen::LLVM_BOOL_PTR    = "i1*";

const std::string LLVMCodeGen::LLVM_INT1        = "i1";
const std::string LLVMCodeGen::LLVM_INT8        = "i8";
const std::string LLVMCodeGen::LLVM_INT32       = "i32";
const std::string LLVMCodeGen::LLVM_INT64       = "i64";
const std::string LLVMCodeGen::LLVM_DOUBLE      = "double";

const std::string LLVMCodeGen::LLVM_GLOBAL_INT_ADDR   = "@.global.i.addr";
const std::string LLVMCodeGen::LLVM_GLOBAL_FLOAT_ADDR = "@.global.f.addr";
const std::string LLVMCodeGen::LLVM_GLOBAL_CHAR_ADDR  = "@.global.c.addr";

const std::string LLVMCodeGen::LLVM_ZERO_INT    = "0";
const std::string LLVMCodeGen::LLVM_ZERO_FLOAT  = "0.0";
const std::string LLVMCodeGen::LLVM_ONE_INT     = "1";

const std::string LLVMCodeGen::LLVM_ENTRY       = ".entry";

const std::string LLVMCodeGen::LLVM_ZEXT        = "zext";
const std::string LLVMCodeGen::LLVM_FPEXT       = "fpext";
const std::string LLVMCodeGen::LLVM_TRUNC       = "trunc";
const std::string LLVMCodeGen::LLVM_FPTRUNC     = "fptrunc";
const std::string LLVMCodeGen::LLVM_SEXT        = "sext";


const std::map<instruction::Operation, std::string> LLVMCodeGen::tcode2llvmInstrMap = {
  { instruction::_ADD,  "add" },
  { instruction::_SUB,  "sub" },
  { instruction::_MUL,  "mul" },
  { instruction::_DIV,  "sdiv" },
  { instruction::_FADD, "fadd" },
  { instruction::_FSUB, "fsub" },
  { instruction::_FMUL, "fmul" },
  { instruction::_FDIV, "fdiv" },
  { instruction::_EQ,   "icmp eq" },
  { instruction::_LT,   "icmp slt" },
  { instruction::_LE,   "icmp sle" },
  { instruction::_FEQ,  "fcmp oeq" },
  { instruction::_FLT,  "fcmp olt" },
  { instruction::_FLE,  "fcmp ole" },
  { instruction::_AND,  "and" },
  { instruction::_OR,   "or" },
};


LLVMCodeGen::LLVMCodeGen(const TypesMgr & Types, const SymTable & Symbols, const code & tCode)
  : Types{Types}, Symbols{Symbols}, tCode{tCode},
    writeI(false), writeF(false), writeC(false), writeLN(false),
    readI(false), readF(false), readC(false),
    haltAndExit(false),
    globalI(false), globalF(false), globalC(false)
{
  std::string failFunc, failTempVar;
  check_SSA_tCode(failFunc, failTempVar);
  if (failFunc != "") {
    std::cerr << std::endl;
    std::cerr << ";;; *****************************************************************************" << std::endl;
    std::cerr << ";;; WARNING: in order to generate LLVM code, this emitter impose the following"    << std::endl;
    std::cerr << ";;;          restriction: the temporal variables in the t-code cannot be multiply" << std::endl;
    std::cerr << ";;;          defined inside a function."                                           << std::endl;
    std::cerr << ";;;          For example, this happens in function '";
    std::cerr << failFunc << "' with temporal '" << failTempVar << "'"       << std::endl;
    std::cerr << ";;; *****************************************************************************" << std::endl;
    std::cerr << std::endl;
    std::exit(EXIT_SUCCESS);
  }
}

void LLVMCodeGen::check_SSA_tCode(std::string & failFunc, std::string & failTempVar) const {
  failFunc = "";
  failTempVar = "";
  for (auto & subr: tCode.get_subroutine_list()) {
    std::map<std::string, int> modTempCounts;
    for (auto & instr: subr.get_instructions()) {
      switch (instr.oper) {
      case instruction::_LABEL:
      case instruction::_UJUMP:
      case instruction::_FJUMP:
      case instruction::_HALT:
      case instruction::_PUSH:
      case instruction::_RETURN:
      case instruction::_XLOAD:
      case instruction::_CLOAD:
      case instruction::_WRITEI:
      case instruction::_WRITEF:
      case instruction::_WRITEC:
      case instruction::_WRITES:
      case instruction::_WRITELN:
      case instruction::_NOOP:
      case instruction::_INVALID:
        break;
      default:                 // Except in instruction::_POP, where is optional (arg1 may be ""),
                               // the argument arg1 always does exist.
        std::string arg1 = getTCodeArg(instr, 1);
        if (isTCodeTemporal(arg1)) {
          modTempCounts[arg1] += 1;
        }
        break;
      }
    }
    for (auto & pair : modTempCounts) {
      if (pair.second > 1) {
        failFunc = subr.get_name();
        failTempVar = pair.first;
        return;
      }
    }
  }
}

bool LLVMCodeGen::isTCodeTemporal(const std::string & tcodeArg) const {
  if (tcodeArg.size() < 2) return false;
  if (tcodeArg[0] != '%')  return false;
  if (not std::isdigit(tcodeArg[1])) return false;
  return true;
}

bool LLVMCodeGen::isTCodeIdentifier(const std::string & tcodeArg) const {
  // IMPORTANT:
  // tcodeArg can not be the arg2 argument of a CHLOAD instruction:
  // %7 = 'a' where oper = _CHLOAD, arg1 = "%7", arg2 = "a"
  if (tcodeArg.size() < 1)  return false;
  if (tcodeArg[0] == '%')   return false;
  if (std::isdigit(tcodeArg[0])) return false;
  return true;
}

void LLVMCodeGen::computeReadWriteHaltInfo() {
  for (auto & subr: tCode.get_subroutine_list()) {
    for (auto & instr: subr.get_instructions()) {
      std::string arg1 = getTCodeArg(instr, 1);
      std::string arg2 = getTCodeArg(instr, 2);
      std::string arg3 = getTCodeArg(instr, 3);
      switch (instr.oper) {
      case instruction::_WRITEI:
        writeI = true;
        break;
      case instruction::_WRITEF:
        writeF = true;
        break;
      case instruction::_WRITEC:
        writeC = true;
        break;
      case instruction::_WRITES:
        if (std::find(writeSAslStrVec.begin(), writeSAslStrVec.end(), arg1) == writeSAslStrVec.end()) {
          writeSAslStrVec.push_back(arg1);
        }
        writeS = true;
        break;
      case instruction::_WRITELN:
        writeLN = true;
        break;
      case instruction::_READI:
        readI = true;
        if (isTCodeTemporal(arg1))
          globalI = true;
        break;
      case instruction::_READF:
        readF = true;
        if (isTCodeTemporal(arg1))
          globalF = true;
        break;
      case instruction::_READC:
        readC = true;
        if (isTCodeTemporal(arg1))
          globalC = true;
        break;
      case instruction::_HALT:
	haltAndExit = true;
	break;
      default:
        break;
      }
    }
  }
}

void LLVMCodeGen::startNewFunction(const subroutine & subr) {
  currentFunctionName = subr.get_name();
  isMain = (currentFunctionName == "main");
  prevInstrIsTerminator = false;
}

void LLVMCodeGen::bindTCodeLocalSymbolsToLLVMTypes(const subroutine & subr) {
  llvmLocalValueVec.clear();
  llvmLocalValueTypeMap.clear();
  llvmLocalValueCountMap.clear();
  std::string funcName = subr.get_name();
  for (auto param : subr.params) {
    std::string llvmType;
    if (param.name == "_result")
      llvmType = getFuncReturnLLVMType(funcName);
    else
      llvmType = getLocalSymbolLLVMType(funcName, param.name, true);
    bindTCodeLocalValueWithType(param.name, llvmType);
  }
  for (auto varlocal : subr.vars) {
    std::string llvmType = getLocalSymbolLLVMType(funcName, varlocal.name);
    bindTCodeLocalValueWithType(varlocal.name, llvmType);
  }
  for (auto instr : subr.get_instructions()) {
    std::string arg1 = getTCodeArg(instr, 1);
    std::string arg2 = getTCodeArg(instr, 2);
    std::string arg3 = getTCodeArg(instr, 3);
    switch (instr.oper) {
    case instruction::_LABEL:
      {
        bindTCodeLocalValueWithType(arg1, LLVM_LABEL);
        break;
      }
    case instruction::_UJUMP:
      {
        bindTCodeLocalValueWithType(arg1, LLVM_LABEL);
        break;
      }
    case instruction::_FJUMP:
      {
        bindTCodeLocalValueWithType(arg1, LLVM_BOOL);
        bindTCodeLocalValueWithType(arg2, LLVM_LABEL);
        break;
      }
    case instruction::_HALT:
      {
        break;
      }
    case instruction::_LOAD:
      {
        if (isTCodeIdentifier(arg1) and isTCodeTemporal(arg2)) {       //  a = %4
          std::string llvmValue1 = getLLVMValue(arg1);
          std::string llvmType1 = getLLVMTypeOfValue(llvmValue1);
          bindTCodeLocalValueWithType(arg2, llvmType1);
        }
        else if (isTCodeTemporal(arg1) and isTCodeIdentifier(arg2)) {  // %4 = a
          std::string llvmValue2 = getLLVMValue(arg2);
          std::string llvmType2 = getLLVMTypeOfValue(llvmValue2);
          bindTCodeLocalValueWithType(arg1, llvmType2);
        }
        else if (isTCodeTemporal(arg1) and isTCodeTemporal(arg2)) {    // %4 = %6
          std::string llvmValue2 = getLLVMValue(arg2);
          std::string llvmType2 = getLLVMTypeOfValue(llvmValue2);
          bindTCodeLocalValueWithType(arg1, llvmType2);
        }
        break;
      }
    case instruction::_ILOAD:
      {
        int n = std::stoi(arg2);
        if (n != 0 and n != 1)
          bindTCodeLocalValueWithType(arg1, LLVM_INT);
        else
          bindTCodeLocalValueWithType(arg1, LLVM_INT_BOOL);
        break;
      }
    case instruction::_FLOAD:
      {
        bindTCodeLocalValueWithType(arg1, LLVM_FLOAT);
        break;
      }
    case instruction::_CHLOAD:
      {
        bindTCodeLocalValueWithType(arg1, LLVM_CHAR);
        break;
      }
    case instruction::_PUSH:
      {
        if (arg1 != "") {
          bindTCodeLocalValueWithType(arg1, LLVM_TYMISS);
          pushTCodeParamCallStack(arg1);
        }
        break;
      }
    case instruction::_POP:
      {
        if (arg1 != "")
          bindTCodeLocalValueWithType(arg1, pendingCallLLVMRetType);
        break;
      }
    case instruction::_CALL:
      {
        std::vector<std::string> llvmParamTypes = getFuncParamsLLVMTypes(arg1);
        int nParams = getFuncNumberOfParams(arg1);
        for (int i = nParams-1; i >= 0; --i) {
          std::string tcodeParam = topPopTCodeParamCallStack();
          std::string llvmParamType = llvmParamTypes[i];
          bindTCodeLocalValueWithType(tcodeParam, llvmParamType);
        }
        std::string retType = getFuncReturnLLVMType(arg1);
        if (retType != "void")
          pendingCallLLVMRetType = retType;
        break;
      }
    case instruction::_RETURN:
      {
        break;
      }
    case instruction::_ALOAD:
      {
        std::string llvmValue2 = getLLVMValue(arg2);
        std::string llvmType2 = getLLVMTypeOfValue(llvmValue2);
        std::string llvmType2Ptr;
        if (isLLVMArrayType(llvmType2))
          llvmType2Ptr = getLLVMArrayTypeAsPointerType(llvmType2);
        else
          llvmType2Ptr = llvmType2;  // getPointerToType(llvmType2);
        bindTCodeLocalValueWithType(arg1, llvmType2Ptr);
        break;
      }
    case instruction::_XLOAD:
      {
        std::string llvmValue1 = getLLVMValue(arg1);
        std::string llvmType1 = getLLVMTypeOfValue(llvmValue1);
        std::string llvmElemType;
        if (isLLVMArrayType(llvmType1))
          llvmElemType = getLLVMElementOfArrayType(llvmType1);
        else if (isPointerType(llvmType1))
          llvmElemType = getPointedType(llvmType1);
        else
          llvmElemType = LLVM_TYERR;
        bindTCodeLocalValueWithType(arg2, LLVM_INT);
        bindTCodeLocalValueWithType(arg3, llvmElemType);
        break;
      }
    case instruction::_LOADX:
      {
        std::string llvmValue2 = getLLVMValue(arg2);
        std::string llvmType2 = getLLVMTypeOfValue(llvmValue2);
        std::string llvmElemType;
        if (isLLVMArrayType(llvmType2))
          llvmElemType = getLLVMElementOfArrayType(llvmType2);
        else if (isPointerType(llvmType2))
          llvmElemType = getPointedType(llvmType2);
        else
          llvmElemType = LLVM_TYERR;
        bindTCodeLocalValueWithType(arg1, llvmElemType);
        bindTCodeLocalValueWithType(arg3, LLVM_INT);
        break;
      }
    case instruction::_LOADC:
      {
        // only: address ASSIG MUL TEMP   (x = *t1)
        std::string llvmValue1 = getLLVMValue(arg1);
        std::string llvmType1 = getLLVMTypeOfValue(llvmValue1);
        std::string llvmTypePtr = getPointerToType(llvmType1);
        bindTCodeLocalValueWithType(arg2, llvmTypePtr);
        break;
      }
    case instruction::_CLOAD:
      {
        // only: MUL TEMP ASSIG address   (*t1 = x)
        std::string llvmValue2 = getLLVMValue(arg2);
        std::string llvmType2 = getLLVMTypeOfValue(llvmValue2);
        std::string llvmTypePtr = getPointerToType(llvmType2);
        bindTCodeLocalValueWithType(arg1, llvmTypePtr);
        break;
      }
    case instruction::_WRITEI:
      {
        bindTCodeLocalValueWithType(arg1, LLVM_INT_BOOL);
        break;
      }
    case instruction::_WRITEF:
      {
        bindTCodeLocalValueWithType(arg1, LLVM_FLOAT);
        break;
      }
    case instruction::_WRITEC:
      {
        bindTCodeLocalValueWithType(arg1, LLVM_CHAR);
        break;
      }
    case instruction::_WRITES:
      {
        break;
      }
    case instruction::_WRITELN:
      {
        break;
      }
    case instruction::_READI:
      {
        bindTCodeLocalValueWithType(arg1, LLVM_INT_BOOL);
        break;
      }
    case instruction::_READF:
      {
        bindTCodeLocalValueWithType(arg1, LLVM_FLOAT);
        break;
      }
    case instruction::_READC:
      {
        bindTCodeLocalValueWithType(arg1, LLVM_CHAR);
        break;
      }
    case instruction::_ADD:
    case instruction::_SUB:
    case instruction::_MUL:
    case instruction::_DIV:
      {
        bindTCodeLocalValueWithType(arg1, LLVM_INT);
        bindTCodeLocalValueWithType(arg2, LLVM_INT);
        bindTCodeLocalValueWithType(arg3, LLVM_INT);
        break;
      }
    case instruction::_EQ:
    case instruction::_LT:
    case instruction::_LE:
      {
        bindTCodeLocalValueWithType(arg1, LLVM_BOOL);
        if (isTCodeIdentifier(arg2) and isTCodeTemporal(arg3)) {
          std::string llvmValue2 = getLLVMValue(arg2);
          std::string llvmType2 = getLLVMTypeOfValue(llvmValue2);
          bindTCodeLocalValueWithType(arg3, llvmType2);
        }
        else if (isTCodeTemporal(arg2) and isTCodeIdentifier(arg3)) {
          std::string llvmValue3 = getLLVMValue(arg3);
          std::string llvmType3 = getLLVMTypeOfValue(llvmValue3);
          bindTCodeLocalValueWithType(arg2, llvmType3);
        }
        else if (isTCodeTemporal(arg2) and isTCodeTemporal(arg3)) {
          bindPairOfTCodeLocalValuesWithTypes(arg2, arg3);
        }
        break;
      }
    case instruction::_FEQ:
    case instruction::_FLT:
    case instruction::_FLE:
      {
        bindTCodeLocalValueWithType(arg1, LLVM_BOOL);
        bindTCodeLocalValueWithType(arg2, LLVM_FLOAT);
        bindTCodeLocalValueWithType(arg3, LLVM_FLOAT);
        break;
      }
    case instruction::_NEG:
      {
        bindTCodeLocalValueWithType(arg1, LLVM_INT);
        bindTCodeLocalValueWithType(arg2, LLVM_INT);
        break;
      }
    case instruction::_FADD:
    case instruction::_FSUB:
    case instruction::_FMUL:
    case instruction::_FDIV:
      {
        bindTCodeLocalValueWithType(arg1, LLVM_FLOAT);
        bindTCodeLocalValueWithType(arg2, LLVM_FLOAT);
        bindTCodeLocalValueWithType(arg3, LLVM_FLOAT);
        break;
      }
    case instruction::_FNEG:
      {
        bindTCodeLocalValueWithType(arg1, LLVM_FLOAT);
        bindTCodeLocalValueWithType(arg2, LLVM_FLOAT);
        break;
      }
    case instruction::_FLOAT:
      {
        bindTCodeLocalValueWithType(arg1, LLVM_FLOAT);
        bindTCodeLocalValueWithType(arg2, LLVM_INT);
        break;
      }
    case instruction::_AND:
    case instruction::_OR:
      {
        bindTCodeLocalValueWithType(arg1, LLVM_BOOL);
        bindTCodeLocalValueWithType(arg2, LLVM_BOOL);
        bindTCodeLocalValueWithType(arg3, LLVM_BOOL);
        break;
      }
    case instruction::_NOT:
      {
        bindTCodeLocalValueWithType(arg1, LLVM_BOOL);
        bindTCodeLocalValueWithType(arg2, LLVM_BOOL);
        break;
      }
    case instruction::_NOOP:
      {
        break;
      }
    default:
      {
        break;
      }
    }
  }
  bool errors = false;
  for (auto & llvmValue : llvmLocalValueVec) {
    std::string llvmType = llvmLocalValueTypeMap.at(llvmValue);
    if (llvmType == LLVM_TYERR or llvmType == LLVM_TYMISS) {
      errors = true;
      break;
    }
  }
  if (errors) {
    std::cerr << "ERROR: some local values of this function can not been binded to a valid type:" << std::endl;
    std::cerr << "++++++++++++++++++++++++++++++++ function: " << funcName << std::endl;
    for (auto & value : llvmLocalValueVec) {
      std::cerr << value << ": \t" << llvmLocalValueTypeMap.at(value) << std::endl;
    }
    std::cerr << "--------------------------------" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  for (auto & llvmValue : llvmLocalValueVec) {
    std::string llvmType = llvmLocalValueTypeMap.at(llvmValue);
    if (llvmType == LLVM_INT_BOOL)
      llvmLocalValueTypeMap[llvmValue] = LLVM_INT;
  }
}

std::string LLVMCodeGen::getFuncReturnLLVMType(const std::string & tcodeFuncIdent) const {
  TypesMgr::TypeId tid = Symbols.getGlobalFunctionType(tcodeFuncIdent);
  TypesMgr::TypeId tr = Types.getFuncReturnType(tid);
  return TypeIdToLLVMType(tr);
}

int LLVMCodeGen::getFuncNumberOfParams(const std::string & tcodeFuncIdent) const {
  TypesMgr::TypeId tid = Symbols.getGlobalFunctionType(tcodeFuncIdent);
  return Types.getNumOfParameters(tid);
}

std::string LLVMCodeGen::getFuncParamLLVMType(const std::string & tcodeFuncIdent, int i) const {
  TypesMgr::TypeId tid = Symbols.getGlobalFunctionType(tcodeFuncIdent);
  TypesMgr::TypeId tParam = Types.getParameterType(tid, i);
  std::string llvmType = TypeIdToLLVMType(tParam, true);
  return llvmType;
}

std::vector<std::string> LLVMCodeGen::getFuncParamsLLVMTypes(const std::string & tcodeFuncIdent) const {
  TypesMgr::TypeId tid = Symbols.getGlobalFunctionType(tcodeFuncIdent);
  std::size_t n = Types.getNumOfParameters(tid);
  std::vector<std::string> typesVec(n);
  for (std::size_t i = 0; i < n; ++i) {
    TypesMgr::TypeId tParam = Types.getParameterType(tid, i);
    typesVec[i] = TypeIdToLLVMType(tParam, true);
  }
  return typesVec;
}

std::string LLVMCodeGen::getLocalSymbolLLVMType(const std::string & tcodeFuncIdent,
                                                const std::string & tcodeSymbolIdent,
                                                bool isParameter) const {
  TypesMgr::TypeId tid = Symbols.getLocalSymbolType(tcodeFuncIdent, tcodeSymbolIdent);
  return TypeIdToLLVMType(tid, isParameter);
}

std::string LLVMCodeGen::TypeIdToLLVMType(TypesMgr::TypeId tid, bool isParameter) const {
  if (Types.isIntegerTy(tid))
    return LLVM_INT;
  else if (Types.isFloatTy(tid))
    return LLVM_FLOAT;
  else if (Types.isBooleanTy(tid))
    return LLVM_BOOL;
  else if (Types.isCharacterTy(tid))
    return LLVM_CHAR;
  else if (Types.isVoidTy(tid))
    return LLVM_VOID;
  else if (Types.isArrayTy(tid)) {
    TypesMgr::TypeId te = Types.getArrayElemType(tid);
    std::string teLLVM = TypeIdToLLVMType(te);
    if (not isParameter) {
      std::size_t n = Types.getArraySize(tid);
      return "[" + std::to_string(n) + " x " + teLLVM + "]";
    }
    else {
      return getPointerToType(teLLVM);  // teLLVM + "*"
    }
  }
  return LLVM_TYERR;
}

void LLVMCodeGen::getLLVMStringFromAslString(const std::string & aslString,
					     std::string & llvmString,
					     std::string::size_type & llvmStringSize) {
  llvmString = aslString.substr(1, aslString.size()-2);
  llvmStringSize = llvmString.size();
  for (const auto& from_to : std::map<std::string, std::string>{{"\\n", "\\0A"}, {"\\t","\\09"}, {"\\\\", "\\\\"}}) {
    std::string from = from_to.first;
    std::string   to = from_to.second;
    std::string::size_type pos = 0;
    while ((pos = llvmString.find(from, pos)) != std::string::npos) {
      llvmString.replace(pos, from.length(), to);
      llvmStringSize = llvmStringSize-from.size()+1;
      pos += to.length();  // Handles case where 'to' is a substring of 'from'
    }
  }
}

void LLVMCodeGen::generateReadWriteHaltBeginEndCode(std::string & begin, std::string & end) {
  begin = end = "";
  computeReadWriteHaltInfo();
  if (writeI or writeF or writeC or writeS or writeLN or readI or readF or readC)
    begin += "\n";
  if (writeI or readI)
    begin += "@.str.i = constant [3 x i8] c\"%d\\00\"\n";
  if (writeF or readF)
    begin += "@.str.f = constant [3 x i8] c\"%g\\00\"\n";
  if (writeC or readC)
    begin += "@.str.c = constant [3 x i8] c\"%c\\00\"\n";
  std::string::size_type n = writeSAslStrVec.size();
  writeSLLVMStrSizeVec = std::vector<std::string::size_type>(n);
  for (std::string::size_type i = 0; i < n; ++i) {
    std::string            llvmStr;
    std::string::size_type llvmStrSize;
    getLLVMStringFromAslString(writeSAslStrVec[i], llvmStr, llvmStrSize);
    begin += "@.str.s." + std::to_string(i+1) + " = constant [" + std::to_string(llvmStrSize+1) + " x i8] c\"" + llvmStr + "\\00\"\n";
    writeSLLVMStrSizeVec[i] = llvmStrSize+1;
  }
  if (writeI or readI or writeF or readF or writeC or readC)
    begin += "\n\n";
  if (globalI)
    begin += "@.global.i.addr = common dso_local global i32 0\n";
  if (globalF)
    begin += "@.global.f.addr = common dso_local global float 0.000000e+00\n";
  if (globalC)
    begin += "@.global.c.addr = common dso_local global i8 0\n";
  if (writeI or readI or writeF or readF or writeC or readC)
    begin += "\n\n";
  if (writeI or writeF or writeC or writeLN or readI or readF or readC or haltAndExit)
    end += "\n";
  if (writeI or writeF or writeC or writeS or writeLN) {
    if (writeI or writeF or writeS)
      end += "declare dso_local i32 @printf(i8*, ...)\n";
    if (writeC or writeLN)
      end += "declare dso_local i32 @putchar(i32)\n";
  }
  if (readI or readF or readC) {
    end += "declare dso_local i32 @__isoc99_scanf(i8*, ...)\n";
  }
  if (haltAndExit) {
    end += "declare dso_local void @exit(i32) noreturn nounwind\n";
  }
  if (writeI or writeF or writeC or writeS or writeLN or readI or readF or readC or haltAndExit)
    end += "\n";
}

std::string LLVMCodeGen::dumpLLVM() {
  std::string llvmCode, llvmBegin, llvmEnd;
  generateReadWriteHaltBeginEndCode(llvmBegin, llvmEnd);
  bindGlobalValuesWithTypes();
  for (auto & subr: tCode.get_subroutine_list()) {
    bindTCodeLocalSymbolsToLLVMTypes(subr);
    startNewFunction(subr);
    llvmCode += dumpSubroutine(subr);
  }
  llvmCode = llvmBegin + llvmCode + llvmEnd;
  return llvmCode;
}

std::string LLVMCodeGen::dumpSubroutine(const subroutine & subr) {
  std::string llvmCode;
  llvmCode += dumpHeader(subr);
  llvmCode += "{\n";
  llvmCode += llvmComment("   ENTRY label:");
  bindLLVMLocalValueWithType(LLVM_ENTRY, LLVM_LABEL);
  llvmCode += createLABEL(LLVM_ENTRY);
  llvmCode += llvmComment("   --------------------- alloca params:");
  llvmCode += dumpAllocaParams(subr);
  llvmCode += llvmComment("   --------------------- alloca local vars:");
  llvmCode += dumpAllocaLocalVars(subr);
  llvmCode += llvmComment("   --------------------- store params:");
  llvmCode += dumpStoreParams(subr);
  llvmCode += llvmComment("   --------------------- instructions:");
  llvmCode += dumpInstructionList(subr);
  llvmCode += "}\n\n";
  return llvmCode;
}

std::string LLVMCodeGen::dumpHeader(const subroutine & subr) {
  std::string llvmCode;
  llvmCode += "define dso_local ";
  std::string funcName = subr.get_name();
  if (funcName == "main") {
    llvmCode += LLVM_INT + " @" + "main" + "() ";
  }
  else {
    llvmCode += getFuncReturnLLVMType(funcName) + " @" + funcName + "(";
    bool firstParam = true;
    for (auto p : subr.params) {
      if (p.name != "_result") {
        std::string llvmValue = getLLVMValue(p.name);
        std::string llvmType  = getLocalSymbolLLVMType(funcName, p.name, true);
        if (not firstParam) llvmCode += ", ";
        else firstParam = false;
        llvmCode += llvmType + " " + llvmValue;
      }
    }
    llvmCode += ") ";
  }
  return llvmCode;
}

std::string LLVMCodeGen::dumpAllocaParams(const subroutine & subr) {
  std::string llvmCode;
  std::string funcName = subr.get_name();
  for (auto p : subr.params) {
    std::string llvmValue = getLLVMValue(p.name);
    std::string llvmType;
    if (p.name == "_result")
      llvmType = getFuncReturnLLVMType(funcName);
    else
      llvmType = getLocalSymbolLLVMType(funcName, p.name, true);
    std::string llvmValueAddr = getLLVMValueAddr(llvmValue);
    std::string llvmTypePtr   = getPointerToType(llvmType);
    bindLLVMLocalValueWithType(llvmValueAddr, llvmTypePtr);
    llvmCode += llvmComment("   param " + p.name + " " + llvmType);
    llvmCode += createALLOCA(llvmValueAddr, llvmType);
  }
  return llvmCode;
}

std::string LLVMCodeGen::dumpAllocaLocalVars(const subroutine & subr) {
  std::string llvmCode;
  std::string funcName = subr.get_name();
  for (auto v : subr.vars) {
    std::string llvmValue     = getLLVMValue(v.name);
    std::string llvmType      = getLocalSymbolLLVMType(funcName, v.name);
    std::string llvmValueAddr = getLLVMValueAddr(llvmValue);
    std::string llvmTypePtr   = getPointerToType(llvmType);
    bindLLVMLocalValueWithType(llvmValueAddr, llvmTypePtr);
    llvmCode += llvmComment("   localVar " + v.name +  " " + llvmType);
    llvmCode += createALLOCA(llvmValueAddr, llvmType);
  }
  return llvmCode;
}

std::string LLVMCodeGen::dumpStoreParams(const subroutine & subr) {
  std::string llvmCode;
  std::string funcName = subr.get_name();
  if (funcName == "main") {
    // std::string llvmValue     = getLLVMValue("_result");
    // // std::string llvmType      = LLVM_INT;
    // std::string llvmValueAddr = getLLVMValueAddr(llvmValue);
    // // std::string llvmTypePtr   = getPointerToType(llvmType);
    // // bindLLVMLocalValueWithType(llvmValue, llvmType);         // DONE IN dumpAllocaParams
    // // bindLLVMLocalValueWithType(llvmValueAddr, llvmTypePtr);  // DONE IN dumpAllocaParams
    // llvmCode += createStore(LLVM_ZERO_INT, llvmValueAddr);    // "store i32 0, i32* %_result.addr";
  }
  if (subr.params.size() > 0) {
    llvmCode += llvmComment("params initialization:");
  }
  for (auto p : subr.params) {
    if (p.name != "_result") {
      std::string llvmValue     = getLLVMValue(p.name);
      std::string llvmValueAddr = getLLVMValueAddr(llvmValue);
      llvmCode += createSTORE(llvmValue, llvmValueAddr);
    }
  }
  return llvmCode;
}

std::string LLVMCodeGen::dumpInstructionList(const subroutine & subr) {
  std::string llvmCode;
  int n = subr.get_instructions().size();
  instructionList instrList = subr.get_instructions();
  for (int i = 0; i < n-1; ++i) {
    llvmCode += llvmComment(instrList[i].dump());
    llvmCode += dumpInstruction(instrList[i], instrList[i+1]);
  }
  llvmCode += llvmComment(instrList[n-1].dump());
  llvmCode += dumpInstruction(instrList[n-1], instruction::NOOP());
  return llvmCode;
}


std::string LLVMCodeGen::dumpInstruction(const instruction & instr,
                                         const instruction & next) {
  std::string llvmCode;

  std::string llvmValue1, llvmValue2, llvmValue3;
  std::string llvmMemCodeValue1, llvmMemCodeValue2, llvmMemCodeValue3;

  std::string tcodeArg1 = getTCodeArg(instr, 1);
  std::string tcodeArg2 = getTCodeArg(instr, 2);
  std::string tcodeArg3 = getTCodeArg(instr, 3);

  switch (instr.oper) {
  case instruction::_LABEL:
    {
      std::string label = tcodeArg1;
      std::string llvmLabel = getLLVMValue(label);
      if (not prevInstrIsTerminator)
        llvmCode += createBR(llvmLabel);
      llvmCode += createLABEL(label);
      break;
    }
  case instruction::_UJUMP:
    {
      std::string label = tcodeArg1;
      std::string llvmLabel = getLLVMValue(label);
      llvmCode += createBR(llvmLabel);
      if (next.oper != instruction::_LABEL and next.oper != instruction::_NOOP) {
        std::string labelDead = createNewPrefixedValueWithType("%.dead.cont", LLVM_LABEL);
        std::string labelDeadName = labelDead.substr(1);
        llvmCode += createLABEL(labelDeadName);
      }
      break;
    }
  case instruction::_FJUMP:
    {
      accessValueOfArgument(tcodeArg1, llvmValue1, llvmMemCodeValue1);
      llvmCode += llvmMemCodeValue1;
      std::string labelJump = getLLVMValue(tcodeArg2);
      if (next.oper != instruction::_LABEL and next.oper != instruction::_NOOP) {
        std::string labelCont = createNewPrefixedValueWithType("%.br.cont", LLVM_LABEL);
        std::string labelContName = labelCont.substr(1);
        llvmCode += createBR(llvmValue1, labelCont, labelJump);
        llvmCode += createLABEL(labelContName);
      }
      else {
        std::string labelCont = getLLVMValue(next.arg1);
        llvmCode += createBR(llvmValue1, labelCont, labelJump);
      }
      break;
    }
  case instruction::_HALT:
    {
      llvmCode += createHALT();
      break;
    }
  case instruction::_LOAD:
    {
      llvmValue1 = getLLVMValue(tcodeArg1);
      llvmValue2 = getLLVMValue(tcodeArg2);
      if (isTCodeIdentifier(tcodeArg1)) {  //  a = %4   or   a = b
        accessValueOfArgument(tcodeArg2, llvmValue2, llvmMemCodeValue2);
        std::string llvmValue1Addr = getLLVMValueAddr(llvmValue1);
        llvmCode += llvmMemCodeValue2;
        llvmCode += createSTORE(llvmValue2, llvmValue1Addr);
      }
      else if (isTCodeIdentifier(tcodeArg2)) {   // %4 = a
        std::string llvmValue2Addr = getLLVMValueAddr(llvmValue2);
        llvmCode += createLOAD(llvmValue1, llvmValue2Addr);
      }
      else {      // %4 = %6
        std::string llvmType = getLLVMTypeOfValue(llvmValue2);
        if (isLLVMAnyIntegerType(llvmType)) {
          std::string llvmTypeOneIntUp = getLLVMTypeOneIntUp(llvmType);
          std::string newValuePrefix = "%.temp." + tcodeArg1.substr(1) + "." + llvmTypeOneIntUp;
          std::string llvmValue2Extended = createNewPrefixedValueWithType(newValuePrefix, llvmTypeOneIntUp);
          llvmCode += createCONVERSION(LLVM_ZEXT, llvmValue2Extended, llvmValue2, llvmTypeOneIntUp);
          llvmCode += createCONVERSION(LLVM_TRUNC, llvmValue1, llvmValue2Extended, llvmType);
        }
        else {  // llvmType == LLVM_FLOAT
          std::string newValuePrefix = "%.temp." + tcodeArg1.substr(1) + ".double";
          std::string llvmValue2FPDouble = createNewPrefixedValueWithType(newValuePrefix, LLVM_DOUBLE);
          llvmCode += createCONVERSION(LLVM_FPEXT, llvmValue2FPDouble, llvmValue2, LLVM_DOUBLE);
          llvmCode += createCONVERSION(LLVM_FPTRUNC, llvmValue1, llvmValue2FPDouble, llvmType);
        }
      }
      break;
    }
  case instruction::_ILOAD:
    {
      llvmValue1 = getLLVMValue(tcodeArg1);
      llvmValue2 = getLLVMValue(tcodeArg2);
      if (isTCodeTemporal(tcodeArg1))
        llvmCode += createCONVERSION(LLVM_TRUNC, llvmValue1, llvmValue2, LLVM_INT64);
      else {
        std::string llvmValue1Addr = getLLVMValueAddr(llvmValue1);
        llvmCode += createSTORE(llvmValue2, llvmValue1Addr);
      }
      break;
    }
  case instruction::_FLOAD:
    {
      llvmValue1 = getLLVMValue(tcodeArg1);
      llvmValue2 = getLLVMValue(tcodeArg2);
      if (isTCodeTemporal(tcodeArg1))
        llvmCode += createCONVERSION(LLVM_FPTRUNC, llvmValue1, llvmValue2, LLVM_DOUBLE);
      else {
        std::string llvmValue1Addr = getLLVMValueAddr(llvmValue1);
        llvmCode += createSTORE(llvmValue2, llvmValue1Addr);
      }
      break;
    }
  case instruction::_CHLOAD:
    {
      llvmValue1 = getLLVMValue(tcodeArg1);
      int asciiCode = getAsciiCode(tcodeArg2);
      llvmValue2 = std::to_string(asciiCode);
      if (isTCodeTemporal(tcodeArg1))
        llvmCode += createCONVERSION(LLVM_TRUNC, llvmValue1, llvmValue2, LLVM_INT32);
      else {
        std::string llvmValue1Addr = getLLVMValueAddr(llvmValue1);
        llvmCode += createSTORE(llvmValue2, llvmValue1Addr);
      }
      break;
    }
  case instruction::_PUSH:
    {
      if (tcodeArg1 != "") {
        accessValueOfArgument(tcodeArg1, llvmValue1, llvmMemCodeValue1);
        std::string llvmType = getLLVMTypeOfValue(llvmValue1);
        llvmCode += llvmMemCodeValue1;
        pushLLVMParamCallStack(llvmValue1);
      }
      else {
        pushLLVMParamCallStack("");
      }
      break;
    }
  case instruction::_POP:
    {
      std::string param = topPopLLVMParamCallStack();
      if (param != "")
        pendingCallArgs.push_back(param);
      if (tcodeArg1 != "") {
        modifyValueOfArgument(tcodeArg1, llvmValue1, llvmMemCodeValue1);
        llvmCode += createCALL(pendingCallFunc, llvmValue1, pendingCallArgs);
        llvmCode += llvmMemCodeValue1;
      }
      else if (isEmptyLLVMParamCallStack()) {
        llvmCode += createCALL(pendingCallFunc, pendingCallArgs);
      }
      break;
    }
  case instruction::_CALL:
    {
      pendingCallFunc = tcodeArg1;
      pendingCallArgs.clear();
      if (isEmptyLLVMParamCallStack())
        llvmCode += createCALL(pendingCallFunc, pendingCallArgs);
      break;
    }
  case instruction::_RETURN:
    {
      std::string retType = getFuncReturnLLVMType(currentFunctionName);
      if (retType == LLVM_VOID) {
        if (isMain)
          llvmCode += createRET(LLVM_ZERO_INT, LLVM_INT);
        else
          llvmCode += createRET();
      }
      else {
        accessValueOfArgument("_result", llvmValue1, llvmMemCodeValue1);
        llvmCode += llvmMemCodeValue1;
        llvmCode += createRET(llvmValue1);
      }
      if (next.oper != instruction::_LABEL and next.oper != instruction::_NOOP) {
        std::string labelDead = createNewPrefixedValueWithType("%.dead.code", LLVM_LABEL);
        std::string labelDeadName = labelDead.substr(1);
        llvmCode += createLABEL(labelDeadName);
      }
      break;
    }
  case instruction::_XLOAD:
    {
      llvmValue1 =  getLLVMValue(tcodeArg1);
      accessValueOfArgument(tcodeArg2, llvmValue2, llvmMemCodeValue2);
      accessValueOfArgument(tcodeArg3, llvmValue3, llvmMemCodeValue3);
      std::string llvmType = getLLVMTypeOfValue(llvmValue1);   // it can  be "array of" or "pointer to"
      std::string llvmElemType;
      if (isLLVMArrayType(llvmType))
        llvmElemType = getLLVMElementOfArrayType(llvmType);
      else if (isPointerType(llvmType))
        llvmElemType = getPointedType(llvmType);
      std::string llvmElemTypePtr = getPointerToType(llvmElemType);
      std::string arrayIndex64 = createNewPrefixedValueWithType("%.idx64", LLVM_INT64);
      std::string arrayPointer = createNewPrefixedValueWithType("%.arrPtr", llvmElemTypePtr);
      std::string llvmValue1Addr;
      if (isTCodeIdentifier(tcodeArg1))
        llvmValue1Addr = getLLVMValueAddr(llvmValue1);
      else
        llvmValue1Addr = llvmValue1;
      llvmCode += llvmMemCodeValue2;
      llvmCode += llvmMemCodeValue3;
      llvmCode += createCONVERSION(LLVM_SEXT, arrayIndex64, llvmValue2, LLVM_INT);
      llvmCode += createGETELEMENTPTR(arrayPointer, llvmValue1Addr, arrayIndex64);
      llvmCode += createSTORE(llvmValue3, arrayPointer);
      break;
    }
  case instruction::_LOADX:
    {
      modifyValueOfArgument(tcodeArg1, llvmValue1, llvmMemCodeValue1);
      llvmValue2 = getLLVMValue(tcodeArg2);
      accessValueOfArgument(tcodeArg3, llvmValue3, llvmMemCodeValue3);
      std::string llvmType = getLLVMTypeOfValue(llvmValue2);   // it can  be "array of" or "pointer to"
      std::string llvmElemType;
      if (isLLVMArrayType(llvmType))
        llvmElemType = getLLVMElementOfArrayType(llvmType);
      else if (isPointerType(llvmType))
        llvmElemType = getPointedType(llvmType);
      std::string llvmElemTypePtr = getPointerToType(llvmElemType);
      std::string arrayIndex64 = createNewPrefixedValueWithType("%.idx64", LLVM_INT64);
      std::string arrayPointer = createNewPrefixedValueWithType("%.arrPtr", llvmElemTypePtr);
      std::string llvmValue2Addr;
      if (isTCodeIdentifier(tcodeArg2))
        llvmValue2Addr = getLLVMValueAddr(llvmValue2);
      else
        llvmValue2Addr = llvmValue2;
      llvmCode += llvmMemCodeValue3;
      llvmCode += createCONVERSION(LLVM_SEXT, arrayIndex64, llvmValue3, LLVM_INT);
      llvmCode += createGETELEMENTPTR(arrayPointer, llvmValue2Addr, arrayIndex64);
      llvmCode += createLOAD(llvmValue1, arrayPointer);
      llvmCode += llvmMemCodeValue1;
      break;
    }
  case instruction::_ALOAD:
    {
      llvmValue1 = getLLVMValue(tcodeArg1);
      llvmValue2 = getLLVMValue(tcodeArg2);
      std::string llvmType2 = getLLVMTypeOfValue(llvmValue2);
      std::string llvmValue2Addr= getLLVMValueAddr(llvmValue2);
      if (isLLVMArrayType(llvmType2))
        llvmCode += createGETELEMENTPTR(llvmValue1, llvmValue2Addr, LLVM_ZERO_INT);
      else if (isPointerType(llvmType2))
        llvmCode += createLOAD(llvmValue1, llvmValue2Addr);
      break;
    }
    /*
  case instruction::_LOADC:
    {
      llvmCode += ind + arg1 + " = *" + arg2 + "\n";
      break;
    }
  case instruction::_CLOAD:
    {
      llvmCode += ind + "*" + arg1 + " = " + arg2 + "\n";
      break;
    }
    */
  case instruction::_WRITEI:
    {
      accessValueOfArgument(tcodeArg1, llvmValue1, llvmMemCodeValue1);
      std::string llvmType1 = getLLVMTypeOfValue(llvmValue1);
      llvmCode += llvmMemCodeValue1;
      std::string printIntValue = llvmValue1;
      if (llvmType1 == LLVM_INT1) {
        printIntValue = createNewPrefixedValueWithType("%.wrti.i32", LLVM_INT32);
        llvmCode += createCONVERSION(LLVM_ZEXT, printIntValue, llvmValue1, LLVM_INT1);
      }
      llvmCode += createPRINTF(printIntValue, LLVM_INT);
      break;
    }
  case instruction::_WRITEF:
    {
      accessValueOfArgument(tcodeArg1, llvmValue1, llvmMemCodeValue1);
      llvmCode += llvmMemCodeValue1;
      std::string fpextValue = createNewPrefixedValueWithType("%.wrtf.double", LLVM_DOUBLE);
      llvmCode += createCONVERSION(LLVM_FPEXT, fpextValue, llvmValue1, LLVM_FLOAT);
      llvmCode += createPRINTF(fpextValue, LLVM_DOUBLE);
      break;
    }
  case instruction::_WRITEC:
    {
      accessValueOfArgument(tcodeArg1, llvmValue1, llvmMemCodeValue1);
      llvmCode += llvmMemCodeValue1;
      std::string zextValue = createNewPrefixedValueWithType("%.wrtc.i32", LLVM_INT32);
      llvmCode += createCONVERSION(LLVM_ZEXT, zextValue, llvmValue1, LLVM_INT8);
      llvmCode += createPUTCHAR(zextValue);
      break;
    }
  case instruction::_WRITES:
    {
      auto it = std::find(writeSAslStrVec.begin(), writeSAslStrVec.end(), tcodeArg1);
      std::size_t i = std::distance(writeSAslStrVec.begin(), it);
      std::string strFormat = "@.str.s." + std::to_string(i+1);
      std::string::size_type llvmStrSize = writeSLLVMStrSizeVec[i];
      llvmCode += createPRINTS(strFormat, llvmStrSize);
      break;
    }
  case instruction::_WRITELN:
    { int asciiNL = int('\n');
      llvmCode += createPUTCHAR(std::to_string(asciiNL));   // "10"
      break;
    }
  case instruction::_READI:
    {
      llvmValue1 = getLLVMValue(tcodeArg1);
      std::string llvmType1 = getLLVMTypeOfValue(llvmValue1);
      if (not isTCodeTemporal(tcodeArg1)) {
        std::string llvmValue1Addr = getLLVMValueAddr(llvmValue1);
        if (llvmType1 == LLVM_INT1) {
          std::string globalInt = createNewPrefixedValueWithType("%.readi.global.i", LLVM_INT32);
          std::string compare0 = createNewPrefixedValueWithType("%.readi.i1.cmp1", LLVM_INT1);
          std::string notCompare0 = createNewPrefixedValueWithType("%.readi.i1.not", LLVM_INT1);
          llvmCode += createSCANF(LLVM_GLOBAL_INT_ADDR);
          llvmCode += createLOAD(globalInt, LLVM_GLOBAL_INT_ADDR);
          llvmCode += createCOMPARISON(instruction::_EQ, compare0, globalInt, LLVM_ZERO_INT, LLVM_INT);
          llvmCode += createNOT(notCompare0, compare0);
          llvmCode += createSTORE(notCompare0, llvmValue1Addr);
        }
        else {
          llvmCode += createSCANF(llvmValue1Addr);
        }
      }
      else {
        if (llvmType1 == LLVM_INT1) {
          std::string globalInt = createNewPrefixedValueWithType("%.readi.global.i", LLVM_INT32);
          std::string compare0 = createNewPrefixedValueWithType("%.readi.i1.cmp1", LLVM_INT1);
          std::string notCompare0 = createNewPrefixedValueWithType("%.readi.i1.not", LLVM_INT1);
          llvmCode += createSCANF(LLVM_GLOBAL_INT_ADDR);
          llvmCode += createLOAD(globalInt, LLVM_GLOBAL_INT_ADDR);
          llvmCode += createCOMPARISON(instruction::_EQ, compare0, globalInt, LLVM_ZERO_INT, LLVM_INT);
          llvmCode += createNOT(llvmValue1, compare0);
        }
        else {
          llvmCode += createSCANF(LLVM_GLOBAL_INT_ADDR);
          llvmCode += createLOAD(llvmValue1, LLVM_GLOBAL_INT_ADDR);
        }
      }
     break;
    }
  case instruction::_READF:
    {
      llvmValue1 = getLLVMValue(tcodeArg1);
      if (not isTCodeTemporal(tcodeArg1)) {
        std::string llvmValue1Addr = getLLVMValueAddr(llvmValue1);
        llvmCode += createSCANF(llvmValue1Addr);
      }
      else {
        llvmCode += createSCANF(LLVM_GLOBAL_FLOAT_ADDR);
        llvmCode += createLOAD(llvmValue1, LLVM_GLOBAL_FLOAT_ADDR);
      }
      break;
    }
  case instruction::_READC:
    {
      llvmValue1 = getLLVMValue(tcodeArg1);
      if (not isTCodeTemporal(tcodeArg1)) {
        std::string llvmValue1Addr = getLLVMValueAddr(llvmValue1);
        llvmCode += createSCANF(llvmValue1Addr);
      }
      else {
        llvmCode += createSCANF(LLVM_GLOBAL_CHAR_ADDR);
        llvmCode += createLOAD(llvmValue1, LLVM_GLOBAL_CHAR_ADDR);
      }
      break;
    }
  case instruction::_ADD:
  case instruction::_SUB:
  case instruction::_MUL:
  case instruction::_DIV:
    {
      modifyValueOfArgument(tcodeArg1, llvmValue1, llvmMemCodeValue1);
      accessValueOfArgument(tcodeArg2, llvmValue2, llvmMemCodeValue2);
      accessValueOfArgument(tcodeArg3, llvmValue3, llvmMemCodeValue3);
      llvmCode += llvmMemCodeValue2;
      llvmCode += llvmMemCodeValue3;
      llvmCode += createARITHMETIC(instr.oper, llvmValue1, llvmValue2, llvmValue3, LLVM_INT);
      llvmCode += llvmMemCodeValue1;
      break;
    }
  case instruction::_EQ:
  case instruction::_LT:
  case instruction::_LE:
    {
      modifyValueOfArgument(tcodeArg1, llvmValue1, llvmMemCodeValue1);
      accessValueOfArgument(tcodeArg2, llvmValue2, llvmMemCodeValue2);
      accessValueOfArgument(tcodeArg3, llvmValue3, llvmMemCodeValue3);
      std::string llvmType23 = LLVM_INT;
      if (isTCodeIdentifier(tcodeArg2) or isTCodeTemporal(tcodeArg2)) {
        std::string llvmValue2 = getLLVMValue(tcodeArg2);
        llvmType23 = getLLVMTypeOfValue(llvmValue2);
      }
      else if (isTCodeIdentifier(tcodeArg3) or isTCodeTemporal(tcodeArg3)) {
        std::string llvmValue3 = getLLVMValue(tcodeArg3);
        llvmType23 = getLLVMTypeOfValue(llvmValue3);
      }
      llvmCode += llvmMemCodeValue2;
      llvmCode += llvmMemCodeValue3;
      llvmCode += createCOMPARISON(instr.oper, llvmValue1, llvmValue2, llvmValue3, llvmType23);
      llvmCode += llvmMemCodeValue1;
      break;
     }
  case instruction::_FEQ:
  case instruction::_FLT:
  case instruction::_FLE:
    {
      modifyValueOfArgument(tcodeArg1, llvmValue1, llvmMemCodeValue1);
      accessValueOfArgument(tcodeArg2, llvmValue2, llvmMemCodeValue2);
      accessValueOfArgument(tcodeArg3, llvmValue3, llvmMemCodeValue3);
      llvmCode += llvmMemCodeValue2;
      llvmCode += llvmMemCodeValue3;
      llvmCode += createCOMPARISON(instr.oper, llvmValue1, llvmValue2, llvmValue3, LLVM_FLOAT);
      llvmCode += llvmMemCodeValue1;
      break;
     }
  case instruction::_NEG:
    {
      modifyValueOfArgument(tcodeArg1, llvmValue1, llvmMemCodeValue1);
      accessValueOfArgument(tcodeArg2, llvmValue2, llvmMemCodeValue2);
      llvmCode += llvmMemCodeValue2;
      llvmCode += createARITHMETIC(instruction::_SUB, llvmValue1, LLVM_ZERO_INT, llvmValue2, LLVM_INT);
      llvmCode += llvmMemCodeValue1;
      break;
    }
  case instruction::_FADD:
  case instruction::_FSUB:
  case instruction::_FMUL:
  case instruction::_FDIV:
    {
      modifyValueOfArgument(tcodeArg1, llvmValue1, llvmMemCodeValue1);
      accessValueOfArgument(tcodeArg2, llvmValue2, llvmMemCodeValue2);
      accessValueOfArgument(tcodeArg3, llvmValue3, llvmMemCodeValue3);
      llvmCode += llvmMemCodeValue2;
      llvmCode += llvmMemCodeValue3;
      llvmCode += createARITHMETIC(instr.oper, llvmValue1, llvmValue2, llvmValue3, LLVM_FLOAT);
      llvmCode += llvmMemCodeValue1;
      break;
    }
  case instruction::_FNEG:
    {
      modifyValueOfArgument(tcodeArg1, llvmValue1, llvmMemCodeValue1);
      if (isTCodeTemporal(tcodeArg1))
        bindLLVMLocalValueWithType(llvmValue1, LLVM_FLOAT);
      accessValueOfArgument(tcodeArg2, llvmValue2, llvmMemCodeValue2);
      llvmCode += llvmMemCodeValue2;
      llvmCode += createFNEG(llvmValue1, llvmValue2);
      llvmCode += llvmMemCodeValue1;
      break;
    }
  case instruction::_FLOAT:
    {
      modifyValueOfArgument(tcodeArg1, llvmValue1, llvmMemCodeValue1);
      accessValueOfArgument(tcodeArg2, llvmValue2, llvmMemCodeValue2);
      llvmCode += llvmMemCodeValue2;
      llvmCode += createSITOFP(llvmValue1, llvmValue2, LLVM_INT);
      llvmCode += llvmMemCodeValue1;
      break;
    }
  case instruction::_AND:
  case instruction::_OR:
    {
      modifyValueOfArgument(tcodeArg1, llvmValue1, llvmMemCodeValue1);
      accessValueOfArgument(tcodeArg2, llvmValue2, llvmMemCodeValue2);
      accessValueOfArgument(tcodeArg3, llvmValue3, llvmMemCodeValue3);
      llvmCode += llvmMemCodeValue2;
      llvmCode += llvmMemCodeValue3;
      llvmCode += createLOGICAL(instr.oper, llvmValue1, llvmValue2, llvmValue3);
      llvmCode += llvmMemCodeValue1;
      break;
    }
  case instruction::_NOT:
    {
      modifyValueOfArgument(tcodeArg1, llvmValue1, llvmMemCodeValue1);
      accessValueOfArgument(tcodeArg2, llvmValue2, llvmMemCodeValue2);
      llvmCode += llvmMemCodeValue2;
      llvmCode += createNOT(llvmValue1, llvmValue2);
      llvmCode += llvmMemCodeValue1;
      break;
    }
  case instruction::_NOOP:
    {
      llvmCode += ";   noop\n";
      break;
    }
  default:
    {
      llvmCode += ";   UNKNOWN\n";
      break;
    }
  }

  prevInstrIsTerminator = (instr.oper == instruction::_UJUMP or
                           instr.oper == instruction::_FJUMP or
                           instr.oper == instruction::_RETURN);
  
  return llvmCode;
}


std::string LLVMCodeGen::getTCodeArg(const instruction & instr, int i) const {
  std::string arg;
  if (i == 1)
    arg = instr.arg1;
  else if (i == 2)
    arg = instr.arg2;
  else     // i == 3
    arg = instr.arg3;
  return arg;
}

std::string LLVMCodeGen::getLLVMValue(const std::string & tcodeIdent) const {
  if (tcodeIdent.size() == 0) return "";
  if (tcodeIdent[0] == '%') return "%.temp." + tcodeIdent.substr(1);
  if (std::isdigit(tcodeIdent[0])) return tcodeIdent;
  return "%"+tcodeIdent;
}

std::string LLVMCodeGen::getLLVMValueAddr(const std::string & llvmValue) const {
  return llvmValue + ".addr";
}

std::string LLVMCodeGen::createALLOCA(const std::string & llvmValueAddr, const std::string & llvmType) const {
  std::string llvmCode;
  llvmCode += INDENT_INSTR + llvmValueAddr + " = alloca " + llvmType + "\n";
  return llvmCode;
}

std::string LLVMCodeGen::createSTORE(const std::string & llvmValue1,
                                     const std::string & llvmValue2Addr) const {
  std::string llvmCode;
  std::string llvmType2Ptr = getLLVMTypeOfValue(llvmValue2Addr);
  std::string llvmType2    = getPointedType(llvmType2Ptr);
  llvmCode += INDENT_INSTR + "store " + llvmType2 + " " + llvmValue1 + ", " + llvmType2Ptr + " " + llvmValue2Addr + "\n";
  return llvmCode;
}

std::string LLVMCodeGen::createLABEL(const std::string & label) const {
  std::string llvmCode;
  llvmCode += INDENT_LABEL + label + ":\n";
  return llvmCode;
}

std::string LLVMCodeGen::createCONVERSION(const std::string & llvmInstr, const std::string & llvmValue1,
                                          const std::string & llvmValue2, const std::string & llvmType2) const {
  std::string llvmCode;
  std::string llvmType1 = getLLVMTypeOfValue(llvmValue1);
  llvmCode += INDENT_INSTR + llvmValue1 + " = " + llvmInstr + " " + llvmType2 + " " + llvmValue2 + " to " + llvmType1 + "\n";
  return llvmCode;
}

std::string LLVMCodeGen::createLOAD(const std::string & llvmValue1, const std::string & llvmValue2Addr) const {
  std::string llvmCode;
  std::string llvmTypePtr = getLLVMTypeOfValue(llvmValue2Addr);
  std::string llvmType    = getPointedType(llvmTypePtr);
  llvmCode += INDENT_INSTR + llvmValue1 + " = load " + llvmType + ", " + llvmTypePtr + " " + llvmValue2Addr + "\n";
  return llvmCode;
}

std::string LLVMCodeGen::createARITHMETIC(instruction::Operation oper, const std::string & llvmValue1,
                                          const std::string & llvmValue2, const std::string & llvmValue3,
                                          const std::string & llvmType23) const {
  std::string llvmCode;
  std::string llvmInstr = tcode2llvmInstrMap.at(oper);
  llvmCode = INDENT_INSTR + llvmValue1 + " = " + llvmInstr + " " + llvmType23 + " " + llvmValue2 + ", " + llvmValue3 + "\n";
  return llvmCode;
}

std::string LLVMCodeGen::createCOMPARISON(instruction::Operation oper, const std::string & llvmValue1,
                                          const std::string & llvmValue2, const std::string & llvmValue3,
                                          const std::string & llvmType23) const {
  std::string llvmCode;
  std::string llvmInstr = tcode2llvmInstrMap.at(oper);
  llvmCode = INDENT_INSTR + llvmValue1 + " = " + llvmInstr + " " + llvmType23 + " " + llvmValue2 + ", " + llvmValue3 + "\n";
  return llvmCode;
}

std::string LLVMCodeGen::createLOGICAL(instruction::Operation oper, const std::string & llvmValue1,
                                       const std::string & llvmValue2, const std::string & llvmValue3) const {
  std::string llvmCode;
  std::string llvmInstr = tcode2llvmInstrMap.at(oper);
  llvmCode = INDENT_INSTR + llvmValue1 + " = " + llvmInstr + " " + LLVM_BOOL + " " + llvmValue2 + ", " + llvmValue3 + "\n";
  return llvmCode;
}

std::string LLVMCodeGen::createNOT(const std::string & llvmValue1, const std::string & llvmValue2) const {
  std::string llvmCode;
  llvmCode = INDENT_INSTR + llvmValue1 + " = xor " + LLVM_BOOL + " " + llvmValue2 + ", " + LLVM_ONE_INT + "\n";
  return llvmCode;
}

std::string LLVMCodeGen::createFNEG(const std::string & llvmValue1, const std::string & llvmValue2) const {
  std::string llvmCode;
  // <result> = fneg [fast-math flags]* <ty> <op1>    ; yields ty:result
  llvmCode += INDENT_INSTR + llvmValue1 + " = fneg " + LLVM_FLOAT + " " + llvmValue2 + "\n";
  return llvmCode;
}

std::string LLVMCodeGen::createSITOFP(const std::string & llvmValue1, const std::string & llvmValue2,
                                      const std::string & llvmType2) const {
  std::string llvmCode;
  // <result> = sitofp <ty> <value> to <ty2>    ; yields ty2
  std::string llvmType1 = getLLVMTypeOfValue(llvmValue1);
  llvmCode += INDENT_INSTR + llvmValue1 + " = sitofp " + llvmType2 + " " + llvmValue2 + " to " + llvmType1 + "\n";
  return llvmCode;
}


std::string LLVMCodeGen::createPRINTF(const std::string & llvmValue, const std::string & llvmType) const {
  std::string llvmCode;
  std::string format;
  if (llvmType == LLVM_INT)
    format = "@.str.i";
  else if (llvmType == LLVM_DOUBLE)
    format = "@.str.f";
  llvmCode += INDENT_INSTR + "call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([3 x i8], [3 x i8]* " + format + ", i64 0, i64 0), " + llvmType + " " + llvmValue + ")\n";
  return llvmCode;
}

std::string LLVMCodeGen::createPRINTS(const std::string & strFormat, const int strSize) const {
  std::string llvmCode;
  llvmCode += INDENT_INSTR + "call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([" + std::to_string(strSize) + " x i8], [" + std::to_string(strSize) + " x i8]* " + strFormat + ", i64 0, i64 0))\n";
  return llvmCode;
}

std::string LLVMCodeGen::createPUTCHAR(const std::string & llvmValue) const {
  std::string llvmCode;
  llvmCode += INDENT_INSTR + "call i32 @putchar(i32 " + llvmValue+ ")\n";
  return llvmCode;
}

std::string LLVMCodeGen::createSCANF(const std::string & llvmValueAddr) const {
  std::string llvmCode;
  std::string format;
  std::string llvmTypePtr = getLLVMTypeOfValue(llvmValueAddr);
  std::string llvmType = getPointedType(llvmTypePtr);
  if (llvmType == LLVM_INT)
    format = "@.str.i";
  else if (llvmType == LLVM_FLOAT)
    format = "@.str.f";
  else  // LLVM_CHAR
    format = "@.str.c";
  llvmCode += INDENT_INSTR + "call i32 (i8*, ...) @__isoc99_scanf(i8* getelementptr inbounds ([3 x i8], [3 x i8]* " + format + ", i64 0, i64 0), " + llvmTypePtr + " " + llvmValueAddr + ")\n";
  return llvmCode;
}

std::string LLVMCodeGen::createHALT() const {
  std::string llvmCode;
  llvmCode += INDENT_INSTR + "call void @exit(i32 1)" + "\n";
  return llvmCode;
}

std::string LLVMCodeGen::createBR(const std::string & llvmValue) const {
  std::string llvmCode;
  llvmCode += INDENT_INSTR + "br label " + llvmValue + "\n";
  return llvmCode;
}

std::string LLVMCodeGen::createBR(const std::string & llvmValue,
                                  const std::string & labelCont, const std::string & labelJump) const {
  std::string llvmCode;
  llvmCode += INDENT_INSTR + "br i1 " + llvmValue + ", label " + labelCont + ", label " + labelJump + "\n";
  return llvmCode;
}

std::string LLVMCodeGen::createRET(const std::string & llvmValue, const std::string & llvmType) const {
  std::string llvmCode;
  llvmCode = INDENT_INSTR + "ret " + llvmType + " " + llvmValue + "\n";
  return llvmCode;
}

std::string LLVMCodeGen::createRET(const std::string & llvmValue) const {
  std::string llvmCode;
  std::string llvmType = getLLVMTypeOfValue(llvmValue);
  llvmCode += INDENT_INSTR + "ret " + llvmType + " " + llvmValue + "\n";
  return llvmCode;
}

std::string LLVMCodeGen::createRET() const {
  std::string llvmCode;
  llvmCode += INDENT_INSTR + "ret void" + "\n";
  return llvmCode;
}

std::string LLVMCodeGen::createCALL(const std::string & tcodeFunc, const std::string & llvmValue1,
                                    const std::vector<std::string> & llvmArgs) const {
  std::string llvmCode;
  std::string llvmRetType = getFuncReturnLLVMType(tcodeFunc);
  int n = llvmArgs.size();
  std::string llvmCodeArgs;
  for (int i = n-1; i >= 0; --i) {
    std::string param = llvmArgs[i];
    std::string paramType = getLLVMTypeOfValue(param);
    if (i == n-1)
      llvmCodeArgs += paramType + " " + param;
    else
      llvmCodeArgs += ", " + paramType + " " + param;
  }
  llvmCode += INDENT_INSTR + llvmValue1 + " = call " + llvmRetType + " @" + tcodeFunc + "(" + llvmCodeArgs + ")\n";
  return llvmCode;
}

std::string LLVMCodeGen::createCALL(const std::string & tcodeFunc,
                                    const std::vector<std::string> & llvmArgs) const {
  std::string llvmCode;
  std::string llvmRetType = getFuncReturnLLVMType(tcodeFunc);
  int n = llvmArgs.size();
  std::string llvmCodeArgs;
  for (int i = n-1; i >= 0; --i) {
    std::string param = llvmArgs[i];
    std::string paramType = getLLVMTypeOfValue(param);
    if (i == n-1)
      llvmCodeArgs += paramType + " " + param;
    else
      llvmCodeArgs += ", " + paramType + " " + param;
  }
  llvmCode += INDENT_INSTR + "call " + llvmRetType + " @" + tcodeFunc + "(" + llvmCodeArgs + ")\n";
  return llvmCode;
}

std::string LLVMCodeGen::createGETELEMENTPTR(const std::string & llvmArrayPointerValue,
                                             const std::string & llvmArrayBaseValue,
                                             const std::string & llvmArrayIndexValue) const {
  std::string llvmCode;
  std::string llvmArrayPtrType = getLLVMTypeOfValue(llvmArrayBaseValue);
  std::string llvmPointedType = getPointedType(llvmArrayPtrType);
  if (isLLVMArrayType(llvmPointedType)) {
    // %arrayidx = getelementptr inbounds [10 x i32], [10 x i32]* %A, i64 0, i64 %idxprom
    llvmCode += INDENT_INSTR + llvmArrayPointerValue + " = getelementptr inbounds " + llvmPointedType + ", " + llvmArrayPtrType + " " + llvmArrayBaseValue + ", i64 0, i64 " + llvmArrayIndexValue + "\n";
  }
  else {
    // %arrayidx = getelementptr inbounds i32, i32* %1, i64 %idxprom
    llvmCode += INDENT_INSTR + llvmArrayPointerValue + " = getelementptr inbounds " + llvmPointedType + ", " + llvmArrayPtrType + " " + llvmArrayBaseValue + ", i64 " + llvmArrayIndexValue + "\n";
  }
  return llvmCode;
}


void LLVMCodeGen::accessValueOfArgument(const std::string & tcodeArgIn,
                                        std::string & llvmValueOut, std::string & llvmAccInstr) {
  // Pre:  if tcodeArgIn is a tcode identifiier then:
  //          * the llvmValueIn corresponding to tcodeArgIn
  //            has been previously typed (using llvmValueTypeMap's)
  //       if tcodeArgIn is a tcode temporal or a constant then:
  //          * -
  // Post: if tcodeArgIn is a tcode identifiier then:
  //          * the new created llvmValueOut uses llvmValueIn as a prefix
  //          * the new created llvmValueOut has been binded to the same type of llvmValueIn
  //          * a new llvmAccInstr is created: a LOAD from the value of llvmValueIn
  //            (in llvmValueInAddr) to the new created value llvmValueOut
  //       if tcodeArgIn is a tcode temporal or a constant then:
  //          * llmValueOut is the llvm value corresponding to tcodeArgIn
  //          * no additional instruction is needed (llvmAccInstr is equal to "")
  if (isTCodeIdentifier(tcodeArgIn)) {
    std::string llvmValueIn     = getLLVMValue(tcodeArgIn);
    std::string llvmType        = getLLVMTypeOfValue(llvmValueIn);
    std::string llvmValueInAddr = getLLVMValueAddr(llvmValueIn);
    llvmValueOut = createNewPrefixedValueWithType(llvmValueIn, llvmType);
    llvmAccInstr = createLOAD(llvmValueOut, llvmValueInAddr);
  }
  else {
    llvmValueOut = getLLVMValue(tcodeArgIn);  // = tcodeArgIn;
    llvmAccInstr = "";
  }
}

void LLVMCodeGen::modifyValueOfArgument(const std::string & tcodeArgIn,
                                        std::string & llvmValueOut, std::string & llvmModInstr) {
  // Pre:  if tcodeArgIn is a tcode identifiier then:
  //          * the llvmValueIn corresponding to tcodeArgIn
  //            has been previously typed (using llvmValueTypeMap's)
  //       if tcodeArgIn is a tcode temporal or a constant then:
  //          * -
  // Post: if tcodeArgIn is a tcode identifiier then:
  //          * the new created llvmValueOut uses llvmValueIn as a prefix
  //          * the new created llvmValueOut is binded to the same type of llvmValueIn
  //          * a new llvmModInstr is created: a STORE of the new created value llvmValueOut
  //            into the memory address of llvmValueIn (llvmValueInAddr)
  //       if tcodeArgIn is a tcode temporal or a constant then:
  //          * llmValueOut is the llvm value corresponding to tcodeArgIn
  //          * no additional instruction is needed (llvmModInstr is equal to "")
  if (isTCodeIdentifier(tcodeArgIn)) {
    std::string llvmValueIn     = getLLVMValue(tcodeArgIn);
    std::string llvmType        = getLLVMTypeOfValue(llvmValueIn);
    std::string llvmValueInAddr = getLLVMValueAddr(llvmValueIn);
    llvmValueOut = createNewPrefixedValueWithType(llvmValueIn, llvmType);
    llvmModInstr = createSTORE(llvmValueOut, llvmValueInAddr);
  }
  else {
    llvmValueOut = getLLVMValue(tcodeArgIn);  // = tcodeArgIn;
    llvmModInstr = "";
  }
}

std::string LLVMCodeGen::createNewPrefixedValueWithType(const std::string & llvmValuePrefix,
                                                        const std::string & llvmType) {
  // This method creates a new llvm value using the llvmLocalValueCountMap to generate  different
  // llvm identifiers.
  // Pre:  * llvmValuePrefix can be a llvm value of a tcode variable or parameter (for example, "%a"),
  //         or a especial prefix used in the tcode to llvm translation (for example, "%.cont").
  // Post: * a completely new llvmValue is generated formed by the prefix, followed by a character '.',
  //         followed by the (integer) value of the counter in the llvmLocalValueCountMap.
  //         The value of llvmLocalValueCountMap is incremented for future uses.
  //       * The new llvm value generated is binded to th type llvmType, using the method
  //         bindLLVMLocalValueWithType
  llvmLocalValueCountMap[llvmValuePrefix] += 1;
  std::string llvmNewValue = llvmValuePrefix + "." + std::to_string(llvmLocalValueCountMap[llvmValuePrefix]);
  bindLLVMLocalValueWithType(llvmNewValue, llvmType);
  return llvmNewValue;
}

void LLVMCodeGen::bindGlobalValuesWithTypes() {
  if (globalI) {
    llvmGlobalValueVec.push_back(LLVM_GLOBAL_INT_ADDR);
    llvmGlobalValueTypeMap[LLVM_GLOBAL_INT_ADDR] = LLVM_INT_PTR;
  }
  if (globalF) {
    llvmGlobalValueVec.push_back(LLVM_GLOBAL_FLOAT_ADDR);
    llvmGlobalValueTypeMap[LLVM_GLOBAL_FLOAT_ADDR] = LLVM_FLOAT_PTR;
  }
  if (globalC) {
    llvmGlobalValueVec.push_back(LLVM_GLOBAL_CHAR_ADDR);
    llvmGlobalValueTypeMap[LLVM_GLOBAL_CHAR_ADDR] = LLVM_CHAR_PTR;
  }
}

void LLVMCodeGen::bindTCodeLocalValueWithType(const std::string & tcodeArg,
                                              const std::string & llvmType) {
  if (isTCodeIdentifier(tcodeArg) or isTCodeTemporal(tcodeArg)) {
    std::string llvmValue = getLLVMValue(tcodeArg);
    if (llvmLocalValueTypeMap.find(llvmValue) == llvmLocalValueTypeMap.end()) {
      llvmLocalValueVec.push_back(llvmValue);
      llvmLocalValueTypeMap[llvmValue]  = llvmType;
      llvmLocalValueCountMap[llvmValue] = 0;
    }
    else {
      std::string llvmCurrentType = llvmLocalValueTypeMap.at(llvmValue);
      if (llvmCurrentType != LLVM_TYERR and llvmType != LLVM_TYMISS) {
        if (llvmCurrentType == LLVM_INT_BOOL) {
          if (llvmType == LLVM_INT or llvmType == LLVM_BOOL or llvmType == LLVM_INT_BOOL)
            llvmLocalValueTypeMap[llvmValue] = llvmType;
          else
            llvmLocalValueTypeMap[llvmValue] = LLVM_TYERR;
        }
        else if (llvmType == LLVM_INT_BOOL) {
          if (llvmCurrentType == LLVM_TYMISS)
            llvmLocalValueTypeMap[llvmValue] = llvmType;
          else if (llvmCurrentType != LLVM_INT and llvmCurrentType != LLVM_BOOL and
                   llvmCurrentType != LLVM_INT_BOOL)
            llvmLocalValueTypeMap[llvmValue] = LLVM_TYERR;
        }
        else if (llvmCurrentType != LLVM_TYMISS and llvmCurrentType != llvmType)
          llvmLocalValueTypeMap[llvmValue] = LLVM_TYERR;
      }
    }
  }
}

void LLVMCodeGen::bindPairOfTCodeLocalValuesWithTypes(const std::string & tcodeArg1,
                                                      const std::string & tcodeArg2) {
  std::string llvmValue1 = getLLVMValue(tcodeArg1);
  std::string llvmValue2 = getLLVMValue(tcodeArg2);
  auto search1 = llvmLocalValueTypeMap.find(tcodeArg1);
  auto search2 = llvmLocalValueTypeMap.find(tcodeArg2);
  if (search1 == llvmLocalValueTypeMap.end() and search2 == llvmLocalValueTypeMap.end()) {
    llvmLocalValueTypeMap[tcodeArg1] = LLVM_TYMISS;
    llvmLocalValueTypeMap[tcodeArg2] = LLVM_TYMISS;
  }
  else if (search2 == llvmLocalValueTypeMap.end()) {
    std::string llvmType1 = llvmLocalValueTypeMap.at(llvmValue1);
    if (llvmType1 == LLVM_TYERR)
      llvmLocalValueTypeMap[tcodeArg2] = LLVM_TYMISS;
    else
      llvmLocalValueTypeMap[tcodeArg2] = llvmType1;
  }
  else if (search1 == llvmLocalValueTypeMap.end()) {
    std::string llvmType2 = llvmLocalValueTypeMap.at(llvmValue2);
    if (llvmType2 == LLVM_TYERR)
      llvmLocalValueTypeMap[tcodeArg1] = LLVM_TYMISS;
    else
      llvmLocalValueTypeMap[tcodeArg1] = llvmType2;
  }
  else {
    std::string llvmType1 = llvmLocalValueTypeMap.at(llvmValue1);
    std::string llvmType2 = llvmLocalValueTypeMap.at(llvmValue2);
    if (llvmType1 != LLVM_TYERR and llvmType2 != LLVM_TYERR) {
      if (llvmType1 != LLVM_TYMISS and llvmType2 == LLVM_TYMISS)
        llvmLocalValueTypeMap[tcodeArg2] = llvmType1;
      else if (llvmType1 == LLVM_TYMISS and llvmType2 != LLVM_TYMISS)
        llvmLocalValueTypeMap[tcodeArg1] = llvmType2;
      else if ((llvmType1 == LLVM_INT or llvmType1 == LLVM_BOOL) and
               llvmType2 == LLVM_INT_BOOL)
        llvmLocalValueTypeMap[tcodeArg2] = llvmType1;
      else if (llvmType1 == LLVM_INT_BOOL and
               (llvmType2 == LLVM_INT or llvmType2 == LLVM_BOOL))
        llvmLocalValueTypeMap[tcodeArg1] = llvmType2;
      else if (llvmType1 != LLVM_TYMISS and llvmType2 != LLVM_TYMISS and
               llvmType1 != llvmType2) {
        llvmLocalValueTypeMap[tcodeArg1] = LLVM_TYERR;
        llvmLocalValueTypeMap[tcodeArg2] = LLVM_TYERR;
      }
    }
  }
}

void LLVMCodeGen::bindLLVMLocalValueWithType(const std::string & llvmValue,
                                             const std::string & llvmType) {
  llvmLocalValueVec.push_back(llvmValue);
  llvmLocalValueTypeMap[llvmValue]  = llvmType;
  llvmLocalValueCountMap[llvmValue] = 0;
}

// void LLVMCodeGen::bindTempWithType(const std::string & tcodeArg,
//                                    const std::string & llvmType) {
//   if (isTCodeTemporal(tcodeArg)) {
//     std::string llvmValue = getLLVMValue(tcodeArg);
//     llvmValueVec.push_back(llvmValue);
//     llvmValueTypeMap[llvmValue]  = llvmType;
//     llvmLocalValueCountMap[llvmValue] = 0;
//   }
// }

std::string LLVMCodeGen::getLLVMTypeOfValue(const std::string & llvmValue) const {
  if (llvmValue[0] == '%')  // local var
    return llvmLocalValueTypeMap.at(llvmValue);
  else  // llvmValue[0] == '@'  // global var
    return llvmGlobalValueTypeMap.at(llvmValue);
}


bool LLVMCodeGen::isLLVMAnyIntegerType(const std::string & llvmType) const {
  return (llvmType == LLVM_INT or llvmType == LLVM_INT8 or llvmType == LLVM_INT1);
}

std::string LLVMCodeGen::getLLVMTypeOneIntUp(const std::string & llvmIntType) const {
  if (llvmIntType == LLVM_INT) // LLVM_INT == LLVM_INT32
    return LLVM_INT64;
  else if (llvmIntType == LLVM_INT8)
    return LLVM_INT32;
  else if (llvmIntType == LLVM_INT1)
    return LLVM_INT8;
  else
    return LLVM_TYERR;
}

bool LLVMCodeGen::isLLVMArrayType(const std::string & llvmType) const {
  std::string::size_type xpos = llvmType.find(" x ");
  return xpos != std::string::npos;
}

std::string LLVMCodeGen::getLLVMElementOfArrayType(const std::string & llvmArrayType) const {
  std::string::size_type xpos = llvmArrayType.find(" x ");
  assert(xpos != std::string::npos);
  std::string::size_type len = llvmArrayType.size();
  std::string elemType = llvmArrayType.substr(xpos+3, len-xpos-4);
  return elemType;
}

std::string LLVMCodeGen::getLLVMArrayTypeAsPointerType(const std::string & llvmArrayType) const {
  std::string elemType = getLLVMElementOfArrayType(llvmArrayType);
  return getPointerToType(elemType);
}

bool LLVMCodeGen::isPointerType(const std::string & llvmType) const {
  std::size_t n = llvmType.size();
  return llvmType[n-1] == '*';
}

std::string LLVMCodeGen::getPointerToType(const std::string & llvmType) const {
  return llvmType + "*";
}

std::string LLVMCodeGen::getPointedType(const std::string & llvmTypePtr) const {
  std::size_t n = llvmTypePtr.size();
  return llvmTypePtr.substr(0,n-1);
}


void LLVMCodeGen::pushTCodeParamCallStack(const std::string & tcodeParam) {
  paramCallsStack.push(tcodeParam);
}

std::string LLVMCodeGen::topPopTCodeParamCallStack() {
  assert(paramCallsStack.size() > 0);
  std::string tcodeParam = paramCallsStack.top();
  paramCallsStack.pop();
  return tcodeParam;
}

void LLVMCodeGen::pushLLVMParamCallStack(const std::string & llvmParam) {
  paramCallsStack.push(llvmParam);
}

std::string LLVMCodeGen::topPopLLVMParamCallStack() {
  assert(paramCallsStack.size() > 0);
  std::string llvmParam = paramCallsStack.top();
  paramCallsStack.pop();
  return llvmParam;
}

bool LLVMCodeGen::isEmptyLLVMParamCallStack() const {
  return paramCallsStack.empty();
}


int LLVMCodeGen::getAsciiCode(const std::string & s) const {
  if (s.size() == 1)
    return int(s[0]);
  else if (s == "\\n")
    return int('\n');
  else if (s == "\\t")
    return int('\t');
  else if (s == "\\\\")
    return int('\\');
  else if (s == "\\\"")
    return int('\"');
  else if (s == "\\\'")
    return int('\'');
  else
    return int(s[1]);
}


std::string LLVMCodeGen::llvmComment(const std::string & comm) const {
  if (COMMENTS_ENABLED) return ";   " + comm + "\n";
  return "";
}
