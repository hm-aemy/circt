// RUN: circt-translate --import-rtlil %S/basic.il | FileCheck %s
// Yosys' RTLIL frontend logs a header and the input filename. A translation
// tool writes its result and nothing else, so stderr must stay empty.
// RUN: circt-translate --import-rtlil %S/basic.il 2>&1 >/dev/null \
// RUN:   | FileCheck %s --check-prefix=QUIET --allow-empty
// QUIET-NOT: {{.}}
// REQUIRES: libyosys

// Yosys' own RTLIL frontend does the parsing; no `.il` parser is written on the
// MLIR side, which is the whole point of linking the library.

// A module attribute becomes an entry in the module's `rtlil_attributes`.
// CHECK-LABEL: rtlil.module @"\\demo"
// CHECK-SAME:    attributes [#rtlil.param<"\\top" 1 : i32>]

// Ports keep their `port_id`, which is what carries the port ordering.
// CHECK-DAG: "rtlil.wire"() <{{{.*}}name = "\\a"{{.*}}port_id = 1 {{.*}}port_input = true{{.*}}> : () -> !rtlil<val[8]>
// CHECK-DAG: "rtlil.wire"() <{{{.*}}name = "\\b"{{.*}}port_id = 2 {{.*}}port_input = true{{.*}}> : () -> !rtlil<val[8]>
// CHECK-DAG: "rtlil.wire"() <{{{.*}}name = "\\y"{{.*}}port_id = 3 {{.*}}port_output = true{{.*}}> : () -> !rtlil<val[8]>
// CHECK-DAG: "rtlil.wire"() <{{{.*}}name = "$tmp"{{.*}}port_id = 0 {{.*}}> : () -> !rtlil<val[8]>

// Every cell imports as a generic `rtlil.cell`: after techmap or abc most cell
// types have no dialect op, and the typed ops carry *derived* parameters that
// could not represent whatever Yosys set. Ports and parameters are sorted by
// name so the output does not depend on Yosys' hash order.
// CHECK: "rtlil.cell"
// CHECK-SAME: name = "$c"
// CHECK-SAME: parameters = [#rtlil.param<"\\A_SIGNED" 0 : i32>, #rtlil.param<"\\A_WIDTH" 8 : i32>, #rtlil.param<"\\B_SIGNED" 0 : i32>, #rtlil.param<"\\B_WIDTH" 8 : i32>, #rtlil.param<"\\Y_WIDTH" 8 : i32>]
// CHECK-SAME: ports = ["\\A", "\\B", "\\Y"]
// CHECK-SAME: type = "$and"

// Module-level `connect` statements become `rtlil.wconnection`.
// CHECK: "rtlil.wconnection"

// An RTLIL `src` attribute becomes the op's Location rather than another entry
// in the attribute dict -- keeping both would duplicate it on every round trip,
// since export re-derives `src` from the location.
// RUN: circt-translate --import-rtlil %S/basic.il --mlir-print-debuginfo \
// RUN:   | FileCheck %s --check-prefix=LOC
// The `src` is gone from the attribute dict...
// LOC: "rtlil.wire"() <{{{.*}}name = "\\a"{{.*}}rtlil_attributes = []{{.*}}
// ...and has become a real source location, keeping the end of the `src`
// range rather than narrowing it to its start.
// LOC-DAG: loc("demo.v":3:1 to :9)
