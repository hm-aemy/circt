// RUN: circt-opt %s --convert-hw-to-rtlil | FileCheck %s

// Each hw.module becomes an rtlil.module: a symbol-carrying op with a graph
// region, not a nested builtin.module. Names carry RTLIL's sigil and nothing
// else -- no uniquing suffix -- so a design that has been through a Yosys
// script still maps back onto the ops it came from.
// CHECK-LABEL: rtlil.module @"\\test"
hw.module @test(in %arg0: i32, in %arg1: i32, in %arg2: i32, in %arg3: i32, out out0: i32, out out1: i32) {
  // CHECK-DAG: %[[OP1:.+]] = "rtlil.wire"() <{{.*}}name = "\\arg0"{{.*}}port_id = 1 {{.*}}port_input = true{{.*}}> : () -> !rtlil<val[32]>
  // CHECK-DAG: %[[OP2:.+]] = "rtlil.wire"() <{{.*}}name = "\\arg1"{{.*}}port_id = 2 {{.*}}port_input = true{{.*}}> : () -> !rtlil<val[32]>
  // CHECK-DAG: %[[OP3:.+]] = "rtlil.wire"() <{{.*}}name = "\\arg2"{{.*}}port_id = 3 {{.*}}port_input = true{{.*}}> : () -> !rtlil<val[32]>
  // CHECK-DAG: %[[OP4:.+]] = "rtlil.wire"() <{{.*}}name = "\\arg3"{{.*}}port_id = 4 {{.*}}port_input = true{{.*}}> : () -> !rtlil<val[32]>

  // Output ports are their own wires, driven by a connection. The cell result
  // wire is not relabelled into the port: it may be a constant, an input port,
  // or shared between two outputs.
  // CHECK-DAG: %[[OUT0:.+]] = "rtlil.wire"() <{{.*}}name = "\\out0"{{.*}}port_id = 5 {{.*}}port_output = true{{.*}}> : () -> !rtlil<val[32]>
  // CHECK-DAG: %[[OUT1:.+]] = "rtlil.wire"() <{{.*}}name = "\\out1"{{.*}}port_id = 6 {{.*}}port_output = true{{.*}}> : () -> !rtlil<val[32]>

  // CHECK-DAG: "rtlil.and"(%[[OP1]], %[[OP2]], %[[RES1:.+]]) <{{.*name = "\$[0-9]+".*type = "\$and".*}}> : (!rtlil<val[32]>, !rtlil<val[32]>, !rtlil<val[32]>) -> ()
  // CHECK-DAG: "rtlil.and"(%[[OP3]], %[[OP4]], %[[RES2:.+]]) <{{.*name = "\$[0-9]+".*type = "\$and".*}}> : (!rtlil<val[32]>, !rtlil<val[32]>, !rtlil<val[32]>) -> ()
  %0 = comb.and %arg0, %arg1 : i32
  %1 = comb.and %arg2, %arg3 : i32
  // CHECK-DAG: "rtlil.wconnection"(%[[OUT0]], %[[RES1]])
  // CHECK-DAG: "rtlil.wconnection"(%[[OUT1]], %[[RES2]])
  hw.output %0, %1 : i32, i32
}
