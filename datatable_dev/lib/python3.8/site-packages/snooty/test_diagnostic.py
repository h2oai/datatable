import pytest
from docutils.frontend import OptionParser

from .diagnostics import Diagnostic, UnexpectedIndentation
from .language_server import DiagnosticSeverity


def test_diagnostics() -> None:
    diagnostic = UnexpectedIndentation((0, 0), 10)
    assert isinstance(diagnostic, UnexpectedIndentation)
    assert diagnostic.severity == Diagnostic.Level.error
    assert diagnostic.start == (0, 0)
    assert diagnostic.end[0] == 10 and diagnostic.end[1] > 100

    # Make sure attempts to access abstract Diagnostic base class
    # results in TypeError
    with pytest.raises(TypeError):
        Diagnostic("foo", (0, 0), 10).severity


def test_conversion() -> None:
    original_levels = [
        OptionParser.thresholds["info"],
        OptionParser.thresholds["warning"],
        OptionParser.thresholds["error"],
        OptionParser.thresholds["severe"],
    ]
    snooty_levels = [Diagnostic.Level.from_docutils(level) for level in original_levels]
    lsp_levels = [DiagnosticSeverity.from_diagnostic(level) for level in snooty_levels]

    assert original_levels == [1, 2, 3, 4]
    assert snooty_levels == [1, 2, 3, 3]
    assert lsp_levels == [3, 2, 1, 1]
