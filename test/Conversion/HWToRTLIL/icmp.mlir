// RUN: circt-opt %s --convert-hw-to-rtlil | FileCheck %s

// CHECK-LABEL: rtlil.module @"\\cmpmod"
hw.module @cmpmod(in %x: i32) {
  // CHECK-DAG: [[X:%[0-9]+]] = "rtlil.wire"() {{.*}}name = "\\x"{{.*}}port_id = 1 {{.*}}port_input = true{{.*}}port_output = false
  // CHECK-DAG: [[CONST500:%[0-9]+]] = const <"00000000000000000000000111110100"> : [32]
  // CHECK-DAG: [[CONST700:%[0-9]+]] = const <"00000000000000000000001010111100"> : [32]
  %1 = hw.constant 500 : i32
  %2 = hw.constant 700 : i32
  // CHECK-DAG: eq "{{[^"]*}}"([[X]], [[CONST700]], [[RES:%[0-9]+]]){{.*}}width = 32
  // CHECK-DAG: [[RES]] = "rtlil.wire"(){{.*}}: () -> !rtlil<val[1]>
  %res1 = comb.icmp bin eq %x, %2 : i32
  %res2 = comb.icmp bin slt %1, %x : i32
  // CHECK-DAG: lt "{{[^"]*}}"([[CONST500]], [[X]], [[RES2:%[0-9]+]]){{.*}}opsSigned = 1 : i32{{.*}}width = 32
  // CHECK-DAG: [[RES2]] = "rtlil.wire"(){{.*}}: () -> !rtlil<val[1]>
}

// Every predicate that has a cell, without `bin`: the cells are the four-state
// Verilog operators, so they already propagate x.
// CHECK-LABEL: rtlil.module @"\\fourstate"
hw.module @fourstate(in %x: i32, in %y: i32,
                     out eq: i1, out ne: i1,
                     out ult: i1, out ule: i1, out ugt: i1, out uge: i1,
                     out slt: i1, out sle: i1, out sgt: i1, out sge: i1) {
  // CHECK-DAG: eq "{{[^"]*}}"({{.*}}opsSigned = 0 : i32{{.*}}width = 32
  %0 = comb.icmp eq %x, %y : i32
  // CHECK-DAG: ne "{{[^"]*}}"({{.*}}opsSigned = 0 : i32{{.*}}width = 32
  %1 = comb.icmp ne %x, %y : i32
  // CHECK-DAG: lt "{{[^"]*}}"({{.*}}opsSigned = 0 : i32{{.*}}width = 32
  %2 = comb.icmp ult %x, %y : i32
  // CHECK-DAG: le "{{[^"]*}}"({{.*}}opsSigned = 0 : i32{{.*}}width = 32
  %3 = comb.icmp ule %x, %y : i32
  // CHECK-DAG: gt "{{[^"]*}}"({{.*}}opsSigned = 0 : i32{{.*}}width = 32
  %4 = comb.icmp ugt %x, %y : i32
  // CHECK-DAG: ge "{{[^"]*}}"({{.*}}opsSigned = 0 : i32{{.*}}width = 32
  %5 = comb.icmp uge %x, %y : i32
  // CHECK-DAG: lt "{{[^"]*}}"({{.*}}opsSigned = 1 : i32{{.*}}width = 32
  %6 = comb.icmp slt %x, %y : i32
  // CHECK-DAG: le "{{[^"]*}}"({{.*}}opsSigned = 1 : i32{{.*}}width = 32
  %7 = comb.icmp sle %x, %y : i32
  // CHECK-DAG: gt "{{[^"]*}}"({{.*}}opsSigned = 1 : i32{{.*}}width = 32
  %8 = comb.icmp sgt %x, %y : i32
  // CHECK-DAG: ge "{{[^"]*}}"({{.*}}opsSigned = 1 : i32{{.*}}width = 32
  %9 = comb.icmp sge %x, %y : i32
  hw.output %0, %1, %2, %3, %4, %5, %6, %7, %8, %9
    : i1, i1, i1, i1, i1, i1, i1, i1, i1, i1
}

// The case comparisons `===` and `!==` keep their own cells, which return a
// defined 0 or 1 rather than propagating x. `bin` does not change the cell.
// CHECK-LABEL: rtlil.module @"\\casecmp"
hw.module @casecmp(in %x: i32, in %y: i32,
                   out ceq: i1, out cne: i1, out bceq: i1, out bcne: i1) {
  // CHECK-DAG: eqx "{{[^"]*}}"({{.*}}opsSigned = 0 : i32{{.*}}width = 32
  %0 = comb.icmp ceq %x, %y : i32
  // CHECK-DAG: nex "{{[^"]*}}"({{.*}}opsSigned = 0 : i32{{.*}}width = 32
  %1 = comb.icmp cne %x, %y : i32
  // CHECK-DAG: eqx "{{[^"]*}}"({{.*}}width = 32
  %2 = comb.icmp bin ceq %x, %y : i32
  // CHECK-DAG: nex "{{[^"]*}}"({{.*}}width = 32
  %3 = comb.icmp bin cne %x, %y : i32
  hw.output %0, %1, %2, %3 : i1, i1, i1, i1
}
