from __future__ import annotations

from abc import abstractmethod
from typing import TYPE_CHECKING

from poetry.core.constraints.version.version_constraint import VersionConstraint


if TYPE_CHECKING:
    from poetry.core.constraints.version.version import Version


class VersionRangeConstraint(VersionConstraint):
    @property
    @abstractmethod
    def min(self) -> Version | None:
        raise NotImplementedError()

    @property
    @abstractmethod
    def max(self) -> Version | None:
        raise NotImplementedError()

    @property
    @abstractmethod
    def full_max(self) -> Version | None:
        raise NotImplementedError()

    @property
    @abstractmethod
    def include_min(self) -> bool:
        raise NotImplementedError()

    @property
    @abstractmethod
    def include_max(self) -> bool:
        raise NotImplementedError()

    def allows_lower(self, other: VersionRangeConstraint) -> bool:
        if self.min is None:
            return other.min is not None

        if other.min is None:
            return False

        if self.min < other.min:
            return True

        if self.min > other.min:
            return False

        return self.include_min and not other.include_min

    def allows_higher(self, other: VersionRangeConstraint) -> bool:
        if self.full_max is None:
            return other.max is not None

        if other.full_max is None:
            return False

        if self.full_max < other.full_max:
            return False

        if self.full_max > other.full_max:
            return True

        return self.include_max and not other.include_max

    def is_strictly_lower(self, other: VersionRangeConstraint) -> bool:
        if self.full_max is None or other.min is None:
            return False

        if self.full_max < other.min:
            return True

        if self.full_max > other.min:
            return False

        return not self.include_max or not other.include_min

    def is_strictly_higher(self, other: VersionRangeConstraint) -> bool:
        return other.is_strictly_lower(self)

    def is_adjacent_to(self, other: VersionRangeConstraint) -> bool:
        if self.max != other.min:
            return False

        return (
            self.include_max
            and not other.include_min
            or not self.include_max
            and other.include_min
        )
