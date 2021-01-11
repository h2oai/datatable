import hashlib
import logging
import os.path
import re
from dataclasses import dataclass, field
from pathlib import Path, PurePath
from typing import Dict, List, Match, MutableSequence, Optional, Tuple

import toml
from typing_extensions import Protocol

from . import n
from .diagnostics import (
    ConstantNotDeclared,
    Diagnostic,
    GitMergeConflictArtifactFound,
    UnmarshallingError,
)
from .flutter import LoadError, check_type, checked
from .n import FileId as FD
from .n import SerializableType as ST

SerializableType = ST
FileId = FD
PAT_VARIABLE = re.compile(r"{\+([\w-]+)\+}")
PAT_GIT_MARKER = re.compile(r"^<<<<<<< .*?^=======\n.*?^>>>>>>>", re.M | re.S)
BuildIdentifierSet = Dict[str, Optional[str]]
logger = logging.getLogger(__name__)


class EmbeddedRstParser(Protocol):
    def parse_block(self, text: str, lineno: int) -> MutableSequence[n.Node]:
        ...

    def parse_inline(self, text: str, lineno: int) -> MutableSequence[n.InlineNode]:
        ...


def normalize_target(target: str) -> str:
    """Normalize targets to allow easy matching against the target
       database: normalize whitespace."""
    return re.sub(r"\s+", " ", target)


class SnootyError(Exception):
    pass


@dataclass
class StaticAsset:
    # "key" must *exactly* match an identifier with which this asset is referred to in source text.
    key: str

    fileid: FileId
    path: Path
    upload: bool
    _checksum: Optional[str]
    _data: Optional[bytes]

    def __hash__(self) -> int:
        return hash(self.fileid)

    def __eq__(self, other: object) -> bool:
        return isinstance(other, StaticAsset) and self.fileid == other.fileid

    def get_checksum(self) -> str:
        self.__load()
        assert self._checksum is not None
        return self._checksum

    def can_upload(self) -> bool:
        """Return True iff the file exists and it's of a file type which should be uploaded
           (e.g. an image)."""
        try:
            self.__load()
        except OSError:
            return False

        return self.upload

    @property
    def data(self) -> bytes:
        self.__load()
        assert self._data is not None
        return self._data

    @classmethod
    def load(
        cls, key: str, fileid: FileId, path: Path, upload: bool = False
    ) -> "StaticAsset":
        return cls(key, fileid, path, upload, None, None)

    def __load(self) -> None:
        if self._data is None:
            self._data = self.path.read_bytes()
            self._checksum = hashlib.blake2b(self._data, digest_size=32).hexdigest()


@checked
@dataclass
class ProjectConfig:
    root: Path
    name: str
    fail_on_diagnostics: bool = field(default=False)
    default_domain: Optional[str] = field(default=None)
    title: str = field(default="untitled")
    source: str = field(default="source")
    constants: Dict[str, object] = field(default_factory=dict)
    deprecated_versions: Optional[Dict[str, List[str]]] = field(default=None)
    intersphinx: List[str] = field(default_factory=list)
    substitutions: Dict[str, str] = field(default_factory=dict)
    # substitution_nodes contains a parsed representation of the substitutions member, and is populated on Project initialization.
    substitution_nodes: Dict[str, List[n.InlineNode]] = field(default_factory=dict)
    toc_landing_pages: List[str] = field(default_factory=list)
    page_groups: Dict[str, List[str]] = field(default_factory=dict)

    @property
    def source_path(self) -> Path:
        result = self.root.joinpath(self.source)
        if os.path.exists(result):
            return result
        # added support for projects that do not have "source" folder.
        self.source = ""
        return self.root

    @property
    def config_path(self) -> Path:
        return self.root.joinpath("snooty.toml")

    def get_fileid(self, path: PurePath) -> FileId:
        result = PurePath(os.path.relpath(path, self.source_path))
        # Ensure that we transform any Windows-style paths into a Posix-style FileId
        return FileId(*result.parts)

    @classmethod
    def open(cls, root: Path) -> Tuple["ProjectConfig", List[Diagnostic]]:
        path = root
        diagnostics: List[Diagnostic] = []
        while path.parent != path:
            try:
                with path.joinpath("snooty.toml").open(encoding="utf-8") as f:
                    data = toml.load(f)
                    data["root"] = path
                    result, parsed_diagnostics = check_type(
                        ProjectConfig, data
                    ).render_constants()
                    return result, parsed_diagnostics
            except FileNotFoundError:
                pass
            except LoadError as err:
                diagnostics.append(UnmarshallingError(str(err), 0))

            path = path.parent

        return cls(root, name="unnamed"), diagnostics

    def render_constants(self) -> Tuple["ProjectConfig", List[Diagnostic]]:
        if not self.constants:
            return self, []
        constants: Dict[str, object] = {}
        all_diagnostics: List[Diagnostic] = []
        for k, v in self.constants.items():
            result, diagnostics = self.substitute(str(v))
            all_diagnostics.extend(diagnostics)
            constants[k] = result

        self.constants = constants
        return self, all_diagnostics

    def read(
        self, path: Path, text: Optional[str] = None
    ) -> Tuple[str, List[Diagnostic]]:
        if text is None:
            text = path.read_text(encoding="utf-8")

        text, diagnostics = self.substitute(text)
        match_found = PAT_GIT_MARKER.finditer(text)

        if match_found:
            for match in match_found:
                lineno = text.count("\n", 0, match.start())
                diagnostics.append(GitMergeConflictArtifactFound(path, lineno))

        return (text, diagnostics)

    def substitute(self, source: str) -> Tuple[str, List[Diagnostic]]:
        """Substitute all placeholders within a string."""
        diagnostics: List[Diagnostic] = []

        def handle_match(match: Match[str]) -> str:
            """Replace a given placeholder match with a value from the project
               configuration. Log a warning if it's not defined."""
            variable_name = match.group(1)
            try:
                return str(self.constants[variable_name])
            except KeyError:
                lineno = source.count("\n", 0, match.start())
                diagnostics.append(ConstantNotDeclared(variable_name, lineno))

            # Return a zero-width space to avoid breaking syntax
            return "\u200b"

        return PAT_VARIABLE.sub(handle_match, source), diagnostics
