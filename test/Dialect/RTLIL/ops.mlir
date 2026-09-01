// RUN: circt-opt %s --verify-diagnostics

// A callee for the `rtlil.instance` below. `rtlil.instance` checks its
// connections against this module's port wires, so the ports have to be real.
rtlil.module @"\\add" {
  %a = "rtlil.wire"() <{name="\\a", is_signed = false, port_id = 1 : i32, port_input = true}> : () -> !rtlil<val[32]>
  %y = "rtlil.wire"() <{name="\\y", is_signed = false, port_id = 2 : i32, port_output = true}> : () -> !rtlil<val[32]>
}

// Wires and cells share a single per-module namespace in RTLIL
// (`RTLIL::Module::add()` asserts on `count_id(name) == 0`), so every name in
// here is distinct -- an exporter handing duplicates to Yosys would take the
// process down with it.
rtlil.module @"\\top" {
  %1 = "rtlil.wire"() <{name="$1", is_signed = false}> : () -> !rtlil<val[32]>
  // A fully attributed wire. `port_id` is 1 rather than an arbitrary number
  // because ports must be numbered exactly 1..N -- `fixup_ports()` renumbers
  // anything else behind the exporter's back.
  %2 = "rtlil.wire"() <{name="$2", is_signed = true, port_input = false, port_output = true, upto = true, port_id = 1 : i32, start_offset = 3 : i32}> : () -> !rtlil<val[64]>

  %3 = "rtlil.const"() <{value = #rtlil.const<"-zx10">}> : () -> !rtlil<val[5]>
  %4 = "rtlil.wire"() <{name="$4", is_signed = false}> : () -> !rtlil<val[32]>
  %5 = "rtlil.wire"() <{name="$5", is_signed = false}> : () -> !rtlil<val[32]>
  %6 = "rtlil.const"() <{value = #rtlil.const<"000000000000000000000000000-zx10">}> : () -> !rtlil<val[32]>

  "rtlil.and"(%1, %6, %4) <{name="$and",width= 32 : i32, opsSigned = 0 : i32}> : (!rtlil<val[32]>, !rtlil<val[32]>, !rtlil<val[32]>) -> ()
  "rtlil.or"(%1, %6, %4) <{name="$or",width= 32 : i32, opsSigned = 0 : i32}> : (!rtlil<val[32]>, !rtlil<val[32]>, !rtlil<val[32]>) -> ()
  "rtlil.sub"(%1, %6, %4) <{name="$sub",width= 32 : i32, opsSigned = 0 : i32}> : (!rtlil<val[32]>, !rtlil<val[32]>, !rtlil<val[32]>) -> ()
  "rtlil.xor"(%1, %6, %4) <{name="$xor",width= 32 : i32, opsSigned = 0 : i32}> : (!rtlil<val[32]>, !rtlil<val[32]>, !rtlil<val[32]>) -> ()
  "rtlil.mul"(%1, %6, %4) <{name="$mul",width= 32 : i32, opsSigned = 0 : i32}> : (!rtlil<val[32]>, !rtlil<val[32]>, !rtlil<val[32]>) -> ()

  %8 = "rtlil.wire"() <{name="$8", is_signed = false}> : () -> !rtlil<val[1]>
  "rtlil.gt"(%1, %6, %8) <{name="$gt",width= 32 : i32, opsSigned = 0 : i32}> : (!rtlil<val[32]>, !rtlil<val[32]>, !rtlil<val[1]>) -> ()
  "rtlil.eq"(%1, %6, %8) <{name="$eq",width= 32 : i32, opsSigned = 0 : i32}> : (!rtlil<val[32]>, !rtlil<val[32]>, !rtlil<val[1]>) -> ()
  "rtlil.ne"(%1, %6, %8) <{name="$ne",width= 32 : i32, opsSigned = 0 : i32}> : (!rtlil<val[32]>, !rtlil<val[32]>, !rtlil<val[1]>) -> ()
  "rtlil.ge"(%1, %6, %8) <{name="$ge",width= 32 : i32, opsSigned = 0 : i32}> : (!rtlil<val[32]>, !rtlil<val[32]>, !rtlil<val[1]>) -> ()
  "rtlil.le"(%1, %6, %8) <{name="$le",width= 32 : i32, opsSigned = 0 : i32}> : (!rtlil<val[32]>, !rtlil<val[32]>, !rtlil<val[1]>) -> ()
  "rtlil.lt"(%1, %6, %8) <{name="$lt",width= 32 : i32, opsSigned = 0 : i32}> : (!rtlil<val[32]>, !rtlil<val[32]>, !rtlil<val[1]>) -> ()

  %clk = "rtlil.wire"() <{name="\\clk", is_signed = false}> : () -> !rtlil<val[1]>

  "rtlil.dff"(%clk, %6, %1) <{name="$dff",width= 32 : i32}> : (!rtlil<val[1]>, !rtlil<val[32]>, !rtlil<val[32]>) -> ()

  "rtlil.aldff"(%clk, %6, %clk, %6, %1) <{name="$aldff",width= 32 : i32}> : (!rtlil<val[1]>, !rtlil<val[32]>,!rtlil<val[1]>,!rtlil<val[32]>, !rtlil<val[32]>) -> ()

  // Port names are the callee's wire names, sigil included -- not bare
  // identifiers: `RTLIL::Cell::setPort` interns them as `IdString`s.
  "rtlil.instance"(%1, %6) <{name="$inst", type=@"\\add", ports = ["\\a", "\\y"], parameters = []}> : (!rtlil<val[32]>, !rtlil<val[32]>) -> ()
}

// A SigSpec: bits [6:3] of a wire concatenated with two constant bits, the
// shape a cell port takes after `opt`/`techmap`/`abc`. Operands are ordered
// least significant first.
rtlil.module @"\\sigspec" {
  %w = "rtlil.wire"() <{name="\\w", is_signed = false}> : () -> !rtlil<val[8]>
  %c = "rtlil.const"() <{value = #rtlil.const<"01">}> : () -> !rtlil<val[2]>
  %s = "rtlil.slice"(%w) <{offset = 3 : i32}> : (!rtlil<val[8]>) -> !rtlil<val[4]>
  %y = "rtlil.concat"(%s, %c) : (!rtlil<val[4]>, !rtlil<val[2]>) -> !rtlil<val[6]>
}

// Parameters an IntegerAttr cannot carry: a bit vector wider than 64 bits, one
// containing x/z, and a string.
rtlil.module @"\\params" {
  %a = "rtlil.wire"() <{name="\\a", is_signed = false}> : () -> !rtlil<val[1]>
  "rtlil.cell"(%a) <{
    name = "$c", type = "$lut", ports = ["\\A"],
    parameters = [
      #rtlil.param<"\\WIDTH" 4 : i32>,
      #rtlil.param<"\\UNDEF" #rtlil.const<"10zx">>,
      #rtlil.param<"\\SRC" "foo.v:3.1-3.9">
    ]
  }> : (!rtlil<val[1]>) -> ()
}
