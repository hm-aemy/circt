autoidx 1
attribute \src "in.sv:1.1-1.1"
module \top
  wire width 4 input 1 \a
  wire width 0 \z
  attribute \src "in.sv:7.1-7.1"
  cell $and $c
    parameter \A_SIGNED 0
    parameter \A_WIDTH 4
    parameter \B_WIDTH 4
    parameter \Y_WIDTH 4
    connect \A \a
    connect \B \z
    connect \Y \a
  end
end
