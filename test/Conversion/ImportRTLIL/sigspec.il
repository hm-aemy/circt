autoidx 1
module \spec
  wire width 8 input 1 \w
  wire width 6 output 2 \y
  wire width 1 \bit
  # A cell port that is neither a whole wire nor a whole constant: bits [6:3]
  # of \w concatenated with two constant bits. This is the ordinary shape after
  # opt/techmap/abc, and the reason rtlil.slice and rtlil.concat exist.
  cell $not $c
    parameter \A_SIGNED 0
    parameter \A_WIDTH 6
    parameter \Y_WIDTH 6
    connect \A { \w [6:3] 2'01 }
    connect \Y \y
  end
  # A single-bit slice stays a slice; a whole-wire chunk stays bare.
  connect \bit \w [2]
end
