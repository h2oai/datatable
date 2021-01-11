import os
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import List

import pytest

from . import language_server
from .diagnostics import DocUtilsParseError, InvalidTableStructure
from .flutter import check_type, checked
from .types import FileId, SerializableType
from .util_test import ast_to_testing_string, check_ast_testing_string

CWD_URL = "file://" + Path().resolve().as_posix()


@checked
@dataclass
class LSPPosition:
    line: int
    character: int


@checked
@dataclass
class LSPRange:
    start: LSPPosition
    end: LSPPosition


@checked
@dataclass
class LSPDiagnostic:
    message: str
    severity: int
    range: LSPRange
    code: str
    source: str


@pytest.mark.skipif(
    "GITHUB_RUN_ID" in os.environ,
    reason="this test is timing-sensitive and doesn't work well in CI",
)
def test_debounce() -> None:
    bounces = [0]

    @language_server.debounce(0.1)
    def inc() -> None:
        bounces[0] += 1

    inc()
    inc()
    inc()

    time.sleep(0.2)
    inc()

    assert bounces[0] == 1


def test_pid_exists() -> None:
    assert language_server.pid_exists(os.getpid())
    # Test that an invalid PID returns False
    assert not language_server.pid_exists(537920)


def test_workspace_entry() -> None:
    entry = language_server.WorkspaceEntry(
        FileId(""),
        "",
        [InvalidTableStructure("foo", 10), DocUtilsParseError("fo", 10, 12)],
    )
    parsed = [
        check_type(LSPDiagnostic, diag) for diag in entry.create_lsp_diagnostics()
    ]

    assert parsed[0] == LSPDiagnostic(
        "foo",
        1,
        LSPRange(LSPPosition(10, 0), LSPPosition(10, 1000)),
        "InvalidTableStructure",
        "snooty",
    )

    assert parsed[1] == LSPDiagnostic(
        "fo",
        2,
        LSPRange(LSPPosition(10, 0), LSPPosition(12, 1000)),
        "DocUtilsParseError",
        "snooty",
    )


def test_language_server() -> None:
    with language_server.LanguageServer(sys.stdin.buffer, sys.stdout.buffer) as server:
        server.m_initialize(None, CWD_URL + "/test_data/test_project")
        assert server.uri_to_fileid(
            CWD_URL + "/test_data/test_project/source/blah/bar.rst"
        ) == FileId("blah/bar.rst")


def test_text_doc_resolve() -> None:
    """Tests to see if m_text_document__resolve() returns the proper path combined with
    appropriate extension"""
    with language_server.LanguageServer(sys.stdin.buffer, sys.stdout.buffer) as server:
        server.m_initialize(None, CWD_URL + "/test_data/test_project")

        assert server.project is not None

        # Set up resolve arguments for testing directive file
        source_path = server.project.config.source_path
        docpath_str = str(source_path.joinpath("foo.rst"))
        test_file = "/images/compass-create-database.png"
        resolve_type = "directive"

        # Set up assertion
        resolve_path = Path(
            server.m_text_document__resolve(test_file, docpath_str, resolve_type)
        )
        expected_path = source_path.joinpath(test_file[1:])

        assert resolve_path == expected_path

        # Resolve arguments for testing doc role target file
        test_file = "index"  # Testing relative path for example
        resolve_type = "doc"

        # Set up assertion
        resolve_path = Path(
            server.m_text_document__resolve(test_file, docpath_str, resolve_type)
        )
        expected_path = source_path.joinpath(test_file).with_suffix(".txt")

        assert resolve_path == expected_path


def test_text_doc_get_page_ast() -> None:
    """Tests to see if m_text_document__get_ast() returns the proper
    page ast for .txt file"""

    # Set up language server
    with language_server.LanguageServer(sys.stdin.buffer, sys.stdout.buffer) as server:
        server.m_initialize(
            None, CWD_URL + "/test_data/test_project_embedding_includes"
        )

        assert server.project is not None

        source_path = server.project.config.source_path

        # Change image path to be full path
        index_ast_string = (
            """<root fileid="index.txt">
            <target domain="std" name="label" html_id="std-label-guides">
            <target_identifier ids="['guides']"><text>Guides</text></target_identifier>
            </target>
            <section>
            <heading id="guides"><text>Guides</text></heading>
            <directive name="figure" alt="Sample images" checksum="10e351828f156afcafc7744c30d7b2564c6efba1ca7c55cac59560c67581f947">
            <text>"""
            + "/images/compass-create-database.png"
            + """</text></directive>
            <directive name="include"><text>/includes/test_rst.rst</text>
            <root fileid="includes/test_rst.rst">
            <directive name="include"><text>/includes/include_child.rst</text>
            <root fileid="includes/include_child.rst">
            <paragraph><text>This is an include in an include</text></paragraph>
            </root></directive>
            </root></directive>

            <directive name="include"><text>/includes/steps/migrate-compose-pr.rst</text>
            <root fileid="includes/steps-migrate-compose-pr.yaml">
            <directive name="steps">
            <directive name="step"><section><heading id="mongodb-atlas-account"><text>MongoDB Atlas account</text>
            </heading><paragraph><text>If you don't have an Atlas account, </text>
            <ref_role domain="std" name="doc" fileid="['/cloud/atlas', '']"><text>create one</text></ref_role>
            <text> now.</text></paragraph></section></directive>
            <directive name="step"><section><heading id="compose-mongodb-deployment">
            <text>Compose MongoDB deployment</text></heading>
            </section></directive></directive>
            </root></directive></section></root>"""
        )

        # ast string for included rst file
        include_child_ast_string = """
            <root fileid="includes/include_child.rst">
            <directive name="include"><text>includes/include_child.rst</text>
            <paragraph><text>This is an include in an include</text></paragraph>
            </directive>
            </root>"""

        # Test it such that for each test file, we get the file's respective ast string
        test_files = ["index.txt", "includes/include_child.rst"]
        expected_ast_strings: List[str] = [index_ast_string, include_child_ast_string]

        for i in range(len(test_files)):
            test_file_path = source_path.joinpath(test_files[i])
            language_server_ast: SerializableType = server.m_text_document__get_page_ast(
                test_file_path.as_posix()
            )
            print()
            print()
            print(ast_to_testing_string(language_server_ast))
            check_ast_testing_string(language_server_ast, expected_ast_strings[i])


def test_text_doc_get_project_name() -> None:
    """Tests to see if m_text_document__get_project_name() returns the correct name found
    in the project's snooty.toml"""

    # Set up language server for test_project
    with language_server.LanguageServer(sys.stdin.buffer, sys.stdout.buffer) as server:
        server.m_initialize(None, CWD_URL + "/test_data/test_project")
        assert server.project is not None

        expected_name = "test_data"  # Found in test_project's snooty.toml
        assert server.m_text_document__get_project_name() == expected_name

    # Set up language server for merge_conflict project
    with language_server.LanguageServer(sys.stdin.buffer, sys.stdout.buffer) as server:
        server.m_initialize(None, CWD_URL + "/test_data/merge_conflict")
        assert server.project is not None

        expected_name = "merge_conflict"  # Found in test_project's snooty.toml
        assert server.m_text_document__get_project_name() == expected_name


def test_text_doc_get_page_fileid() -> None:
    """Tests to see if m_text_document__get_page_fileid gets the correct fileid for
    a file given its path."""

    with language_server.LanguageServer(sys.stdin.buffer, sys.stdout.buffer) as server:
        server.m_initialize(
            None, CWD_URL + "/test_data/test_project_embedding_includes"
        )

        assert server.project is not None

        # Set up file path for index.txt
        source_path = server.project.config.source_path
        test_file = "index.txt"
        test_file_path = str(source_path.joinpath(test_file))
        expected_fileid = "index"

        assert (
            server.m_text_document__get_page_fileid(test_file_path) == expected_fileid
        )

        # Test it on an include file this time
        test_file = "includes/include_child.rst"
        test_file_path = str(source_path.joinpath(test_file))
        expected_fileid = "includes/include_child"

        assert (
            server.m_text_document__get_page_fileid(test_file_path) == expected_fileid
        )


def test_reporting_config_error() -> None:
    with language_server.LanguageServer(sys.stdin.buffer, sys.stdout.buffer) as server:
        server.m_initialize(None, CWD_URL + "/test_data/bad_project")
        doc = {
            "uri": f"{CWD_URL}/test_data/bad_project/snooty.toml",
            "languageId": "",
            "version": 0,
            "text": "",
        }
        server.m_text_document__did_open(doc)
        server.m_text_document__did_close({"uri": doc["uri"]})
