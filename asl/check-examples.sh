#!/bin/bash

#--------------------------------------------
function check_chkt_example() {
    expected=$1
    produced=$2
    
    diff $expected $produced >tmp.diff
    if (test $? == 0); then
	echo "OK"
    else
	echo "Wrong output"
	cat tmp.diff
	echo ""
    fi
    rm -f tmp.diff
}

#--------------------------------------------
function check_genc_example() {	
    expected=$1
    produced=$2
    
    diff $expected $produced >tmp.diff
    if (test $? == 0); then
   	echo "OK"
    else
	echo "Wrong output"
	cat tmp.diff
 	echo ""
    fi
    rm -f tmp.diff
}

########### check 'jpbasic_chkt' examples
echo "======================================================="
echo "=== BEGIN examples/jpbasic_chkt typecheck ============="
for f in ../examples/jpbasic_chkt_*.asl; do
    echo -n "****" $(basename "$f") "...." 
    ./asl "$f" 2>&1 | egrep ^L >tmp.err
    check_chkt_example "${f/asl/err}" tmp.err 
    rm -f tmp.err
done
echo "=== END examples/jpbasic_chkt typecheck ==============="
echo "======================================================="

########### check all 'jp_chkt' examples
echo ""
echo "======================================================="
echo "=== BEGIN examples/jp_chkt_* typecheck ================"
for f in ../examples/jp_chkt_*.asl; do
    echo -n "****" $(basename "$f") "...." 
    ./asl "$f" 2>&1 | grep '^L' >tmp.err
    check_chkt_example "${f/asl/err}" tmp.err 
    rm -f tmp.err
done
echo "=== END examples/jp_chkt_* typecheck =================="
echo "======================================================="

########### check all 'jpbasic_genc' examples
echo ""
echo "======================================================="
echo "=== BEGIN examples/jpbasic_genc_* codegen ============="
for f in ../examples/jpbasic_genc_*.asl; do
    echo -n "****" $(basename "$f") "...." 
    ./asl "$f" >tmp.t 2>&1 
    if (test $? != 0); then
       echo "Compilation errors"
    else
       ../tvm/tvm tmp.t < "${f/asl/in}" >tmp.out
       check_genc_example "${f/asl/out}" tmp.out
    fi
    rm -f tmp.t tmp.out tmp.diff
done
echo "=== END examples/jpbasic_genc_* codegen ==============="
echo "======================================================="

########### check all 'jp_genc' examples
echo ""
echo "======================================================="
echo "=== BEGIN examples/jp_genc_* codegen =================="
for f in ../examples/jp_genc_*.asl; do
    echo -n "****" $(basename "$f") "...." 
    ./asl "$f" >tmp.t 2>&1 
    if (test $? != 0); then
       echo "Compilation errors"
    else
       ../tvm/tvm tmp.t < "${f/asl/in}" >tmp.out
       check_genc_example "${f/asl/out}" tmp.out
    fi
    rm -f tmp.t tmp.out tmp.diff
done
echo "=== END examples/jp_genc_* codegen ===================="
echo "======================================================="
