# A Simple Language
This is a compiler for ASL (A Simple Language) written in C++, main project of Compilers (FIB-UPC) 2023.

## Prerequisits

Have ANTLRv4 installed on your GNU/Linux system on `/usr/local` (otherwise the Makefile won't compile the compiler). 

## How to compile the compiler

`make antlr` first, then `make` on the `asl` directory. This will compile a binary file `./aslasl` that compiles ASL.

## How to execute?

The compiler translates ASL code into TVM code. In order to execute the TVM code you can use the `tvm` executable provided in this repo.

## ASL: Syntax and Semantics

ASL Description
===============

You must build a compiler of ASL (*A Simple Language*), an imperative high-level programming language with the basic data types (integer, boolean, character, float), usual control structures (if-then-else, while, functions, ...), and arrays (one-dimension, basic type elements).

Here you will find a definition of the syntax of the language, and the expected behavior of the compiler in most frequent situations.

You will be provided example test suites with the errors that the compiler is expected to produce in each case.

--------------------------------------------------------------------

Data types and variable declarations
------------------------------------

Variables must be declared before being used.
Variable declarations can appear exclusively at the beggining of a function, and have the form:

::

  var id1, id2, .., idn : type

Valid identifiers start by a letter, optionally followed by any string made of letters, digits, or underscores.
Valid types for variables include basic --or primitive-- types, as well as arrays.
  
Basic types are:

- ``int`` : Integer
- ``float`` : Real
- ``bool`` : Boolean
- ``char`` : Character
  
Arrays can contain elements of basic types only, and are defined as follows:

``array [<size>] of <type>``    
  (where ``<size>`` is an integer constant determining the number of elements and ``<type>`` is a basic type).

Examples of declarations:

::

  var a, b : int
  var a3, b_2, c3_x_ : bool
  var x : float
  var p, q : array [10] of char


Literal values of type ``int`` consist of one or more digits.
  
Literal values of type ``float`` consist of one or more digits followed by a decimal point, followed by one or more digits.

Literal values of type ``bool`` are ``true`` and ``false``.

Literal values of type ``char`` are enclosed in single quotes, e.g. ``'a'``, ``'5'``, ``'@'``. Escaped characters ``'\n'``, ``'\t'``, and ``'\''`` are allowed.


  
--------------------------------------------------------------------
  
Functions, parameters, and results
----------------------------------

Functions are declared using keywords ``func`` and ``endfunc``.
A comma-separated list of parameters (which maybe empty) is defined in parenthesis after the function name. For each parameter, name and type must be provided (i.e. brief parameter declaration of the form ``x,y:int`` is not allowed).

Finally, the return type of the function (if any) is declared after a colon.
Functions can only return basic types.

All parameters are passed by value, except arrays, which are passed by reference.

Integer values can be passed as arguments to float parameters, in which case an implicit conversion to float is performed.

Declaration of local variables is located at the beggining of the function, following by the instructions in the function body.

Functions (either void or not) may include ``return`` instructions at any point, with an argument of the appropriate type (or nothing for void functions). Integer values can be returned inside float functions, and an implicit cast is performed.

Example of a function with boolean result:

::
 
  func fun1 (a: array [10] of int, b: float) : bool
    var i : int
    var x : bool
    i = 5;
    if a[i]>b then
       return a[i+1]!=a[i-1];
    else
       x = false;
       return a[i-2]>0 or x;
    endif

  endfunc

Example of a *void* function:

::
 
  func fun2 (a: array [10] of int, i: int)
    var i : int
    i = 5;
    if a[i]>b then
      a[i+1] = a[i-1];
      return;
    else
       x = false;
    endif

    return;
  endfunc


Functions that do not return a value can be called only as instructions.
Functions returning a value can be called either inside expressions, or as instructions. In the later case, the return value is ignored.


--------------------------------------------------------------------
    
Operators and precedence
------------------------

ASL supports all usual aritmethic, relational, and boolean operators, with the following precedences and left associativity:

+-----------------------------------------------+--------------+
| *Operation*                                   | *Precedence* |
+===============================================+==============+
| ``[]``                                        | Highest      |
+-----------------------------------------------+--------------+
| ``not`` ``+`` ``-`` (used as unary operators) |              |
+-----------------------------------------------+--------------+
| ``*`` ``/`` ``%``                             |              |
+-----------------------------------------------+--------------+
| ``+`` ``-``                                   |              |
+-----------------------------------------------+--------------+
| ``==`` ``!=`` ``>`` ``>=`` ``<=`` ``<``       |              |
+-----------------------------------------------+--------------+
| ``and``                                       |              |
+-----------------------------------------------+--------------+
| ``or``                                        | Lowest       |
+-----------------------------------------------+--------------+

As customary, these precedences may be overriden enclosing subexpressions in parenthesis.

All relational operators can be applied to all basic types, except booleans, that admit only ``==`` and ``!=``.

Numeric types (float and int) can also be compared (after performing an implicit conversion to float).

Module operation (``%``) can only be applied to integer operands. The expected behaviour for negative operands is the same than in C++.

When integer values are aritmethically operated with float values, an implicit conversion to float is performed before the actual operation, and the result of the operation has type float.

--------------------------------------------------------------------

Basic instructions
------------------

Basic instructions include assignment, ``return`` statement, and read/write statements.
All basic instructions must end in a semicolon ``;``

Assignment
^^^^^^^^^^

::

   var x,y,i,j : int
   var a,b,c : array [10] of int
   x = 0;
   y = 2 + x;
   a[i+1] = x*2 - b[j];
   c = a;
   

Both primitive types and arrays can be assigned.
    
Integer values can be assigned to float variables after an implicit conversion (but not the other way round). 
       

Input/output statements
^^^^^^^^^^^^^^^^^^^^^^^

* **Read** : reads a value from the standard input and stores it in given l-value. If the type of the input data does not match the l-value, the behaviour is undefined.

  Example:
    
  ::

    read x;
    
* **Write** : prints to the standard output the value of given expression.

  Example:
    
  ::

    write x+2;
    write '\n';

  Output statement ``write`` accepts also a string literal. This is the only use of string allowed in ASL.
    
  Example:
    
  ::

    write "The result is: ";
    write x+2;
    write '\n';
    

Return statement
^^^^^^^^^^^^^^^^

Return statement causes current function to stop execution and return to the caller. If the function has a return value, the result expression must be given as argument to the ``return`` instruction.

This instruction can appear anywhere in a function.
If the function contains paths not ending in a ``return`` instruction, the function will return an undefined value when reaching the end of its code.

Examples:
    
::

  func f() : bool 
     ...
     ...
     return x>0 or not b;
  endfunc

       
  func g() 
     ...
     if (err!=0) then return; endif
     ...     
  endfunc

  
--------------------------------------------------------------------       

Control structures
------------------

ASL has two control structures: one for alternative composition and one for iterative composition:

* Alternative composition: alternative statements can be built with
  keywords ``if``, ``then``, ``else``, and ``endif``.

  The condition must be of boolean type.
  The ``else`` branch is optional.

  Example of an ``if-then-else`` construction
  
  ::

    if a>b then
       x = 0;
    else
       x = 2 + y;
    endif

  Examples of ``if-then`` constructions
  
  ::

    if x+1<0 then
       a=b;
    endif

* Iterative statements may be expressed with the ``while-do-endwhile`` construction

  The condition must be of boolean type.

  Example of ``while-do-endwhile`` construction
  
  ::

    while a>0 do
       s = s + a;
       a = a - 1;
    endwhile

