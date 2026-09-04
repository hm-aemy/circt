// Locations survive the pair intact. `src` alone cannot carry them: it is a
// flat list of `file:line.col` ranges, so a NameLoc's name, a CallSiteLoc's
// stack and a FusedLoc's metadata would all be lost. Export writes a second
// `\circt.loc` attribute holding the location in MLIR's own syntax, and import
// prefers it over `src`.
// RUN: circt-translate --export-rtlil %s 2>/dev/null \
// RUN:   | circt-translate --import-rtlil --mlir-print-debuginfo 2>/dev/null \
// RUN:   | FileCheck %s
// REQUIRES: libyosys

// Both attributes are written, and `src` stays the flattened form Yosys itself
// understands and propagates through a script.
// RUN: circt-translate --export-rtlil %s 2>/dev/null | FileCheck %s --check-prefix=IL

// IL-DAG: attribute \src "in.sv:3.5-3.12"
// IL-DAG: attribute \circt.loc "loc(\"in.sv\":3:5 to :12)"
// A CallSiteLoc flattens to its callee for `src`; the stack lives in the other.
// IL-DAG: attribute \src "in.sv:7.1-7.1"
// IL-DAG: attribute \circt.loc "loc(callsite(\"in.sv\":7:1 at \"top.sv\":1:1))"

// CHECK-DAG: #[[A:.+]] = loc("in.sv":3:5 to :12)
// CHECK-DAG: #[[NAMED:.+]] = loc("q"(
// CHECK-DAG: #[[FUSED:.+]] = loc(fused[
// CHECK-DAG: #[[CALL:.+]] = loc(callsite(
// CHECK-DAG: name = "\\a"{{.*}}loc(#[[A]])
// CHECK-DAG: name = "\\b"{{.*}}loc(#[[FUSED]])
// CHECK-DAG: name = "\\y"{{.*}}loc(#[[NAMED]])
// CHECK-DAG: "rtlil.cell"{{.*}}loc(#[[CALL]])

// Neither attribute reappears in `rtlil_attributes`; both are the Location.
// CHECK-NOT: circt.loc
// CHECK-NOT: rtlil.param<"\\src"

"rtlil.module"() <{sym_name = "\\top", avail_parameters = [], rtlil_attributes = []}> ({
  %a = "rtlil.wire"() <{name = "\\a", is_signed = false, port_id = 1 : i32, start_offset = 0 : i32, port_input = true, port_output = false, upto = false, rtlil_attributes = []}> : () -> !rtlil<val[4]> loc("in.sv":3:5 to :12)
  %b = "rtlil.wire"() <{name = "\\b", is_signed = false, port_id = 2 : i32, start_offset = 0 : i32, port_input = true, port_output = false, upto = false, rtlil_attributes = []}> : () -> !rtlil<val[4]> loc(fused["in.sv":9:1, "in.sv":10:2])
  %y = "rtlil.wire"() <{name = "\\y", is_signed = false, port_id = 3 : i32, start_offset = 0 : i32, port_input = false, port_output = true, upto = false, rtlil_attributes = []}> : () -> !rtlil<val[4]> loc("q"("in.sv":11:3))
  "rtlil.and"(%a, %b, %y) <{name = "$and", width = 4 : i32, opsSigned = 0 : i32}> : (!rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[4]>) -> () loc(callsite("in.sv":7:1 at "top.sv":1:1))
}) : () -> () loc("in.sv":1:1)
