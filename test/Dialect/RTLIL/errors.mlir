// RUN: circt-opt %s --verify-diagnostics --split-input-file

rtlil.module @"\\top" {
  %1 = "rtlil.wire"() <{name="$1"}> : () -> !rtlil<val[32 : i32]>
  // expected-error@-1 {{'rtlil.wire' op requires attribute 'is_signed'}}
}

// -----

rtlil.module @"\\top" {
  %1 = "rtlil.const"() <{value = [0 : i8]}> : () -> !rtlil<val[32: i32]>
  // expected-error@-1 {{'rtlil.const' op failed to verify that bitwidth matches}}
}

// -----

rtlil.module @"\\top" {
  %1 = "rtlil.const"() <{value = [5 : i8]}> : () -> !rtlil<val[1: i32]>
  // expected-error@-1 {{'rtlil.const' op attribute 'value' failed to satisfy constraint: constant multi-valued bitvec}}
}

// -----

rtlil.module @"\\top" {
  %1 = "rtlil.wire"() <{name="$1", is_signed = false}> : () -> !rtlil<val[32 : i32]>
  %2 = "rtlil.wire"() <{name="$2", is_signed = false}> : () -> !rtlil<val[31 : i32]>
  %3 = "rtlil.wire"() <{name="$3", is_signed = false}> : () -> !rtlil<val[32 : i32]>

  "rtlil.and"(%1, %2, %3) <{name = "$4", width=32 : i32, opsSigned = 0 : i32}> : (!rtlil<val[32: i32]>, !rtlil<val[31: i32]>, !rtlil<val[32: i32]>) -> ()
  // expected-error@-1 {{'rtlil.and' op failed to verify that input 1 width is $width}}
}

// -----

rtlil.module @"\\top" {
  %1 = "rtlil.wire"() <{name="$1", is_signed = false}> : () -> !rtlil<val[32 : i32]>
  %2 = "rtlil.wire"() <{name="$2", is_signed = false}> : () -> !rtlil<val[32 : i32]>
  %3 = "rtlil.wire"() <{name="$3", is_signed = false}> : () -> !rtlil<val[32 : i32]>

  "rtlil.and"(%1, %3) <{name = "$4", width=32 : i32, opsSigned = 0 : i32}> : (!rtlil<val[32: i32]>, !rtlil<val[32: i32]>) -> ()
  // expected-error@-1 {{'rtlil.and' op failed to verify that has 3 connections}}
}

// -----

rtlil.module @"\\top" {
  %1 = "rtlil.wire"() <{name="$1", is_signed = false}> : () -> !rtlil<val[32 : i32]>
  %2 = "rtlil.wire"() <{name="$2", is_signed = false}> : () -> !rtlil<val[32 : i32]>
  %3 = "rtlil.wire"() <{name="$3", is_signed = false}> : () -> !rtlil<val[32 : i32]>

  "rtlil.mux"(%1, %2, %3) <{name = "$4", width=32 : i32}> : (!rtlil<val[32: i32]>, !rtlil<val[32 : i32]>, !rtlil<val[32: i32]>) -> ()
  // expected-error@-1 {{'rtlil.mux' op failed to verify that has 4 connections}}
}

// -----

rtlil.module @"\\top" {
  %1 = "rtlil.wire"() <{name="$1", is_signed = false}> : () -> !rtlil<val[32 : i32]>
  %2 = "rtlil.wire"() <{name="$2", is_signed = false}> : () -> !rtlil<val[32 : i32]>
  %3 = "rtlil.wire"() <{name="$3", is_signed = false}> : () -> !rtlil<val[32 : i32]>
  %select = "rtlil.wire"() <{name="$select", is_signed = false}> : () -> !rtlil<val[2 : i32]>


  "rtlil.mux"(%1, %2, %select, %3) <{name = "$4", width=32 : i32}> : (!rtlil<val[32: i32]>, !rtlil<val[32 : i32]>,!rtlil<val[2 : i32]>, !rtlil<val[32: i32]>) -> ()
  // expected-error@-1 {{'rtlil.mux' op failed to verify that input 2 width is 1}}
}

// -----

rtlil.module @"\\top" {
  %1 = "rtlil.wire"() <{name="$1", is_signed = false}> : () -> !rtlil<val[31 : i32]>
  %2 = "rtlil.wire"() <{name="$2", is_signed = false}> : () -> !rtlil<val[32 : i32]>
  %3 = "rtlil.wire"() <{name="$3", is_signed = false}> : () -> !rtlil<val[32 : i32]>
  %select = "rtlil.wire"() <{name="$select", is_signed = false}> : () -> !rtlil<val[1 : i32]>


  "rtlil.mux"(%1, %2, %select, %3) <{name = "$4", width=32 : i32}> : (!rtlil<val[31: i32]>, !rtlil<val[32 : i32]>,!rtlil<val[1 : i32]>, !rtlil<val[32: i32]>) -> ()
  // expected-error@-1 {{'rtlil.mux' op failed to verify that input 0 width is $width}}
}

// -----

rtlil.module @"\\top" {
  %1 = "rtlil.wire"() <{name="$1", is_signed = false}> : () -> !rtlil<val[32 : i32]>
  %2 = "rtlil.wire"() <{name="$2", is_signed = false}> : () -> !rtlil<val[32 : i32]>
  %3 = "rtlil.wire"() <{name="$3", is_signed = false}> : () -> !rtlil<val[1 : i32]>


  "rtlil.gt"(%1, %2, %3) <{name = "$4", width=32 : i32}> : (!rtlil<val[32: i32]>, !rtlil<val[32 : i32]>, !rtlil<val[1: i32]>) -> ()
  // expected-error@-1 {{'rtlil.gt' op requires attribute 'opsSigned'}}
}

// -----

rtlil.module @"\\top" {
  %1 = "rtlil.wire"() <{name="$1", is_signed = false}> : () -> !rtlil<val[32 : i32]>
  %2 = "rtlil.wire"() <{name="$2", is_signed = false}> : () -> !rtlil<val[32 : i32]>
  %3 = "rtlil.wire"() <{name="$3", is_signed = false}> : () -> !rtlil<val[1 : i32]>


  "rtlil.gt"(%1, %1, %2, %3) <{name = "$4", width=32 : i32, opsSigned = 0 : i32}> : (!rtlil<val[32: i32]>,!rtlil<val[32: i32]>, !rtlil<val[32 : i32]>, !rtlil<val[1: i32]>) -> ()
  // expected-error@-1 {{'rtlil.gt' op failed to verify that has 3 connections}}
}

// -----

rtlil.module @"\\top" {
  %1 = "rtlil.wire"() <{name="$1", is_signed = false}> : () -> !rtlil<val[31 : i32]>
  %2 = "rtlil.wire"() <{name="$2", is_signed = false}> : () -> !rtlil<val[32 : i32]>
  %3 = "rtlil.wire"() <{name="$3", is_signed = false}> : () -> !rtlil<val[1 : i32]>


  "rtlil.gt"(%1, %2, %3) <{name = "$4", width=32 : i32, opsSigned = 1 : i32}> : (!rtlil<val[31: i32]>,!rtlil<val[32 : i32]>, !rtlil<val[1: i32]>) -> ()
  // expected-error@-1 {{'rtlil.gt' op failed to verify that input 0 width is $width}}
}

// -----

rtlil.module @"\\top" {
  %1 = "rtlil.wire"() <{name="$1", is_signed = false}> : () -> !rtlil<val[32 : i32]>
  %2 = "rtlil.wire"() <{name="$2", is_signed = false}> : () -> !rtlil<val[32 : i32]>
  %3 = "rtlil.wire"() <{name="$3", is_signed = false}> : () -> !rtlil<val[32 : i32]>


  "rtlil.gt"(%1, %2, %3) <{name = "$4", width=32 : i32, opsSigned = 1 : i32}> : (!rtlil<val[32: i32]>,!rtlil<val[32 : i32]>, !rtlil<val[32: i32]>) -> ()
  // expected-error@-1 {{'rtlil.gt' op failed to verify that input 2 width is 1}}
}

// -----

rtlil.module @"\\top" {
  %1 = "rtlil.wire"() <{name="$1", is_signed = false}> : () -> !rtlil<val[32 : i32]>
  %2 = "rtlil.wire"() <{name="$2", is_signed = false}> : () -> !rtlil<val[32 : i32]>
  %3 = "rtlil.wire"() <{name="$3", is_signed = false}> : () -> !rtlil<val[32 : i32]>

  "rtlil.dff"(%1, %2, %3) <{name="$4", width=32 : i32}> : (!rtlil<val[32: i32]>,!rtlil<val[32 : i32]>, !rtlil<val[32: i32]>) -> ()
  // expected-error@-1 {{'rtlil.dff' op failed to verify that input 0 width is 1}}
}

// -----

rtlil.module @"\\top" {
  %1 = "rtlil.wire"() <{name="$1", is_signed = false}> : () -> !rtlil<val[1 : i32]>
  %2 = "rtlil.wire"() <{name="$2", is_signed = false}> : () -> !rtlil<val[32 : i32]>
  %3 = "rtlil.wire"() <{name="$3", is_signed = false}> : () -> !rtlil<val[33 : i32]>

  "rtlil.dff"(%1, %2, %3) <{name="$4", width=32 : i32}> : (!rtlil<val[1: i32]>,!rtlil<val[32 : i32]>, !rtlil<val[33: i32]>) -> ()
  // expected-error@-1 {{'rtlil.dff' op failed to verify that input 2 width is $width}}
}

// -----

rtlil.module @"\\top" {
  %1 = "rtlil.wire"() <{name="$1", is_signed = false}> : () -> !rtlil<val[1 : i32]>
  %reset = "rtlil.wire"() <{name="$reset", is_signed = false}> : () -> !rtlil<val[2 : i32]>
  %2 = "rtlil.wire"() <{name="$2", is_signed = false}> : () -> !rtlil<val[32 : i32]>
  %3 = "rtlil.wire"() <{name="$3", is_signed = false}> : () -> !rtlil<val[32 : i32]>

  "rtlil.aldff"(%1, %2, %reset, %3, %3) <{name="$4", width=32 : i32}> : (!rtlil<val[1: i32]>,!rtlil<val[32 : i32]>,!rtlil<val[2 : i32]>,!rtlil<val[32 : i32]>, !rtlil<val[32: i32]>) -> ()
  // expected-error@-1 {{'rtlil.aldff' op failed to verify that input 2 width is 1}}
}

// -----

rtlil.module @"\\top" {
  %1 = "rtlil.wire"() <{name="$1", is_signed = false}> : () -> !rtlil<val[1 : i32]>
  %reset = "rtlil.wire"() <{name="$reset", is_signed = false}> : () -> !rtlil<val[1 : i32]>
  %2 = "rtlil.wire"() <{name="$2", is_signed = false}> : () -> !rtlil<val[32 : i32]>
  %3 = "rtlil.wire"() <{name="$3", is_signed = false}> : () -> !rtlil<val[32 : i32]>

  "rtlil.aldff"(%1, %2, %reset, %3, %3, %3) <{name="$4", width=32 : i32}> : (!rtlil<val[1: i32]>,!rtlil<val[32 : i32]>,!rtlil<val[1 : i32]>,!rtlil<val[32 : i32]>, !rtlil<val[32 : i32]>, !rtlil<val[32: i32]>) -> ()
  // expected-error@-1 {{'rtlil.aldff' op failed to verify that has 5 connections}}
}

// -----

rtlil.module @"\\top" {
  %1 = "rtlil.wire"() <{name="$1", is_signed = false}> : () -> !rtlil<val[1 : i32]>
  %reset = "rtlil.wire"() <{name="$reset", is_signed = false}> : () -> !rtlil<val[1 : i32]>
  %2 = "rtlil.wire"() <{name="$2", is_signed = false}> : () -> !rtlil<val[64 : i32]>
  %3 = "rtlil.wire"() <{name="$3", is_signed = false}> : () -> !rtlil<val[32 : i32]>

  "rtlil.aldff"(%1, %2, %reset, %3, %3) <{name="$4", width=32 : i32}> : (!rtlil<val[1: i32]>,!rtlil<val[64 : i32]>,!rtlil<val[1 : i32]>,!rtlil<val[32 : i32]>, !rtlil<val[32: i32]>) -> ()
  // expected-error@-1 {{'rtlil.aldff' op failed to verify that input 1 width is $width}}
}
// -----

// expected-error@+1 {{'rtlil.module' op name top is not a valid RTLIL identifier}}
rtlil.module @top {
}

// -----

rtlil.module @"\\top" {
  // expected-error@+1 {{'rtlil.wire' op name w is not a valid RTLIL identifier}}
  %1 = "rtlil.wire"() <{name="w", is_signed = false}> : () -> !rtlil<val[1 : i32]>
}

// -----

rtlil.module @"\\top" {
  // A space is a control character as far as RTLIL::IdString is concerned.
  // expected-error@+1 {{op name \a b is not a valid RTLIL identifier}}
  %1 = "rtlil.wire"() <{name="\\a b", is_signed = false}> : () -> !rtlil<val[1 : i32]>
}

// -----

rtlil.module @"\\top" {
  // expected-note@+1 {{previously declared here}}
  %1 = "rtlil.wire"() <{name="$dup", is_signed = false}> : () -> !rtlil<val[1 : i32]>
  // expected-error@+1 {{'rtlil.wire' op redeclares the RTLIL name $dup}}
  %2 = "rtlil.wire"() <{name="$dup", is_signed = false}> : () -> !rtlil<val[1 : i32]>
}

// -----

// Wires and cells share one namespace, so a cell may not take a wire's name.
rtlil.module @"\\top" {
  // expected-note@+1 {{previously declared here}}
  %1 = "rtlil.wire"() <{name="$x", is_signed = false}> : () -> !rtlil<val[1 : i32]>
  // expected-error@+1 {{'rtlil.and' op redeclares the RTLIL name $x}}
  "rtlil.and"(%1, %1, %1) <{name="$x", width=1 : i32, opsSigned = 0 : i32}> : (!rtlil<val[1 : i32]>, !rtlil<val[1 : i32]>, !rtlil<val[1 : i32]>) -> ()
}

// -----

// Ports must be numbered 1..N: `fixup_ports()` silently renumbers otherwise.
// expected-error@+1 {{'rtlil.module' op port_ids must be exactly 1..1, but the largest is 7}}
rtlil.module @"\\top" {
  %1 = "rtlil.wire"() <{name="\\p", is_signed = false, port_id = 7 : i32, port_input = true}> : () -> !rtlil<val[1 : i32]>
}

// -----

rtlil.module @"\\top" {
  // expected-note@+1 {{already used here}}
  %1 = "rtlil.wire"() <{name="\\p", is_signed = false, port_id = 1 : i32, port_input = true}> : () -> !rtlil<val[1 : i32]>
  // expected-error@+1 {{'rtlil.wire' op reuses port_id 1}}
  %2 = "rtlil.wire"() <{name="\\q", is_signed = false, port_id = 1 : i32, port_input = true}> : () -> !rtlil<val[1 : i32]>
}

// -----

rtlil.module @"\\top" {
  // expected-error@+1 {{'rtlil.instance' op references unknown module \nope}}
  "rtlil.instance"() <{name="$i", type=@"\\nope", ports = [], parameters = []}> : () -> ()
}
