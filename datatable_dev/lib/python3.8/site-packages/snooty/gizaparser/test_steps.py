from pathlib import Path, PurePath
from typing import Dict, List, Optional, Tuple

from ..diagnostics import Diagnostic
from ..page import Page
from ..parser import EmbeddedRstParser
from ..types import ProjectConfig
from ..util_test import ast_to_testing_string, check_ast_testing_string
from .steps import GizaStepsCategory


def test_step() -> None:
    root_path = Path("test_data/test_gizaparser")
    project_config, project_diagnostics = ProjectConfig.open(root_path)
    assert project_diagnostics == []

    category = GizaStepsCategory(project_config)
    path = root_path.joinpath(Path("source/includes/steps-test.yaml"))
    child_path = root_path.joinpath(Path("source/includes/steps-test-child.yaml"))
    grandchild_path = root_path.joinpath(
        Path("source/includes/steps-test-grandchild.yaml")
    )

    all_diagnostics: Dict[PurePath, List[Diagnostic]] = {}
    for current_path in [path, child_path, grandchild_path]:
        steps, text, parse_diagnostics = category.parse(current_path)
        category.add(current_path, text, steps)
        if parse_diagnostics:
            all_diagnostics[path] = parse_diagnostics

    assert len(category) == 3
    file_id, giza_node = next(category.reify_all_files(all_diagnostics))

    def create_page(filename: Optional[str]) -> Tuple[Page, EmbeddedRstParser]:
        page = Page.create(path, filename, "")
        return (page, EmbeddedRstParser(project_config, page, all_diagnostics[path]))

    pages = category.to_pages(path, create_page, giza_node.data)
    assert [page.fake_full_path().as_posix() for page in pages] == [
        "test_data/test_gizaparser/source/includes/steps/test.rst"
    ]
    # Ensure that no diagnostics were raised
    all_diagnostics = {k: v for k, v in all_diagnostics.items() if v}
    assert not all_diagnostics
    print(repr(ast_to_testing_string(pages[0].ast)))
    check_ast_testing_string(
        pages[0].ast,
        """
<root fileid="includes/steps-test.yaml">
<directive name="steps">
<directive name="step">
    <section>
    <heading id="import-the-public-key-used-by-the-package-management-system">
        <text>Import the </text>
        <emphasis><text>public key</text></emphasis>
        <text> used by the </text>
        <reference refuri="https://en.wikipedia.org/wiki/Package_manager">
        <text>package management system</text>
        </reference>
    </heading>
    <paragraph>
        <text>Issue the following command to import the\n</text>
        <reference refuri="https://www.mongodb.org/static/pgp/server-3.4.asc">
        <text>MongoDB public GPG Key</text></reference>
    </paragraph></section></directive>
<directive name="step">
    <section>
    <heading id="create-a-etc-apt-sources-list-d-mongodb-org-3-4-list-file-for-mongodb">
        <text>Create a </text><literal><text>
        /etc/apt/sources.list.d/mongodb-org-3.4.list</text></literal><text> file for </text>
        <role name="guilabel"><text>MongoDB</text></role>
        <text>.</text>
    </heading>
    <section><heading id="optional-action-heading">
        <text>Optional: action heading</text></heading>
        <paragraph>
            <text>Create the list file using the command appropriate for your version\nof Debian.</text>
        </paragraph>
        <paragraph><text>action-content</text></paragraph>
        <paragraph><text>action-post</text></paragraph>
    </section></section></directive>
<directive name="step"><section>
    <heading id="reload-local-package-database">
        <text>Reload local package database.</text>
    </heading>
    <paragraph>
        <text>Issue the following command to reload the local package database:</text>
    </paragraph>
    <code copyable="True" lang="sh">sudo apt-get update\n</code>
    </section></directive>
<directive name="step"><section>
    <heading id="install-the-mongodb-packages">
        <text>Install the MongoDB packages.</text>
    </heading>
    <paragraph><text>hi</text></paragraph>
    <paragraph>
        <text>You can install either the latest stable version of MongoDB or a\nspecific version of MongoDB.</text>
    </paragraph>
    <code lang="sh" copyable="True">
    echo "mongodb-org hold" | sudo dpkg --set-selections
    </code><paragraph><text>bye</text></paragraph></section></directive>
</directive></root>""",
    )
