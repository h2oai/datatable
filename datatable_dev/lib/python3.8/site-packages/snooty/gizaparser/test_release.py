from pathlib import Path, PurePath
from typing import Dict, List, Optional, Tuple

from ..diagnostics import Diagnostic
from ..page import Page
from ..parser import EmbeddedRstParser
from ..types import ProjectConfig
from ..util_test import check_ast_testing_string
from .release import GizaReleaseSpecificationCategory


def test_release_specification() -> None:
    root_path = Path("test_data/test_gizaparser")
    project_config = ProjectConfig(root_path, "")
    project_config.constants["version"] = "3.4"
    category = GizaReleaseSpecificationCategory(project_config)
    path = root_path.joinpath(Path("source/includes/release-specifications.yaml"))
    parent_path = root_path.joinpath(Path("source/includes/release-base.yaml"))

    def add_main_file() -> List[Diagnostic]:
        extracts, text, parse_diagnostics = category.parse(path)
        category.add(path, text, extracts)
        assert len(parse_diagnostics) == 0
        assert len(extracts) == 2
        return parse_diagnostics

    def add_parent_file() -> List[Diagnostic]:
        extracts, text, parse_diagnostics = category.parse(parent_path)
        category.add(parent_path, text, extracts)
        assert len(parse_diagnostics) == 0
        assert len(extracts) == 2
        return parse_diagnostics

    all_diagnostics: Dict[PurePath, List[Diagnostic]] = {}
    all_diagnostics[path] = add_main_file()
    all_diagnostics[parent_path] = add_parent_file()

    assert len(category) == 2
    file_id, giza_node = next(category.reify_all_files(all_diagnostics))

    def create_page(filename: Optional[str]) -> Tuple[Page, EmbeddedRstParser]:
        page = Page.create(path, filename, "")
        return (page, EmbeddedRstParser(project_config, page, all_diagnostics[path]))

    pages = category.to_pages(path, create_page, giza_node.data)
    assert [page.fake_full_path().as_posix() for page in pages] == [
        "test_data/test_gizaparser/source/includes/release/untar-release-osx-x86_64.rst",
        "test_data/test_gizaparser/source/includes/release/install-ent-windows-default.rst",
    ]

    assert all((not diagnostics for diagnostics in all_diagnostics.values()))

    check_ast_testing_string(
        pages[0].ast,
        """<root fileid="includes/release-specifications.yaml"><directive name="release_specification">
        <code lang="sh" copyable="True">
        tar -zxvf mongodb-macos-x86_64-3.4.tgz\n</code>
        </directive></root>""",
    )

    check_ast_testing_string(
        pages[1].ast,
        """<root fileid="includes/release-specifications.yaml"><directive name="release_specification">
            <code lang="bat" copyable="True">
            msiexec.exe /l*v mdbinstall.log  /qb /i mongodb-win32-x86_64-enterprise-windows-64-3.4-signed.msi\n
            </code>
            </directive></root>""",
    )
