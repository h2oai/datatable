#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-

from .node import Node
from .context import CModuleNode



# Perhaps this should be moved into the 'exec' folder
class NodeSoup(object):
    """
    Replacement for :class:`EvaluationModule`.

    Stages to prepare a soup:

    start
        Initial step: add all component nodes into the common evaluation
        context (which is this class).

    stir
        Connect nodes together into the evaluation graph, optimize the graph
        according to the types of nodes used.

    stew
        If necessary, generate C code to aid with evaluation of the graph, then
        compile the code and insert into the runtime.

    serve
        Actually evaluate the graph, and produce the final result.
    """

    #---------------------------------------------------------------------------
    # Scramble phase
    #---------------------------------------------------------------------------

    def __init__(self, verbose=False):
        # If this flag is set, then print diagnostic information about the
        # inner workings of the datatable evaluation process.
        self._verbose = verbose

        # Hashmap of all the nodes in the soup. The names of these nodes should
        # reflect their functionality: for example, the `rows` argument of the
        # datatable() call produces a :class:`RowFilterNode` which is added to
        # the soup under the key "rows". Then any other node that needs to query
        # this node can find it in the soup by this name. The names are also
        # used to eliminate common subexpressions: for example if multiple
        # columns refer to node "mean(f.colA)" then only a single node will be
        # added to the soup and computed, and all the columns will reuse this
        # result.
        self._nodes = {}


    def add(self, key, node):
        """Add a node into the soup."""
        assert key not in self._nodes, "%s is already in the soup" % key
        assert isinstance(node, Node)
        self._nodes[key] = node
        node.soup = self


    #---------------------------------------------------------------------------
    # Stir phase
    #---------------------------------------------------------------------------

    def stir(self):
        assert self.has("result"), "'result' node was not added to the soup"
        for node in self._nodes.values():
            node.stir()


    def has(self, key):
        return key in self._nodes


    def get(self, key):
        if key == "c" and key not in self._nodes:
            self.add("c", CModuleNode())
        return self._nodes.get(key, None)



    #---------------------------------------------------------------------------
    # Stew phase
    #---------------------------------------------------------------------------

    def stew(self):
        result_node = self.get("result")
        return result_node.get_result()
