// RUN: circt-opt %s --convert-hw-to-rtlil | FileCheck %s

// CHECK: rtlil.module [[ORMOD:@"\\\\ormod"]]
hw.module @ormod(in %x: i32, in %y: i32, out res1: i32, out res2: i32) {
  // CHECK-DAG: "rtlil.wire"() {{.*}}name = "\\x"{{.*}}port_id = 1 {{.*}}port_input = true{{.*}}port_output = false
  // CHECK-DAG: "rtlil.wire"() {{.*}}name = "\\y"{{.*}}port_id = 2 {{.*}}port_input = true{{.*}}port_output = false
  // CHECK-DAG: "rtlil.wire"() {{.*}}name = "\\res1"{{.*}}port_id = 3 {{.*}}port_input = false{{.*}}port_output = true
  // CHECK-DAG: "rtlil.wire"() {{.*}}name = "\\res2"{{.*}}port_id = 4 {{.*}}port_input = false{{.*}}port_output = true
  %1 = hw.constant 500 : i32
  %2 = hw.constant 700 : i32
  %res1 = comb.or %x, %1 : i32
  %res2 = comb.or %y, %2 : i32
  hw.output %res1, %res2 : i32, i32
}

// CHECK-LABEL: rtlil.module @"\\test"
hw.module @test(in %x : i32, in %y : i32) {
  // The caller spells the callee's port names directly rather than looking them
  // up: with no uniquing suffix, `\` + the port name is the callee's wire name.
  //
  // The two result wires must also get distinct auto-generated names -- wires
  // and cells share one namespace, and a duplicate makes Yosys abort.
  // CHECK-DAG: %[[R1:.+]] = "rtlil.wire"() <{{.*}}name = "$[[N1:[0-9]+]]"{{.*}}
  // CHECK-DAG: %[[R2:.+]] = "rtlil.wire"() <{{.*}}name = "$[[N2:[0-9]+]]"{{.*}}
  // CHECK-DAG: "rtlil.instance"(%[[X:.+]], %[[Y:.+]], %[[R1]], %[[R2]]) <{{{.*}}name = "\\instance1"{{.*}}ports = ["\\x", "\\y", "\\res1", "\\res2"]{{.*}}type = [[ORMOD]]}> : (!rtlil<val[32 : i32]>, !rtlil<val[32 : i32]>, !rtlil<val[32 : i32]>, !rtlil<val[32 : i32]>) -> ()
  %1, %2 = hw.instance "instance1" @ormod(x: %x : i32, y: %y : i32) -> (res1: i32, res2: i32)
}
