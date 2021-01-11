import enum
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple, Union

from . import n
from .n import SerializableType


class MakeCorrectionMixin:
    def did_you_mean(self) -> List[str]:
        """Suggest one or more possible corrections to the reStructuredText that this
           diagnostic is about."""
        raise NotImplementedError()


class Diagnostic:
    def __init__(
        self,
        message: str,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        self.message = message

        if isinstance(start, int):
            start_line, start_column = start, 0
        else:
            start_line, start_column = start
        self.start = (start_line, start_column)

        if end is None:
            end_line, end_column = start_line, 1000
        elif isinstance(end, int):
            end_line, end_column = end, 1000
        else:
            end_line, end_column = end
        self.end = (end_line, end_column)

    class Level(enum.IntEnum):
        info = 1
        warning = 2
        error = 3

        @classmethod
        def from_docutils(cls, level: int) -> "Diagnostic.Level":
            level = max(level, cls.info)
            level = min(level, cls.error)
            return cls(level)

    @property
    def severity(self) -> "Diagnostic.Level":
        raise TypeError("Cannot access the severity of an abstract base Diagnostic")

    @property
    def severity_string(self) -> str:
        return self.severity.name.title()

    def serialize(self) -> n.SerializedNode:
        """Create dict containing diagnostic attributes for neatly reporting diagnostics at program completion"""
        diag: Dict[str, SerializableType] = {}
        diag["severity"] = self.severity_string.upper()
        diag["start"] = self.start[0]
        diag["message"] = self.message
        return diag


class UnexpectedIndentation(Diagnostic, MakeCorrectionMixin):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__("Unexpected indentation", start, end)

    def did_you_mean(self) -> List[str]:
        return [".. blockquote::"]


class InvalidURL(Diagnostic):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__("Invalid URL", start, end)


class ExpectedPathArg(Diagnostic):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        name: str,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__(f'"{name}" expected a path argument', start, end)
        self.name = name


class UnnamedPage(Diagnostic):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        filename: str,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__(f"Page title not found: {filename}", start, end)
        self.filename = filename


class ExpectedImageArg(Diagnostic):
    severity = Diagnostic.Level.error


class ImageSuggested(Diagnostic):
    severity = Diagnostic.Level.info

    def __init__(
        self,
        name: str,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__(f'"{name}" expected an image argument', start, end)
        self.name = name


class InvalidField(Diagnostic):
    severity = Diagnostic.Level.error


class GitMergeConflictArtifactFound(Diagnostic):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        path: Path,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__(
            f"Git merge conflict artifact found in {str(path)} on line {str(start)}",
            start,
            end,
        )
        self.path = path


class DocUtilsParseError(Diagnostic):
    severity = Diagnostic.Level.warning


class ErrorParsingYAMLFile(Diagnostic):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        path: Optional[Path],
        reason: str,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        message = (
            f"Error parsing YAML file {str(path)}: {reason}"
            if path
            else f"Error parsing YAML: {reason}"
        )
        super().__init__(message, start, end)
        self.path = path
        self.reason = reason


class InvalidInclude(Diagnostic):
    severity = Diagnostic.Level.error


class InvalidLiteralInclude(Diagnostic):
    severity = Diagnostic.Level.error


class SubstitutionRefError(Diagnostic):
    severity = Diagnostic.Level.error


class ConstantNotDeclared(Diagnostic):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        name: str,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__(f"{name} not defined as a source constant", start, end)
        self.name = name


class InvalidTableStructure(Diagnostic):
    severity = Diagnostic.Level.error


class MissingOption(Diagnostic):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__("'.. option::' must follow '.. program::'", start, end)


class MissingRef(Diagnostic):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        name: str,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__(f"Missing ref; all {name} must define a ref", start, end)
        self.name = name


class MalformedGlossary(Diagnostic):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__(
            "Malformed glossary: glossary must contain only a definition list",
            start,
            end,
        )


class FailedToInheritRef(Diagnostic):
    severity = Diagnostic.Level.error


class RefAlreadyExists(Diagnostic):
    severity = Diagnostic.Level.error


class UnknownSubstitution(Diagnostic):
    severity = Diagnostic.Level.warning


class TargetNotFound(Diagnostic):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        name: str,
        target: str,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__(f'Target not found: "{name}:{target}"', start, end)
        self.name = name
        self.target = target


class AmbiguousTarget(Diagnostic):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        name: str,
        target: str,
        candidates: List[str],
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__(
            f'Ambiguous target: "{name}:{target}". Locations: {", ".join(candidates)}',
            start,
            end,
        )
        self.name = name
        self.target = target
        self.candidates = candidates


class TodoInfo(Diagnostic):
    severity = Diagnostic.Level.info


class UnmarshallingError(Diagnostic):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        reason: str,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__(f"Unmarshalling Error: {reason}", start, end)
        self.reason = reason


class CannotOpenFile(Diagnostic):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        path: Path,
        reason: str,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__(f"Error opening {str(path)}: {reason}", start, end)
        self.path = path
        self.reason = reason


class CannotRenderOpenAPI(Diagnostic):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        path: Path,
        reason: str,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__(
            f"Failed to render OpenAPI template for {str(path)}: {reason}", start, end
        )
        self.path = path
        self.reason = reason


class MissingTocTreeEntry(Diagnostic):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        entry: str,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__(f"Could not locate toctree entry {entry}", start, end)
        self.entry = entry


class UnknownTabset(Diagnostic):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        tabset: str,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__(
            f"""Tabset "{tabset}" is not defined in rstspec.toml""", start, end
        )
        self.tabset = tabset


class UnknownTabID(Diagnostic):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        tabid: str,
        tabset: str,
        reason: str,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__(
            f"""tab id "{tabid}" given in "{tabset}" tabset is unrecognized: {reason}""",
            start,
            end,
        )
        self.tabid = tabid
        self.tabset = tabset
        self.reason = reason


class TabMustBeDirective(Diagnostic):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        tab_type: str,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__(
            f"Tab sets may only contain tab directives, but found {tab_type}",
            start,
            end,
        )
        self.tab_type = tab_type


class IncorrectMonospaceSyntax(Diagnostic, MakeCorrectionMixin):
    severity = Diagnostic.Level.warning

    def __init__(
        self,
        text: str,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__("Monospace text uses two backticks (``)", start, end)
        self.text = text

    def did_you_mean(self) -> List[str]:
        return [f"``{self.text}``"]


class IncorrectLinkSyntax(Diagnostic, MakeCorrectionMixin):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        parts: Tuple[str, str],
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__("Malformed external link", start, end)
        self.parts = parts

    def did_you_mean(self) -> List[str]:
        return [f"`{self.parts[0]} <{self.parts[1]}>`__"]


class MissingTab(Diagnostic):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        tabs: Set[str],
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__(
            f"One or more set of tabs on this page was missing the following tab(s): {tabs}",
            start,
            end,
        )
        self.tabs = tabs


class ExpectedTabs(Diagnostic):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__(
            "Expected tabs directive when tabs-selector directive in use", start, end,
        )


class DuplicateDirective(Diagnostic):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        name: str,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__(
            f"""Directive "{name}" should only appear once per page""", start, end,
        )
        self.name = name


class RemovedLiteralBlockSyntax(Diagnostic):
    severity = Diagnostic.Level.error

    def __init__(
        self,
        start: Union[int, Tuple[int, int]],
        end: Union[None, int, Tuple[int, int]] = None,
    ) -> None:
        super().__init__(
            "Literal block syntax is unsupported; use a code-block directive instead",
            start,
            end,
        )
