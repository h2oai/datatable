import contextlib
import inspect
import os
import sys
import tempfile
import textwrap
import xml.etree.ElementTree as ET
from collections import defaultdict
from pathlib import Path
from typing import Any, Dict, Iterator, List
from xml.sax.saxutils import escape

from . import n
from .diagnostics import Diagnostic
from .page import Page
from .parser import Project
from .types import BuildIdentifierSet, FileId, SerializableType

__all__ = ("eprint", "ast_to_testing_string", "assert_etree_equals")


def eprint(*args: str) -> None:
    """print() to stderr."""
    print(*args, file=sys.stderr)


class FinalAssertionError(AssertionError):
    """An AssertionError whose details were already logged to stderr."""

    pass


def ast_to_testing_string(ast: Any) -> str:
    """Create an XML string representation of an AST node."""
    if isinstance(ast, n.Node):
        ast = ast.serialize()

    value = ast.get("value", "")
    children = ast.get("children", [])
    attr_pairs = [
        (k, v)
        for k, v in ast.items()
        if k
        not in ("argument", "term", "value", "children", "type", "position", "options")
        and v
    ]

    attr_pairs.extend((k, v) for k, v in ast.get("options", {}).items())
    attrs = " ".join('{}="{}"'.format(k, escape(str(v))) for k, v in attr_pairs)
    contents = (
        escape(value)
        if value
        else (
            "".join(ast_to_testing_string(child) for child in children)
            if children
            else ""
        )
    )
    if "argument" in ast:
        contents = (
            "".join(ast_to_testing_string(part) for part in ast["argument"]) + contents
        )

    if "term" in ast:
        contents = (
            "<term>"
            + "".join(ast_to_testing_string(part) for part in ast["term"])
            + "</term>"
            + contents
        )

    return "<{}{}>{}</{}>".format(
        ast["type"], " " + attrs if attrs else "", contents, ast["type"]
    )


def assert_etree_equals(e1: ET.Element, goal: ET.Element) -> None:
    """Assert that two XML Elements are the same. If there is a difference in a child,
       log the difference to stderr."""
    assert e1.tag == goal.tag
    if e1.text and goal.text:
        assert (e1.text.strip() if e1.text else "") == (
            goal.text.strip() if goal.text else ""
        )

    # Comparing the tail is interesting because we want to consider
    # "<whitespace>" and None to be equivalent. Coerce None to an empty
    # string, and strip both sides.
    assert (e1.tail or "").strip() == (goal.tail or "").strip()

    assert e1.attrib == goal.attrib
    assert len(e1) == len(goal)
    for c1, goalc in zip(e1, goal):
        try:
            assert_etree_equals(c1, goalc)
        except AssertionError as err:
            # If the assertion has already been logged, don't do it again.
            if isinstance(err, FinalAssertionError):
                raise err

            # Report this tree diff to stderr.
            wrapper = textwrap.TextWrapper(
                width=100, initial_indent="  ", subsequent_indent="  "
            )
            eprint(
                "{}\n{}\nshould be\n{}".format(
                    err,
                    "\n".join(wrapper.wrap(ET.tostring(c1, encoding="unicode"))),
                    "\n".join(wrapper.wrap(ET.tostring(goalc, encoding="unicode"))),
                )
            )

            # Inform higher stack frames not to log this exception
            raise FinalAssertionError(err)


def check_ast_testing_string(ast: Any, testing_string: str) -> None:
    """Ensure that an AST node matches the given testing XML string, using ast_to_testing_string()."""
    correct_tree = ET.fromstring(testing_string)
    evaluating_tree = ET.fromstring(ast_to_testing_string(ast))
    assert_etree_equals(evaluating_tree, correct_tree)


class BackendTestResults:
    """A utility class for tracking the output of a build in tests."""

    def __init__(self) -> None:
        self.diagnostics: Dict[FileId, List[Diagnostic]] = defaultdict(list)
        self.pages: Dict[FileId, Page] = {}

    def on_progress(self, progress: int, total: int, message: str) -> None:
        pass

    def on_diagnostics(self, path: FileId, diagnostics: List[Diagnostic]) -> None:
        self.diagnostics[path].extend(diagnostics)

    def on_update(
        self,
        prefix: List[str],
        build_identifiers: BuildIdentifierSet,
        page_id: FileId,
        page: Page,
    ) -> None:
        self.pages[page_id] = page

    def on_update_metadata(
        self,
        prefix: List[str],
        build_identifiers: BuildIdentifierSet,
        field: Dict[str, SerializableType],
    ) -> None:
        pass

    def on_delete(self, page_id: FileId, build_identifiers: BuildIdentifierSet) -> None:
        pass

    def flush(self) -> None:
        pass


@contextlib.contextmanager
def make_test(files: Dict[Path, str], name: str = "") -> Iterator[BackendTestResults]:
    """Create a temporary test project with the given files."""
    need_to_make_snooty_toml = Path("snooty.toml") not in files
    if need_to_make_snooty_toml:
        # Create a reasonable name for this project: go with the caller name
        name = inspect.stack()[2].function

    with tempfile.TemporaryDirectory(prefix=f"{name}-") as tempdir:
        root = Path(tempdir)
        if need_to_make_snooty_toml:
            root.joinpath("snooty.toml").write_text(f'name = "{name}"\n')

        for filename, file_text in files.items():
            assert not filename.is_absolute()
            file_path = root.joinpath(filename)
            if file_path.parent != root:
                os.makedirs(file_path.parent, exist_ok=True)
            file_path.write_text(file_text)

        backend = BackendTestResults()

        project = Project(root, backend, {})
        project.build()

    yield backend
