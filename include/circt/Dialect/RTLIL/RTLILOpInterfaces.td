//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef CIRCT_DIALECT_RTLIL_RTLILOPINTERFACES_TD
#define CIRCT_DIALECT_RTLIL_RTLILOPINTERFACES_TD

include "mlir/IR/OpBase.td"

def CellOpInterface : OpInterface<"CellOpInterface"> {
  let description = [{
    A common view of `RTLIL::Cell`, implemented by `rtlil.cell`,
    `rtlil.instance` and the typed cell ops.

    The verifier checks that ports and connections line up by index, and that
    each port is a valid RTLIL identifier named at most once.
  }];
  let cppNamespace = "::circt::rtlil";
  let verify = [{
    return ::circt::rtlil::verifyCellOpInterface($_op);
  }];
  let methods = [
    InterfaceMethod<[{
      Returns the cell name.
    }], "::llvm::StringRef", "getCellName", (ins),
    /*methodBody*/ "",
    /*defaultImpl*/ [{
      return $_op.getName();
    }]>,
    InterfaceMethod<[{
      Returns the cell type, which for `rtlil.instance` is the module name.
    }], "::llvm::StringRef", "getCellType", (ins),
    "", [{
      return $_op.getType();
    }]>,
    InterfaceMethod<[{
      Returns the connections, index-parallel to the ports.
    }], "::mlir::OperandRange", "getCellConnections", (ins),
    "", [{
      return $_op.getConnections();
    }]>,
    InterfaceMethod<[{
      Returns the port names.
    }], "::mlir::ArrayAttr", "getCellPorts", (ins),
    "", [{
      return $_op.getPorts();
    }]>,
    InterfaceMethod<[{
      Returns the parameters.
    }], "::mlir::ArrayAttr", "getCellParameters", (ins),
    "", [{
      return $_op.getParameters();
    }]>
  ];
}

#endif // CIRCT_DIALECT_RTLIL_RTLILOPINTERFACES_TD
