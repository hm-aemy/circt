//===- RTLILToCore.h - RTLIL to Core conversion pass -------------*- C++
//-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares passes which together will convert RTLIL operations to
// Core operations.
//
//===----------------------------------------------------------------------===//

#ifndef CIRCT_CONVERSION_RTLILTOCORE_H
#define CIRCT_CONVERSION_RTLILTOCORE_H

#include "circt/Support/LLVM.h"

namespace circt {

#define GEN_PASS_DECL_CONVERTRTLILTOCORE
#include "circt/Conversion/Passes.h.inc"

} // namespace circt

#endif // CIRCT_CONVERSION_RTLILTOCORE_H
