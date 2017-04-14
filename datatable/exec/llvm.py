#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import os
import re
import subprocess
from llvmlite import binding

__all__ = ("inject_c_code", )



def inject_c_code(cc, func_names):
    try:
        llvm = _fix_clang_llvm(_c_to_llvm(cc))
        _compile_llvmir(_engine, llvm)
    except RuntimeError as e:
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
    clang = os.environ.get("CLANG", "clang")
    proc = subprocess.Popen(args=[clang, "-x", "c", "-S", "-emit-llvm",
                                  "-o", "-", "-"],
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    out, err = proc.communicate(input=code.encode())
    if err:
        raise RuntimeError("LLVM compilation error:\n" + err.decode())
    return out.decode()


def _fix_clang_llvm(llvm):
    return re.sub(r"(load|getelementptr inbounds) (\%?[\w.]+\**)\*",
                  r"\1 \2, \2*", llvm)


def _compile_llvmir(engine, code):
    m = binding.parse_assembly(code)
    m.verify()
    engine.add_module(m)
    engine.finalize_object()
    return m


_engine = _create_execution_engine()
