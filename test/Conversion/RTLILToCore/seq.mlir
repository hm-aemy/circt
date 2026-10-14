// RUN: circt-opt %s --convert-rtlil-to-core | FileCheck %s
// RUN: circt-opt %s --pass-pipeline='builtin.module(convert-rtlil-to-core, hw.module(cse, llhd-combine-drives, llhd-sig2reg, cse, canonicalize))' | FileCheck %s --check-prefix=CORE

// CORE-NOT: llhd.

// A `$dff` becomes a `seq.compreg` clocked by its probed CLK. A public cell
// name becomes the register's name and inner symbol.
// CHECK-LABEL: hw.module @dffmod(in %clk : i1, in %d : i8, out q : i8, out r : i8)
// CORE-LABEL: hw.module @dffmod(in %clk : i1, in %d : i8, out q : i8, out r : i8)
rtlil.module @"\\dffmod" {
  // CHECK-DAG: [[CLK:%.+]] = llhd.sig name "clk" : <i1>
  // CHECK-DAG: [[D:%.+]] = llhd.sig name "d" : <i8>
  %clk = wire "\\clk" input port 1 : !rtlil<val[1]>
  %d = wire "\\d" input port 2 : !rtlil<val[8]>
  // CHECK-DAG: %q = llhd.sig : <i8>
  // CHECK-DAG: %r = llhd.sig : <i8>
  %q = wire "\\q" output port 3 : !rtlil<val[8]>
  %r = wire "\\r" output port 4 : !rtlil<val[8]>
  // CHECK-DAG: [[CLK_PRB:%.+]] = llhd.prb [[CLK]] : i1
  // CHECK-DAG: [[CLOCK:%.+]] = seq.to_clock [[CLK_PRB]]
  // CHECK-DAG: [[D_PRB:%.+]] = llhd.prb [[D]] : i8
  // CHECK-DAG: [[REG:%.+]] = seq.compreg sym @myreg [[D_PRB]], [[CLOCK]] : i8
  // CHECK-DAG: llhd.drv %q, [[REG]] after {{%.+}} : i8
  // CORE-DAG: [[CLOCK:%.+]] = seq.to_clock %clk
  // CORE-DAG: %myreg = seq.compreg sym @myreg %d, [[CLOCK]] : i8
  dff "\\myreg"(%clk, %d, %q) {width = 8 : i32} : !rtlil<val[1]>, !rtlil<val[8]>, !rtlil<val[8]>
  // Generated names stay anonymous.
  // CHECK-DAG: [[ANON:%.+]] = seq.compreg {{%.+}}, {{%.+}} : i8
  // CHECK-DAG: llhd.drv %r, [[ANON]] after {{%.+}} : i8
  // CORE-DAG: [[ANON:%.+]] = seq.compreg %myreg, [[CLOCK]] : i8
  dff "$1"(%clk, %q, %r) {width = 8 : i32} : !rtlil<val[1]>, !rtlil<val[8]>, !rtlil<val[8]>
  // CORE: hw.output %myreg, [[ANON]] : i8, i8
}
