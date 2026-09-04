autoidx 1
attribute \src "in.sv:1.1-1.1"
module \top
  wire width 4 input 1 \a
  wire width 4 output 2 \y
  wire width 0 \z
  connect \y \a
  connect \z \z
end
