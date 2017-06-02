#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import os
import re
import subprocess
from llvmlite import binding
from datatable.utils.terminal import term

__all__ = ("inject_c_code", )



def inject_c_code(cc, func_names):
    try:
        llvmir = _c_to_llvm(cc)
        _compile_llvmir(_engine, llvmir)
    except RuntimeError as e:
        mm = re.search(r"<stdin>:(\d+):(\d+):\s*(.*)", e.args[0])
        if mm:
            lineno = int(mm.group(1))
            charno = int(mm.group(2))
            lines = cc.split("\n")
            for i in range(len(lines)):
                print(lines[i])
                if i == lineno - 1:
                    print(" " * (charno - 1) +
                          term.bold("^--- ") +
                          term.bright_red(mm.group(3)))
        else:
            print("\nError while trying to compile this code:\n")
            print(cc)
            print()
        raise e
    return [_engine.get_function_address(f) for f in func_names]


def _create_execution_engine():
    """
    Create an ExecutionEngine suitable for JIT code generation on the host
    CPU. The engine is reusable for any number of modules.
    """
    # Initialization...
    binding.initialize()
    binding.initialize_native_target()
    binding.initialize_native_asmprinter()
    # Create a target machine representing the host
    target = binding.Target.from_default_triple()
    target_machine = target.create_target_machine()
    # And an execution engine with an empty backing module
    backing_mod = binding.parse_assembly("")
    engine = binding.create_mcjit_compiler(backing_mod, target_machine)
    return engine


def _c_to_llvm(code):
    if "LLVM4" in os.environ:
        clang = "%s/bin/clang" % os.path.expanduser(os.environ["LLVM4"])
        if not os.path.isfile(clang):
            raise RuntimeError("File %s not found. Please ensure that variable "
                               "LLVM4 points to a valid Clang+Llvm-4.0 "
                               "installation folder" % clang)
    else:
        raise RuntimeError("Environment variable LLVM4 is not set. This "
                           "variable should point to the Clang+Llvm-4.0 "
                           "installation folder.")
    proc = subprocess.Popen(args=[clang, "-x", "c", "-S", "-emit-llvm",
                                  "-o", "-", "-"],
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    out, err = proc.communicate(input=code.encode())
    if err:
        raise RuntimeError("LLVM compilation error:\n" + err.decode())
    return out.decode()



def _compile_llvmir(engine, code):
    m = binding.parse_assembly(code)
    m.verify()
    engine.add_module(m)
    engine.finalize_object()
    return m


_engine = _create_execution_engine()
