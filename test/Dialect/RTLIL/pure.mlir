// RUN: circt-opt %s --cse | FileCheck %s

// `rtlil.const` is Pure, so the usual passes may merge two identical constants
// and drop one nothing uses. Both are safe: export materialises a constant
// from its defining op at each use rather than emitting it once as a signal.

// CHECK-LABEL: rtlil.module @"\\top"
rtlil.module @"\\top" {
  %w = rtlil.wire "\\w" : !rtlil<val[4]>

  // CHECK: %[[C:.+]] = const <"1010"> : [4]
  // CHECK-NOT: const
  %a = rtlil.const <"1010"> : !rtlil<val[4]>
  %b = rtlil.const <"1010"> : !rtlil<val[4]>
  %unused = rtlil.const <"1111"> : !rtlil<val[4]>

  // CHECK: "rtlil.wconnection"(%{{.+}}, %[[C]])
  // CHECK: "rtlil.wconnection"(%{{.+}}, %[[C]])
  "rtlil.wconnection"(%w, %a) : (!rtlil<val[4]>, !rtlil<val[4]>) -> ()
  "rtlil.wconnection"(%w, %b) : (!rtlil<val[4]>, !rtlil<val[4]>) -> ()
}
