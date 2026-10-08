// RUN: circt-opt %s --convert-hw-to-rtlil | FileCheck %s

// CHECK-LABEL: rtlil.module @"\\regmod"
hw.module @regmod(in %clk : !seq.clock, in %reset : i1) {
  // CHECK-DAG: [[CLK:%[0-9]+]] = wire "{{[^"]*}}" input port 1 :
  // CHECK-DAG: [[RESET:%[0-9]+]] = wire "{{[^"]*}}" input port 2 :
  // CHECK-DAG: [[CONST500:%[0-9]+]] = const <"00000000000000000000000111110100"> : !rtlil<val[32]>
  // CHECK-DAG: [[CONST700:%[0-9]+]] = const <"00000000000000000000001010111100"> : !rtlil<val[32]>
  %1 = hw.constant 500 : i32
  %2 = hw.constant 700 : i32
  // CHECK-DAG: dff "\\inner_symbol"([[CLK]], [[CONST500]], [[RES:%[0-9]+]]){{.*}}width = 32
  // CHECK-DAG: [[RES]] = wire "{{[^"]*}}" : [32]
  %res1 = seq.compreg sym @inner_symbol %1, %clk: i32
  // A `seq.compreg` reset is sampled with the clock, matching the synchronous
  // reset `SeqToSV` lowers the same op to: a mux picking the reset value in
  // front of a plain flop.
  // CHECK-DAG: mux "{{[^"]*}}"([[CONST500]], [[CONST700]], [[RESET]], [[MUX2:%[0-9]+]]){{.*}}width = 32
  // CHECK-DAG: dff "{{[^"]*}}"([[CLK]], [[MUX2]], [[RES2:%[0-9]+]]){{.*}}width = 32
  // CHECK-DAG: [[RES2]] = wire "{{[^"]*}}" : [32]
  // CHECK-DAG: [[MUX2]] = wire "{{[^"]*}}" : [32]
  %res2 = seq.compreg %1, %clk reset %reset, %2: i32
  // A distinct inner symbol: two registers sharing one would collide in the
  // module's single wire/cell namespace, which is now a verifier error rather
  // than something a uniquing suffix papers over.
  // CHECK-DAG: dff "\\inner_symbol2"([[CLK]], [[CONST500]], [[RES3:%[0-9]+]]){{.*}}width = 32
  // CHECK-DAG: [[RES3]] = wire "{{[^"]*}}" : [32]
  %res3 = seq.firreg %1 clock %clk sym @inner_symbol2 : i32
  // CHECK-DAG: aldff "{{[^"]*}}"([[CLK]], [[CONST500]], [[RESET]], [[CONST700]], [[RES5:%[0-9]+]]){{.*}}width = 32
  // CHECK-DAG: [[RES5]] = wire "{{[^"]*}}" : [32]
  %res5 = seq.firreg %1 clock %clk reset async %reset, %2 : i32


  // A synchronous `seq.firreg` reset is a mux in front of the flop, so the
  // reset value is sampled at the clock edge like any other input.
  // CHECK-DAG: mux "{{[^"]*}}"([[CONST500]], [[CONST700]], [[RESET]], [[MUX4:%[0-9]+]]){{.*}}width = 32
  // CHECK-DAG: dff "{{[^"]*}}"([[CLK]], [[MUX4]], [[RES4:%[0-9]+]]){{.*}}width = 32
  // CHECK-DAG: [[RES4]] = wire "{{[^"]*}}" : [32]
  // CHECK-DAG: [[MUX4]] = wire "{{[^"]*}}" : [32]
  %res4 = seq.firreg %1 clock %clk reset sync %reset, %2 : i32
}
