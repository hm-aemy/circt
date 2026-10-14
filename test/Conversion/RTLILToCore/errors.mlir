// RUN: circt-opt --convert-rtlil-to-core %s --split-input-file --verify-diagnostics

// expected-error@below {{failed to legalize operation 'rtlil.module'}}
rtlil.module @"\\inout" {
  // expected-error@below {{inout ports are not supported}}
  %a = wire "\\a" inout port 1 : !rtlil<val[1]>
}

// -----

rtlil.module @"\\xconst" {
  %o = wire "\\o" output port 1 : !rtlil<val[2]>
  // expected-error@below {{with bits other than 0 and 1 is not supported}}
  // expected-error@below {{failed to legalize operation 'rtlil.const'}}
  %c = const <"x1"> : !rtlil<val[2]>
  wconnection %o, %c : !rtlil<val[2]>
}

// -----

rtlil.module @"\\unsupported" {
  %a = wire "\\a" input port 1 : !rtlil<val[1]>
  %o = wire "\\o" output port 2 : !rtlil<val[1]>
  // expected-error@below {{failed to legalize operation 'rtlil.sshr'}}
  sshr "$1"(%a, %a, %o) {aSigned = 0 : i32, width = 1 : i32} : !rtlil<val[1]>, !rtlil<val[1]>, !rtlil<val[1]>
}

// -----

rtlil.module @"\\concatboth" {
  %a = wire "\\a" input port 1 : !rtlil<val[1]>
  %b = wire "\\b" output port 2 : !rtlil<val[1]>
  %o = wire "\\o" output port 3 : !rtlil<val[2]>
  // expected-error@below {{is both driven and read}}
  // expected-error@below {{failed to legalize operation 'rtlil.concat'}}
  %cat = concat %a, %b : (!rtlil<val[1]>, !rtlil<val[1]>) -> !rtlil<val[2]>
  wconnection %cat, %o : !rtlil<val[2]>
  wconnection %o, %cat : !rtlil<val[2]>
}
