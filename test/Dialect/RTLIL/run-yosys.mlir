// RUN: circt-opt %s --convert-hw-to-rtlil \
// RUN:   --rtlil-run-yosys='script={opt;techmap;abc -g AND,OR,XOR;opt_clean}' \
// RUN:   | FileCheck %s
// REQUIRES: libyosys

// Path B end to end: hw/comb -> rtlil dialect -> RTLIL::Design -> Yosys passes
// -> RTLIL::Design -> rtlil dialect, all in one process with no file written.

hw.module @demo(in %a: i8, in %b: i8, in %c: i8, out y: i8) {
  %0 = comb.and %a, %b : i8
  %1 = comb.or %0, %c : i8
  hw.output %1 : i8
}

// Public names survive the script untouched -- Yosys rewrites `$`-prefixed
// identifiers freely but carries `\`-prefixed ones through. This is what makes
// the result recognisable as the module that went in, and what lets
// `hierarchy` address it by name.
// `hierarchy -check -auto-top` ran and marked the top module.
// CHECK-LABEL: rtlil.module @"\\demo"
// CHECK-SAME:    attributes [#rtlil.param<"\\top" 1 : i32>]
// CHECK-DAG: wire "\\a" input port 1
// CHECK-DAG: wire "\\b" input port 2
// CHECK-DAG: wire "\\c" input port 3
// CHECK-DAG: wire "\\y" output port 4

// Cells come back with gate-level types the dialect has no op for, so they are
// generic `rtlil.cell`s. `abc -g AND,OR,XOR` maps an 8-bit (a & b) | c onto one
// AND and one OR per bit.
// CHECK-DAG: cell "{{[^"]*}}" "$_AND_" [
// CHECK-DAG: cell "{{[^"]*}}" "$_OR_" [

// Those gates drive single bits of the 8-bit wires, so every port is a slice.
// Without `rtlil.slice` this design could not be imported at all.
// CHECK-DAG: "rtlil.slice"(%{{.+}}) <{offset = 0 : i32}> : (!rtlil<val[8]>) -> !rtlil<val[1]>
// CHECK-DAG: "rtlil.slice"(%{{.+}}) <{offset = 7 : i32}> : (!rtlil<val[8]>) -> !rtlil<val[1]>
