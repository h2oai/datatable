from __future__ import annotations

from typing import TYPE_CHECKING

from poetry.core.constraints.version.empty_constraint import EmptyConstraint
from poetry.core.constraints.version.version_constraint import VersionConstraint
from poetry.core.constraints.version.version_range_constraint import (
    VersionRangeConstraint,
)


if TYPE_CHECKING:
    from poetry.core.constraints.version.version import Version


class VersionUnion(VersionConstraint):
    """
    A version constraint representing a union of multiple disjoint version
    ranges.

    An instance of this will only be created if the version can't be represented
    as a non-compound value.
    """

    def __init__(self, *ranges: VersionRangeConstraint) -> None:
        self._ranges = list(ranges)

    @property
    def ranges(self) -> list[VersionRangeConstraint]:
        return self._ranges

    @classmethod
    def of(cls, *ranges: VersionConstraint) -> VersionConstraint:
        from poetry.core.constraints.version.version_range import VersionRange

        flattened: list[VersionRangeConstraint] = []
        for constraint in ranges:
            if constraint.is_empty():
                continue

            if isinstance(constraint, VersionUnion):
                flattened += constraint.ranges
                continue

            assert isinstance(constraint, VersionRangeConstraint)
            flattened.append(constraint)

        if not flattened:
            return EmptyConstraint()

        if any([constraint.is_any() for constraint in flattened]):
            return VersionRange()

        # Only allow Versions and VersionRanges here so we can more easily reason
        # about everything in flattened. _EmptyVersions and VersionUnions are
        # filtered out above.
        for constraint in flattened:
            if not isinstance(constraint, VersionRangeConstraint):
                raise ValueError(f"Unknown VersionConstraint type {constraint}.")

        flattened.sort()

        merged: list[VersionRangeConstraint] = []
        for constraint in flattened:
            # Merge this constraint with the previous one, but only if they touch.
            if not merged or (
                not merged[-1].allows_any(constraint)
                and not merged[-1].is_adjacent_to(constraint)
            ):
                merged.append(constraint)
            else:
                new_constraint = merged[-1].union(constraint)
                assert isinstance(new_constraint, VersionRangeConstraint)
                merged[-1] = new_constraint

        if len(merged) == 1:
            return merged[0]

        return VersionUnion(*merged)

    def is_empty(self) -> bool:
        return False

    def is_any(self) -> bool:
        return False

    def is_simple(self) -> bool:
        return self.excludes_single_version()

    def allows(self, version: Version) -> bool:
        return any([constraint.allows(version) for constraint in self._ranges])

    def allows_all(self, other: VersionConstraint) -> bool:
        our_ranges = iter(self._ranges)
        their_ranges = iter(other.flatten())

        our_current_range = next(our_ranges, None)
        their_current_range = next(their_ranges, None)

        while our_current_range and their_current_range:
            if our_current_range.allows_all(their_current_range):
                their_current_range = next(their_ranges, None)
            else:
                our_current_range = next(our_ranges, None)

        return their_current_range is None

    def allows_any(self, other: VersionConstraint) -> bool:
        our_ranges = iter(self._ranges)
        their_ranges = iter(other.flatten())

        our_current_range = next(our_ranges, None)
        their_current_range = next(their_ranges, None)

        while our_current_range and their_current_range:
            if our_current_range.allows_any(their_current_range):
                return True

            if their_current_range.allows_higher(our_current_range):
                our_current_range = next(our_ranges, None)
            else:
                their_current_range = next(their_ranges, None)

        return False

    def intersect(self, other: VersionConstraint) -> VersionConstraint:
        our_ranges = iter(self._ranges)
        their_ranges = iter(other.flatten())
        new_ranges = []

        our_current_range = next(our_ranges, None)
        their_current_range = next(their_ranges, None)

        while our_current_range and their_current_range:
            intersection = our_current_range.intersect(their_current_range)

            if not intersection.is_empty():
                new_ranges.append(intersection)

            if their_current_range.allows_higher(our_current_range):
                our_current_range = next(our_ranges, None)
            else:
                their_current_range = next(their_ranges, None)

        return VersionUnion.of(*new_ranges)

    def union(self, other: VersionConstraint) -> VersionConstraint:
        return VersionUnion.of(self, other)

    def difference(self, other: VersionConstraint) -> VersionConstraint:
        our_ranges = iter(self._ranges)
        their_ranges = iter(other.flatten())
        new_ranges: list[VersionConstraint] = []

        state = {
            "current": next(our_ranges, None),
            "their_range": next(their_ranges, None),
        }

        def their_next_range() -> bool:
            state["their_range"] = next(their_ranges, None)
            if state["their_range"]:
                return True

            assert state["current"] is not None
            new_ranges.append(state["current"])
            our_current = next(our_ranges, None)
            while our_current:
                new_ranges.append(our_current)
                our_current = next(our_ranges, None)

            return False

        def our_next_range(include_current: bool = True) -> bool:
            if include_current:
                assert state["current"] is not None
                new_ranges.append(state["current"])

            our_current = next(our_ranges, None)
            if not our_current:
                return False

            state["current"] = our_current

            return True

        while True:
            if state["their_range"] is None:
                break

            assert state["current"] is not None
            if state["their_range"].is_strictly_lower(state["current"]):
                if not their_next_range():
                    break

                continue

            if state["their_range"].is_strictly_higher(state["current"]):
                if not our_next_range():
                    break

                continue

            difference = state["current"].difference(state["their_range"])
            if isinstance(difference, VersionUnion):
                assert len(difference.ranges) == 2
                new_ranges.append(difference.ranges[0])
                state["current"] = difference.ranges[-1]

                if not their_next_range():
                    break
            elif difference.is_empty():
                if not our_next_range(False):
                    break
            else:
                assert isinstance(difference, VersionRangeConstraint)
                state["current"] = difference

                if state["current"].allows_higher(state["their_range"]):
                    if not their_next_range():
                        break
                else:
                    if not our_next_range():
                        break

        if not new_ranges:
            return EmptyConstraint()

        if len(new_ranges) == 1:
            return new_ranges[0]

        return VersionUnion.of(*new_ranges)

    def flatten(self) -> list[VersionRangeConstraint]:
        return self.ranges

    def _exclude_single_wildcard_range_string(self) -> str:
        """
        Helper method to convert this instance into a wild card range
        string.
        """
        if not self.excludes_single_wildcard_range():
            raise ValueError("Not a valid wildcard range")

        # we assume here that since it is a single exclusion range
        # that it is one of "< 2.0.0 || >= 2.1.0" or ">= 2.1.0 || < 2.0.0"
        # and the one with the max is the first part
        idx_order = (0, 1) if self._ranges[0].max else (1, 0)
        one = self._ranges[idx_order[0]].max
        assert one is not None
        two = self._ranges[idx_order[1]].min
        assert two is not None

        # versions can have both semver and non semver parts
        parts_one = [
            one.major,
            one.minor or 0,
            one.patch or 0,
            *list(one.non_semver_parts or []),
        ]
        parts_two = [
            two.major,
            two.minor or 0,
            two.patch or 0,
            *list(two.non_semver_parts or []),
        ]

        # we assume here that a wildcard range implies that the part following the
        # first part that is different in the second range is the wildcard, this means
        # that multiple wildcards are not supported right now.
        parts = []

        for idx, part in enumerate(parts_one):
            parts.append(str(part))
            if parts_two[idx] != part:
                # since this part is different the next one is the wildcard
                # for example, "< 2.0.0 || >= 2.1.0" gets us a wildcard range
                # 2.0.*
                parts.append("*")
                break
        else:
            # we should not ever get here, however it is likely that poorly
            # constructed metadata exists
            raise ValueError("Not a valid wildcard range")

        return f"!={'.'.join(parts)}"

    @staticmethod
    def _excludes_single_wildcard_range_check_is_valid_range(
        one: VersionRangeConstraint, two: VersionRangeConstraint
    ) -> bool:
        """
        Helper method to determine if two versions define a single wildcard range.

        In cases where !=2.0.* was parsed by us, the union is of the range
        <2.0.0 || >=2.1.0. In user defined ranges, precision might be different.
        For example, a union <2.0 || >= 2.1.0 is still !=2.0.*. In order to
        handle these cases we make sure that if precisions do not match, extra
        checks are performed to validate that the constraint is a valid single
        wildcard range.
        """

        assert one.max is not None
        assert two.min is not None

        max_precision = max(one.max.precision, two.min.precision)

        if max_precision <= 3:
            # In cases where both versions have a precision less than 3,
            # we can make use of the next major/minor/patch versions.
            return two.min in {
                one.max.next_major(),
                one.max.next_minor(),
                one.max.next_patch(),
            }
        else:
            # When there are non-semver parts in one of the versions, we need to
            # ensure we use zero padded version and in addition to next major/minor/
            # patch versions, also check each next release for the extra parts.
            from_parts = one.max.__class__.from_parts

            _extras: list[list[int]] = []
            _versions: list[Version] = []

            for _version in [one.max, two.min]:
                _extra = list(_version.non_semver_parts or [])

                while len(_extra) < (max_precision - 3):
                    # pad zeros for extra parts to ensure precisions are equal
                    _extra.append(0)

                # create a new release with unspecified parts padded with zeros
                _padded_version: Version = from_parts(
                    major=_version.major,
                    minor=_version.minor or 0,
                    patch=_version.patch or 0,
                    extra=tuple(_extra),
                )

                _extras.append(_extra)
                _versions.append(_padded_version)

            _extra_one = _extras[0]
            _padded_version_one = _versions[0]
            _padded_version_two = _versions[1]

            _check_versions = {
                _padded_version_one.next_major(),
                _padded_version_one.next_minor(),
                _padded_version_one.next_patch(),
            }

            # for each non-semver (extra) part, bump a version
            for idx in range(len(_extra_one)):
                _extra = [
                    *_extra_one[: idx - 1],
                    (_extra_one[idx] + 1),
                    *_extra_one[idx + 1 :],
                ]
                _check_versions.add(
                    from_parts(
                        _padded_version_one.major,
                        _padded_version_one.minor,
                        _padded_version_one.patch,
                        tuple(_extra),
                    )
                )

            return _padded_version_two in _check_versions

    def excludes_single_wildcard_range(self) -> bool:
        from poetry.core.constraints.version.version_range import VersionRange

        if len(self._ranges) != 2:
            return False

        idx_order = (0, 1) if self._ranges[0].max else (1, 0)
        one = self._ranges[idx_order[0]]
        two = self._ranges[idx_order[1]]

        is_range_exclusion = (
            one.max and not one.include_max and two.min and two.include_min
        )

        if not is_range_exclusion:
            return False

        if not self._excludes_single_wildcard_range_check_is_valid_range(one, two):
            return False

        return isinstance(VersionRange().difference(self), VersionRange)

    def excludes_single_version(self) -> bool:
        from poetry.core.constraints.version.version import Version
        from poetry.core.constraints.version.version_range import VersionRange

        return isinstance(VersionRange().difference(self), Version)

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, VersionUnion):
            return False

        return self._ranges == other.ranges

    def __hash__(self) -> int:
        h = hash(self._ranges[0])

        for range in self._ranges[1:]:
            h ^= hash(range)

        return h

    def __str__(self) -> str:
        from poetry.core.constraints.version.version_range import VersionRange

        if self.excludes_single_version():
            return f"!={VersionRange().difference(self)}"

        try:
            return self._exclude_single_wildcard_range_string()
        except ValueError:
            return " || ".join([str(r) for r in self._ranges])
