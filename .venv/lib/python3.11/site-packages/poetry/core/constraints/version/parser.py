from __future__ import annotations

import re

from typing import TYPE_CHECKING

from poetry.core.constraints.version.exceptions import ParseConstraintError
from poetry.core.version.exceptions import InvalidVersion


if TYPE_CHECKING:
    from poetry.core.constraints.version.version_constraint import VersionConstraint


def parse_constraint(constraints: str) -> VersionConstraint:
    if constraints == "*":
        from poetry.core.constraints.version.version_range import VersionRange

        return VersionRange()

    or_constraints = re.split(r"\s*\|\|?\s*", constraints.strip())
    or_groups = []
    for constraints in or_constraints:
        # allow trailing commas for robustness (even though it may not be
        # standard-compliant it seems to occur in some packages)
        constraints = constraints.rstrip(",").rstrip()
        and_constraints = re.split(
            "(?<!^)(?<![~=>< ,]) *(?<!-)[, ](?!-) *(?!,|$)", constraints
        )
        constraint_objects = []

        if len(and_constraints) > 1:
            for constraint in and_constraints:
                constraint_objects.append(parse_single_constraint(constraint))
        else:
            constraint_objects.append(parse_single_constraint(and_constraints[0]))

        if len(constraint_objects) == 1:
            constraint = constraint_objects[0]
        else:
            constraint = constraint_objects[0]
            for next_constraint in constraint_objects[1:]:
                constraint = constraint.intersect(next_constraint)

        or_groups.append(constraint)

    if len(or_groups) == 1:
        return or_groups[0]
    else:
        from poetry.core.constraints.version.version_union import VersionUnion

        return VersionUnion.of(*or_groups)


def parse_single_constraint(constraint: str) -> VersionConstraint:
    from poetry.core.constraints.version.patterns import BASIC_CONSTRAINT
    from poetry.core.constraints.version.patterns import CARET_CONSTRAINT
    from poetry.core.constraints.version.patterns import TILDE_CONSTRAINT
    from poetry.core.constraints.version.patterns import TILDE_PEP440_CONSTRAINT
    from poetry.core.constraints.version.patterns import X_CONSTRAINT
    from poetry.core.constraints.version.version import Version
    from poetry.core.constraints.version.version_range import VersionRange
    from poetry.core.constraints.version.version_union import VersionUnion

    m = re.match(r"(?i)^v?[xX*](\.[xX*])*$", constraint)
    if m:
        return VersionRange()

    # Tilde range
    m = TILDE_CONSTRAINT.match(constraint)
    if m:
        try:
            version = Version.parse(m.group("version"))
        except InvalidVersion as e:
            raise ParseConstraintError(
                f"Could not parse version constraint: {constraint}"
            ) from e

        high = version.stable.next_minor()
        if version.release.precision == 1:
            high = version.stable.next_major()

        return VersionRange(version, high, include_min=True)

    # PEP 440 Tilde range (~=)
    m = TILDE_PEP440_CONSTRAINT.match(constraint)
    if m:
        try:
            version = Version.parse(m.group("version"))
        except InvalidVersion as e:
            raise ParseConstraintError(
                f"Could not parse version constraint: {constraint}"
            ) from e

        if version.release.precision == 2:
            high = version.stable.next_major()
        else:
            high = version.stable.next_minor()

        return VersionRange(version, high, include_min=True)

    # Caret range
    m = CARET_CONSTRAINT.match(constraint)
    if m:
        try:
            version = Version.parse(m.group("version"))
        except InvalidVersion as e:
            raise ParseConstraintError(
                f"Could not parse version constraint: {constraint}"
            ) from e

        return VersionRange(version, version.next_breaking(), include_min=True)

    # X Range
    m = X_CONSTRAINT.match(constraint)
    if m:
        op = m.group("op")
        major = int(m.group(2))
        minor = m.group(3)

        if minor is not None:
            version = Version.from_parts(major, int(minor), 0)
            result: VersionConstraint = VersionRange(
                version, version.next_minor(), include_min=True
            )
        else:
            if major == 0:
                result = VersionRange(max=Version.from_parts(1, 0, 0))
            else:
                version = Version.from_parts(major, 0, 0)

                result = VersionRange(version, version.next_major(), include_min=True)

        if op == "!=":
            result = VersionRange().difference(result)

        return result

    # Basic comparator
    m = BASIC_CONSTRAINT.match(constraint)
    if m:
        op = m.group("op")
        version_string = m.group("version")

        if version_string == "dev":
            version_string = "0.0-dev"

        try:
            version = Version.parse(version_string)
        except InvalidVersion as e:
            raise ParseConstraintError(
                f"Could not parse version constraint: {constraint}"
            ) from e

        if op == "<":
            return VersionRange(max=version)
        if op == "<=":
            return VersionRange(max=version, include_max=True)
        if op == ">":
            return VersionRange(min=version)
        if op == ">=":
            return VersionRange(min=version, include_min=True)
        if op == "!=":
            return VersionUnion(VersionRange(max=version), VersionRange(min=version))
        return version

    raise ParseConstraintError(f"Could not parse version constraint: {constraint}")
