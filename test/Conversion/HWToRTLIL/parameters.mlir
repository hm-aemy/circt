// RUN: circt-opt %s --hw-specialize --convert-hw-to-rtlil | FileCheck %s

// `hw-specialize` repoints the instances at the specialized copies but leaves
// the parametric templates behind, so the conversion has to drop them itself.
// CHECK-NOT: hw.module
// CHECK-NOT: parameters = [#hw.param

// CHECK-DAG: rtlil.module [[SUB8:@"\\\\sub_W_8"]]
// CHECK-DAG: rtlil.module [[SUB4:@"\\\\sub_W_4"]]
hw.module @sub<W: i32 = 8>(in %a : !hw.int<#hw.param.decl.ref<"W">>,
                           out b : !hw.int<#hw.param.decl.ref<"W">>) {
  hw.output %a : !hw.int<#hw.param.decl.ref<"W">>
}

// CHECK-LABEL: rtlil.module @"\\top"
hw.module @top(in %x : i8, in %z : i4, out y : i8, out w : i4) {
  // CHECK-DAG: instance "\\i0" [[SUB8]] [{{.*}}] parameters [] : !rtlil<val[8]>, !rtlil<val[8]>
  // CHECK-DAG: instance "\\i1" [[SUB4]] [{{.*}}] parameters [] : !rtlil<val[4]>, !rtlil<val[4]>
  %0 = hw.instance "i0" @sub<W: i32 = 8>(a: %x: i8) -> (b: i8)
  %1 = hw.instance "i1" @sub<W: i32 = 4>(a: %z: i4) -> (b: i4)
  hw.output %0, %1 : i8, i4
}
