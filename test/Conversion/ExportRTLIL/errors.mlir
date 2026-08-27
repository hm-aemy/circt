// RUN: circt-translate --export-rtlil --split-input-file --verify-diagnostics %s
// REQUIRES: libyosys

// Everything here would make Yosys call `log_error`, which ends the process
// with no diagnostic at all. Each case has to come out as an ordinary MLIR
// error instead -- the regression being guarded against is a silent exit, so
// these assert on the message rather than just on failure.
//
// The first three are caught by the `rtlil.module` verifier before the exporter
// runs. The exporter re-checks them anyway, since it must not assume it was
// handed verified IR, but under `circt-translate` the verifier gets there first.

rtlil.module @"\\top" {
  // expected-note@+1 {{previously declared here}}
  %1 = "rtlil.wire"() <{name="$dup", is_signed = false}> : () -> !rtlil<val[1]>
  // expected-error@+1 {{redeclares the RTLIL name $dup}}
  %2 = "rtlil.wire"() <{name="$dup", is_signed = false}> : () -> !rtlil<val[1]>
}

// -----

// A name with no sigil: `RTLIL::IdString` asserts on this.
// expected-error@+1 {{'rtlil.module' op name nosigil is not a valid RTLIL identifier}}
rtlil.module @nosigil {
}

// -----

// A space in an identifier: Yosys rejects any byte at or below a space.
rtlil.module @"\\top" {
  // expected-error@+1 {{'rtlil.wire' op name \a b is not a valid RTLIL identifier}}
  %1 = "rtlil.wire"() <{name="\\a b", is_signed = false}> : () -> !rtlil<val[1]>
}

// -----

// Two modules of the same name. `Design::addModule` asserts on this, but it
// never gets the chance: `rtlil.module` is a `Symbol` in the top-level module's
// symbol table, so MLIR rejects the duplicate first. The exporter checks anyway,
// for callers that build a design programmatically rather than by parsing.
// expected-note@+1 {{see existing symbol definition here}}
rtlil.module @"\\dup" {
}
// expected-error@+1 {{redefinition of symbol named '\dup'}}
rtlil.module @"\\dup" {
}
