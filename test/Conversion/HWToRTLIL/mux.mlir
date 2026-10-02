// RUN: circt-opt %s --convert-hw-to-rtlil | FileCheck %s

// CHECK-LABEL: rtlil.module @"\\muxmod"
hw.module @muxmod(in %select : i1) {
  // CHECK-DAG: [[SELECT:%[0-9]+]] = "rtlil.wire"() {{.*}}port_id = 1 {{.*}}port_input = true{{.*}}port_output = false
  // CHECK-DAG: [[CONST500:%[0-9]+]] = const <"00000000000000000000000111110100"> : [32]
  // CHECK-DAG: [[CONST700:%[0-9]+]] = const <"00000000000000000000001010111100"> : [32]
  %1 = hw.constant 500 : i32
  %2 = hw.constant 700 : i32
  // CHECK-DAG: "rtlil.mux"([[CONST700]], [[CONST500]], [[SELECT]], [[RES:%[0-9+]]]){{.*}}ports = ["\\A", "\\B", "\\S", "\\Y"]{{.*}}type = "$mux"{{.*}}width = 32
  // CHECK-DAG: [[RES]] = "rtlil.wire"(){{.*}}: () -> !rtlil<val[32]>
  %res1 = comb.mux bin %select, %1, %2 : i32
}
