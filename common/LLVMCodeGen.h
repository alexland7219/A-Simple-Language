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

#pragma once

#include "TypesMgr.h"
#include "SymTable.h"
#include "code.h"

#include <string>
#include <vector>
#include <map>
#include <stack>

// using namespace std;

class code;
class subroutine;
class instruction;

class LLVMCodeGen {
 private:
  const TypesMgr & Types;
  const SymTable & Symbols;
  const code     & tCode;
  
  static const bool COMMENTS_ENABLED;
  static const std::string INDENT_INSTR;
  static const std::string INDENT_LABEL;
  static const std::string LLVM_INT;
  static const std::string LLVM_FLOAT;
  static const std::string LLVM_CHAR;
  static const std::string LLVM_BOOL;
  static const std::string LLVM_VOID;
  static const std::string LLVM_LABEL;
  static const std::string LLVM_TYERR;
  static const std::string LLVM_TYMISS;
  static const std::string LLVM_INT_BOOL;
  static const std::string LLVM_INT_PTR;
  static const std::string LLVM_FLOAT_PTR;
  static const std::string LLVM_CHAR_PTR;
  static const std::string LLVM_BOOL_PTR;
  static const std::string LLVM_INT1;
  static const std::string LLVM_INT8;
  static const std::string LLVM_INT32;
  static const std::string LLVM_INT64;
  static const std::string LLVM_DOUBLE;
  static const std::string LLVM_GLOBAL_INT_ADDR;
  static const std::string LLVM_GLOBAL_FLOAT_ADDR;
  static const std::string LLVM_GLOBAL_CHAR_ADDR;
  static const std::string LLVM_ZERO_INT;
  static const std::string LLVM_ZERO_FLOAT;
  static const std::string LLVM_ONE_INT;
  static const std::string LLVM_ENTRY;
  static const std::string LLVM_ZEXT;
  static const std::string LLVM_FPEXT;
  static const std::string LLVM_TRUNC;
  static const std::string LLVM_FPTRUNC;
  static const std::string LLVM_SEXT;
  static const std::map<instruction::Operation, std::string> tcode2llvmInstrMap;

  bool writeI, writeF, writeC, writeS, writeLN;
  bool readI, readF, readC;
  bool haltAndExit;
  bool globalI, globalF, globalC, globalS;
  std::vector<std::string>            writeSAslStrVec;
  std::vector<std::string::size_type> writeSLLVMStrSizeVec;
  std::string currentFunctionName;
  bool isMain;
  bool prevInstrIsTerminator;
  std::vector<std::string>           llvmLocalValueVec;
  std::map<std::string, std::string> llvmLocalValueTypeMap;
  std::vector<std::string>           llvmGlobalValueVec;
  std::map<std::string, std::string> llvmGlobalValueTypeMap;
  std::map<std::string, int>         llvmLocalValueCountMap;
  std::stack<std::string>            paramCallsStack;
  std::string                        pendingCallLLVMRetType;
  std::string                        pendingCallFunc;
  std::vector<std::string>           pendingCallArgs;

  void check_SSA_tCode(std::string & failFunc, std::string & failTempVar) const;
  bool isTCodeTemporal   (const std::string & tcodeArg) const;
  bool isTCodeIdentifier (const std::string & tcodeArg) const;

  void computeReadWriteHaltInfo();
  std::string              getFuncReturnLLVMType  (const std::string & tcodeFuncIdent)        const;
  int                      getFuncNumberOfParams  (const std::string & tcodeFuncIdent)        const;
  std::string              getFuncParamLLVMType   (const std::string & tcodeFuncIdent, int n) const;
  std::vector<std::string> getFuncParamsLLVMTypes (const std::string & tcodeFuncIdent)        const;

  std::string              getLocalSymbolLLVMType (const std::string & tcodeFuncIdent,
                                                   const std::string & tcodeSymbolIdent,
                                                   bool isParameter = false) const;
  std::string TypeIdToLLVMType(TypesMgr::TypeId tid, bool isParameter = false) const;

  void getLLVMStringFromAslString(const std::string & aslString,
				  std::string & llvmString,
				  std::string::size_type & llvmStringSize);
  void generateReadWriteHaltBeginEndCode(std::string & begin, std::string & end) ;
  void startNewFunction(const subroutine & subr);
  void bindTCodeLocalSymbolsToLLVMTypes(const subroutine & subr);
  std::string dumpSubroutine(const subroutine & subr);
  std::string dumpHeader(const subroutine & subr);
  std::string dumpAllocaParams(const subroutine & subr);
  std::string dumpAllocaLocalVars(const subroutine & subr);
  std::string dumpStoreParams(const subroutine & subr);
  std::string dumpInstructionList(const subroutine & subr);
  std::string dumpInstruction(const instruction & instr,
                              const instruction & next);
  std::string getTCodeArg(const instruction & intr, int i) const;
  std::string getLLVMValue(const std::string & tcodeIdent) const;
  std::string getLLVMValueAddr(const std::string & llvmValue) const;

  std::string createALLOCA(const std::string & llvmValueAddr, const std::string & llvmType) const;
  std::string createSTORE(const std::string & llvmValue1, const std::string & llvmValue2) const;
  std::string createLABEL(const std::string & label) const;
  std::string createCONVERSION(const std::string & llvmInstr, const std::string & llvmValue1,
                               const std::string & llvmValue2, const std::string & llvmType2) const;
  std::string createLOAD(const std::string & llvmValue1, const std::string & llvmValue2) const;
  std::string createARITHMETIC(instruction::Operation oper, const std::string & llvmValue1,
                               const std::string & llvmValue2, const std::string & llvmValue3,
                               const std::string & llvmType23) const;
  std::string createCOMPARISON(instruction::Operation oper, const std::string & llvmValue1,
                               const std::string & llvmValue2, const std::string & llvmValue3,
                               const std::string & llvmType23) const;
  std::string createLOGICAL(instruction::Operation oper, const std::string & llvmValue1,
                            const std::string & llvmValue2, const std::string & llvmValue3) const;
  std::string createNOT(const std::string & llvmValue1, const std::string & llvmValue2) const;
  std::string createFNEG(const std::string & llvmValue1, const std::string & llvmValue2) const;
  std::string createSITOFP(const std::string & llvmValue1,
                           const std::string & llvmValue2, const std::string & llvmType2) const;
  std::string createPRINTF(const std::string & llvmValue, const std::string & llvmType) const;
  std::string createPRINTS(const std::string & str, const int sz) const;
  std::string createPUTCHAR(const std::string & llvmValue) const;
  std::string createSCANF(const std::string & llvmValueAddr) const;
  std::string createHALT() const;
  std::string createBR(const std::string & llvmValue) const;
  std::string createBR(const std::string & llvmValue,
                       const std::string & labelCont, const std::string & labelJump) const;
  std::string createRET(const std::string & llvmValue, const std::string & llvmType) const;
  std::string createRET(const std::string & llvmValue) const;
  std::string createRET() const;
  std::string createCALL(const std::string & tcodeFunc, const std::string & llvmValue1,
                         const std::vector<std::string> & llvmArgs) const;
  std::string createCALL(const std::string & tcodeFunc,
                         const std::vector<std::string> & llvmArgs) const;
  std::string createGETELEMENTPTR(const std::string & llvmArrayPointerValue,
                                  const std::string & llvmArrayBaseValue,
                                  const std::string & llvmArrayIndexValue) const;

  void accessValueOfArgument(const std::string & tcodeArgIn,
                             std::string & llvmArgOut, std::string & llvmAccInstr);
  void modifyValueOfArgument(const std::string & tCodeArgIn,
                             std::string & llvmValueOut, std::string & llvmModInstr);

  std::string createNewPrefixedValueWithType(const std::string & llvmValuePrefix,
                                             const std::string & llvmType);
  void bindGlobalValuesWithTypes();
  void bindTCodeLocalValueWithType(const std::string & tcodeArg, const std::string & llvmType);
  void bindPairOfTCodeLocalValuesWithTypes(const std::string & tcodeArg1,
                                           const std::string & tcodeArg2);
  void bindLLVMLocalValueWithType(const std::string & llvmValue, const std::string & llvmType);
  std::string getLLVMTypeOfValue(const std::string & llvmValue) const;

  bool isLLVMAnyIntegerType(const std::string & llvmType) const;
  std::string getLLVMTypeOneIntUp(const std::string & llvmIntType) const;
  bool isLLVMArrayType(const std::string & llvmType) const;
  std::string getLLVMElementOfArrayType(const std::string & llvmArrayType) const;
  std::string getLLVMArrayTypeAsPointerType(const std::string & llvmArrayType) const;
  bool isPointerType(const std::string & llvmType) const;
  std::string getPointerToType(const std::string & llvmType) const;
  std::string getPointedType(const std::string & llvmTypePtr) const;
  
  void        pushTCodeParamCallStack(const std::string & tcodeParam);
  std::string topPopTCodeParamCallStack();
  void        pushLLVMParamCallStack(const std::string & llvmParam);
  std::string topPopLLVMParamCallStack();
  bool        isEmptyLLVMParamCallStack() const;

  int getAsciiCode(const std::string & s) const;

  std::string llvmComment(const std::string & comm) const;

public:
  LLVMCodeGen(const TypesMgr & Types, const SymTable & Symbols, const code & tCode);
  std::string dumpLLVM();
};
