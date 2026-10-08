// RUN: circt-opt %s --convert-hw-to-rtlil | FileCheck %s

// An RTLIL cell takes exactly two inputs, so the n-ary form that
// canonicalization produces folds back into a left-leaning chain of cells with
// a wire between each pair.
// CHECK-LABEL: rtlil.module @"\\narymod"
hw.module @narymod(in %a: i8, in %b: i8, in %c: i8, out o: i8) {
  // CHECK-DAG: [[A:%[0-9]+]] = wire "\\a"
  // CHECK-DAG: [[B:%[0-9]+]] = wire "\\b"
  // CHECK-DAG: [[C:%[0-9]+]] = wire "\\c"
  // CHECK-DAG: [[O:%[0-9]+]] = wire "\\o"
  // CHECK-DAG: xor "{{[^"]*}}"([[A]], [[B]], [[T:%[0-9]+]]) {{.*}}width = 8
  // CHECK-DAG: xor "{{[^"]*}}"([[T]], [[C]], [[RES:%[0-9]+]]) {{.*}}width = 8
  // CHECK-DAG: wconnection [[O]], [[RES]] :
  %0 = comb.xor %a, %b, %c : i8
  hw.output %0 : i8
}

// CHECK-LABEL: rtlil.module @"\\arithmod"
hw.module @arithmod(in %a: i8, in %b: i8, out o0: i8, out o1: i8, out o2: i8, out o3: i8, out o4: i8) {
  // CHECK-DAG: mul "{{[^"]*}}"({{.*}}) {{.*}}opsSigned = 0 : i32{{.*}}width = 8
  %0 = comb.mul %a, %b : i8
  // The signed and unsigned pairs share a cell type and differ only in
  // `A_SIGNED`/`B_SIGNED`, which `opsSigned` stands for.
  // CHECK-DAG: div "{{[^"]*}}"({{.*}}) {{.*}}opsSigned = 0 : i32
  %1 = comb.divu %a, %b : i8
  // CHECK-DAG: div "{{[^"]*}}"({{.*}}) {{.*}}opsSigned = 1 : i32
  %2 = comb.divs %a, %b : i8
  // CHECK-DAG: mod "{{[^"]*}}"({{.*}}) {{.*}}opsSigned = 0 : i32
  %3 = comb.modu %a, %b : i8
  // CHECK-DAG: mod "{{[^"]*}}"({{.*}}) {{.*}}opsSigned = 1 : i32
  %4 = comb.mods %a, %b : i8
  hw.output %0, %1, %2, %3, %4 : i8, i8, i8, i8, i8
}

// The shift cells read B as unsigned, so only `A_SIGNED` varies. An arithmetic
// right shift is `$sshr` with it set, not `$shr`.
// CHECK-LABEL: rtlil.module @"\\shiftmod"
hw.module @shiftmod(in %a: i8, in %b: i8, out o0: i8, out o1: i8, out o2: i8) {
  // CHECK-DAG: shl "{{[^"]*}}"({{.*}}) {aSigned = 0 : i32{{.*}}width = 8
  %0 = comb.shl %a, %b : i8
  // CHECK-DAG: shr "{{[^"]*}}"({{.*}}) {aSigned = 0 : i32
  %1 = comb.shru %a, %b : i8
  // CHECK-DAG: sshr "{{[^"]*}}"({{.*}}) {aSigned = 1 : i32
  %2 = comb.shrs %a, %b : i8
  hw.output %0, %1, %2 : i8, i8, i8
}

// Parity is the XOR reduction cell: A is the whole input, Y is one bit.
// CHECK-LABEL: rtlil.module @"\\paritymod"
hw.module @paritymod(in %a: i8, out o: i1) {
  // CHECK-DAG: [[A:%[0-9]+]] = wire "\\a"
  // CHECK-DAG: reduce_xor "{{[^"]*}}"([[A]], [[RES:%[0-9]+]]) {aSigned = 0 : i32{{.*}}width = 8
  // CHECK-DAG: [[RES]] = wire "{{[^"]*}}" : [1]
  %0 = comb.parity %a : i8
  hw.output %0 : i1
}

// No cell for either of these: the copies and the reversed bits are chunks of
// one SigSpec.
// CHECK-LABEL: rtlil.module @"\\widthmod"
hw.module @widthmod(in %a: i2, out o0: i6, out o1: i2) {
  // CHECK-DAG: [[A:%[0-9]+]] = wire "\\a"
  // CHECK-DAG: concat [[A]], [[A]], [[A]] : (!rtlil<val[2]>, !rtlil<val[2]>, !rtlil<val[2]>) -> !rtlil<val[6]>
  %0 = comb.replicate %a : (i2) -> i6
  // CHECK-DAG: [[HI:%[0-9]+]] = slice [[A]] offset 1 : (!rtlil<val[2]>) -> !rtlil<val[1]>
  // CHECK-DAG: [[LO:%[0-9]+]] = slice [[A]] offset 0 : (!rtlil<val[2]>) -> !rtlil<val[1]>
  // CHECK-DAG: concat [[HI]], [[LO]] : (!rtlil<val[1]>, !rtlil<val[1]>) -> !rtlil<val[2]>
  %1 = comb.reverse %a : i2
  hw.output %0, %1 : i6, i2
}

// A replicate of one is the value itself.
// CHECK-LABEL: rtlil.module @"\\singlemod"
hw.module @singlemod(in %a: i4, out o: i4) {
  // CHECK-DAG: [[A:%[0-9]+]] = wire "\\a"
  // CHECK-DAG: [[O:%[0-9]+]] = wire "\\o"
  // CHECK-NOT: concat
  // CHECK-DAG: wconnection [[O]], [[A]] :
  %0 = comb.replicate %a : (i4) -> i4
  hw.output %0 : i4
}
