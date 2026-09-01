// RUN: circt-opt %s --convert-hw-to-rtlil | FileCheck %s

// The operands come back in the opposite order.
// CHECK-LABEL: rtlil.module @"\\concatmod"
hw.module @concatmod(in %msb : i3, in %mid : i2, in %lsb : i1, out res : i6) {
  // CHECK-DAG: [[MSB:%[0-9]+]] = "rtlil.wire"() {{.*}}name = "\\msb"{{.*}}port_id = 1 {{.*}}port_input = true
  // CHECK-DAG: [[MID:%[0-9]+]] = "rtlil.wire"() {{.*}}name = "\\mid"{{.*}}port_id = 2 {{.*}}port_input = true
  // CHECK-DAG: [[LSB:%[0-9]+]] = "rtlil.wire"() {{.*}}name = "\\lsb"{{.*}}port_id = 3 {{.*}}port_input = true
  // CHECK-DAG: [[RES:%[0-9]+]] = "rtlil.wire"() {{.*}}name = "\\res"{{.*}}port_output = true
  // No cell and no intermediate wire.
  // CHECK-DAG: [[CAT:%[0-9]+]] = "rtlil.concat"([[LSB]], [[MID]], [[MSB]]) : (!rtlil<val[1]>, !rtlil<val[2]>, !rtlil<val[3]>) -> !rtlil<val[6]>
  // CHECK-DAG: "rtlil.wconnection"([[RES]], [[CAT]])
  %0 = comb.concat %msb, %mid, %lsb : i3, i2, i1
  hw.output %0 : i6
}

// `lowBit` carries straight over to `offset`.
// CHECK-LABEL: rtlil.module @"\\extractmod"
hw.module @extractmod(in %in : i8, out res : i3) {
  // CHECK-DAG: [[IN:%[0-9]+]] = "rtlil.wire"() {{.*}}name = "\\in"{{.*}}port_input = true
  // CHECK-DAG: [[RES:%[0-9]+]] = "rtlil.wire"() {{.*}}name = "\\res"{{.*}}port_output = true
  // CHECK-DAG: [[SLICE:%[0-9]+]] = "rtlil.slice"([[IN]]) <{offset = 3 : i32}> : (!rtlil<val[8]>) -> !rtlil<val[3]>
  // CHECK-DAG: "rtlil.wconnection"([[RES]], [[SLICE]])
  %0 = comb.extract %in from 3 : (i8) -> i3
  hw.output %0 : i3
}

// Slices stay values, so a cell takes them directly as operands.
// CHECK-LABEL: rtlil.module @"\\mixedmod"
hw.module @mixedmod(in %in : i8, out res : i4) {
  // CHECK-DAG: [[IN:%[0-9]+]] = "rtlil.wire"() {{.*}}name = "\\in"{{.*}}port_input = true
  // CHECK-DAG: [[HI:%[0-9]+]] = "rtlil.slice"([[IN]]) <{offset = 4 : i32}> : (!rtlil<val[8]>) -> !rtlil<val[4]>
  // CHECK-DAG: [[LO:%[0-9]+]] = "rtlil.slice"([[IN]]) <{offset = 0 : i32}> : (!rtlil<val[8]>) -> !rtlil<val[4]>
  // CHECK-DAG: "rtlil.and"([[HI]], [[LO]], [[AND:%[0-9]+]]){{.*}}width = 4
  %hi = comb.extract %in from 4 : (i8) -> i4
  %lo = comb.extract %in from 0 : (i8) -> i4
  %0 = comb.and %hi, %lo : i4
  hw.output %0 : i4
}

// A concat of two extracts of the same value.
// CHECK-LABEL: rtlil.module @"\\swapmod"
hw.module @swapmod(in %in : i8, out res : i8) {
  // CHECK-DAG: [[IN:%[0-9]+]] = "rtlil.wire"() {{.*}}name = "\\in"{{.*}}port_input = true
  // CHECK-DAG: [[HI:%[0-9]+]] = "rtlil.slice"([[IN]]) <{offset = 4 : i32}> : (!rtlil<val[8]>) -> !rtlil<val[4]>
  // CHECK-DAG: [[LO:%[0-9]+]] = "rtlil.slice"([[IN]]) <{offset = 0 : i32}> : (!rtlil<val[8]>) -> !rtlil<val[4]>
  // CHECK-DAG: "rtlil.concat"([[HI]], [[LO]]) : (!rtlil<val[4]>, !rtlil<val[4]>) -> !rtlil<val[8]>
  %hi = comb.extract %in from 4 : (i8) -> i4
  %lo = comb.extract %in from 0 : (i8) -> i4
  %0 = comb.concat %lo, %hi : i4, i4
  hw.output %0 : i8
}
