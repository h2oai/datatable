from __future__ import annotations

from poetry.core.constraints.generic.base_constraint import BaseConstraint
from poetry.core.constraints.generic.constraint import Constraint
from poetry.core.constraints.generic.empty_constraint import EmptyConstraint
from poetry.core.constraints.generic.multi_constraint import MultiConstraint


class UnionConstraint(BaseConstraint):
    def __init__(self, *constraints: BaseConstraint) -> None:
        self._constraints = constraints

    @property
    def constraints(self) -> tuple[BaseConstraint, ...]:
        return self._constraints

    def allows(
        self,
        other: BaseConstraint,
    ) -> bool:
        return any(constraint.allows(other) for constraint in self._constraints)

    def allows_any(self, other: BaseConstraint) -> bool:
        if other.is_empty():
            return False

        if other.is_any():
            return True

        if isinstance(other, (UnionConstraint, MultiConstraint)):
            constraints = other.constraints
        else:
            constraints = (other,)

        return any(
            our_constraint.allows_any(their_constraint)
            for our_constraint in self._constraints
            for their_constraint in constraints
        )

    def allows_all(self, other: BaseConstraint) -> bool:
        if other.is_any():
            return False

        if other.is_empty():
            return True

        if isinstance(other, (UnionConstraint, MultiConstraint)):
            constraints = other.constraints
        else:
            constraints = (other,)

        our_constraints = iter(self._constraints)
        their_constraints = iter(constraints)
        our_constraint = next(our_constraints, None)
        their_constraint = next(their_constraints, None)

        while our_constraint and their_constraint:
            if our_constraint.allows_all(their_constraint):
                their_constraint = next(their_constraints, None)
            else:
                our_constraint = next(our_constraints, None)

        return their_constraint is None

    def intersect(self, other: BaseConstraint) -> BaseConstraint:
        if other.is_any():
            return self

        if other.is_empty():
            return other

        if isinstance(other, Constraint):
            if self.allows(other):
                return other

            return EmptyConstraint()

        # Two remaining cases: an intersection with another union, or an intersection
        # with a multi.
        #
        # In the first case:
        # (A or B) and (C or D) => (A and C) or (A and D) or (B and C) or (B and D)
        #
        # In the second case:
        # (A or B) and (C and D) => (A and C and D) or (B and C and D)
        new_constraints = []
        if isinstance(other, UnionConstraint):
            for our_constraint in self._constraints:
                for their_constraint in other.constraints:
                    intersection = our_constraint.intersect(their_constraint)

                    if (
                        not intersection.is_empty()
                        and intersection not in new_constraints
                    ):
                        new_constraints.append(intersection)

        else:
            assert isinstance(other, MultiConstraint)

            for our_constraint in self._constraints:
                intersection = our_constraint
                for their_constraint in other.constraints:
                    intersection = intersection.intersect(their_constraint)

                if not intersection.is_empty() and intersection not in new_constraints:
                    new_constraints.append(intersection)

        if not new_constraints:
            return EmptyConstraint()

        if len(new_constraints) == 1:
            return new_constraints[0]

        return UnionConstraint(*new_constraints)

    def union(self, other: BaseConstraint) -> UnionConstraint:
        if not isinstance(other, Constraint):
            raise ValueError("Unimplemented constraint union")

        constraints = self._constraints
        if other not in self._constraints:
            constraints += (other,)

        return UnionConstraint(*constraints)

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, UnionConstraint):
            return False

        return set(self._constraints) == set(other._constraints)

    def __hash__(self) -> int:
        h = hash("union")
        for constraint in self._constraints:
            h ^= hash(constraint)

        return h

    def __str__(self) -> str:
        constraints = []
        for constraint in self._constraints:
            constraints.append(str(constraint))

        return " || ".join(constraints)
