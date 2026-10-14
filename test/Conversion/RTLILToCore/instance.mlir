// RUN: circt-opt %s --convert-rtlil-to-core | FileCheck %s
// RUN: circt-opt %s --pass-pipeline='builtin.module(convert-rtlil-to-core, hw.module(cse, llhd-combine-drives, llhd-sig2reg, cse, canonicalize))' | FileCheck %s --check-prefix=CORE

// CORE-NOT: llhd.

// An instance may come before its callee. Connections are matched to the
// callee ports by name, in any order: inputs are probed, outputs driven. The
// callee port order is its `port_id` order.
// CHECK-LABEL: hw.module @top(in %a : i4, in %b : i4, out o : i4)
// CORE-LABEL: hw.module @top(in %a : i4, in %b : i4, out o : i4)
rtlil.module @"\\top" {
  // CHECK-DAG: [[A:%.+]] = llhd.sig name "a" : <i4>
  // CHECK-DAG: [[B:%.+]] = llhd.sig name "b" : <i4>
  %a = wire "\\a" input port 1 : !rtlil<val[4]>
  %b = wire "\\b" input port 2 : !rtlil<val[4]>
  // CHECK-DAG: %o = llhd.sig : <i4>
  %o = wire "\\o" output port 3 : !rtlil<val[4]>
  // CHECK-DAG: [[A_PRB:%.+]] = llhd.prb [[A]] : i4
  // CHECK-DAG: [[B_PRB:%.+]] = llhd.prb [[B]] : i4
  // CHECK-DAG: %u0.y, %u0.z = hw.instance "u0" @sub(x: [[A_PRB]]: i4, w: [[B_PRB]]: i4) -> (y: i4, z: i4)
  // CHECK-DAG: llhd.drv %o, %u0.y after {{%.+}} : i4
  // CORE: %u0.y, %u0.z = hw.instance "u0" @sub(x: %a: i4, w: %b: i4) -> (y: i4, z: i4)
  // CORE: hw.output %u0.y : i4
  // The output `\z` is left unconnected.
  instance "\\u0" @"\\sub" ["\\y" = %o, "\\w" = %b, "\\x" = %a] parameters [] : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[4]>
}

// CHECK-LABEL: hw.module @sub(in %x : i4, out y : i4, in %w : i4, out z : i4)
rtlil.module @"\\sub" {
  %x = wire "\\x" input port 1 : !rtlil<val[4]>
  %y = wire "\\y" output port 2 : !rtlil<val[4]>
  %w = wire "\\w" input port 3 : !rtlil<val[4]>
  %z = wire "\\z" output port 4 : !rtlil<val[4]>
  and "$1"(%x, %w, %y) {opsSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[4]>
  or "$2"(%x, %w, %z) {opsSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[4]>
}

// A concat on an instance output is driven by it.
// CHECK-LABEL: hw.module @split(in %a : i4, in %b : i4, out lo : i2, out hi : i2)
rtlil.module @"\\split" {
  %a = wire "\\a" input port 1 : !rtlil<val[4]>
  %b = wire "\\b" input port 2 : !rtlil<val[4]>
  %lo = wire "\\lo" output port 3 : !rtlil<val[2]>
  %hi = wire "\\hi" output port 4 : !rtlil<val[2]>
  // CHECK-DAG: [[CAT:%.+]] = llhd.sig : <i4>
  // CHECK-DAG: [[CAT_PRB:%.+]] = llhd.prb [[CAT]] : i4
  // CHECK-DAG: comb.extract [[CAT_PRB]] from 0 : (i4) -> i2
  // CHECK-DAG: comb.extract [[CAT_PRB]] from 2 : (i4) -> i2
  %cat = concat %lo, %hi : (!rtlil<val[2]>, !rtlil<val[2]>) -> !rtlil<val[4]>
  // CHECK-DAG: %u1.y, %u1.z = hw.instance "u1" @sub
  // CHECK-DAG: llhd.drv [[CAT]], %u1.y after {{%.+}} : i4
  instance "\\u1" @"\\sub" ["\\x" = %a, "\\w" = %b, "\\y" = %cat] parameters [] : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[4]>
}
