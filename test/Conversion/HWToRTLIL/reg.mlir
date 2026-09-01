// RUN: circt-opt %s --convert-hw-to-rtlil | FileCheck %s

// CHECK-LABEL: rtlil.module @"\\regmod"
hw.module @regmod(in %clk : !seq.clock, in %reset : i1) {
  // CHECK-DAG: [[CLK:%[0-9]+]] = "rtlil.wire"() {{.*}}port_id = 1 {{.*}}port_input = true{{.*}}port_output = false
  // CHECK-DAG: [[RESET:%[0-9]+]] = "rtlil.wire"() {{.*}}port_id = 2 {{.*}}port_input = true{{.*}}port_output = false
  // CHECK-DAG: [[CONST500:%[0-9]+]] = const "00000000000000000000000111110100" : [32]
  // CHECK-DAG: [[CONST700:%[0-9]+]] = const "00000000000000000000001010111100" : [32]
  %1 = hw.constant 500 : i32
  %2 = hw.constant 700 : i32
  // CHECK-DAG: "rtlil.dff"([[CLK]], [[CONST500]], [[RES:%[0-9]+]]){{.*}}name = "\\inner_symbol"{{.*}}ports = ["\\CLK", "\\D", "\\Q"]{{.*}}type = "$dff"{{.*}}width = 32
  // CHECK-DAG: [[RES]] = "rtlil.wire"(){{.*}}: () -> !rtlil<val[32]>
  %res1 = seq.compreg sym @inner_symbol %1, %clk: i32
  // A `seq.compreg` reset is sampled with the clock, matching the synchronous
  // reset `SeqToSV` lowers the same op to: a mux picking the reset value in
  // front of a plain flop.
  // CHECK-DAG: "rtlil.mux"([[CONST500]], [[CONST700]], [[RESET]], [[MUX2:%[0-9]+]]){{.*}}ports = ["\\A", "\\B", "\\S", "\\Y"]{{.*}}type = "$mux"{{.*}}width = 32
  // CHECK-DAG: "rtlil.dff"([[CLK]], [[MUX2]], [[RES2:%[0-9]+]]){{.*}}ports = ["\\CLK", "\\D", "\\Q"]{{.*}}type = "$dff"{{.*}}width = 32
  // CHECK-DAG: [[RES2]] = "rtlil.wire"(){{.*}}: () -> !rtlil<val[32]>
  // CHECK-DAG: [[MUX2]] = "rtlil.wire"(){{.*}}: () -> !rtlil<val[32]>
  %res2 = seq.compreg %1, %clk reset %reset, %2: i32
  // A distinct inner symbol: two registers sharing one would collide in the
  // module's single wire/cell namespace, which is now a verifier error rather
  // than something a uniquing suffix papers over.
  // CHECK-DAG: "rtlil.dff"([[CLK]], [[CONST500]], [[RES3:%[0-9]+]]){{.*}}name = "\\inner_symbol2"{{.*}}ports = ["\\CLK", "\\D", "\\Q"]{{.*}}type = "$dff"{{.*}}width = 32
  // CHECK-DAG: [[RES3]] = "rtlil.wire"(){{.*}}: () -> !rtlil<val[32]>
  %res3 = seq.firreg %1 clock %clk sym @inner_symbol2 : i32
  // CHECK-DAG: "rtlil.aldff"([[CLK]], [[CONST500]], [[RESET]], [[CONST700]], [[RES5:%[0-9]+]]){{.*}}ports = ["\\CLK", "\\D", "\\ALOAD", "\\AD", "\\Q"]{{.*}}type = "$aldff"{{.*}}width = 32
  // CHECK-DAG: [[RES5]] = "rtlil.wire"(){{.*}}: () -> !rtlil<val[32]>
  %res5 = seq.firreg %1 clock %clk reset async %reset, %2 : i32


  // A synchronous `seq.firreg` reset is a mux in front of the flop, so the
  // reset value is sampled at the clock edge like any other input.
  // CHECK-DAG: "rtlil.mux"([[CONST500]], [[CONST700]], [[RESET]], [[MUX4:%[0-9]+]]){{.*}}ports = ["\\A", "\\B", "\\S", "\\Y"]{{.*}}type = "$mux"{{.*}}width = 32
  // CHECK-DAG: "rtlil.dff"([[CLK]], [[MUX4]], [[RES4:%[0-9]+]]){{.*}}ports = ["\\CLK", "\\D", "\\Q"]{{.*}}type = "$dff"{{.*}}width = 32
  // CHECK-DAG: [[RES4]] = "rtlil.wire"(){{.*}}: () -> !rtlil<val[32]>
  // CHECK-DAG: [[MUX4]] = "rtlil.wire"(){{.*}}: () -> !rtlil<val[32]>
  %res4 = seq.firreg %1 clock %clk reset sync %reset, %2 : i32
}
