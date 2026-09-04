// A `SigSpec` is not an `AttrObject`, so it has no `src` of its own and its
// location is necessarily the enclosing cell's or, for a module-level
// connection, the module's. That location is shared by every signal under it,
// so the diagnostic has to name the signal itself.
// REQUIRES: libyosys

// The cell's own `src` locates it, and the message says which port.
// RUN: not circt-translate --import-rtlil %S/zero-width-cell.il 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CELL
// CELL: in.sv:7:1: error: port \B of cell $c is a zero-width signal, which the rtlil dialect cannot represent

// A connection has no `src` at all and Yosys does not name it, so the index
// within the module is the only handle on which one failed. Both sides are
// reported: the import walks them before checking either.
// RUN: not circt-translate --import-rtlil %S/zero-width-connection.il 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CONN
// CONN-DAG: in.sv:1:1: error: the left-hand side of connection 1 of module \top is a zero-width signal
// CONN-DAG: in.sv:1:1: error: the right-hand side of connection 1 of module \top is a zero-width signal
