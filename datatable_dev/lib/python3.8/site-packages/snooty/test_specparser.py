import pytest

from . import specparser


def test_load() -> None:
    spec = specparser.Spec.loads(
        """
    [meta]
    version = 0

    [enum]
    user_level = ["beginner", "intermediate", "advanced"]

    [directive._parent]
    content_type = "block"
    options.foo = ["path", "uri"]

    [directive.child]
    inherit = "_parent"
    argument_type = "user_level"
    deprecated = true

    [role._parent]
    help = "test-role"
    type = "text"

    [role.child]
    inherit = "_parent"

    [rstobject._parent]
    help = "test-rstobject"

    [rstobject.child]
    inherit = "_parent"
    """
    )

    assert spec.meta.version == 0
    assert spec.enum["user_level"] == ["beginner", "intermediate", "advanced"]
    assert spec.directive["child"] == specparser.Directive(
        inherit="_parent",
        example=None,
        help=None,
        content_type="block",
        argument_type="user_level",
        required_context=None,
        deprecated=True,
        domain=None,
        options={"foo": [specparser.PrimitiveType.path, specparser.PrimitiveType.uri]},
        name="child",
    )

    # Test these in the opposite order of the definition to ensure that each "type" of definition
    # has a separate inheritance namespace
    assert spec.rstobject["child"].help == "test-rstobject"
    assert spec.role["child"].help == "test-role"
    assert spec.role["child"].type == specparser.PrimitiveRoleType.text

    validator = spec.get_validator(
        [specparser.PrimitiveType.nonnegative_integer, "user_level"]
    )
    assert validator("10") == 10
    assert validator("intermediate") == "intermediate"
    with pytest.raises(ValueError):
        validator("-10")
    with pytest.raises(ValueError):
        validator("foo")


def test_inheritance_cycle() -> None:
    with pytest.raises(ValueError):
        specparser.Spec.loads(
            """
        [meta]
        version = 0

        [directive.parent]
        inherit = "child"

        [directive.child]
        inherit = "parent"
        """
        )


def test_missing_parent() -> None:
    with pytest.raises(ValueError):
        specparser.Spec.loads(
            """
        [meta]
        version = 0

        [directive._parent]
        content_type = "block"

        [directive.child]
        inherit = "parent"
        """
        )


def test_bad_type() -> None:
    spec = specparser.Spec.loads(
        """
    [meta]
    version = 0
    """
    )

    with pytest.raises(ValueError):
        spec.get_validator("gjriojwe")


def test_bad_version() -> None:
    with pytest.raises(ValueError):
        specparser.Spec.loads(
            """
        [meta]
        version = -1"""
        )

    with pytest.raises(ValueError):
        specparser.Spec.loads(
            """
        [meta]
        version = 1"""
        )
