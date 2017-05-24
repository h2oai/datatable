#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-



class Node(object):
    """
    Single node within a datatable evaluation graph.
    """

    def __init__(self):
        self._soup = None  # type: NodeSoup


    @property
    def soup(self):
        if self._soup is None:
            raise RuntimeError("Node %s was not added to a NodeSoup"
                               % self.__class__.__name__)
        return self._soup

    @soup.setter
    def soup(self, s):
        self._soup = s
        self._added_into_soup()


    def _added_into_soup(self):
        pass

    def stir(self):
        pass

    def get_result(self):
        raise NotImplementedError("%s.get_result() is not implemented"
                                  % self.__class__.__name__)
