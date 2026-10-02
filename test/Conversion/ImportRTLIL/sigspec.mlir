// RUN: circt-translate --import-rtlil %S/sigspec.il | FileCheck %s
// REQUIRES: libyosys

// CHECK-LABEL: rtlil.module @"\\spec"
// CHECK-DAG: %[[W:.+]] = "rtlil.wire"() <{{{.*}}name = "\\w"{{.*}}> : () -> !rtlil<val[8]>

// `{ \w [6:3] 2'01 }` is four bits of \w above two constant bits. Chunks are
// least significant first, so the constant comes first and the slice second --
// the opposite of RTLIL's textual `{ msb ... lsb }`.
// CHECK-DAG: %[[C:.+]] = const <"01"> : [2]
// CHECK-DAG: %[[S:.+]] = "rtlil.slice"(%[[W]]) <{offset = 3 : i32}> : (!rtlil<val[8]>) -> !rtlil<val[4]>
// CHECK-DAG: %[[A:.+]] = "rtlil.concat"(%[[C]], %[[S]]) : (!rtlil<val[2]>, !rtlil<val[4]>) -> !rtlil<val[6]>
// CHECK: "rtlil.cell"(%[[A]],
// CHECK-SAME: type = "$not"

// A one-bit pick is still a slice, and the wire it comes from stays bare.
// CHECK-DAG: %[[B:.+]] = "rtlil.slice"(%[[W]]) <{offset = 2 : i32}> : (!rtlil<val[8]>) -> !rtlil<val[1]>
// CHECK-DAG: "rtlil.wconnection"(%{{.+}}, %[[B]])
