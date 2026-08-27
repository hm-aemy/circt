// RUN: circt-opt %s --rtlil-run-yosys='script=no_such_pass' --verify-diagnostics
// REQUIRES: libyosys

// A bad script must produce a diagnostic rather than ending the process.
//
// This is the only test that notices if `-fexceptions` stopped reaching
// `YosysScript.cpp`: `log_cmd_error` falls back to `log_error` -> `_Exit(1)`
// when the throw is not set up or not caught, and circt-opt would then exit
// with no diagnostic at all rather than failing this check.

// The failing command is named, and Yosys' own log is attached as a note so
// there is something to debug with.
// expected-error@below {{yosys command 'no_such_pass' failed: No such command: no_such_pass}}
// expected-note@below {{yosys log:}}
module {
  rtlil.module @"\\demo" {
    %0 = "rtlil.wire"() <{name = "\\a", is_signed = false, port_id = 1 : i32, port_input = true}> : () -> !rtlil<val[8]>
  }
}
