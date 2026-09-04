// RUN: circt-opt %s --convert-hw-to-rtlil \
// RUN:   --rtlil-run-yosys='script={techmap;abc -script +strash;dc2}' \
// RUN:   | FileCheck %s
// REQUIRES: libyosys

// A `;` inside a `+`-prefixed argument belongs to that argument: `abc -script
// +strash;dc2` is one command, and splitting it would hand Yosys a `dc2` it
// does not know. The run has to succeed for the pass to emit anything at all.

hw.module @demo(in %a: i8, in %b: i8, out y: i8) {
  %0 = comb.and %a, %b : i8
  hw.output %0 : i8
}

// CHECK-LABEL: rtlil.module @"\\demo"
// CHECK: rtlil.wire
