// RUN: circt-translate --import-rtlil %S/sigspec.il | FileCheck %s
// REQUIRES: libyosys

// CHECK-LABEL: rtlil.module @"\\spec"
// CHECK-DAG: %[[W:.+]] = wire "\\w" input port 1 : [8]

// `{ \w [6:3] 2'01 }` is four bits of \w above two constant bits. Chunks are
// least significant first, so the constant comes first and the slice second --
// the opposite of RTLIL's textual `{ msb ... lsb }`.
// CHECK-DAG: %[[C:.+]] = const <"01"> : [2]
// CHECK-DAG: %[[S:.+]] = slice %[[W]] offset 3 : (!rtlil<val[8]>) -> !rtlil<val[4]>
// CHECK-DAG: %[[A:.+]] = concat %[[C]], %[[S]] : (!rtlil<val[2]>, !rtlil<val[4]>) -> !rtlil<val[6]>
// CHECK: cell "$c" "$not" ["\\A" = %[[A]],

// A one-bit pick is still a slice, and the wire it comes from stays bare.
// CHECK-DAG: %[[B:.+]] = slice %[[W]] offset 2 : (!rtlil<val[8]>) -> !rtlil<val[1]>
// CHECK-DAG: "rtlil.wconnection"(%{{.+}}, %[[B]])
