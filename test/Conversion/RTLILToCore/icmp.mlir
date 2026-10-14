// RUN: circt-opt %s --convert-rtlil-to-core | FileCheck %s

// Comparison cells become `comb.icmp`. `opsSigned` picks the signed predicate
// for the ordering cells. `$eqx`/`$nex` are the case equality `ceq`/`cne`.
// CHECK-LABEL: hw.module @cmp(
rtlil.module @"\\cmp" {
  %a = wire "\\a" input port 1 : !rtlil<val[4]>
  %b = wire "\\b" input port 2 : !rtlil<val[4]>
  %o = wire "\\o" output port 3 : !rtlil<val[1]>
  %t1 = wire "$t1" : !rtlil<val[1]>
  %t2 = wire "$t2" : !rtlil<val[1]>
  %t3 = wire "$t3" : !rtlil<val[1]>
  %t4 = wire "$t4" : !rtlil<val[1]>
  %t5 = wire "$t5" : !rtlil<val[1]>
  %t6 = wire "$t6" : !rtlil<val[1]>
  %t7 = wire "$t7" : !rtlil<val[1]>
  %t8 = wire "$t8" : !rtlil<val[1]>
  %t9 = wire "$t9" : !rtlil<val[1]>
  %t10 = wire "$t10" : !rtlil<val[1]>
  %t11 = wire "$t11" : !rtlil<val[1]>
  %t12 = wire "$t12" : !rtlil<val[1]>
  // CHECK-DAG: comb.icmp ult {{%.+}}, {{%.+}} : i4
  lt "$1"(%a, %b, %t1) {opsSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[1]>
  // CHECK-DAG: comb.icmp slt {{%.+}}, {{%.+}} : i4
  lt "$2"(%a, %b, %t2) {opsSigned = 1 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[1]>
  // CHECK-DAG: comb.icmp ule {{%.+}}, {{%.+}} : i4
  le "$3"(%a, %b, %t3) {opsSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[1]>
  // CHECK-DAG: comb.icmp sle {{%.+}}, {{%.+}} : i4
  le "$4"(%a, %b, %t4) {opsSigned = 1 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[1]>
  // CHECK-DAG: comb.icmp ugt {{%.+}}, {{%.+}} : i4
  gt "$5"(%a, %b, %t5) {opsSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[1]>
  // CHECK-DAG: comb.icmp sgt {{%.+}}, {{%.+}} : i4
  gt "$6"(%a, %b, %t6) {opsSigned = 1 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[1]>
  // CHECK-DAG: comb.icmp uge {{%.+}}, {{%.+}} : i4
  ge "$7"(%a, %b, %t7) {opsSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[1]>
  // CHECK-DAG: comb.icmp sge {{%.+}}, {{%.+}} : i4
  ge "$8"(%a, %b, %t8) {opsSigned = 1 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[1]>
  // CHECK-DAG: comb.icmp eq {{%.+}}, {{%.+}} : i4
  eq "$9"(%a, %b, %t9) {opsSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[1]>
  // CHECK-DAG: comb.icmp ne {{%.+}}, {{%.+}} : i4
  ne "$10"(%a, %b, %t10) {opsSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[1]>
  // CHECK-DAG: comb.icmp ceq {{%.+}}, {{%.+}} : i4
  eqx "$11"(%a, %b, %t11) {opsSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[1]>
  // CHECK-DAG: comb.icmp cne {{%.+}}, {{%.+}} : i4
  nex "$12"(%a, %b, %t12) {opsSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[1]>
  wconnection %o, %t1 : !rtlil<val[1]>
}
