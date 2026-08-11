// RUN: circt-opt %s --convert-hw-to-rtlil | FileCheck %s

// CHECK-LABEL: rtlil.module @"\\ormod"
hw.module @ormod(in %x: i32, in %y: i32, out res1: i32, out res2: i32) {
  // CHECK-DAG: [[X:%[0-9]+]] = "rtlil.wire"() {{.*}}name = "\\x"{{.*}}port_id = 1 {{.*}}port_input = true{{.*}}port_output = false
  // CHECK-DAG: [[Y:%[0-9]+]] = "rtlil.wire"() {{.*}}name = "\\y"{{.*}}port_id = 2 {{.*}}port_input = true{{.*}}port_output = false
  // CHECK-DAG: [[RES1:%[0-9]+]] = "rtlil.wire"() {{.*}}name = "\\res1"{{.*}}port_id = 3 {{.*}}port_input = false{{.*}}port_output = true
  // CHECK-DAG: [[RES2:%[0-9]+]] = "rtlil.wire"() {{.*}}name = "\\res2"{{.*}}port_id = 4 {{.*}}port_input = false{{.*}}port_output = true
  // CHECK-DAG: [[CONST500:%[0-9]+]] = "rtlil.const"() {{.*}}value = [0 : i8, 0 : i8, 1 : i8, 0 : i8, 1 : i8, 1 : i8, 1 : i8, 1 : i8, 1 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8]{{.*}} : () -> !rtlil<val[32 : i32]>
  // CHECK-DAG: [[CONST700:%[0-9]+]] = "rtlil.const"() {{.*}}value = [0 : i8, 0 : i8, 1 : i8, 1 : i8, 1 : i8, 1 : i8, 0 : i8, 1 : i8, 0 : i8, 1 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8, 0 : i8]{{.*}} : () -> !rtlil<val[32 : i32]>
  %1 = hw.constant 500 : i32
  %2 = hw.constant 700 : i32
  // The cells drive internal wires; the output ports are driven from those by a
  // connection.
  // CHECK-DAG: "rtlil.or"([[X]], [[CONST500]], [[OR1:%[0-9]+]]) {{.*}}
  // CHECK-DAG: "rtlil.or"([[Y]], [[CONST700]], [[OR2:%[0-9]+]]) {{.*}}
  %res1 = comb.or %x, %1 : i32
  %res2 = comb.or %y, %2 : i32
  // CHECK-DAG: "rtlil.wconnection"([[RES1]], [[OR1]])
  // CHECK-DAG: "rtlil.wconnection"([[RES2]], [[OR2]])
  hw.output %res1, %res2 : i32, i32
}
