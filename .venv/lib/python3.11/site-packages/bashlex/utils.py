try:
    from collections.abc import MutableSet, Mapping
except ImportError:
    # Python 2 fallback
    from collections import MutableSet, Mapping


class typedset(MutableSet):
    def __init__(self, type_, iterable=[]):
        self._s = set()
        self._type = type_
        for v in iterable:
            self.add(v)

    def add(self, value):
        if not isinstance(value, self._type):
            raise ValueError('can only add items of type %s to this set' % self._type)
        self._s.add(value)

    def discard(self, value):
        self._s.discard(value)

    def __contains__(self, value):
        return self._s.__contains__(value)

    def __iter__(self):
        return self._s.__iter__()

    def __len__(self):
        return len(self._s)

    def __and__(self, value):
        if isinstance(value, self._type):
            value = set([value])
        return self._s.__and__(value)

    def __or__(self, value):
        if isinstance(value, self._type):
            value = set([value])
        return self._s.__or__(value)

    def __ior__(self, value):
        if isinstance(value, self._type):
            value = set([value])
        self._s.__ior__(value)
        return self

    #def __sub__(self, value):
    #    if isinstance(value, self._type):
    #        value = set([value])
    #    return self._s.__sub__(value)

    def __repr__(self):
        return self._s.__repr__()

class frozendict(Mapping):
    def __init__(self, *args, **kwargs):
        self.__dict = dict(*args, **kwargs)
        self.__hash = None

    def __getitem__(self, key):
        return self.__dict[key]

    def copy(self, **add_or_replace):
        return frozendict(self, **add_or_replace)

    def __iter__(self):
        return iter(self.__dict)

    def __len__(self):
        return len(self.__dict)

    def __repr__(self):
        return '<frozendict %s>' % repr(self.__dict)
