// The pair is an identity on the supported subset: import a design Yosys wrote,
// export it, and read it back through Yosys' own frontend again.
// RUN: circt-translate --import-rtlil %S/../ImportRTLIL/basic.il 2>/dev/null \
// RUN:   | circt-translate --export-rtlil 2>/dev/null \
// RUN:   | FileCheck %s
// REQUIRES: libyosys

// CHECK: module \demo
// CHECK-DAG: wire width 8 input 1 \a
// CHECK-DAG: wire width 8 input 2 \b
// CHECK-DAG: wire width 8 output 3 \y
// CHECK-DAG: wire width 8 $tmp
// CHECK: cell $and $c
// CHECK-DAG: parameter \A_SIGNED 0
// CHECK-DAG: parameter \A_WIDTH 8
// CHECK-DAG: parameter \Y_WIDTH 8
// CHECK-DAG: connect \A \a
// CHECK-DAG: connect \B \b
// CHECK-DAG: connect \Y $tmp
// CHECK: end
// CHECK: connect \y $tmp
// CHECK: end
