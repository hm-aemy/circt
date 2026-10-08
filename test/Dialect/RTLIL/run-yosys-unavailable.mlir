// RUN: circt-opt %s --rtlil-run-yosys --verify-diagnostics
// UNSUPPORTED: libyosys

// Without libyosys the pass is still registered, but only reports an error.

// expected-error@below {{rtlil-run-yosys is not available: CIRCT was built without libyosys}}
module {
}
