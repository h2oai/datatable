from dataclasses import dataclass
from pathlib import Path, PurePath
from typing import Dict, List, Optional

from ..diagnostics import Diagnostic
from ..types import ProjectConfig
from . import nodes
from .release import GizaReleaseSpecificationCategory


@dataclass
class Child:
    foo: str


@dataclass
class Parent:
    foo2: str


@dataclass
class SubstitutionTest(Parent):
    foo: str
    child: Child


def test_substitution() -> None:
    diagnostics: List[Diagnostic] = []
    replacements = {"verb": "test", "noun": "substitution"}
    test_string = r"{{verb}}ing {{noun}}. {{verb}}."
    substituted_string = "testing substitution. test."
    assert (
        nodes.substitute_text(test_string, replacements, diagnostics)
        == substituted_string
    )

    obj = object()
    assert nodes.substitute(obj, replacements, diagnostics) is obj

    # Test complex substitution
    node = SubstitutionTest(foo=test_string, foo2=test_string, child=Child(test_string))
    substituted_node = nodes.substitute(node, replacements, diagnostics)
    assert substituted_node == SubstitutionTest(
        foo=substituted_string, foo2=substituted_string, child=Child(substituted_string)
    )

    # Make sure that no substitution == ''
    assert diagnostics == []
    del replacements["noun"]
    assert (
        nodes.substitute_text(test_string, replacements, diagnostics)
        == "testing . test."
    )
    assert len(diagnostics) == 1

    # Ensure the identity of the zero-substitutions case remains the same
    diagnostics = []
    test_string = "foo"
    assert nodes.substitute_text(test_string, {}, diagnostics) is test_string
    assert not diagnostics


def test_inheritance() -> None:
    @dataclass
    class TestNode(nodes.Inheritable):
        content: Optional[str]

    project_config, diagnostics = ProjectConfig.open(Path("test_data"))
    parent = TestNode(
        ref="_parent",
        replacement={"foo": "bar", "old": ""},
        source=None,
        inherit=None,
        content="{{bar}}",
    )
    child = TestNode(
        ref="child",
        replacement={"bar": "baz", "old": "new"},
        source=nodes.Inherit("self.yaml", "parent"),
        inherit=None,
        content=None,
    )
    parent = nodes.inherit(project_config, parent, None, diagnostics)
    child = nodes.inherit(project_config, child, parent, diagnostics)

    assert child.replacement == {"foo": "bar", "bar": "baz", "old": "new"}
    assert child.content == "baz"
    assert not diagnostics


def test_reify_all_files() -> None:
    """Test to see if repeated refs in a YAML are detected"""
    project_config = ProjectConfig(Path("test_data"), "")
    project_config.constants["version"] = "3.4"

    # Place good path and bad path here
    paths = (
        "test_data/test_gizaparser/source/includes/release-base.yaml",
        "test_data/test_gizaparser/source/includes/release-base-repeat.yaml",
    )

    for i, path in enumerate(paths):
        test_path = Path(path)

        # Change GizaCategory based on file type (steps, release, etc.)
        category = GizaReleaseSpecificationCategory(project_config)
        all_diagnostics: Dict[PurePath, List[Diagnostic]] = {}

        extracts, text, diagnostics = category.parse(test_path)
        category.add(test_path, text, extracts)
        all_diagnostics[test_path] = diagnostics

        file_id, giza_node = next(category.reify_all_files(all_diagnostics))

        if i % 2:
            assert len(all_diagnostics[test_path]) > 0
        else:
            assert len(all_diagnostics[test_path]) == 0
