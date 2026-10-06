//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the pass which converts the HW, Comb and Seq dialects to
// the RTLIL dialect.
//
//===----------------------------------------------------------------------===//

#ifndef CIRCT_CONVERSION_HWTORTLIL_H
#define CIRCT_CONVERSION_HWTORTLIL_H

#include "circt/Support/LLVM.h"

namespace circt {

#define GEN_PASS_DECL_CONVERTHWTORTLIL
#include "circt/Conversion/Passes.h.inc"

} // namespace circt

#endif // CIRCT_CONVERSION_HWTORTLIL_H
