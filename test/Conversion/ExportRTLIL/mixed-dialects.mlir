// RUN: circt-translate --export-rtlil %s | FileCheck %s
// REQUIRES: libyosys

// `convert-hw-to-rtlil` rewrites comb and seq and leaves everything else where
// it found it, so firtool output reaches this translation with the metadata
// dialects still in the module. Registering only the RTLIL dialect made that
// fail to parse, long before the exporter could ignore it.

rtlil.module @"\\top" {
  %a = rtlil.wire "\\a" input port 1 : !rtlil<val[8]>
  %y = rtlil.wire "\\y" output port 2 : !rtlil<val[8]>
  rtlil.wconnection %y, %a : !rtlil<val[8]>
}

om.class @top_Class(%basepath: !om.basepath) {
  om.class.fields
}

hw.hierpath private @path [@"\\top"]

sv.macro.decl @SOME_MACRO

emit.file "metadata.txt" {
  sv.verbatim "nothing to see here"
}

// The RTLIL that comes out mentions the design and nothing else.
// CHECK: module \top
// CHECK-DAG: wire width 8 input 1 \a
// CHECK-DAG: wire width 8 output 2 \y
// CHECK: connect \y \a
// CHECK: end
// CHECK-NOT: om.class
// CHECK-NOT: metadata.txt
