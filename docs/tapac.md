# TAPA Compiler

## Overview

The task of `tapac` is to compile a translation unit of a subset of C++ to a set of Verilog files. The translation unit MUST have a top-level task, which corresponds to the top-level module in the generated Verilog files.

## TAPA Tasks

There are two types of tasks: lower-level tasks and upper-level tasks. The top-level task is just a special task marked as the entry point to the tasks.

Read-only scalars, `tapa::istream`, `tapa::ostream`, and `tapa::mmap` are allowed as the interface parameters of tasks. Tasks cannot have return values.

### Lower-Level Task

A lower-level task cannot instantiate `tapa::stream`s or other tasks. It can own the `tapa::mmap`s and read from / write to `tapa::stream`s.

Pipelined loops cannot use blocking reads.

### Upper-Level Task

An upper-level task cannot instantiate anything other than `tapa::stream` and children tasks. It can forward scalars and `tapa::mmap`s to children tasks and instantiate `tapa::stream`s.

Scalars can be forwarded to multiple children tasks. `tapa::mmap`s MUST be forwarded to one and one only one child task.

## Compilation Flow

1. [x] Obtain the list of tasks from the top-level task.
2. [x] Do source-to-source transformation to obtain the HLS code for each task.
3. [x] Send transformed tasks to HLS and obtain the Verilog files.
4. [x] Modify the Verilog files generated for upper-level tasks to connect to the lower-level tasks.
