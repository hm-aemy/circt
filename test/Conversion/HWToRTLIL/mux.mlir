// RUN: circt-opt %s --convert-hw-to-rtlil | FileCheck %s

// CHECK-LABEL: rtlil.module @"\\muxmod"
hw.module @muxmod(in %select : i1) {
  // CHECK-DAG: [[SELECT:%[0-9]+]] = wire "{{[^"]*}}" input port 1 :
  // CHECK-DAG: [[CONST500:%[0-9]+]] = const <"00000000000000000000000111110100"> : !rtlil<val[32]>
  // CHECK-DAG: [[CONST700:%[0-9]+]] = const <"00000000000000000000001010111100"> : !rtlil<val[32]>
  %1 = hw.constant 500 : i32
  %2 = hw.constant 700 : i32
  // CHECK-DAG: mux "{{[^"]*}}"([[CONST700]], [[CONST500]], [[SELECT]], [[RES:%[0-9+]]]){{.*}}width = 32
  // CHECK-DAG: [[RES]] = wire "{{[^"]*}}" : [32]
  %res1 = comb.mux bin %select, %1, %2 : i32
}
