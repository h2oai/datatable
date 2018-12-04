#!/usr/bin/env python3
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import os
import re
import subprocess
from datatable.utils.terminal import term
from datatable.utils.typechecks import TValueError

__all__ = ("llvm", )



class Llvm:
    __slots__ = ["_clang", "_engine", "_binding", "_initialized"]

    def __init__(self):
        self._clang = None
        self._engine = None
        self._binding = None
        self._initialized = False


    #---------------------------------------------------------------------------
    # Public API
    #---------------------------------------------------------------------------

    @property
    def available(self):
        self._init_all()
        return bool(self._clang)


    def jit(self, cc, func_names):
        self._init_all()
        try:
            llvmir = self._c_to_llvm(cc)
            self._compile_llvmir(llvmir)
        except RuntimeError as e:
            mm = re.search(r"<stdin>:(\d+):(\d+):\s*(.*)", e.args[0])
            if mm:
                lineno = int(mm.group(1))
                charno = int(mm.group(2))
                lines = cc.split("\n")
                for i, line in enumerate(lines):
                    print(line)
                    if i == lineno - 1:
                        print(" " * (charno - 1) +
                              term.bold("^--- ") +
                              term.bright_red(mm.group(3)))
            else:
                print("\nError while trying to compile this code:\n")
                print(cc)
                print()
            raise e
        return [self._engine.get_function_address(f) for f in func_names]


    #---------------------------------------------------------------------------
    # Initialization
    #---------------------------------------------------------------------------

    def _init_all(self):
        if not self._initialized:
            self._initialized = True
            try:
                self._clang = self._find_clang()
                self._engine = self._create_execution_engine()
            except:
                self._clang = None
                self._engine = None

    def _find_clang(self):
        for evar in ["LLVM4", "LLVM5", "LLVM6"]:
            if evar not in os.environ:
                curdir = os.path.dirname(os.path.abspath(__file__))
                d = os.path.abspath(os.path.join(curdir, "..", evar.lower()))
                if os.path.isdir(d):
                    os.environ[evar] = d
            if evar in os.environ:
                clang = "%s/bin/clang" % os.path.expanduser(os.environ[evar])
                if os.path.isfile(clang):
                    return clang
        return None

    def _create_execution_engine(self):
        """
        Create an ExecutionEngine suitable for JIT code generation on the host
        CPU. The engine is reusable for any number of modules.
        """
        from llvmlite import binding
        if not self._clang:
            return None
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
        self._binding = binding
        return engine


    #---------------------------------------------------------------------------
    # Compiling and executing C code
    #---------------------------------------------------------------------------

    def _c_to_llvm(self, code):
        if self._clang is None:
            raise TValueError("LLVM execution engine is not available")
        proc = subprocess.Popen(args=[self._clang, "-x", "c", "-S",
                                      "-emit-llvm", "-o", "-", "-"],
                                stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT)
        out, err = proc.communicate(input=code.encode())
        if err:
            raise RuntimeError("LLVM compilation error:\n" + err.decode())
        return out.decode()


    def _compile_llvmir(self, llvm_code):
        m = self._binding.parse_assembly(llvm_code)
        m.verify()
        self._engine.add_module(m)
        self._engine.finalize_object()
        return m


llvm = Llvm()
