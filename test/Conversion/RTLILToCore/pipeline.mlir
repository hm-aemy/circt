// RUN: circt-opt %s --pass-pipeline='builtin.module(convert-rtlil-to-core, hw.module(cse, llhd-combine-drives, llhd-sig2reg, cse, canonicalize))' | FileCheck %s
// RUN: circt-opt %s --pass-pipeline='builtin.module(convert-rtlil-to-core, hw.module(cse, llhd-combine-drives, llhd-sig2reg, cse, canonicalize))' | FileCheck %s --check-prefix=CORE

// The LLHD passes resolve every signal with a unique driver, leaving only Core
// ops.

// CORE-NOT: llhd.

// CHECK-LABEL: hw.module @andmod(in %a : i8, in %b : i8, in %c : i8, out o : i8, out p : i4)
rtlil.module @"\\andmod" {
  %c = wire "\\c" input port 3 : !rtlil<val[8]>
  %b = wire "\\b" input port 2 : !rtlil<val[8]>
  %a = wire "\\a" input port 1 : !rtlil<val[8]>
  %o = wire "\\o" output port 4 : !rtlil<val[8]>
  %p = wire "\\p" output port 5 : !rtlil<val[4]>
  %t1 = wire "$1" : !rtlil<val[8]>
  %t3 = wire "$3" : !rtlil<val[8]>
  // CHECK-DAG: [[AND:%.+]] = comb.and %a, %b, %c : i8
  and "$4"(%t1, %c, %t3) {opsSigned = 0 : i32, width = 8 : i32} : !rtlil<val[8]>, !rtlil<val[8]>, !rtlil<val[8]>
  and "$2"(%a, %b, %t1) {opsSigned = 0 : i32, width = 8 : i32} : !rtlil<val[8]>, !rtlil<val[8]>, !rtlil<val[8]>
  // CHECK-DAG: [[SLICE:%.+]] = comb.extract [[AND]] from 2 : (i8) -> i4
  %s = slice %t3 offset 2 : (!rtlil<val[8]>) -> !rtlil<val[4]>
  wconnection %o, %t3 : !rtlil<val[8]>
  wconnection %p, %s : !rtlil<val[4]>
  // CHECK: hw.output [[AND]], [[SLICE]] : i8, i4
}

// CHECK-LABEL: hw.module @constmod(in %a : i2, out o : i4)
rtlil.module @"\\constmod" {
  %a = wire "\\a" input port 1 : !rtlil<val[2]>
  %o = wire "\\o" output port 2 : !rtlil<val[4]>
  // CHECK-DAG: [[C:%.+]] = hw.constant -2 : i2
  %c = const <"10"> : !rtlil<val[2]>
  // CHECK-DAG: [[CAT:%.+]] = comb.concat [[C]], %a : i2, i2
  %cat = concat %a, %c : (!rtlil<val[2]>, !rtlil<val[2]>) -> !rtlil<val[4]>
  and "$1"(%cat, %cat, %o) {opsSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[4]>
  // CHECK: hw.output [[CAT]] : i4
}

// A wire nobody drives nor uses is dropped.
// CHECK-LABEL: hw.module @splitmod(in %a : i4, in %b : i4, out lo : i2, out hi : i2)
rtlil.module @"\\splitmod" {
  %a = wire "\\a" input port 1 : !rtlil<val[4]>
  %b = wire "\\b" input port 2 : !rtlil<val[4]>
  %lo = wire "\\lo" output port 3 : !rtlil<val[2]>
  %hi = wire "\\hi" output port 4 : !rtlil<val[2]>
  %unused = wire "\\unused" : !rtlil<val[1]>
  %y = concat %lo, %hi : (!rtlil<val[2]>, !rtlil<val[2]>) -> !rtlil<val[4]>
  // CHECK-DAG: [[AND:%.+]] = comb.and %a, %b : i4
  // CHECK-DAG: [[LO:%.+]] = comb.extract [[AND]] from 0 : (i4) -> i2
  // CHECK-DAG: [[HI:%.+]] = comb.extract [[AND]] from 2 : (i4) -> i2
  and "$1"(%a, %b, %y) {opsSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[4]>
  // CHECK: hw.output [[LO]], [[HI]] : i2, i2
}

// Drives to slices covering all bits of a wire are merged.
// CHECK-LABEL: hw.module @partial(in %a : i2, in %b : i2, out o : i4)
rtlil.module @"\\partial" {
  %a = wire "\\a" input port 1 : !rtlil<val[2]>
  %b = wire "\\b" input port 2 : !rtlil<val[2]>
  %o = wire "\\o" output port 3 : !rtlil<val[4]>
  %lo = slice %o offset 0 : (!rtlil<val[4]>) -> !rtlil<val[2]>
  %hi = slice %o offset 2 : (!rtlil<val[4]>) -> !rtlil<val[2]>
  wconnection %hi, %b : !rtlil<val[2]>
  wconnection %lo, %a : !rtlil<val[2]>
  // CHECK: [[CAT:%.+]] = comb.concat %b, %a : i2, i2
  // CHECK: hw.output [[CAT]] : i4
}

// CHECK-LABEL: hw.module @unary(in %a : i4, out not : i4, out neg : i4, out pos : i4)
rtlil.module @"\\unary" {
  %a = wire "\\a" input port 1 : !rtlil<val[4]>
  %not = wire "\\not" output port 2 : !rtlil<val[4]>
  %neg = wire "\\neg" output port 3 : !rtlil<val[4]>
  %pos = wire "\\pos" output port 4 : !rtlil<val[4]>
  // CHECK-DAG: [[ONES:%.+]] = hw.constant -1 : i4
  // CHECK-DAG: [[NOT:%.+]] = comb.xor %a, [[ONES]] : i4
  not "$1"(%a, %not) {aSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>
  // CHECK-DAG: [[ZERO:%.+]] = hw.constant 0 : i4
  // CHECK-DAG: [[NEG:%.+]] = comb.sub [[ZERO]], %a : i4
  neg "$2"(%a, %neg) {aSigned = 1 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>
  pos "$3"(%a, %pos) {aSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>
  // CHECK: hw.output [[NOT]], [[NEG]], %a : i4, i4, i4
}

// CHECK-LABEL: hw.module @logic(in %a : i4, in %b : i4, out and : i1, out or : i1, out not : i1)
rtlil.module @"\\logic" {
  %a = wire "\\a" input port 1 : !rtlil<val[4]>
  %b = wire "\\b" input port 2 : !rtlil<val[4]>
  %and = wire "\\and" output port 3 : !rtlil<val[1]>
  %or = wire "\\or" output port 4 : !rtlil<val[1]>
  %not = wire "\\not" output port 5 : !rtlil<val[1]>
  // CHECK-DAG: [[ZERO:%.+]] = hw.constant 0 : i4
  // CHECK-DAG: [[A:%.+]] = comb.icmp ne %a, [[ZERO]] : i4
  // CHECK-DAG: [[B:%.+]] = comb.icmp ne %b, [[ZERO]] : i4
  // CHECK-DAG: [[AND:%.+]] = comb.and [[A]], [[B]] : i1
  // CHECK-DAG: [[OR:%.+]] = comb.or [[A]], [[B]] : i1
  // CHECK-DAG: [[NOT:%.+]] = comb.icmp eq %a, [[ZERO]] : i4
  logic_and "$1"(%a, %b, %and) {opsSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[1]>
  logic_or "$2"(%a, %b, %or) {opsSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[1]>
  logic_not "$3"(%a, %not) {aSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[1]>
  // CHECK: hw.output [[AND]], [[OR]], [[NOT]] : i1, i1, i1
}
