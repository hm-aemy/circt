// RUN: circt-opt --convert-hw-to-rtlil %s --verify-diagnostics --split-input-file

// RTLIL has no wildcard-compare cell. The case predicates `ceq`/`cne` do have
// one, `$eqx`/`$nex`.
hw.module @top(in %x : i32, in %y : i32, in %z : i32, in %select: i1) {
  %res = comb.icmp bin wne %y, %z : i32
  // expected-error@-1 {{failed to legalize operation 'comb.icmp' that was explicitly marked illegal}}
}

// -----

hw.module @top(in %y : i32, in %z : i32) {
  %res = comb.icmp weq %y, %z : i32
  // expected-error@-1 {{failed to legalize operation 'comb.icmp' that was explicitly marked illegal}}
}

// -----

hw.module @top(in %x : i4, in %y : i4, in %z : i4, in %clk: !seq.clock) {
  // expected-error@+1 {{failed to legalize operation 'seq.initial' that was explicitly marked illegal}}
  %init0, %init1, %init2 = seq.initial () {
    %cst1 = hw.constant 1 : i4
    %cst2 = hw.constant 2 : i4
    %cst3 = hw.constant 3 : i4
    seq.yield %cst1, %cst2, %cst3 : i4, i4, i4
  } : () -> (!seq.immutable<i4>, !seq.immutable<i4>, !seq.immutable<i4>)
  %res = seq.compreg %z, %clk initial %init0 : i4
}

// -----

hw.module @top(in %x : i4, in %y : i4, in %z : i4, in %clk: !seq.clock) {
  %res = seq.firreg %z clock %clk preset 12 : i4
  // expected-error@-1 {{failed to legalize operation 'seq.firreg' that was explicitly marked illegal}}
}
// -----

hw.module @sub<W: i32 = 7>(in %a : i8, out b : i8) {
  hw.output %a : i8
}
hw.module @top(in %x : i8, out y : i8) {
  // expected-error@+1 {{'hw.instance' op has parameters, which the rtlil dialect cannot represent on a module with a body; run 'hw-specialize' first}}
  %0 = hw.instance "i0" @sub<W: i32 = 7>(a: %x: i8) -> (b: i8)
  hw.output %0 : i8
}

// -----

// expected-note@+1 {{module declared here}}
hw.module.extern @BB(in %a : i8, out b : i8)
hw.module @top(in %x : i8, out y : i8) {
  // expected-error@+1 {{'hw.instance' op instantiates 'BB', which has no body; the rtlil dialect cannot represent extern or generated modules yet}}
  %0 = hw.instance "i0" @BB(a: %x: i8) -> (b: i8)
  hw.output %0 : i8
}

// -----

// A zero-width value has no `!rtlil.val`: Yosys drops a zero-width signal
// rather than carrying it, so `ImportRTLIL` rejects one too. Failing the
// legalization here keeps the two ends agreeing.
hw.module @top(in %x : i8, out y : i8) {
  // expected-error@+1 {{failed to legalize operation 'comb.extract' that was explicitly marked illegal}}
  %0 = comb.extract %x from 0 : (i8) -> i0
  %1 = comb.concat %x, %0 : i8, i0
  hw.output %1 : i8
}

// -----

// Zero-width ports have no RTLIL representation.
// expected-error@below {{port 'x' has type 'i0', which has no RTLIL representation}}
hw.module @zero_width_input(in %x : i0, in %y : i1, out z : i1) {
  hw.output %y : i1
}

// -----

// expected-error@below {{port 'y' has type 'i0', which has no RTLIL representation}}
hw.module @zero_width_output(in %x : i1, out y : i0) {
  %c = hw.constant 0 : i0
  hw.output %c : i0
}

// -----

// A register of zero width cannot be converted.
hw.module @zero_width_reg(in %clk : !seq.clock, in %d : i1) {
  %c = hw.constant 0 : i0
  // expected-error@below {{failed to legalize operation 'seq.compreg' that was explicitly marked illegal}}
  %r = seq.compreg %c, %clk : i0
}
