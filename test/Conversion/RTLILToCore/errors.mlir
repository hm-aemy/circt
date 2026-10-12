// RUN: circt-opt --convert-rtlil-to-core %s --verify-diagnostics

// Pass not yet implemented, adding test for structure.
// expected-error@below {{RTLIL to Core conversion not implemented yet}}
module {
}
