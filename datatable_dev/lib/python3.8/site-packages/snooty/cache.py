from collections import defaultdict
from dataclasses import dataclass, field
from typing import DefaultDict, Dict, Generic, Iterator, Optional, Set, Tuple, TypeVar

_T = TypeVar("_T")


@dataclass
class Cache(Generic[_T]):
    """A versioned cache that associates a (_T, int) pair with an arbitrary object and
       an integer version. Whenever the key is re-assigned, the version is incremented."""

    _cache: Dict[Tuple[_T, int], object] = field(default_factory=dict)
    _keys_of_each_fileid: DefaultDict[_T, Set[int]] = field(
        default_factory=lambda: defaultdict(set)
    )
    _versions: DefaultDict[Tuple[_T, int], int] = field(
        default_factory=lambda: defaultdict(int)
    )

    def __setitem__(self, key: Tuple[_T, int], value: object) -> None:
        if key in self._cache:
            self._cache[key] = value
        else:
            self._cache[key] = value

        self._versions[key] += 1
        self._keys_of_each_fileid[key[0]].add(key[1])

    def __delitem__(self, fileid: _T) -> None:
        keys = self._keys_of_each_fileid[fileid]
        del self._keys_of_each_fileid[fileid]
        for key in keys:
            del self._cache[(fileid, key)]

    def __getitem__(self, key: Tuple[_T, int]) -> Optional[object]:
        return self._cache.get(key, None)

    def get_versions(self, fileid: _T) -> Iterator[int]:
        for key, version in self._versions.items():
            if key[0] == fileid:
                yield version
