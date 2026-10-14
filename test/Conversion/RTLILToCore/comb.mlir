// RUN: circt-opt %s --convert-rtlil-to-core | FileCheck %s

// Inputs become arguments in `port_id` order, not in body order. Every wire
// becomes a signal. A cell probes its inputs and drives its output, which may
// be read before the cell.
// CHECK-LABEL: hw.module @andmod(in %a : i8, in %b : i8, in %c : i8, out o : i8, out p : i4)
rtlil.module @"\\andmod" {
  // CHECK-DAG: [[C:%.+]] = llhd.sig name "c" : <i8>
  // CHECK-DAG: llhd.drv [[C]], %c after {{%.+}} : i8
  // CHECK-DAG: [[B:%.+]] = llhd.sig name "b" : <i8>
  // CHECK-DAG: llhd.drv [[B]], %b after {{%.+}} : i8
  // CHECK-DAG: [[A:%.+]] = llhd.sig name "a" : <i8>
  // CHECK-DAG: llhd.drv [[A]], %a after {{%.+}} : i8
  %c = wire "\\c" input port 3 : !rtlil<val[8]>
  %b = wire "\\b" input port 2 : !rtlil<val[8]>
  %a = wire "\\a" input port 1 : !rtlil<val[8]>
  // CHECK-DAG: %o = llhd.sig : <i8>
  // CHECK-DAG: %p = llhd.sig : <i4>
  %o = wire "\\o" output port 4 : !rtlil<val[8]>
  %p = wire "\\p" output port 5 : !rtlil<val[4]>
  // Generated names stay anonymous.
  // CHECK-DAG: [[T1:%.+]] = llhd.sig : <i8>
  // CHECK-DAG: [[T3:%.+]] = llhd.sig : <i8>
  %t1 = wire "$1" : !rtlil<val[8]>
  %t3 = wire "$3" : !rtlil<val[8]>
  // CHECK-DAG: [[T1_PRB:%.+]] = llhd.prb [[T1]] : i8
  // CHECK-DAG: [[C_PRB:%.+]] = llhd.prb [[C]] : i8
  // CHECK-DAG: [[AND2:%.+]] = comb.and [[T1_PRB]], [[C_PRB]] : i8
  // CHECK-DAG: llhd.drv [[T3]], [[AND2]] after {{%.+}} : i8
  and "$4"(%t1, %c, %t3) {opsSigned = 0 : i32, width = 8 : i32} : !rtlil<val[8]>, !rtlil<val[8]>, !rtlil<val[8]>
  // CHECK-DAG: [[A_PRB:%.+]] = llhd.prb [[A]] : i8
  // CHECK-DAG: [[B_PRB:%.+]] = llhd.prb [[B]] : i8
  // CHECK-DAG: [[AND1:%.+]] = comb.and [[A_PRB]], [[B_PRB]] : i8
  // CHECK-DAG: llhd.drv [[T1]], [[AND1]] after {{%.+}} : i8
  and "$2"(%a, %b, %t1) {opsSigned = 0 : i32, width = 8 : i32} : !rtlil<val[8]>, !rtlil<val[8]>, !rtlil<val[8]>
  // CHECK-DAG: [[OFF:%.+]] = hw.constant 2 : i3
  // CHECK-DAG: [[SLICE:%.+]] = llhd.sig.extract [[T3]] from [[OFF]] : <i8> -> <i4>
  %s = slice %t3 offset 2 : (!rtlil<val[8]>) -> !rtlil<val[4]>
  // CHECK-DAG: [[T3_PRB:%.+]] = llhd.prb [[T3]] : i8
  // CHECK-DAG: llhd.drv %o, [[T3_PRB]] after {{%.+}} : i8
  wconnection %o, %t3 : !rtlil<val[8]>
  // CHECK-DAG: [[SLICE_PRB:%.+]] = llhd.prb [[SLICE]] : i4
  // CHECK-DAG: llhd.drv %p, [[SLICE_PRB]] after {{%.+}} : i4
  wconnection %p, %s : !rtlil<val[4]>
  // CHECK-DAG: [[O_PRB:%.+]] = llhd.prb %o : i8
  // CHECK-DAG: [[P_PRB:%.+]] = llhd.prb %p : i4
  // CHECK: hw.output [[O_PRB]], [[P_PRB]] : i8, i4
}

// A constant is a signal holding its initial value. A concat that is read is
// driven by its inputs. `rtlil.concat` is least significant first,
// `comb.concat` most.
// CHECK-LABEL: hw.module @constmod(in %a : i2, out o : i4)
rtlil.module @"\\constmod" {
  // CHECK-DAG: [[A:%.+]] = llhd.sig name "a" : <i2>
  %a = wire "\\a" input port 1 : !rtlil<val[2]>
  // CHECK-DAG: %o = llhd.sig : <i4>
  %o = wire "\\o" output port 2 : !rtlil<val[4]>
  // CHECK-DAG: [[INIT:%.+]] = hw.constant -2 : i2
  // CHECK-DAG: [[C:%.+]] = llhd.sig [[INIT]] : <i2>
  %c = const <"10"> : !rtlil<val[2]>
  // CHECK-DAG: [[CAT:%.+]] = llhd.sig : <i4>
  // CHECK-DAG: [[C_PRB:%.+]] = llhd.prb [[C]] : i2
  // CHECK-DAG: [[A_PRB:%.+]] = llhd.prb [[A]] : i2
  // CHECK-DAG: [[CAT_VAL:%.+]] = comb.concat [[C_PRB]], [[A_PRB]] : i2, i2
  // CHECK-DAG: llhd.drv [[CAT]], [[CAT_VAL]] after {{%.+}} : i4
  %cat = concat %a, %c : (!rtlil<val[2]>, !rtlil<val[2]>) -> !rtlil<val[4]>
  // CHECK-DAG: [[CAT_PRB0:%.+]] = llhd.prb [[CAT]] : i4
  // CHECK-DAG: [[CAT_PRB1:%.+]] = llhd.prb [[CAT]] : i4
  // CHECK-DAG: [[AND:%.+]] = comb.and [[CAT_PRB0]], [[CAT_PRB1]] : i4
  // CHECK-DAG: llhd.drv %o, [[AND]] after {{%.+}} : i4
  and "$1"(%cat, %cat, %o) {opsSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[4]>
}

// A concat that is driven drives each of its inputs with its bits.
// CHECK-LABEL: hw.module @splitmod(in %a : i4, in %b : i4, out lo : i2, out hi : i2)
rtlil.module @"\\splitmod" {
  %a = wire "\\a" input port 1 : !rtlil<val[4]>
  %b = wire "\\b" input port 2 : !rtlil<val[4]>
  // CHECK-DAG: %lo = llhd.sig : <i2>
  // CHECK-DAG: %hi = llhd.sig : <i2>
  %lo = wire "\\lo" output port 3 : !rtlil<val[2]>
  %hi = wire "\\hi" output port 4 : !rtlil<val[2]>
  // CHECK-DAG: %unused = llhd.sig : <i1>
  %unused = wire "\\unused" : !rtlil<val[1]>
  // CHECK-DAG: [[CAT:%.+]] = llhd.sig : <i4>
  // CHECK-DAG: [[CAT_PRB:%.+]] = llhd.prb [[CAT]] : i4
  // CHECK-DAG: [[LO:%.+]] = comb.extract [[CAT_PRB]] from 0 : (i4) -> i2
  // CHECK-DAG: llhd.drv %lo, [[LO]] after {{%.+}} : i2
  // CHECK-DAG: [[HI:%.+]] = comb.extract [[CAT_PRB]] from 2 : (i4) -> i2
  // CHECK-DAG: llhd.drv %hi, [[HI]] after {{%.+}} : i2
  %y = concat %lo, %hi : (!rtlil<val[2]>, !rtlil<val[2]>) -> !rtlil<val[4]>
  // CHECK-DAG: [[AND:%.+]] = comb.and {{%.+}}, {{%.+}} : i4
  // CHECK-DAG: llhd.drv [[CAT]], [[AND]] after {{%.+}} : i4
  and "$1"(%a, %b, %y) {opsSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[4]>
}

// A concat on the Y port of `or` and `xor` is driven, too.
// CHECK-LABEL: hw.module @orxormod(in %a : i2, in %b : i2, out lo : i1, out hi : i1, out lo2 : i1, out hi2 : i1)
rtlil.module @"\\orxormod" {
  %a = wire "\\a" input port 1 : !rtlil<val[2]>
  %b = wire "\\b" input port 2 : !rtlil<val[2]>
  %lo = wire "\\lo" output port 3 : !rtlil<val[1]>
  %hi = wire "\\hi" output port 4 : !rtlil<val[1]>
  %lo2 = wire "\\lo2" output port 5 : !rtlil<val[1]>
  %hi2 = wire "\\hi2" output port 6 : !rtlil<val[1]>
  // CHECK-DAG: [[OR_CAT:%.+]] = llhd.sig : <i2>
  // CHECK-DAG: [[OR_PRB:%.+]] = llhd.prb [[OR_CAT]] : i2
  // CHECK-DAG: [[OR_LO:%.+]] = comb.extract [[OR_PRB]] from 0 : (i2) -> i1
  // CHECK-DAG: llhd.drv %lo, [[OR_LO]] after {{%.+}} : i1
  // CHECK-DAG: [[OR_HI:%.+]] = comb.extract [[OR_PRB]] from 1 : (i2) -> i1
  // CHECK-DAG: llhd.drv %hi, [[OR_HI]] after {{%.+}} : i1
  %y = concat %lo, %hi : (!rtlil<val[1]>, !rtlil<val[1]>) -> !rtlil<val[2]>
  // CHECK-DAG: [[OR:%.+]] = comb.or {{%.+}}, {{%.+}} : i2
  // CHECK-DAG: llhd.drv [[OR_CAT]], [[OR]] after {{%.+}} : i2
  or "$1"(%a, %b, %y) {opsSigned = 0 : i32, width = 2 : i32} : !rtlil<val[2]>, !rtlil<val[2]>, !rtlil<val[2]>
  // CHECK-DAG: [[XOR_CAT:%.+]] = llhd.sig : <i2>
  // CHECK-DAG: [[XOR_PRB:%.+]] = llhd.prb [[XOR_CAT]] : i2
  // CHECK-DAG: [[XOR_LO:%.+]] = comb.extract [[XOR_PRB]] from 0 : (i2) -> i1
  // CHECK-DAG: llhd.drv %lo2, [[XOR_LO]] after {{%.+}} : i1
  // CHECK-DAG: [[XOR_HI:%.+]] = comb.extract [[XOR_PRB]] from 1 : (i2) -> i1
  // CHECK-DAG: llhd.drv %hi2, [[XOR_HI]] after {{%.+}} : i1
  %z = concat %lo2, %hi2 : (!rtlil<val[1]>, !rtlil<val[1]>) -> !rtlil<val[2]>
  // CHECK-DAG: [[XOR:%.+]] = comb.xor {{%.+}}, {{%.+}} : i2
  // CHECK-DAG: llhd.drv [[XOR_CAT]], [[XOR]] after {{%.+}} : i2
  xor "$2"(%a, %b, %z) {opsSigned = 0 : i32, width = 2 : i32} : !rtlil<val[2]>, !rtlil<val[2]>, !rtlil<val[2]>
}

// Cells whose signedness does not matter at equal widths map one-to-one.
// CHECK-LABEL: hw.module @arith(in %a : i4, in %b : i4, out add : i4, out sub : i4, out mul : i4, out shl : i4, out shr : i4, out par : i1)
rtlil.module @"\\arith" {
  %a = wire "\\a" input port 1 : !rtlil<val[4]>
  %b = wire "\\b" input port 2 : !rtlil<val[4]>
  %add = wire "\\add" output port 3 : !rtlil<val[4]>
  %sub = wire "\\sub" output port 4 : !rtlil<val[4]>
  %mul = wire "\\mul" output port 5 : !rtlil<val[4]>
  %shl = wire "\\shl" output port 6 : !rtlil<val[4]>
  %shr = wire "\\shr" output port 7 : !rtlil<val[4]>
  %par = wire "\\par" output port 8 : !rtlil<val[1]>
  // CHECK-DAG: [[ADD:%.+]] = comb.add {{%.+}}, {{%.+}} : i4
  // CHECK-DAG: llhd.drv %add, [[ADD]] after {{%.+}} : i4
  add "$1"(%a, %b, %add) {opsSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[4]>
  // CHECK-DAG: [[SUB:%.+]] = comb.sub {{%.+}}, {{%.+}} : i4
  // CHECK-DAG: llhd.drv %sub, [[SUB]] after {{%.+}} : i4
  sub "$2"(%a, %b, %sub) {opsSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[4]>
  // CHECK-DAG: [[MUL:%.+]] = comb.mul {{%.+}}, {{%.+}} : i4
  // CHECK-DAG: llhd.drv %mul, [[MUL]] after {{%.+}} : i4
  mul "$3"(%a, %b, %mul) {opsSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[4]>
  // CHECK-DAG: [[SHL:%.+]] = comb.shl {{%.+}}, {{%.+}} : i4
  // CHECK-DAG: llhd.drv %shl, [[SHL]] after {{%.+}} : i4
  shl "$4"(%a, %b, %shl) {aSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[4]>
  // CHECK-DAG: [[SHR:%.+]] = comb.shru {{%.+}}, {{%.+}} : i4
  // CHECK-DAG: llhd.drv %shr, [[SHR]] after {{%.+}} : i4
  shr "$5"(%a, %b, %shr) {aSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[4]>
  // CHECK-DAG: [[PAR:%.+]] = comb.parity {{%.+}} : i4
  // CHECK-DAG: llhd.drv %par, [[PAR]] after {{%.+}} : i1
  reduce_xor "$6"(%a, %par) {aSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[1]>
}

// `opsSigned` picks the signed or unsigned division and remainder.
// CHECK-LABEL: hw.module @divmod(in %a : i4, in %b : i4, out divu : i4, out divs : i4, out modu : i4, out mods : i4)
rtlil.module @"\\divmod" {
  %a = wire "\\a" input port 1 : !rtlil<val[4]>
  %b = wire "\\b" input port 2 : !rtlil<val[4]>
  %divu = wire "\\divu" output port 3 : !rtlil<val[4]>
  %divs = wire "\\divs" output port 4 : !rtlil<val[4]>
  %modu = wire "\\modu" output port 5 : !rtlil<val[4]>
  %mods = wire "\\mods" output port 6 : !rtlil<val[4]>
  // CHECK-DAG: [[DIVU:%.+]] = comb.divu {{%.+}}, {{%.+}} : i4
  // CHECK-DAG: llhd.drv %divu, [[DIVU]] after {{%.+}} : i4
  div "$1"(%a, %b, %divu) {opsSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[4]>
  // CHECK-DAG: [[DIVS:%.+]] = comb.divs {{%.+}}, {{%.+}} : i4
  // CHECK-DAG: llhd.drv %divs, [[DIVS]] after {{%.+}} : i4
  div "$2"(%a, %b, %divs) {opsSigned = 1 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[4]>
  // CHECK-DAG: [[MODU:%.+]] = comb.modu {{%.+}}, {{%.+}} : i4
  // CHECK-DAG: llhd.drv %modu, [[MODU]] after {{%.+}} : i4
  mod "$3"(%a, %b, %modu) {opsSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[4]>
  // CHECK-DAG: [[MODS:%.+]] = comb.mods {{%.+}}, {{%.+}} : i4
  // CHECK-DAG: llhd.drv %mods, [[MODS]] after {{%.+}} : i4
  mod "$4"(%a, %b, %mods) {opsSigned = 1 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[4]>
}

// `$sshr` is an arithmetic shift only for a signed A.
// CHECK-LABEL: hw.module @sshr(in %a : i4, in %b : i4, out s : i4, out u : i4)
rtlil.module @"\\sshr" {
  %a = wire "\\a" input port 1 : !rtlil<val[4]>
  %b = wire "\\b" input port 2 : !rtlil<val[4]>
  %s = wire "\\s" output port 3 : !rtlil<val[4]>
  %u = wire "\\u" output port 4 : !rtlil<val[4]>
  // CHECK-DAG: [[SHRS:%.+]] = comb.shrs {{%.+}}, {{%.+}} : i4
  // CHECK-DAG: llhd.drv %s, [[SHRS]] after {{%.+}} : i4
  sshr "$1"(%a, %b, %s) {aSigned = 1 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[4]>
  // CHECK-DAG: [[SHRU:%.+]] = comb.shru {{%.+}}, {{%.+}} : i4
  // CHECK-DAG: llhd.drv %u, [[SHRU]] after {{%.+}} : i4
  sshr "$2"(%a, %b, %u) {aSigned = 0 : i32, width = 4 : i32} : !rtlil<val[4]>, !rtlil<val[4]>, !rtlil<val[4]>
}
