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
    A common view of `RTLIL::Cell`, so that any RTLIL cell op can be walked
    without knowing which one it is. Implemented by `rtlil.cell`, the typed
    cell ops and `rtlil.instance`.

    A port and the connection it names share an index, which the interface
    verifier enforces.
  }];
  let cppNamespace = "::circt::rtlil";
  let verify = [{
    return ::circt::rtlil::verifyCellOpInterface($_op);
  }];
  let methods = [
    InterfaceMethod<[{
      Gets the name of the cell op.
    }], "::llvm::StringRef", "getCellName", (ins),
    /*methodBody*/ "",
    /*defaultImpl*/ [{
      return $_op.getName();
    }]>,
    InterfaceMethod<[{
      Gets the type of the cell op. For `rtlil.instance` this is the name of
      the module it refers to.
    }], "::llvm::StringRef", "getCellType", (ins),
    "", [{
      return $_op.getType();
    }]>,
    InterfaceMethod<[{
      Gets the connections of the cell op.
    }], "::mlir::OperandRange", "getCellConnections", (ins),
    "", [{
      return $_op.getConnections();
    }]>,
    InterfaceMethod<[{
      Gets the ports of the cell op.
    }], "::mlir::ArrayAttr", "getCellPorts", (ins),
    "", [{
      return $_op.getPorts();
    }]>,
    InterfaceMethod<[{
      Gets the parameters of the cell op.
    }], "::mlir::ArrayAttr", "getCellParameters", (ins),
    "", [{
      return $_op.getParameters();
    }]>
  ];
}

#endif // CIRCT_DIALECT_RTLIL_RTLILOPINTERFACES_TD
