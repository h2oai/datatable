import enum
import logging
import os
from snooty.lsp import CompletionItemKind
import sys
import threading
from urllib.parse import urlparse, unquote
from urllib.request import url2pathname
from collections import defaultdict
from dataclasses import dataclass
from functools import wraps
from pathlib import Path, PurePath
from typing import (
    Any,
    BinaryIO,
    Callable,
    DefaultDict,
    Dict,
    List,
    Optional,
    TypeVar,
    Union,
    cast,
)

import pyls_jsonrpc.dispatchers
import pyls_jsonrpc.endpoint
import pyls_jsonrpc.streams

from . import n, util
from .diagnostics import Diagnostic
from .flutter import check_type, checked
from .page import Page
from .parser import Project
from .types import BuildIdentifierSet, FileId, SerializableType

_F = TypeVar("_F", bound=Callable[..., Any])
Uri = str
PARENT_PROCESS_WATCH_INTERVAL_SECONDS = 60
logger = logging.getLogger(__name__)


class debounce:
    def __init__(self, wait: float) -> None:
        self.wait = wait

    def __call__(self, fn: _F) -> _F:
        wait = self.wait

        @wraps(fn)
        def debounced(*args: Any, **kwargs: Any) -> Any:
            def action() -> None:
                fn(*args, **kwargs)

            try:
                getattr(debounced, "debounce_timer").cancel()
            except AttributeError:
                pass
            timer = threading.Timer(wait, action)
            setattr(debounced, "debounce_timer", timer)
            timer.start()

        return cast(_F, debounced)


@checked
@dataclass
class Position:
    line: int
    character: int


@checked
@dataclass
class Range:
    start: Position
    end: Position


@checked
@dataclass
class Location:
    uri: Uri
    range: Range


@checked
@dataclass
class TextDocumentIdentifier:
    uri: Uri


@checked
@dataclass
class TextDocumentItem:
    uri: Uri
    languageId: str
    version: int
    text: str


@checked
@dataclass
class VersionedTextDocumentIdentifier(TextDocumentIdentifier):
    version: Union[int, None]


@checked
@dataclass
class TextDocumentContentChangeEvent:
    range: Optional[Range]
    rangeLength: Optional[int]
    text: str


@checked
@dataclass
class DiagnosticRelatedInformation:
    location: Location
    message: str


@checked
@dataclass
class LanguageServerDiagnostic:
    range: Range
    severity: Optional[int]
    code: Union[int, str, None]
    source: Optional[str]
    message: str
    relatedInformation: Optional[List[DiagnosticRelatedInformation]]


@checked
@dataclass
class Command:
    title: str
    command: str
    arguments: Optional[object]


@checked
@dataclass
class TextEdit:
    range: Range
    newText: str


@checked
@dataclass
class TextDocumentEdit:
    textDocument: VersionedTextDocumentIdentifier
    edits: List[TextEdit]

@checked
@dataclass
class CompletionItem:
    label: str
    kind: CompletionItemKind
    detail: str
    documentation: str
    sortText: str

@checked
@dataclass
class CompletionList:
    isIncomplete: bool
    items: List[CompletionItem]


if sys.platform == "win32":
    import ctypes

    kernel32 = ctypes.windll.kernel32
    PROCESS_QUERY_INFROMATION = 0x1000

    def pid_exists(pid: int) -> bool:
        process = kernel32.OpenProcess(PROCESS_QUERY_INFROMATION, 0, pid)
        if process != 0:
            kernel32.CloseHandle(process)
            return True
        return False


else:

    def pid_exists(pid: int) -> bool:
        try:
            os.kill(pid, 0)
        except ProcessLookupError:
            return False
        else:
            return True


class Backend:
    def __init__(self, server: "LanguageServer") -> None:
        self.server = server
        self.pending_diagnostics: DefaultDict[FileId, List[Diagnostic]] = defaultdict(
            list
        )

    def on_progress(self, progress: int, total: int, message: str) -> None:
        pass

    def on_diagnostics(self, fileid: FileId, diagnostics: List[Diagnostic]) -> None:
        self.pending_diagnostics[fileid].extend(diagnostics)
        self.server.notify_diagnostics()

    def on_update(
        self,
        prefix: List[str],
        build_identifiers: BuildIdentifierSet,
        page_id: FileId,
        page: Page,
    ) -> None:
        pass

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


class DiagnosticSeverity(enum.IntEnum):
    """The Language Server Protocol's DiagnosticSeverity namespace enumeration.
       See: https://microsoft.github.io/language-server-protocol/specification#diagnostic"""

    error = 1
    warning = 2
    information = 3
    hint = 4

    @classmethod
    def from_diagnostic(cls, level: Diagnostic.Level) -> "DiagnosticSeverity":
        """Convert an internal Snooty Diagnostic's level to a DiagnosticSeverity value."""
        if level is Diagnostic.Level.info:
            return cls.information
        elif level is Diagnostic.Level.warning:
            return cls.warning
        elif level is Diagnostic.Level.error:
            return cls.error


@dataclass
class WorkspaceEntry:
    page_id: FileId
    document_uri: Uri
    diagnostics: List[Diagnostic]

    def create_lsp_diagnostics(self) -> List[object]:
        return [
            {
                "range": {
                    "start": {
                        "line": diagnostic.start[0],
                        "character": diagnostic.start[1],
                    },
                    "end": {"line": diagnostic.end[0], "character": diagnostic.end[1]},
                },
                "severity": DiagnosticSeverity.from_diagnostic(diagnostic.severity),
                "message": diagnostic.message,
                "code": type(diagnostic).__name__,
                "source": "snooty",
            }
            for diagnostic in self.diagnostics
        ]


class LanguageServer(pyls_jsonrpc.dispatchers.MethodDispatcher):
    def __init__(self, rx: BinaryIO, tx: BinaryIO) -> None:
        self.backend = Backend(self)
        self.project: Optional[Project] = None
        self.workspace: Dict[str, WorkspaceEntry] = {}
        self.diagnostics: Dict[PurePath, List[Diagnostic]] = {}

        self._jsonrpc_stream_reader = pyls_jsonrpc.streams.JsonRpcStreamReader(rx)
        self._jsonrpc_stream_writer = pyls_jsonrpc.streams.JsonRpcStreamWriter(tx)
        self._endpoint = pyls_jsonrpc.endpoint.Endpoint(
            self, self._jsonrpc_stream_writer.write
        )
        self._shutdown = False

    def start(self) -> None:
        self._jsonrpc_stream_reader.listen(self._endpoint.consume)

    def notify_diagnostics(self) -> None:
        """Handle the backend notifying us that diagnostics are available to be pulled."""
        if not self.project:
            logger.debug("Received diagnostics, but project not ready")
            return

        for fileid, diagnostics in self.backend.pending_diagnostics.items():
            self._set_diagnostics(fileid, diagnostics)

        self.backend.pending_diagnostics.clear()

    def update_file(self, page_path: Path, change: Optional[str] = None) -> None:
        if not self.project:
            return

        if page_path.suffix not in util.SOURCE_FILE_EXTENSIONS:
            return

        self.project.update(page_path, change)

    def _set_diagnostics(self, fileid: FileId, diagnostics: List[Diagnostic]) -> None:
        self.diagnostics[fileid] = diagnostics
        uri = self.fileid_to_uri(fileid)
        workspace_item = self.workspace.get(uri, None)
        if workspace_item is None:
            workspace_item = WorkspaceEntry(fileid, uri, [])

        workspace_item.diagnostics = diagnostics
        self._endpoint.notify(
            "textDocument/publishDiagnostics",
            params={"uri": uri, "diagnostics": workspace_item.create_lsp_diagnostics()},
        )

    def uri_to_fileid(self, uri: Uri) -> FileId:
        if not self.project:
            raise TypeError("Cannot map uri to fileid before a project is open")

        parsed = urlparse(uri)
        if parsed.scheme != "file":
            raise ValueError("Only file:// URIs may be resolved", uri)

        path = self.uriToPath(uri)
        return self.project.config.get_fileid(path)

    def fileid_to_uri(self, fileid: FileId) -> str:
        if not self.project:
            raise TypeError("Cannot map fileid to uri before a project is open")

        return self.project.config.source_path.joinpath(fileid).as_uri()

    def m_initialize(
        self,
        processId: Optional[int] = None,
        rootUri: Optional[Uri] = None,
        **kwargs: object,
    ) -> SerializableType:
        if rootUri:            
            root_path = self.uriToPath(rootUri)
            self.project = Project(root_path, self.backend, {})
            self.notify_diagnostics()

            # XXX: Disabling the postprocessor is temporary until we can test
            # its usage in the language server more extensively
            self.project.build(postprocess=False)

        if processId is not None:

            def watch_parent_process(pid: int) -> None:
                # exist when the given pid is not alive
                if not pid_exists(pid):
                    logger.info("parent process %s is not alive", pid)
                    self.m_exit()
                logger.debug("parent process %s is still alive", pid)
                threading.Timer(
                    PARENT_PROCESS_WATCH_INTERVAL_SECONDS,
                    watch_parent_process,
                    args=[pid],
                ).start()

            watching_thread = threading.Thread(
                target=watch_parent_process, args=(processId,)
            )
            watching_thread.daemon = True
            watching_thread.start()

        return {
            "capabilities":
            {
                "textDocumentSync": 1,
                "completionProvider": {
                    "resolveProvider": False, # We know everything ahead of time
                    "TriggerCharacters": ['/']
                }
            }
        }

    def completions(self, doc_uri, position):
        """
        Given the filename, return the completion items of the page.
        """

        if self.project is None:
            logger.warn("Project uninitialized")
            return None

        filePath = self.uriToPath(doc_uri)
        line_content = self.project.get_line_content(filePath, position["line"])
        column: int = position["character"]
        if column > 1 and line_content[column - 2:column] == '`/':
            completions = []
            for name in self.project.queryFileNames():
                completions.append(CompletionItem(name["file"], CompletionItemKind.File, name["path"], "", name["file"]))
            return CompletionList(False, completions)
        return None

    @staticmethod
    def uriToPath(uri: Uri) -> Path:
        parsed = urlparse(uri)
        host = "{0}{0}{mnt}{0}".format(os.path.sep, mnt=parsed.netloc)
        return Path(os.path.abspath(
            os.path.join(host, url2pathname(unquote(parsed.path)))
        ))

    def m_initialized(self, **kwargs: object) -> None:
        # Ignore this message to avoid logging a pointless warning
        pass

    def m_text_document__completion(self, textDocument=None, position=None, **_kwargs):
        return self.completions(textDocument['uri'], position)

    def m_text_document__resolve(
        self, fileName: str, docPath: str, resolveType: str
    ) -> str:
        """Given an artifact's path relative to the project's source directory,
        return a corresponding source file path relative to the project's root."""

        if self.project is None:
            logger.warn("Project uninitialized")
            return fileName

        if resolveType == "doc":
            resolved_target_path = util.add_doc_target_ext(
                fileName, PurePath(docPath), self.project.config.source_path
            )
            return str(resolved_target_path)
        elif resolveType == "directive":
            return str(self.project.config.source_path) + fileName
        else:
            logger.error("resolveType is not supported")
            return fileName

    def m_text_document__get_page_ast(self, fileName: str) -> SerializableType:
        """
        Given the filename, return the ast of the page that is created from parsing that file.
        If the file is a .rst file, we return an ast that emulates the ast of a .txt
        file containing a single include directive to said .rst file.
        """

        if self.project is None:
            logger.warn("Project uninitialized")
            return None

        filePath = Path(fileName)
        page_ast = self.project.get_page_ast(filePath)

        # If rst file, insert its ast into a pseudo ast object
        if filePath.suffix == ".rst":
            # Copy ast of previewed file into a modified version
            if isinstance(page_ast, n.Parent):
                children = page_ast.children
            else:
                children = []
            rst_ast = n.Directive(
                page_ast.start,
                children,
                "",
                "include",
                [n.Text((0,), self.project.config.get_fileid(filePath).as_posix())],
                {},
            )

            # Insert modified ast as a child of a pseudo empty page ast
            pseudo_ast = n.Root((0,), [], self.project.config.get_fileid(filePath), {})
            pseudo_ast.children.append(rst_ast)
            return pseudo_ast.serialize()

        return page_ast.serialize()

    def m_text_document__get_project_name(self) -> SerializableType:
        """Get the project's name from its ProjectConfig"""
        # This method may later be refactored to obtain other ProjectConfig data
        # (https://github.com/mongodb/snooty-parser/pull/44#discussion_r336749209)
        if not self.project:
            logger.warn("Project uninitialized")
            return None

        return self.project.get_project_name()

    def m_text_document__get_page_fileid(self, filePath: str) -> SerializableType:
        """Given a path to a file, return its fileid as a string"""
        if not self.project:
            logger.warn("Project uninitialized")
            return None

        fileid = self.project.config.get_fileid(PurePath(filePath))
        return fileid.without_known_suffix

    def m_text_document__did_open(self, textDocument: SerializableType) -> None:
        if not self.project:
            return

        item = check_type(TextDocumentItem, textDocument)
        fileid = self.uri_to_fileid(item.uri)
        page_path = self.project.get_full_path(fileid)
        entry = WorkspaceEntry(fileid, item.uri, [])
        self.workspace[item.uri] = entry
        self.update_file(page_path, item.text)

    @debounce(0.2)
    def m_text_document__did_change(
        self, textDocument: SerializableType, contentChanges: SerializableType
    ) -> None:
        if not self.project:
            return

        identifier = check_type(VersionedTextDocumentIdentifier, textDocument)
        page_path = self.project.get_full_path(self.uri_to_fileid(identifier.uri))
        assert isinstance(contentChanges, list)
        change = next(
            check_type(TextDocumentContentChangeEvent, x) for x in contentChanges
        )
        self.update_file(page_path, change.text)

    def m_text_document__did_close(self, textDocument: SerializableType) -> None:
        if not self.project:
            return

        identifier = check_type(TextDocumentIdentifier, textDocument)
        page_path = self.project.get_full_path(self.uri_to_fileid(identifier.uri))
        del self.workspace[identifier.uri]
        self.update_file(page_path)

    def m_shutdown(self, **_kwargs: object) -> None:
        self._shutdown = True

    def m_exit(self, **_kwargs: object) -> None:
        self._endpoint.shutdown()
        if self.project:
            self.project.stop_monitoring()

    def __enter__(self) -> "LanguageServer":
        return self

    def __exit__(self, *args: object) -> None:
        self.m_shutdown()
        self.m_exit()

def flatten(list_of_lists):
    return [item for lst in list_of_lists for item in lst]

def start() -> None:
    stdin, stdout = sys.stdin.buffer, sys.stdout.buffer
    server = LanguageServer(stdin, stdout)
    logger.info("Started")
    server.start()
