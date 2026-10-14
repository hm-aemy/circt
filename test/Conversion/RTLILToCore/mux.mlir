// RUN: circt-opt %s --convert-rtlil-to-core | FileCheck %s
// RUN: circt-opt %s --pass-pipeline='builtin.module(convert-rtlil-to-core, hw.module(cse, llhd-combine-drives, llhd-sig2reg, cse, canonicalize))' | FileCheck %s --check-prefix=CORE

// CORE-NOT: llhd.

// `rtlil.mux` drives Y with B while S is 1, so B is the true value.
// CHECK-LABEL: hw.module @muxmod(in %a : i8, in %b : i8, in %s : i1, out y : i8)
// CORE-LABEL: hw.module @muxmod(in %a : i8, in %b : i8, in %s : i1, out y : i8)
rtlil.module @"\\muxmod" {
  // CHECK-DAG: [[A:%.+]] = llhd.sig name "a" : <i8>
  // CHECK-DAG: [[B:%.+]] = llhd.sig name "b" : <i8>
  // CHECK-DAG: [[S:%.+]] = llhd.sig name "s" : <i1>
  %a = wire "\\a" input port 1 : !rtlil<val[8]>
  %b = wire "\\b" input port 2 : !rtlil<val[8]>
  %s = wire "\\s" input port 3 : !rtlil<val[1]>
  // CHECK-DAG: %y = llhd.sig : <i8>
  %y = wire "\\y" output port 4 : !rtlil<val[8]>
  // CHECK-DAG: [[A_PRB:%.+]] = llhd.prb [[A]] : i8
  // CHECK-DAG: [[B_PRB:%.+]] = llhd.prb [[B]] : i8
  // CHECK-DAG: [[S_PRB:%.+]] = llhd.prb [[S]] : i1
  // CHECK-DAG: [[MUX:%.+]] = comb.mux [[S_PRB]], [[B_PRB]], [[A_PRB]] : i8
  // CHECK-DAG: llhd.drv %y, [[MUX]] after {{%.+}} : i8
  // CORE: [[MUX:%.+]] = comb.mux %s, %b, %a : i8
  // CORE: hw.output [[MUX]] : i8
  mux "$1"(%a, %b, %s, %y) {width = 8 : i32} : !rtlil<val[8]>, !rtlil<val[8]>, !rtlil<val[1]>, !rtlil<val[8]>
}

// `rtlil.pmux` becomes a `comb.mux` chain in which the lowest set bit of S
// wins.
// CORE-LABEL: hw.module @pmuxmod(in %a : i4, in %b : i8, in %s : i2, out y : i4)
rtlil.module @"\\pmuxmod" {
  %a = wire "\\a" input port 1 : !rtlil<val[4]>
  %b = wire "\\b" input port 2 : !rtlil<val[8]>
  %s = wire "\\s" input port 3 : !rtlil<val[2]>
  %y = wire "\\y" output port 4 : !rtlil<val[4]>
  // CORE-DAG: [[S1:%.+]] = comb.extract %s from 1 : (i2) -> i1
  // CORE-DAG: [[B1:%.+]] = comb.extract %b from 4 : (i8) -> i4
  // CORE-DAG: [[S0:%.+]] = comb.extract %s from 0 : (i2) -> i1
  // CORE-DAG: [[B0:%.+]] = comb.extract %b from 0 : (i8) -> i4
  // CORE-DAG: [[MUX1:%.+]] = comb.mux [[S1]], [[B1]], %a : i4
  // CORE-DAG: [[MUX0:%.+]] = comb.mux [[S0]], [[B0]], [[MUX1]] : i4
  // CORE: hw.output [[MUX0]] : i4
  pmux "$1"(%a, %b, %s, %y) {sWidth = 2 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[8]>, !rtlil<val[2]>, !rtlil<val[4]>
}
