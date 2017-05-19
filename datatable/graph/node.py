#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from .context import EvaluationContext


class Node(object):
    """
    Single node within a datatable evaluation graph.
    """

    def __init__(self):
        self._context = None  # type: EvaluationContext


    def use_context(self, context):
        assert isinstance(context, EvaluationContext)
        assert self._context is None, "Attempt to set context for a second time"
        self._context = context
        self.on_context_set()


    def on_context_set(self):
        pass


    @property
    def context(self):
        assert self._context, ("Context was not set on a node of class %s"
                               % self.__class__.__name__)
        return self._context
