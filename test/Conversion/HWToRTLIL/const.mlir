// RUN: circt-opt %s --convert-hw-to-rtlil | FileCheck %s

// CHECK-LABEL: rtlil.module @"\\ormod"
hw.module @ormod(in %x: i32, in %y: i32, out res1: i32, out res2: i32) {
  // CHECK-DAG: [[X:%[0-9]+]] = wire "\\x" input port 1 :
  // CHECK-DAG: [[Y:%[0-9]+]] = wire "\\y" input port 2 :
  // CHECK-DAG: [[RES1:%[0-9]+]] = wire "\\res1" output port 3
  // CHECK-DAG: [[RES2:%[0-9]+]] = wire "\\res2" output port 4
  // CHECK-DAG: [[CONST500:%[0-9]+]] = const <"00000000000000000000000111110100"> : [32]
  // CHECK-DAG: [[CONST700:%[0-9]+]] = const <"00000000000000000000001010111100"> : [32]
  %1 = hw.constant 500 : i32
  %2 = hw.constant 700 : i32
  // The cells drive internal wires; the output ports are driven from those by a
  // connection.
  // CHECK-DAG: or "{{[^"]*}}"([[X]], [[CONST500]], [[OR1:%[0-9]+]])
  // CHECK-DAG: or "{{[^"]*}}"([[Y]], [[CONST700]], [[OR2:%[0-9]+]])
  %res1 = comb.or %x, %1 : i32
  %res2 = comb.or %y, %2 : i32
  // CHECK-DAG: wconnection [[RES1]], [[OR1]] :
  // CHECK-DAG: wconnection [[RES2]], [[OR2]] :
  hw.output %res1, %res2 : i32, i32
}
