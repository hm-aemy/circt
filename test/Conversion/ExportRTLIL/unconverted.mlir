// RUN: not circt-translate --export-rtlil %s 2>&1 | FileCheck %s
// REQUIRES: libyosys

// `rtlil-run-yosys` may skip an unconverted `hw.module` and leave it in the
// file, but an `.il` missing half the design is a wrong answer that looks like
// a right one, so the translation refuses and names what would go missing.
// Metadata with no hardware meaning is dropped without comment.

// CHECK: error: cannot export a design that is not fully converted: 2 hardware modules would be missing from the output; run 'convert-hw-to-rtlil' first
// CHECK-DAG: note: 'unconverted' is not an 'rtlil.module'
// CHECK-DAG: note: 'blackbox' is not an 'rtlil.module'
// CHECK-NOT: 'leftovers_Class'

rtlil.module @"\\converted" {
  %w = "rtlil.wire"() <{name = "\\w", is_signed = false}> : () -> !rtlil<val[1]>
}

hw.module @unconverted(in %a: i8, out y: i8) {
  hw.output %a : i8
}

hw.module.extern @blackbox(in %a: i8)

om.class @leftovers_Class(%basepath: !om.basepath) {
  om.class.fields
}
