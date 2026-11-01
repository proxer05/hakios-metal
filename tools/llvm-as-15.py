#!/usr/bin/env python3
"""llvm-as via llvmlite (LLVM 15): the AIR front end's bitcode reader reads
LLVM 14/15-era records; the host's llvm-as 23 emits newer ones.

usage: llvm-as-15.py -o out.bc in.ll
"""
import sys

import llvmlite.binding as llvm

args = sys.argv[1:]
out = None
src = None
i = 0
while i < len(args):
    if args[i] == '-o':
        out = args[i + 1]
        i += 2
    else:
        src = args[i]
        i += 1

# initialization is automatic in llvmlite 0.44+
mod = llvm.parse_assembly(open(src).read())
mod.verify()
open(out, 'wb').write(mod.as_bitcode())
