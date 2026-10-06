// RUN: circt-opt %s --verify-diagnostics --split-input-file

// The module's body checks read this wire's name, so they must not run before
// the wire's own verifier has rejected it.
rtlil.module @"\\top" {
  %1 = "rtlil.wire"() <{}> : () -> !rtlil<val[32]>
  // expected-error@-1 {{'rtlil.wire' op requires attribute 'name'}}
}

// -----

// Likewise for a cell's name.
rtlil.module @"\\top" {
  %a = rtlil.wire "\\a" : !rtlil<val[1]>
  // expected-error@+1 {{'rtlil.cell' op requires attribute 'name'}}
  "rtlil.cell"(%a) <{type = "$not", ports = ["\\A"], parameters = []}> : (!rtlil<val[1]>) -> ()
}

// -----

rtlil.module @"\\top" {
  %1 = "rtlil.const"() <{value = #rtlil.const<"0">}> : () -> !rtlil<val[32]>
  // expected-error@-1 {{'rtlil.const' op failed to verify that bitwidth matches}}
}

// -----

rtlil.module @"\\top" {
  // A bit vector is written with the RTLIL state characters and nothing else.
  // expected-error@+1 {{expected a bit string of '0', '1', 'x', 'z' and '-'}}
  %1 = "rtlil.const"() <{value = #rtlil.const<"2">}> : () -> !rtlil<val[1]>
}

// -----

rtlil.module @"\\top" {
  %1 = rtlil.wire "$1" : !rtlil<val[32]>
  %2 = rtlil.wire "$2" : !rtlil<val[31]>
  %3 = rtlil.wire "$3" : !rtlil<val[32]>

  "rtlil.and"(%1, %2, %3) <{name = "$4", width=32 : i32, opsSigned = 0 : i32}> : (!rtlil<val[32]>, !rtlil<val[31]>, !rtlil<val[32]>) -> ()
  // expected-error@-1 {{'rtlil.and' op failed to verify that input 1 width is $width}}
}

// -----

rtlil.module @"\\top" {
  %1 = rtlil.wire "$1" : !rtlil<val[32]>
  %2 = rtlil.wire "$2" : !rtlil<val[32]>
  %3 = rtlil.wire "$3" : !rtlil<val[32]>

  "rtlil.and"(%1, %3) <{name = "$4", width=32 : i32, opsSigned = 0 : i32}> : (!rtlil<val[32]>, !rtlil<val[32]>) -> ()
  // expected-error@-1 {{'rtlil.and' op failed to verify that has 3 connections}}
}

// -----

rtlil.module @"\\top" {
  %1 = rtlil.wire "$1" : !rtlil<val[32]>
  %2 = rtlil.wire "$2" : !rtlil<val[32]>
  %3 = rtlil.wire "$3" : !rtlil<val[32]>

  "rtlil.mux"(%1, %2, %3) <{name = "$4", width=32 : i32}> : (!rtlil<val[32]>, !rtlil<val[32]>, !rtlil<val[32]>) -> ()
  // expected-error@-1 {{'rtlil.mux' op failed to verify that has 4 connections}}
}

// -----

rtlil.module @"\\top" {
  %1 = rtlil.wire "$1" : !rtlil<val[32]>
  %2 = rtlil.wire "$2" : !rtlil<val[32]>
  %3 = rtlil.wire "$3" : !rtlil<val[32]>
  %select = rtlil.wire "$select" : !rtlil<val[2]>


  "rtlil.mux"(%1, %2, %select, %3) <{name = "$4", width=32 : i32}> : (!rtlil<val[32]>, !rtlil<val[32]>,!rtlil<val[2]>, !rtlil<val[32]>) -> ()
  // expected-error@-1 {{'rtlil.mux' op failed to verify that input 2 width is 1}}
}

// -----

rtlil.module @"\\top" {
  %1 = rtlil.wire "$1" : !rtlil<val[31]>
  %2 = rtlil.wire "$2" : !rtlil<val[32]>
  %3 = rtlil.wire "$3" : !rtlil<val[32]>
  %select = rtlil.wire "$select" : !rtlil<val[1]>


  "rtlil.mux"(%1, %2, %select, %3) <{name = "$4", width=32 : i32}> : (!rtlil<val[31]>, !rtlil<val[32]>,!rtlil<val[1]>, !rtlil<val[32]>) -> ()
  // expected-error@-1 {{'rtlil.mux' op failed to verify that input 0 width is $width}}
}

// -----

rtlil.module @"\\top" {
  %1 = rtlil.wire "$1" : !rtlil<val[32]>
  %2 = rtlil.wire "$2" : !rtlil<val[32]>
  %3 = rtlil.wire "$3" : !rtlil<val[1]>


  "rtlil.gt"(%1, %2, %3) <{name = "$4", width=32 : i32}> : (!rtlil<val[32]>, !rtlil<val[32]>, !rtlil<val[1]>) -> ()
  // expected-error@-1 {{'rtlil.gt' op requires attribute 'opsSigned'}}
}

// -----

rtlil.module @"\\top" {
  %1 = rtlil.wire "$1" : !rtlil<val[32]>
  %2 = rtlil.wire "$2" : !rtlil<val[32]>
  %3 = rtlil.wire "$3" : !rtlil<val[1]>


  "rtlil.gt"(%1, %1, %2, %3) <{name = "$4", width=32 : i32, opsSigned = 0 : i32}> : (!rtlil<val[32]>,!rtlil<val[32]>, !rtlil<val[32]>, !rtlil<val[1]>) -> ()
  // expected-error@-1 {{'rtlil.gt' op failed to verify that has 3 connections}}
}

// -----

rtlil.module @"\\top" {
  %1 = rtlil.wire "$1" : !rtlil<val[31]>
  %2 = rtlil.wire "$2" : !rtlil<val[32]>
  %3 = rtlil.wire "$3" : !rtlil<val[1]>


  "rtlil.gt"(%1, %2, %3) <{name = "$4", width=32 : i32, opsSigned = 1 : i32}> : (!rtlil<val[31]>,!rtlil<val[32]>, !rtlil<val[1]>) -> ()
  // expected-error@-1 {{'rtlil.gt' op failed to verify that input 0 width is $width}}
}

// -----

rtlil.module @"\\top" {
  %1 = rtlil.wire "$1" : !rtlil<val[32]>
  %2 = rtlil.wire "$2" : !rtlil<val[32]>
  %3 = rtlil.wire "$3" : !rtlil<val[32]>


  "rtlil.gt"(%1, %2, %3) <{name = "$4", width=32 : i32, opsSigned = 1 : i32}> : (!rtlil<val[32]>,!rtlil<val[32]>, !rtlil<val[32]>) -> ()
  // expected-error@-1 {{'rtlil.gt' op failed to verify that input 2 width is 1}}
}

// -----

rtlil.module @"\\top" {
  %1 = rtlil.wire "$1" : !rtlil<val[32]>
  %2 = rtlil.wire "$2" : !rtlil<val[32]>
  %3 = rtlil.wire "$3" : !rtlil<val[32]>

  "rtlil.dff"(%1, %2, %3) <{name="$4", width=32 : i32}> : (!rtlil<val[32]>,!rtlil<val[32]>, !rtlil<val[32]>) -> ()
  // expected-error@-1 {{'rtlil.dff' op failed to verify that input 0 width is 1}}
}

// -----

rtlil.module @"\\top" {
  %1 = rtlil.wire "$1" : !rtlil<val[1]>
  %2 = rtlil.wire "$2" : !rtlil<val[32]>
  %3 = rtlil.wire "$3" : !rtlil<val[33]>

  "rtlil.dff"(%1, %2, %3) <{name="$4", width=32 : i32}> : (!rtlil<val[1]>,!rtlil<val[32]>, !rtlil<val[33]>) -> ()
  // expected-error@-1 {{'rtlil.dff' op failed to verify that input 2 width is $width}}
}

// -----

rtlil.module @"\\top" {
  %1 = rtlil.wire "$1" : !rtlil<val[1]>
  %reset = rtlil.wire "$reset" : !rtlil<val[2]>
  %2 = rtlil.wire "$2" : !rtlil<val[32]>
  %3 = rtlil.wire "$3" : !rtlil<val[32]>

  "rtlil.aldff"(%1, %2, %reset, %3, %3) <{name="$4", width=32 : i32}> : (!rtlil<val[1]>,!rtlil<val[32]>,!rtlil<val[2]>,!rtlil<val[32]>, !rtlil<val[32]>) -> ()
  // expected-error@-1 {{'rtlil.aldff' op failed to verify that input 2 width is 1}}
}

// -----

rtlil.module @"\\top" {
  %1 = rtlil.wire "$1" : !rtlil<val[1]>
  %reset = rtlil.wire "$reset" : !rtlil<val[1]>
  %2 = rtlil.wire "$2" : !rtlil<val[32]>
  %3 = rtlil.wire "$3" : !rtlil<val[32]>

  "rtlil.aldff"(%1, %2, %reset, %3, %3, %3) <{name="$4", width=32 : i32}> : (!rtlil<val[1]>,!rtlil<val[32]>,!rtlil<val[1]>,!rtlil<val[32]>, !rtlil<val[32]>, !rtlil<val[32]>) -> ()
  // expected-error@-1 {{'rtlil.aldff' op failed to verify that has 5 connections}}
}

// -----

rtlil.module @"\\top" {
  %1 = rtlil.wire "$1" : !rtlil<val[1]>
  %reset = rtlil.wire "$reset" : !rtlil<val[1]>
  %2 = rtlil.wire "$2" : !rtlil<val[64]>
  %3 = rtlil.wire "$3" : !rtlil<val[32]>

  "rtlil.aldff"(%1, %2, %reset, %3, %3) <{name="$4", width=32 : i32}> : (!rtlil<val[1]>,!rtlil<val[64]>,!rtlil<val[1]>,!rtlil<val[32]>, !rtlil<val[32]>) -> ()
  // expected-error@-1 {{'rtlil.aldff' op failed to verify that input 1 width is $width}}
}
// -----

// expected-error@+1 {{'rtlil.module' op module name 'top' is not a valid RTLIL identifier}}
rtlil.module @top {
}

// -----

rtlil.module @"\\top" {
  // expected-error@+1 {{'rtlil.wire' op name 'w' is not a valid RTLIL identifier}}
  %1 = rtlil.wire "w" : !rtlil<val[1]>
}

// -----

rtlil.module @"\\top" {
  // A space is a control character as far as RTLIL::IdString is concerned.
  // expected-error@+1 {{op name '\a b' is not a valid RTLIL identifier}}
  %1 = rtlil.wire "\\a b" : !rtlil<val[1]>
}

// -----

rtlil.module @"\\top" {
  // expected-note@+1 {{previously declared here}}
  %1 = rtlil.wire "$dup" : !rtlil<val[1]>
  // expected-error@+1 {{'rtlil.wire' op redeclares name '$dup'}}
  %2 = rtlil.wire "$dup" : !rtlil<val[1]>
}

// -----

// Wires and cells share one namespace, so a cell may not take a wire's name.
rtlil.module @"\\top" {
  // expected-note@+1 {{previously declared here}}
  %1 = rtlil.wire "$x" : !rtlil<val[1]>
  // expected-error@+1 {{'rtlil.and' op redeclares name '$x'}}
  "rtlil.and"(%1, %1, %1) <{name="$x", width=1 : i32, opsSigned = 0 : i32}> : (!rtlil<val[1]>, !rtlil<val[1]>, !rtlil<val[1]>) -> ()
}

// -----

rtlil.module @"\\top" {
  // expected-error@+1 {{'builtin.unrealized_conversion_cast' op is not allowed in an RTLIL module}}
  %0 = "builtin.unrealized_conversion_cast"() : () -> i1
}

// -----

// Ports must be numbered 1..N: `fixup_ports()` silently renumbers otherwise.
// expected-error@+1 {{'rtlil.module' op port_ids must be exactly 1..1, but the largest is 7}}
rtlil.module @"\\top" {
  %1 = rtlil.wire "\\p" input port 7 : !rtlil<val[1]>
}

// -----

rtlil.module @"\\top" {
  // expected-note@+1 {{already used here}}
  %1 = rtlil.wire "\\p" input port 1 : !rtlil<val[1]>
  // expected-error@+1 {{'rtlil.wire' op reuses port_id 1}}
  %2 = rtlil.wire "\\q" input port 1 : !rtlil<val[1]>
}

// -----

rtlil.module @"\\top" {
  // expected-error@+1 {{'rtlil.instance' op references unknown module '\nope'}}
  "rtlil.instance"() <{name="$i", type=@"\\nope", ports = [], parameters = []}> : () -> ()
}

// -----

rtlil.module @"\\top" {
  %w = rtlil.wire "\\w" : !rtlil<val[8]>
  // expected-error@+1 {{'rtlil.slice' op slice of 4 bits at offset 6 is out of bounds for the 8-bit input}}
  %s = "rtlil.slice"(%w) <{offset = 6 : i32}> : (!rtlil<val[8]>) -> !rtlil<val[4]>
}

// -----

rtlil.module @"\\top" {
  %w = rtlil.wire "\\w" : !rtlil<val[8]>
  // expected-error@+1 {{'rtlil.concat' op result width 8 does not match total operand width 16}}
  %y = "rtlil.concat"(%w, %w) : (!rtlil<val[8]>, !rtlil<val[8]>) -> !rtlil<val[8]>
}

// -----

rtlil.module @"\\top" {
  %a = rtlil.wire "\\a" : !rtlil<val[1]>
  // A parameter name is an RTLIL identifier too, sigil and all.
  // expected-error@+1 {{parameter name 'WIDTH' is not a valid RTLIL identifier}}
  "rtlil.cell"(%a) <{name = "$c", type = "$lut", ports = ["\\A"], parameters = [#rtlil.param<"WIDTH" 4 : i32>]}> : (!rtlil<val[1]>) -> ()
}

// -----

rtlil.module @"\\top" {
  %a = rtlil.wire "\\a" : !rtlil<val[1]>
  // expected-error@+1 {{parameter '\WIDTH' must be an integer, a bit vector or a string}}
  "rtlil.cell"(%a) <{name = "$c", type = "$lut", ports = ["\\A"], parameters = [#rtlil.param<"\\WIDTH" unit>]}> : (!rtlil<val[1]>) -> ()
}

// -----

// `RTLIL::Module::connect` asserts `GetSize(lhs) == GetSize(rhs)`, and the
// constant-folding branch above that assert indexes the RHS over the LHS'
// length, so a short RHS reads out of bounds first.
rtlil.module @"\\top" {
  %a = rtlil.wire "\\a" : !rtlil<val[32]>
  %b = rtlil.wire "\\b" : !rtlil<val[8]>
  // expected-error@+1 {{'rtlil.wconnection' op left-hand side is 32 bits but the right-hand side is 8}}
  "rtlil.wconnection"(%a, %b) : (!rtlil<val[32]>, !rtlil<val[8]>) -> ()
}

// -----

// `fixup_ports()` zeroes the port_id of any wire with no direction flag, so
// this port would silently vanish from `module->ports`.
rtlil.module @"\\top" {
  // expected-error@+1 {{'rtlil.wire' op port_id 1 without input or output flag}}
  %p = rtlil.wire "\\p" port 1 : !rtlil<val[1]>
}

// -----

// `$ports` and `$connections` are index-parallel.
rtlil.module @"\\top" {
  %a = rtlil.wire "\\a" : !rtlil<val[1]>
  // expected-error@+1 {{'rtlil.cell' op has 2 port names but 1 connections; the two are index-parallel}}
  "rtlil.cell"(%a) <{name = "$c", type = "$lut", ports = ["\\A", "\\Y"], parameters = []}> : (!rtlil<val[1]>) -> ()
}

// -----

// A port name is an RTLIL identifier: `setPort` interns it as an `IdString`.
rtlil.module @"\\top" {
  %a = rtlil.wire "\\a" : !rtlil<val[1]>
  // expected-error@+1 {{'rtlil.cell' op port name 'A' is not a valid RTLIL identifier}}
  "rtlil.cell"(%a) <{name = "$c", type = "$lut", ports = ["A"], parameters = []}> : (!rtlil<val[1]>) -> ()
}

// -----

// `connections_` is a dict, so the second `setPort` would drop the first.
rtlil.module @"\\top" {
  %a = rtlil.wire "\\a" : !rtlil<val[1]>
  // expected-error@+1 {{'rtlil.cell' op connects port \A twice, at operands 0 and 1}}
  "rtlil.cell"(%a, %a) <{name = "$c", type = "$lut", ports = ["\\A", "\\A"], parameters = []}> : (!rtlil<val[1]>, !rtlil<val[1]>) -> ()
}

// -----

// An instance may leave a port unconnected, but it may not invent one.
// expected-note@+1 {{module declared here}}
rtlil.module @"\\callee" {
  %a = rtlil.wire "\\a" input port 1 : !rtlil<val[8]>
}
rtlil.module @"\\top" {
  %w = rtlil.wire "\\w" : !rtlil<val[8]>
  // expected-error@+1 {{'rtlil.instance' op connects port '\nope', which module '\callee' does not declare}}
  "rtlil.instance"(%w) <{name="$i", type=@"\\callee", ports = ["\\nope"], parameters = []}> : (!rtlil<val[8]>) -> ()
}

// -----

rtlil.module @"\\callee" {
  // expected-note@+1 {{port declared here}}
  %a = rtlil.wire "\\a" input port 1 : !rtlil<val[8]>
}
rtlil.module @"\\top" {
  %w = rtlil.wire "\\w" : !rtlil<val[32]>
  // expected-error@+1 {{'rtlil.instance' op connects 32 bits to port '\a', which is 8 bits wide}}
  "rtlil.instance"(%w) <{name="$i", type=@"\\callee", ports = ["\\a"], parameters = []}> : (!rtlil<val[32]>) -> ()
}
