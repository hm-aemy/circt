hw.module @Foo(out x : i42) {
  %c0_i42 = hw.constant 0 : i42
  %c1_i42 = hw.constant 1 : i42
  %c2_i42 = hw.constant 2 : i42
  %c3_i42 = hw.constant 3 : i42
  %c42_i42 = hw.constant 42 : i42

  %0 = llhd.combinational -> i42 {
    cf.br ^outerHeader(%c0_i42, %c0_i42 : i42, i42)
  ^outerHeader(%i: i42, %x1: i42):  // 2 preds: ^bb0, ^innerExit
    %1 = comb.icmp slt %i, %c2_i42 : i42
    cf.cond_br %1, ^innerHeader(%c0_i42, %x1 : i42, i42), ^outerExit
  ^innerHeader(%j: i42, %x2: i42):  // 2 preds: ^outerHeader, ^innerBody
    %2 = comb.icmp slt %j, %c3_i42 : i42
    cf.cond_br %2, ^innerBody, ^innerExit
  ^innerBody:  // pred: ^innerHeader
    %7 = comb.add %x2, %c42_i42 : i42
    %jp = comb.add %j, %c1_i42 : i42
    cf.br ^innerHeader(%jp, %7 : i42, i42)
  ^innerExit:  // pred: ^innerHeader
    %ip = comb.add %i, %c1_i42 : i42
    cf.br ^outerHeader(%ip, %x2 : i42, i42)
  ^outerExit:  // pred: ^outerHeader
    llhd.yield %x1 : i42
  }

  hw.output %0 : i42
}
