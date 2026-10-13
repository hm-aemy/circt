// RUN: circt-opt %s --pass-pipeline='builtin.module(convert-rtlil-to-core, hw.module(cse, llhd-combine-drives, llhd-sig2reg, cse, canonicalize))' | FileCheck %s

// Wires without a unique driver are legal RTLIL. They convert, and remain as
// signals after `llhd-sig2reg`.

// CHECK-LABEL: hw.module @partialundriven
rtlil.module @"\\partialundriven" {
  %a = wire "\\a" input port 1 : !rtlil<val[2]>
  %o = wire "\\o" output port 2 : !rtlil<val[4]>
  // CHECK: %o = llhd.sig : <i4>
  // CHECK: [[LO:%.+]] = llhd.sig.extract %o
  // CHECK: llhd.drv [[LO]], %a
  %lo = slice %o offset 0 : (!rtlil<val[4]>) -> !rtlil<val[2]>
  wconnection %lo, %a : !rtlil<val[2]>
}

// CHECK-LABEL: hw.module @undriven
rtlil.module @"\\undriven" {
  %o = wire "\\o" output port 1 : !rtlil<val[1]>
  // CHECK: %t = llhd.sig : <i1>
  // CHECK: [[T:%.+]] = llhd.prb %t
  // CHECK: hw.output [[T]]
  %t = wire "\\t" : !rtlil<val[1]>
  wconnection %o, %t : !rtlil<val[1]>
}

// CHECK-LABEL: hw.module @multidriven
rtlil.module @"\\multidriven" {
  %a = wire "\\a" input port 1 : !rtlil<val[1]>
  %b = wire "\\b" input port 2 : !rtlil<val[1]>
  %o = wire "\\o" output port 3 : !rtlil<val[1]>
  // CHECK: %o = llhd.sig : <i1>
  // CHECK: llhd.drv %o, %a
  // CHECK: llhd.drv %o, %b
  wconnection %o, %a : !rtlil<val[1]>
  wconnection %o, %b : !rtlil<val[1]>
}

// CHECK-LABEL: hw.module @driveinput
rtlil.module @"\\driveinput" {
  %a = wire "\\a" input port 1 : !rtlil<val[1]>
  %b = wire "\\b" input port 2 : !rtlil<val[1]>
  %o = wire "\\o" output port 3 : !rtlil<val[1]>
  // CHECK: [[A:%.+]] = llhd.sig name "a" : <i1>
  // CHECK: llhd.drv [[A]], %a
  // CHECK: llhd.drv [[A]], %b
  wconnection %a, %b : !rtlil<val[1]>
  wconnection %o, %a : !rtlil<val[1]>
}
