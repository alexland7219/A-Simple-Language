//////////////////////////////////////////////////////////////////////
//
//    TypeCheckVisitor - Walk the parser tree to do the semantic
//                       typecheck for the Asl programming language
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
//////////////////////////////////////////////////////////////////////

#include "TypeCheckVisitor.h"
#include "antlr4-runtime.h"

#include "../common/TypesMgr.h"
#include "../common/SymTable.h"
#include "../common/TreeDecoration.h"
#include "../common/SemErrors.h"

#include <iostream>
#include <string>

// uncomment the following line to enable debugging messages with DEBUG*
//#define DEBUG_BUILD
#include "../common/debug.h"

// using namespace std;


// Constructor
TypeCheckVisitor::TypeCheckVisitor(TypesMgr       & Types,
                                   SymTable       & Symbols,
                                   TreeDecoration & Decorations,
                                   SemErrors      & Errors) :
  Types{Types},
  Symbols{Symbols},
  Decorations{Decorations},
  Errors{Errors} {
}

// Accessor/Mutator to the attribute currFunctionType
TypesMgr::TypeId TypeCheckVisitor::getCurrentFunctionTy() const {
  return currFunctionType;
}

void TypeCheckVisitor::setCurrentFunctionTy(TypesMgr::TypeId type) {
  currFunctionType = type;
}

// Methods to visit each kind of node:
//
antlrcpp::Any TypeCheckVisitor::visitProgram(AslParser::ProgramContext *ctx) {
  DEBUG_ENTER();
  SymTable::ScopeId sc = getScopeDecor(ctx);
  Symbols.pushThisScope(sc);
  for (auto ctxFunc : ctx->function()) { 
    visit(ctxFunc);
  }
  if (Symbols.noMainProperlyDeclared())
    Errors.noMainProperlyDeclared(ctx);
  Symbols.popScope();
  Errors.print();
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitFunction(AslParser::FunctionContext *ctx) {
  DEBUG_ENTER();
  SymTable::ScopeId sc = getScopeDecor(ctx);
  Symbols.pushThisScope(sc);

  //we have to create the function type (return and parameters)
  TypesMgr::TypeId typeFunction = Types.createVoidTy();
  if(ctx->basic_type()){
    visit(ctx->basic_type());
    typeFunction = getTypeDecor(ctx->basic_type());
    //std::cout << Types.to_string(typeFunction) << std::endl;
  }
  //parameters aren't needed at all, have fun programming them
  std::vector<TypesMgr::TypeId> lparams = {};
  
  typeFunction = Types.createFunctionTy(lparams,typeFunction);
  setCurrentFunctionTy(typeFunction);
  //std::cout << "Reported type of function is " << Types.to_string(typeFunction) << std::endl;


  visit(ctx->statements());
  Symbols.popScope();
  DEBUG_EXIT();
  return 0;
}

// antlrcpp::Any TypeCheckVisitor::visitDeclarations(AslParser::DeclarationsContext *ctx) {
//   DEBUG_ENTER();
//   antlrcpp::Any r = visitChildren(ctx);
//   DEBUG_EXIT();
//   return r;
// }

// antlrcpp::Any TypeCheckVisitor::visitVariable_decl(AslParser::Variable_declContext *ctx) {
//   DEBUG_ENTER();
//   antlrcpp::Any r = visitChildren(ctx);
//   DEBUG_EXIT();
//   return r;
// }

// antlrcpp::Any TypeCheckVisitor::visitType(AslParser::TypeContext *ctx) {
//   DEBUG_ENTER();
//   antlrcpp::Any r = visitChildren(ctx);
//   DEBUG_EXIT();
//   return r;
// }

antlrcpp::Any TypeCheckVisitor::visitStatements(AslParser::StatementsContext *ctx) {
  DEBUG_ENTER();
  visitChildren(ctx);
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitAssignStmt(AslParser::AssignStmtContext *ctx) {
  DEBUG_ENTER();
  visit(ctx->left_expr());
  visit(ctx->expr());
  TypesMgr::TypeId t1 = getTypeDecor(ctx->left_expr());
  TypesMgr::TypeId t2 = getTypeDecor(ctx->expr());
  //std::cout << "### left expression is type " << Types.to_string(t1) << std::endl;
  //std::cout << "### right value is type " << Types.to_string(t2) << '\n' << std::endl;

  if ((not Types.isErrorTy(t1)) and (not Types.isErrorTy(t2)) and (not Types.isVoidTy(t2)) and 
      (not Types.copyableTypes(t1, t2)))
    Errors.incompatibleAssignment(ctx->ASSIGN()); 
  if ((not Types.isErrorTy(t1)) and (not getIsLValueDecor(ctx->left_expr())))
    Errors.nonReferenceableLeftExpr(ctx->left_expr());
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitIfStmt(AslParser::IfStmtContext *ctx) {
  DEBUG_ENTER();
  visit(ctx->expr());
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr());
  if ((not Types.isErrorTy(t1)) and (not Types.isBooleanTy(t1)))
    Errors.booleanRequired(ctx);
  
  for (auto& ct : ctx->statements()) visit(ct);

  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitReturn(AslParser::ReturnContext *ctx) {
  DEBUG_ENTER();
  TypesMgr::TypeId functionID = getCurrentFunctionTy();

  if (ctx->expr()){
    visit(ctx->expr());
    TypesMgr::TypeId typeExpr = getTypeDecor(ctx->expr());
    TypesMgr::TypeId typeRet = Types.getFuncReturnType(functionID);
    //std::cout << Types.to_string(typeRet) << std::endl;

    //std::cout << "Return type is " << Types.to_string(t) << " and function type is " << Types.to_string(getCurrentFunctionTy()) << std::endl;

    //checking if its a void function with a return value or if its a function with different return types (Thanks to type coercion floats can be written as ints)
    if ((not Types.isErrorTy(typeExpr)) and (Types.isVoidFunction(functionID))){
      Errors.incompatibleReturn(ctx->RETURN());  
    } 

    else if( (not Types.isErrorTy(typeExpr)) and (not Types.equalTypes(typeRet, typeExpr))){
      
      if(not (Types.isFloatTy(typeRet) and Types.isIntegerTy(typeExpr))){
        Errors.incompatibleReturn(ctx->RETURN());
      } 
    }
  }
  else if(not Types.isVoidFunction(functionID)){
    Errors.incompatibleReturn(ctx->RETURN());
  } 
  
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitWhileStmt(AslParser::WhileStmtContext *ctx) {
  DEBUG_ENTER();
  visit(ctx->expr());
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr());

  if ((not Types.isErrorTy(t1)) and (not Types.isBooleanTy(t1)))
    Errors.booleanRequired(ctx);
  
  visit(ctx->statements());
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitProcCall(AslParser::ProcCallContext *ctx) {
  DEBUG_ENTER();

  //Getting the function ID
  visit(ctx->ident());
  TypesMgr::TypeId t = getTypeDecor(ctx->ident());

  //std::cout << "Visiting call to function " << ctx->ident()->getText() << std::endl;

  // Vector of the parameters of the function call (even if the function doesn't exist)
  using std::vector;
  vector<TypesMgr::TypeId> paramVect;
  paramVect.resize(static_cast<int> (ctx->expr().size()));

  for (uint i = 0; i < paramVect.size(); ++i){
    visit(ctx->expr(i));
    TypesMgr::TypeId texp = getTypeDecor(ctx->expr(i));

    //std::cout << "Parameter #" << i << " has type " << Types.to_string(texp) << std::endl;
    paramVect[i] = texp;
  }

  if (Types.isErrorTy(t)){
    putTypeDecor(ctx, Types.createErrorTy());
  }
  else if (not Types.isFunctionTy(t)) Errors.isNotCallable(ctx);
  else {
    TypesMgr::TypeId tRet = Symbols.getType(ctx->ident()->getText());
    putTypeDecor(ctx, Types.getFuncReturnType(tRet));

    //std::cout << "Call to function " << ctx->ident()->getText() << " has type " << Types.to_string(tRet) << std::endl;

    //int NumArgs = ctx->expr().size();
    std::size_t NumParameters = Types.getNumOfParameters(t);
    if (paramVect.size() != NumParameters){
      Errors.numberOfParameters(ctx->ident());
      
      putIsLValueDecor(ctx, false);
      DEBUG_EXIT();
      return 0;
    } 
    //It may happen that an integer value is given to feed a float parameter anyway, but no any other type mismatch is correct
    auto types = Types.getFuncParamsTypes(t);

    for(unsigned int i = 0; i < paramVect.size(); ++i){
      //visit(ctx->expr(i));
      //TypesMgr::TypeId texp = getTypeDecor(ctx->expr(i));

      if (not Types.equalTypes(paramVect[i], types[i]))
      {
        //if there's a type mismatch, then we gotta check if type coertion can be applied (case of floats)
        if(not Types.isErrorTy(paramVect[i]) and not(Types.isIntegerTy(paramVect[i]) and Types.isFloatTy(types[i])))
          Errors.incompatibleParameter(ctx->expr(i), i+1, ctx);
      }
    }



  }


  putIsLValueDecor(ctx,false);
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitReadStmt(AslParser::ReadStmtContext *ctx) {
  DEBUG_ENTER();
  visit(ctx->left_expr());
  TypesMgr::TypeId t1 = getTypeDecor(ctx->left_expr());
  if ((not Types.isErrorTy(t1)) and (not Types.isPrimitiveTy(t1)) and
      (not Types.isFunctionTy(t1)))
    Errors.readWriteRequireBasic(ctx);
  if ((not Types.isErrorTy(t1)) and (not getIsLValueDecor(ctx->left_expr())))
    Errors.nonReferenceableExpression(ctx);
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitWriteExpr(AslParser::WriteExprContext *ctx) {
  DEBUG_ENTER();
  visit(ctx->expr());
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr());
  if ((not Types.isErrorTy(t1)) and (not Types.isPrimitiveTy(t1)))
    Errors.readWriteRequireBasic(ctx);
  DEBUG_EXIT();
  return 0;
}

// antlrcpp::Any TypeCheckVisitor::visitWriteString(AslParser::WriteStringContext *ctx) {
//   DEBUG_ENTER();
//   antlrcpp::Any r = visitChildren(ctx);
//   DEBUG_EXIT();
//   return r;
// }

/*
antlrcpp::Any TypeCheckVisitor::visitLeft_expr(AslParser::Left_exprContext *ctx) {
  DEBUG_ENTER();
  visit(ctx->ident());
  TypesMgr::TypeId t1 = getTypeDecor(ctx->ident());
  putTypeDecor(ctx, t1);
  bool b = getIsLValueDecor(ctx->ident());
  putIsLValueDecor(ctx, b);
  DEBUG_EXIT();
  return 0;
}*/

antlrcpp::Any TypeCheckVisitor::visitSimpleIdent(AslParser::SimpleIdentContext *ctx){
  DEBUG_ENTER();
  visit(ctx->ident());
  TypesMgr::TypeId t1 = getTypeDecor(ctx->ident());
  putTypeDecor(ctx, t1);
  bool b = getIsLValueDecor(ctx->ident());
  putIsLValueDecor(ctx, b);
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitArrayIdent(AslParser::ArrayIdentContext *ctx){
  DEBUG_ENTER();
  visit(ctx->ident());
  visit(ctx->expr());

  TypesMgr::TypeId texp = getTypeDecor(ctx->expr());
  TypesMgr::TypeId t = getTypeDecor(ctx->ident());
  bool isLvalue = getIsLValueDecor(ctx->ident());
  //si no es error s'inicialitza a 1
  bool isArray = not Types.isErrorTy(t);
  TypesMgr::TypeId decoration = t;

  if((not Types.isErrorTy(t)) and (not Types.isArrayTy(t))){
    //decorem el lvalue com error
    decoration = Types.createErrorTy();     
    isLvalue = false;
    isArray = false;
    Errors.nonArrayInArrayAccess(ctx);
  }

  if ((not Types.isErrorTy(texp)) and (not Types.isIntegerTy(texp))){
    Errors.nonIntegerIndexInArrayAccess(ctx->expr());    
  }

  if(isArray){
    decoration = Types.getArrayElemType(t);
  }

  putTypeDecor(ctx,decoration);
  putIsLValueDecor(ctx, isLvalue);
  DEBUG_EXIT();

  return 0;

}

antlrcpp::Any TypeCheckVisitor::visitCall(AslParser::CallContext *ctx)
{
  DEBUG_ENTER();

  //Getting the function ID
  visit(ctx->ident());
  TypesMgr::TypeId t = getTypeDecor(ctx->ident());

  //std::cout << "Visiting call to function " << ctx->ident()->getText() << std::endl;

  // Vector of the parameters of the function call (even if the function doesn't exist)
  using std::vector;
  vector<TypesMgr::TypeId> paramVect;
  paramVect.resize(static_cast<int> (ctx->expr().size()));

  for (uint i = 0; i < paramVect.size(); ++i){
    visit(ctx->expr(i));
    TypesMgr::TypeId texp = getTypeDecor(ctx->expr(i));

    //std::cout << "Parameter #" << i << " has type " << Types.to_string(texp) << std::endl;
    paramVect[i] = texp;
  }

  if (Types.isErrorTy(t)){
    putTypeDecor(ctx, Types.createErrorTy());
  }
  else if (not Types.isFunctionTy(t)) Errors.isNotCallable(ctx);
  else {
    TypesMgr::TypeId tRet = Symbols.getType(ctx->ident()->getText());
    putTypeDecor(ctx, Types.getFuncReturnType(tRet));

    //std::cout << "Call to function " << ctx->ident()->getText() << " has type " << Types.to_string(tRet) << std::endl;

    //int NumArgs = ctx->expr().size();
    std::size_t NumParameters = Types.getNumOfParameters(t);
    if (paramVect.size() != NumParameters){
      Errors.numberOfParameters(ctx->ident());
      
      putIsLValueDecor(ctx, false);
      DEBUG_EXIT();
      return 0;
    } 
    //It may happen that an integer value is given to feed a float parameter anyway, but no any other type mismatch is correct
    auto types = Types.getFuncParamsTypes(t);

    for(unsigned int i = 0; i < paramVect.size(); ++i){
      //visit(ctx->expr(i));
      //TypesMgr::TypeId texp = getTypeDecor(ctx->expr(i));

      //Checking for types mismatch
      if (not Types.equalTypes(paramVect[i], types[i]))
      {
        //if there's a type mismatch, then we gotta check if type coertion can be applied (case of floats)
        if(not Types.isErrorTy(paramVect[i]) and not(Types.isIntegerTy(paramVect[i]) and Types.isFloatTy(types[i])))
          Errors.incompatibleParameter(ctx->expr(i), i+1, ctx);
      }
    }

    if (Types.isVoidFunction(t)) Errors.isNotFunction(ctx);


  }


  putIsLValueDecor(ctx,false);
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitArray(AslParser::ArrayContext *ctx)
{
  DEBUG_ENTER();
  visit(ctx->ident());
  visit(ctx->expr());

  TypesMgr::TypeId texp = getTypeDecor(ctx->expr());
  TypesMgr::TypeId t = getTypeDecor(ctx->ident());

  if (not Types.isErrorTy(texp) && not Types.isIntegerTy(texp)){
        Errors.nonIntegerIndexInArrayAccess(ctx->expr());
  }

  if (not Types.isErrorTy(t) && not Types.isArrayTy(t)){
    Errors.nonArrayInArrayAccess(ctx);
    putTypeDecor(ctx, Types.createErrorTy());  
  } 

  if(Types.isArrayTy(t)) putTypeDecor(ctx, Types.getArrayElemType(t));

  bool b = getIsLValueDecor(ctx->ident());
  putIsLValueDecor(ctx, b);
  DEBUG_EXIT();

  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitParen(AslParser::ParenContext *ctx)
{
  DEBUG_ENTER();
  visit(ctx->expr());
  TypesMgr::TypeId t = getTypeDecor(ctx->expr());

  putTypeDecor(ctx, t);
  putIsLValueDecor(ctx, false);
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitArithmetic(AslParser::ArithmeticContext *ctx) {
  DEBUG_ENTER();
  visit(ctx->expr(0));
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr(0));
  visit(ctx->expr(1));
  TypesMgr::TypeId t2 = getTypeDecor(ctx->expr(1));
  TypesMgr::TypeId ret;

/*
    // Module operation (%) can only be applied to integer operands. The expected behaviour for negative operands is the same than in C++.
  if ((not Types.isErrorTy(t2)) and (not Types.isIntegerTy(t2) || not Types.isIntegerTy(t1)) and (ctx->MOD()))
    Errors.incompatibleOperator(ctx->op);
*/

  if(ctx->MOD()){
    if (((not Types.isErrorTy(t1)) and (not Types.isIntegerTy(t1))) or ((not Types.isErrorTy(t2)) and (not Types.isIntegerTy(t2))))
      Errors.incompatibleOperator(ctx->op);
    
    ret = Types.createIntegerTy();
  } 
  else{
    if (((not Types.isErrorTy(t1)) and (not Types.isNumericTy(t1))) or ((not Types.isErrorTy(t2)) and (not Types.isNumericTy(t2))))
      Errors.incompatibleOperator(ctx->op);


    if (Types.isFloatTy(t1) || Types.isFloatTy(t2)) ret = Types.createFloatTy();
    else ret = Types.createIntegerTy();

  }

  putTypeDecor(ctx, ret);
  putIsLValueDecor(ctx, false);
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitUnary(AslParser::UnaryContext *ctx)
{
  DEBUG_ENTER();
  visit(ctx->expr());
  TypesMgr::TypeId t = getTypeDecor(ctx->expr());


  if (not Types.isErrorTy(t)){
    if (not Types.isNumericTy(t) && (ctx->PLUS() || ctx->SUB()))
      Errors.incompatibleOperator(ctx->op);
    else if (not Types.isBooleanTy(t) && ctx->NOT())
      Errors.incompatibleOperator(ctx->op);
  }

  if (ctx->NOT()) putTypeDecor(ctx, Types.createBooleanTy());
  else if ((ctx->PLUS() || ctx->SUB()) && Types.isFloatTy(t))  putTypeDecor(ctx, Types.createFloatTy());
  else putTypeDecor(ctx, Types.createIntegerTy());
  
  putIsLValueDecor(ctx, false);
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitLogic(AslParser::LogicContext *ctx) {
  DEBUG_ENTER();
  visit(ctx->expr(0));
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr(0));
  visit(ctx->expr(1));
  TypesMgr::TypeId t2 = getTypeDecor(ctx->expr(1));
  if ((not Types.isErrorTy(t1) and (not Types.isBooleanTy(t1))) or
      (not Types.isErrorTy(t2) and (not Types.isBooleanTy(t2))))
        Errors.incompatibleOperator(ctx->op);
  TypesMgr::TypeId t = Types.createBooleanTy();
  putTypeDecor(ctx, t);
  putIsLValueDecor(ctx, false);
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitRelational(AslParser::RelationalContext *ctx) {
  DEBUG_ENTER();
  visit(ctx->expr(0));
  TypesMgr::TypeId t1 = getTypeDecor(ctx->expr(0));
  visit(ctx->expr(1));
  TypesMgr::TypeId t2 = getTypeDecor(ctx->expr(1));
  std::string oper = ctx->op->getText();
  if ((not Types.isErrorTy(t1)) and (not Types.isErrorTy(t2)) and
      (not Types.comparableTypes(t1, t2, oper)))
    Errors.incompatibleOperator(ctx->op);
  TypesMgr::TypeId t = Types.createBooleanTy();
  putTypeDecor(ctx, t);
  putIsLValueDecor(ctx, false);
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitValue(AslParser::ValueContext *ctx) {
  DEBUG_ENTER();
  TypesMgr::TypeId t;

  if (ctx->INTVAL()) t = Types.createIntegerTy();
  else if (ctx->FLOATVAL()) t = Types.createFloatTy();
  else if (ctx->CHARVAL()) t = Types.createCharacterTy();
  else if (ctx->BOOLVAL()) t = Types.createBooleanTy();

  putTypeDecor(ctx, t);
  putIsLValueDecor(ctx, false);
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitExprIdent(AslParser::ExprIdentContext *ctx) {
  DEBUG_ENTER();
  visit(ctx->ident());
  TypesMgr::TypeId t1 = getTypeDecor(ctx->ident());
  putTypeDecor(ctx, t1);
  bool b = getIsLValueDecor(ctx->ident());
  putIsLValueDecor(ctx, b);
  DEBUG_EXIT();
  return 0;
}

antlrcpp::Any TypeCheckVisitor::visitIdent(AslParser::IdentContext *ctx) {
  DEBUG_ENTER();
  std::string ident = ctx->getText();
  if (Symbols.findInStack(ident) == -1) {
    Errors.undeclaredIdent(ctx->ID());
    TypesMgr::TypeId te = Types.createErrorTy();
    putTypeDecor(ctx, te);
    putIsLValueDecor(ctx, true);
  }
  else {
    TypesMgr::TypeId t1 = Symbols.getType(ident);
    putTypeDecor(ctx, t1);
    if (Symbols.isFunctionClass(ident))
      putIsLValueDecor(ctx, false);
    else
      putIsLValueDecor(ctx, true);
  }
  DEBUG_EXIT();
  return 0;
}



// Getters for the necessary tree node atributes:
//   Scope, Type ans IsLValue
SymTable::ScopeId TypeCheckVisitor::getScopeDecor(antlr4::ParserRuleContext *ctx) {
  return Decorations.getScope(ctx);
}
TypesMgr::TypeId TypeCheckVisitor::getTypeDecor(antlr4::ParserRuleContext *ctx) {
  return Decorations.getType(ctx);
}
bool TypeCheckVisitor::getIsLValueDecor(antlr4::ParserRuleContext *ctx) {
  return Decorations.getIsLValue(ctx);
}

// Setters for the necessary tree node attributes:
//   Scope, Type ans IsLValue
void TypeCheckVisitor::putScopeDecor(antlr4::ParserRuleContext *ctx, SymTable::ScopeId s) {
  Decorations.putScope(ctx, s);
}
void TypeCheckVisitor::putTypeDecor(antlr4::ParserRuleContext *ctx, TypesMgr::TypeId t) {
  Decorations.putType(ctx, t);
}
void TypeCheckVisitor::putIsLValueDecor(antlr4::ParserRuleContext *ctx, bool b) {
  Decorations.putIsLValue(ctx, b);
}
