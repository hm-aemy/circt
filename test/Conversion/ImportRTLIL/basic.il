autoidx 1
attribute \top 1
module \demo
  attribute \src "demo.v:3.1-3.9"
  wire width 8 input 1 \a
  wire width 8 input 2 \b
  wire width 8 output 3 \y
  wire width 8 $tmp
  cell $and $c
    parameter \A_SIGNED 0
    parameter \B_SIGNED 0
    parameter \A_WIDTH 8
    parameter \B_WIDTH 8
    parameter \Y_WIDTH 8
    connect \A \a
    connect \B \b
    connect \Y $tmp
  end
  connect \y $tmp
end
