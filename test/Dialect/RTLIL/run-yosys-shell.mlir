// RUN: not circt-opt %s --rtlil-run-yosys='script={!echo hi}' 2>&1 \
// RUN:   | FileCheck %s --check-prefix=BANG
// RUN: not circt-opt %s --rtlil-run-yosys='script={opt; exec -- echo hi}' 2>&1 \
// RUN:   | FileCheck %s --check-prefix=EXEC
// REQUIRES: libyosys

// A pass option must not run shell commands, so both ways Yosys offers to do
// that are rejected before any command runs.
// BANG: error: Yosys command '!echo hi' runs a shell command, which is not supported
// EXEC: error: Yosys command 'exec -- echo hi' runs a shell command, which is not supported
module {
  rtlil.module @"\\demo" {
    %0 = rtlil.wire "\\a" input port 1 : !rtlil<val[8]>
  }
}
