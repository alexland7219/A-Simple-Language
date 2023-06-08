//////////////////////////////////////////////////////////////////////
//
//    debug - define some optional macros useful in debugging
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

#include "antlr4-runtime.h"

#include <iostream>
#include <string>

// using namespace std;


//////////////////////////////////////////////////////////////////////
// This file contains 3 DEBUG macros to be used in the visitors:
//   DEBUG(x)          : with a 'message' x (to use anywhere):
//                       DEBUG("a:" << a << " b:" << b);
//   DEBUG_ENTER()     : for the enter message in a rule method, and
//   DEBUG_EXIT()      : for the exit message in a rule method
//
// These messages can be enabled in a specific module/visitor
// defining the variable DEBUG_BUILD *before* the inclusion
// of this file

#ifdef DEBUG_BUILD
  static int _i_ = 0;
  static int _delta_i_ = 2;
  std::string _incr_indent_() { std::string s = std::string(_i_, ' '); _i_ += _delta_i_; return s; }
  std::string _decr_indent_() { _i_ -= _delta_i_; std::string s = std::string(_i_, ' '); return s; }
  #define DEBUG(x) do { std::cout << x << std::endl; } while (0)
  #define DEBUG_ENTER() DEBUG(_incr_indent_() << ">>> enter " << std::string(__func__).substr(5) << " [source pos " << ctx->getStart()->getLine() << ":" << ctx->getStart()->getCharPositionInLine() << "] [module: " << std::string(typeid(*this).name()).substr(2,std::string(typeid(*this).name()).find("Visitor")-2) << "]")
  #define DEBUG_EXIT() DEBUG( _decr_indent_() << ">>> exit " << std::string(__func__).substr(5) << " [source pos " << ctx->getStart()->getLine() << ":" << ctx->getStart()->getCharPositionInLine() << "] [module: " << std::string(typeid(*this).name()).substr(2,std::string(typeid(*this).name()).find("Visitor")-2) << "]")
#else
  #define DEBUG(x)
  #define DEBUG_ENTER()
  #define DEBUG_EXIT()
#endif
