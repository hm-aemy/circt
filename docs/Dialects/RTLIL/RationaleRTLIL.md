# RTLIL Dialect Rationale

This document describes the intention of the RTLIL dialect, the design
decisions, the current status and the outline for future enhancements.
This follows in the spirit of other
[MLIR Rationale docs](https://mlir.llvm.org/docs/Rationale/).

[TOC]

## Introduction to the RTLIL Dialect

The RTLIL dialect serves as an exchange dialect between CIRCT and Yosys.
For its internal representation, Yosys uses the
[RTL Intermediate Language (RTLIL)](https://yosyshq.readthedocs.io/projects/yosys/en/latest/yosys_internals/formats/rtlil_rep.html).
The RTLIL dialect reflects the design of this representation.
In a simplistic view, RTLIL consists of modules which contain cells connected by
wires. A cell is a container for logic and memory elements, specified by the
type and further specialized with attributes.
By mirroring the Yosys RTLIL IR in the CIRCT RTLIL dialect, the translation
between the representations is straightforward.
To convert a design to the RTLIL dialect, a pass transforms core dialect
operations into RTLIL operations.

## Design considerations

In Yosys RTLIL, logic and memory elements are represented by cells. The type of
the cell is stored in a string parameter. The RTLIL dialect defines the
operation `CellOp` to represent this general cell and uses `CellOpInterface` to
define common methods to work with a cell. Concrete operations like `MuxOp` use
this interface to allow for a common access to the arguments, while defining
distinct operations which access type arguments to the correct string value.

Yosys uses multi-valued logic. This is expressed with `MValueType`, which is a
bit array of states, where each state can be of: low, high, unknown,
high-impedance or don't care.

## Core to RTLIL Lowering

Yosys requires unique naming across all elements, this is achieved by
using a global counter in the lowering pass to make each name unique.

## Current Status and Roadmap

In a first step, support for a limited number of `seq` and `comb` operations for
RTLIL is implemented to demonstrate the working principal. Support for more
operations will be added over time.

The RTLIL dialect serves as an exchange dialect to and from Yosys, the
conversion from RTLIL to the core dialects will follow.

Additionally, it is planned to extend Yosys RTLIL with properties in order to
support non-functional information annotation. This can then be used to pass
along information from other CIRCT dialects, for example to facilitate formal
verification. By allowing an export to and import from Yosys, this will make it
possible to exchange further design information which can be used for
optimization and analysis of the design.
