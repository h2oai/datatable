import sys
import threading
import time
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import DefaultDict, Dict, List

from . import n
from .diagnostics import ConstantNotDeclared, Diagnostic, GitMergeConflictArtifactFound
from .page import Page
from .parser import Project
from .types import BuildIdentifierSet, FileId, ProjectConfig, SerializableType
from .util import ast_dive
from .util_test import check_ast_testing_string

build_identifiers: BuildIdentifierSet = {"commit_hash": "123456", "patch_id": "789"}


@dataclass
class Backend:
    metadata: Dict[str, SerializableType] = field(default_factory=dict)
    pages: Dict[FileId, Page] = field(default_factory=dict)
    updates: List[FileId] = field(default_factory=list)
    diagnostics: DefaultDict[FileId, List[Diagnostic]] = field(
        default_factory=lambda: defaultdict(list)
    )

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
        self.updates.append(page_id)

    def on_update_metadata(
        self,
        prefix: List[str],
        build_identifiers: BuildIdentifierSet,
        field: Dict[str, SerializableType],
    ) -> None:
        self.metadata.update(field)

    def on_delete(self, page_id: FileId, build_identifiers: BuildIdentifierSet) -> None:
        pass

    def flush(self) -> None:
        pass


def test() -> None:
    backend = Backend()
    n_threads = len(threading.enumerate())
    with Project(Path("test_data/test_project"), backend, build_identifiers) as project:
        project.build()
        # Ensure that filesystem monitoring threads have been started
        assert len(threading.enumerate()) > n_threads

        # Ensure that the correct pages and assets exist
        index_id = FileId("index.txt")
        assert list(backend.pages.keys()) == [index_id]
        # Confirm that no diagnostics were created
        assert backend.diagnostics[index_id] == []
        code_length = 0
        checksums: List[str] = []
        index = backend.pages[index_id]
        assert len(index.static_assets) == 1
        assert not index.pending_tasks
        for node in ast_dive(index.ast):
            if isinstance(node, n.Code):
                code_length += len(node.value)
            elif isinstance(node, n.Directive) and node.name == "figure":
                checksums.append(node.options["checksum"])
        assert code_length == 345
        assert checksums == [
            "10e351828f156afcafc7744c30d7b2564c6efba1ca7c55cac59560c67581f947"
        ]
        assert backend.updates == [index_id]

        # Skip the remainder of the tests on non-Darwin platforms; they fail for
        # unknown reasons.
        if sys.platform != "darwin":
            return

        figure_id = FileId("images/compass-create-database.png")
        with project._lock:
            assert list(
                project._project.expensive_operation_cache.get_versions(figure_id)
            ) == [1]
        with project.config.source_path.joinpath(figure_id).open(mode="r+b") as f:
            text = f.read()
            f.seek(0)
            f.truncate(0)
            f.write(text)
            f.flush()
        time.sleep(0.1)
        with project._lock:
            assert list(
                project._project.expensive_operation_cache.get_versions(figure_id)
            ) == [2]

        # Ensure that the page has been reparsed 2 times
        assert backend.updates == [index_id, index_id]

        # Ensure that published-branches.yaml has been parsed
        (
            published_branches,
            published_branch_diagnostics,
        ) = project._project.get_parsed_branches()
        assert len(published_branch_diagnostics) == 0
        assert project.config.title == "MongoDB title"
        assert published_branches and published_branches.serialize() == {
            "git": {"branches": {"manual": "master", "published": ["master", "v1.0"]}},
            "version": {
                "published": ["1.1", "1.0"],
                "active": ["1.1", "1.0"],
                "stable": None,
                "upcoming": None,
            },
        }

    # Ensure that any filesystem monitoring threads have been shut down
    assert len(threading.enumerate()) == n_threads


def test_merge_conflict() -> None:
    project_path = Path("test_data/merge_conflict")
    project_config, _ = ProjectConfig.open(project_path)
    file_path = Path("test_data/merge_conflict/source/index.txt")
    _, project_diagnostics = project_config.read(file_path)

    assert isinstance(
        project_diagnostics[-1], GitMergeConflictArtifactFound
    ) and project_diagnostics[-1].start == (68, 0)
    assert isinstance(
        project_diagnostics[-2], GitMergeConflictArtifactFound
    ) and project_diagnostics[-2].start == (35, 0)


def test_bad_project() -> None:
    backend = Backend()
    Project(Path("test_data/bad_project"), backend, build_identifiers)
    fileid = FileId("snooty.toml")
    assert list(backend.diagnostics.keys()) == [fileid]
    diagnostics = backend.diagnostics[fileid]
    assert len(diagnostics) == 1
    assert isinstance(diagnostics[0], ConstantNotDeclared)


def test_missing_deprecated_versions() -> None:
    backend = Backend()
    project = Project(Path("test_data/test_project"), backend, build_identifiers)
    project.build()
    assert "deprecated_versions" not in backend.metadata


def test_not_a_project() -> None:
    backend = Backend()
    project = Project(Path("test_data/not_a_project"), backend, build_identifiers)
    project.build()


def test_get_ast() -> None:
    backend = Backend()
    project = Project(Path("test_data/get-preview"), backend, build_identifiers)
    project.build()
    ast = project.get_page_ast(
        Path("test_data/get-preview/source/index.txt").absolute()
    )
    check_ast_testing_string(
        ast,
        """<root fileid="index.txt">
        <section>
        <heading id="index"><text>Index</text></heading>
        <paragraph><text>Test.</text></paragraph>
        <directive name="include"><text>/includes/steps/test.rst</text>
            <root fileid="includes/steps-test.yaml">
            <directive name="steps"><directive name="step"><section><heading id="identify-the-privileges-granted-by-a-role">
            <text>Identify the privileges granted by a role.</text></heading>
            <paragraph><text>this is a test step.</text></paragraph></section></directive></directive>
            </root>
        </directive>
        <directive name="include"><text>/includes/extracts/test.rst</text>
            <root fileid="includes/extracts-test.yaml">
                <directive name="extract">
                    <paragraph><text>test extract</text></paragraph>
                </directive>
            </root>
        </directive>
        <directive name="include">
            <text>/includes/release/pin-version-intro.rst</text>
            <root fileid="includes/release-pinning.yaml">
                <directive name="release_specification">
                    <paragraph><text>To install a specific release, you must specify each component package
individually along with the version number, as in the
following example:</text></paragraph>
                </directive>
            </root>
        </directive></section></root>""",
    )
