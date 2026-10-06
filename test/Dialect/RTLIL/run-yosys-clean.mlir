// RUN: circt-opt %s --rtlil-run-yosys='hierarchy=false script={log x;;}' \
// RUN:   | FileCheck %s --check-prefix=CLEAN
// RUN: circt-opt %s --rtlil-run-yosys='hierarchy=false script={log x;}' \
// RUN:   | FileCheck %s --check-prefix=KEEP
// REQUIRES: libyosys

// As in Yosys, a command ending in `;;` is followed by `clean`, which removes
// the unused internal wire. A single `;` only ends the command.

rtlil.module @"\\demo" {
  %0 = rtlil.wire "$unused" : !rtlil<val[1]>
}

// CLEAN-LABEL: rtlil.module @"\\demo"
// CLEAN-NOT: wire "$unused"

// KEEP-LABEL: rtlil.module @"\\demo"
// KEEP: wire "$unused"
