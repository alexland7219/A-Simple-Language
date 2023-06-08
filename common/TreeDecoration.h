//////////////////////////////////////////////////////////////////////
//
//    TreeDecoration - Tree node attributes for
//                     the Asl programming language
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

#pragma once

#include "TypesMgr.h"
#include "SymTable.h"

#include "antlr4-runtime.h"
#include "tree/ParseTreeProperty.h"

// using namespace std;


//////////////////////////////////////////////////////////////////////
// Class TreeDecoration: the nodes of the parser tree generated
// by the antlr4 parser, whose base type is
// antlr4::ParserRuleContext *, can have different attributes.
// TreeDecoration groups all of them, and uses different
// ParseTreeProperty to save this information.
// Currently three kinds of attributes may be present:
//   - scope, for nodes like the program, or functions
//   - type, for expressions or type especification
//   - isLValue, for expressions
// Different visitors set and access these attributes:
//   - SymbolsVisitor     [TypeCheck phase 1]
//       * set and access the scope attribute
//       * set and access the type attribute (in type declarations)
//   - TypeCheckVisitor   [TypeCheck phase 2]
//       * access the scope attribute
//       * set and access the type attribute (in expressions)
//       * set and access the isLValue attribute (in expressions)
//   - CodeGenVisitor     [Code Generation]
//       * access the scope attribute
//       * access the type attribute

class TreeDecoration {

public:
  TreeDecoration() = default;

  // Getters:
  SymTable::ScopeId getScope    (antlr4::ParserRuleContext *ctx);
  TypesMgr::TypeId  getType     (antlr4::ParserRuleContext *ctx);
  bool              getIsLValue (antlr4::ParserRuleContext *ctx);

  // Setters:
  void putScope    (antlr4::ParserRuleContext *ctx, SymTable::ScopeId s);
  void putType     (antlr4::ParserRuleContext *ctx, TypesMgr::TypeId t);
  void putIsLValue (antlr4::ParserRuleContext *ctx, bool b);

private:
  antlr4::tree::ParseTreeProperty<SymTable::ScopeId> ScopeDecor;
  antlr4::tree::ParseTreeProperty<TypesMgr::TypeId>  TypeDecor;
  antlr4::tree::ParseTreeProperty<bool>              IsLValueDecor;

};  // class TreeDecoration
