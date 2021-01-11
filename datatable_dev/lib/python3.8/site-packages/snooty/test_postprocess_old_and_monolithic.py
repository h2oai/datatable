from pathlib import Path
from typing import Any, Dict, List, cast

import pytest

from . import n
from .diagnostics import AmbiguousTarget, MissingTocTreeEntry, TargetNotFound
from .parser import Project
from .test_project import Backend
from .types import BuildIdentifierSet, FileId
from .util_test import ast_to_testing_string, check_ast_testing_string

ROOT_PATH = Path("test_data")


@pytest.fixture(scope="module")
def backend() -> Backend:
    backend = Backend()
    build_identifiers: BuildIdentifierSet = {"commit_hash": "123456", "patch_id": "678"}
    with Project(
        Path("test_data/test_postprocessor"), backend, build_identifiers
    ) as project:
        project.build()

    return backend


def test_slug_title_mapping(backend: Backend) -> None:
    # Ensure that the correct pages and assets exist
    slugToTitle = cast(
        Dict[str, List[n.SerializedNode]], backend.metadata["slugToTitle"]
    )

    # page3 is not included in slug-title mapping because it lacks a heading.
    assert len(slugToTitle) == 5
    assert slugToTitle["index"][0]["value"] == "Connection Limits and Cluster Tier"
    assert slugToTitle["page1"][0]["value"] == "Print this heading"
    assert (
        slugToTitle["page2"][0]["value"] == "Heading is not at the top for some reason"
    )
    assert slugToTitle["page4"][0]["value"] == "Skip includes"


def test_deprecated_versions(backend: Backend) -> None:
    assert backend.metadata["deprecated_versions"] == {
        "manual": ["v2.2", "v2.4", "v2.6", "v3.0", "v3.2", "v3.4"],
        "mms": ["v1.1", "v1.2", "v1.3"],
    }


def test_expand_includes(backend: Backend) -> None:
    page4_id = FileId("page4.txt")
    ast = backend.pages[page4_id].ast
    include_node = ast.children[0]
    assert isinstance(include_node, n.Directive)
    assert include_node.name == "include"

    check_ast_testing_string(
        include_node,
        """
        <directive name="include">
            <text>/includes/test.rst</text>
            <root fileid="includes/test.rst">
                <target domain="std" name="label" html_id="std-label-connection-limits">
                    <target_identifier ids="['connection-limits']">
                        <text>Skip includes</text>
                    </target_identifier>
                </target>
                <section>
                    <heading id="skip-includes">
                        <text>Skip includes</text>
                    </heading>
                    <directive name="meta" keywords="connect"></directive>
                </section>
            </root>
        </directive>""",
    )


def test_validate_ref_targets(backend: Backend) -> None:
    page_id = FileId("refrole.txt")
    ast = backend.pages[page_id].ast

    # Assert that targets found in intersphinx inventories are correctly resolved
    paragraph = ast.children[0]
    assert isinstance(paragraph, n.Parent)
    ref_role = paragraph.children[0]
    # print(ast_to_testing_string(ref_role))
    check_ast_testing_string(
        ref_role,
        """<ref_role
        domain="mongodb"
        name="setting"
        target="net.port"
        url="https://docs.mongodb.com/manual/reference/configuration-options/#net.port">
        <literal><text>net.port</text></literal>
        </ref_role>""",
    )

    # Assert that local targets are correctly resolved, and that appropriate
    # element ID disambiguation is added.
    paragraph = ast.children[3]
    assert isinstance(paragraph, n.Parent)
    ref_role = paragraph.children[1]
    # print(ast_to_testing_string(ref_role))
    check_ast_testing_string(
        ref_role,
        """<ref_role
        domain="mongodb"
        name="method"
        target="amethod"
        fileid="['index', 'mongodb-method-amethod-1']">
        <literal><text>amethod()</text></literal>
        </ref_role>""",
    )

    # Assert that labels are correctly resolved
    paragraph = ast.children[1]
    assert isinstance(paragraph, n.Parent)
    ref_role = paragraph.children[-2]
    print(ast_to_testing_string(ref_role))
    check_ast_testing_string(
        ref_role,
        """<ref_role
        domain="std"
        name="label"
        target="global-writes-zones"
        fileid="['index', 'std-label-global-writes-zones']"><emphasis><text>Global</text></emphasis><text> Writes Zones</text></ref_role>""",
    )

    # Assert that local targets with a prefix correctly resolved
    paragraph = ast.children[4]
    assert isinstance(paragraph, n.Parent)
    ref_role = paragraph.children[1]
    print(ast_to_testing_string(ref_role))
    check_ast_testing_string(
        ref_role,
        """<ref_role
        domain="mongodb"
        name="binary"
        target="bin.mongod"
        url="https://docs.mongodb.com/manual/reference/program/mongod/#bin.mongod">
        <literal><text>mongod</text></literal>
        </ref_role>""",
    )
    # Assert that refs beginning with number and containing underscores, work
    paragraph = ast.children[5]
    assert isinstance(paragraph, n.Parent)
    ref_role = paragraph.children[2]
    check_ast_testing_string(
        ref_role,
        """<ref_role
        domain="std"
        name="label"
        target="100_stacked_example"
        fileid="['index', 'std-label-100_stacked_example']">
        <text>100 Stacked Examples</text>
        </ref_role>""",
    )

    paragraph = ast.children[6]
    assert isinstance(paragraph, n.Parent)
    ref_role = paragraph.children[2]
    check_ast_testing_string(
        ref_role,
        """<ref_role
        domain="std"
        name="label"
        target="z100_stacked_example"
        fileid="['index', 'std-label-z100_stacked_example']">
        <text>Z100 Stacked Examples</text>
        </ref_role>""",
    )
    # Check that undeclared targets raise an error
    diagnostics = backend.diagnostics[page_id]
    assert len(diagnostics) == 3
    assert any(
        isinstance(diagnostic, TargetNotFound)
        and diagnostic.target == "global-writes-collections"
        for diagnostic in diagnostics
    )
    assert any(
        isinstance(diagnostic, AmbiguousTarget)
        and diagnostic.candidates == ["index", "index", "index"]
        for diagnostic in diagnostics
    )

    # Check that targets are matched case-sensitive
    assert any(
        isinstance(diagnostic, TargetNotFound)
        and diagnostic.target == "CONNECTION-limits"
        for diagnostic in diagnostics
    )


def test_role_explicit_title(backend: Backend) -> None:
    page_id = FileId("index.txt")
    ast = backend.pages[page_id].ast
    assert isinstance(ast, n.Root)

    # Assert that ref_roles with an explicit title work
    paragraph = cast(Any, ast).children[1].children[3].children[1].children[2]
    assert isinstance(paragraph, n.Paragraph)
    ref_role = paragraph.children[1]
    print(ast_to_testing_string(ref_role))
    check_ast_testing_string(
        ref_role,
        """<ref_role
        domain="std"
        name="label"
        target="global-writes-zones"
        fileid="['index', 'std-label-global-writes-zones']">
        <text>explicit title</text>
        </ref_role>""",
    )


def test_toctree(backend: Backend) -> None:
    assert backend.metadata["toctree"] == {
        "children": [
            {
                "children": [],
                "options": {"drawer": True},
                "slug": "page1",
                "title": [
                    {
                        "position": {"start": {"line": 4}},
                        "type": "text",
                        "value": "Print this heading",
                    }
                ],
            },
            {
                "options": {"drawer": False},
                "slug": "page2",
                "title": [
                    {
                        "position": {"start": {"line": 19}},
                        "type": "text",
                        "value": "Heading is not at the top for some reason",
                    }
                ],
                "children": [
                    {
                        "children": [],
                        "title": [
                            {
                                "position": {"start": {"line": 0}},
                                "type": "text",
                                "value": "MongoDB Connector for Business Intelligence",
                            }
                        ],
                        "url": "https://docs.mongodb.com/bi-connector/current/",
                    },
                    {
                        "children": [],
                        "options": {"drawer": False},
                        "slug": "page3",
                        "title": None,
                    },
                    {
                        "children": [],
                        "options": {"drawer": True},
                        "slug": "page4",
                        "title": [
                            {
                                "position": {"start": {"line": 0}},
                                "type": "text",
                                "value": "Page Four",
                            }
                        ],
                    },
                ],
            },
        ],
        "title": [
            {
                "position": {"start": {"line": 0}},
                "type": "text",
                "value": "MongoDB title",
            }
        ],
        "slug": "/",
    }

    assert any(
        isinstance(x, MissingTocTreeEntry)
        for x in backend.diagnostics[FileId("page1.txt")]
    )


def test_breadcrumbs(backend: Backend) -> None:
    # Ensure that the correct pages and assets exist for breadcrumbs
    pages: Dict[str, Any] = cast(Dict[str, Any], backend.metadata["parentPaths"])

    assert len(pages) == 4
    assert len(pages["page1"]) == 0

    assert len(pages["page2"]) == 0

    assert len(pages["page3"]) == 1
    assert pages["page3"] == ["page2"]


def test_toctree_order(backend: Backend) -> None:
    # Ensure that the correct pages and assets exist for toctree order
    order: List[str] = cast(List[str], backend.metadata["toctreeOrder"])
    assert order == ["/", "page1", "page2", "page3", "page4"]


def test_target_titles(backend: Backend) -> None:
    page_id = FileId("index.txt")
    ast = backend.pages[page_id].ast

    # Assert that titles are correctly located
    section = ast.children[1]
    assert isinstance(section, n.Parent)
    section = section.children[3]
    assert isinstance(section, n.Parent)
    target1 = section.children[-2]
    target2 = section.children[-1]
    check_ast_testing_string(
        target1,
        """<target domain="std" name="label" html_id="std-label-a-sibling-node"><target_identifier ids="['a-sibling-node']"><text>Testing Sibling Nodes</text></target_identifier></target>""",
    )
    check_ast_testing_string(
        target2,
        """<target domain="std" name="label" html_id="std-label-another-target-for-a-sibling-node"><target_identifier ids="['another-target-for-a-sibling-node']"><text>Testing Sibling Nodes</text></target_identifier></target>""",
    )


def test_program_option(backend: Backend) -> None:
    page_id = FileId("a-program.txt")
    ast = backend.pages[page_id].ast
    assert isinstance(ast, n.Root)

    section: Any = ast.children[0]
    include = section.children[2]
    program1 = section.children[1]
    option1_1 = include.children[0]
    option1_2 = section.children[3]
    program2 = section.children[4]
    option2_1 = section.children[5]

    # Test directives
    check_ast_testing_string(
        program1,
        """
        <target domain="std" name="program" html_id="std-program-a-program">
        <directive_argument><literal><text>a-program</text></literal></directive_argument>
        <target_identifier ids="['a-program']"><text>a-program</text></target_identifier>
        </target>
    """,
    )

    check_ast_testing_string(
        option1_1,
        """
        <root fileid="includes/option-version.rst">
        <target domain="std" name="option" html_id="std-option-a-program.--version">
        <directive_argument><literal><text>--version, -v</text></literal></directive_argument>
        <target_identifier ids="['--version', 'a-program.--version']"><text>a-program --version</text></target_identifier>
        <target_identifier ids="['-v', 'a-program.-v']"><text>a-program -v</text></target_identifier>
        <paragraph><text>Displays the program version.</text></paragraph>
        </target>
        </root>
    """,
    )

    check_ast_testing_string(
        option1_2,
        """
        <target domain="std" name="option" html_id="std-option-a-program.--config">
        <directive_argument><literal><text>--config &lt;filename&gt;, -f &lt;filename&gt;</text></literal></directive_argument>
        <target_identifier ids="['--config', 'a-program.--config']"><text>a-program --config</text></target_identifier>
        <target_identifier ids="['-f', 'a-program.-f']"><text>a-program -f</text></target_identifier>
        <paragraph><text>Chooses a configuration file to load.</text></paragraph>
        </target>
    """,
    )

    check_ast_testing_string(
        program2,
        """
        <target domain="std" name="program" html_id="std-program-a-second-program">
        <directive_argument><literal><text>a-second-program</text></literal></directive_argument>
        <target_identifier ids="['a-second-program']"><text>a-second-program</text></target_identifier>
        </target>
    """,
    )

    check_ast_testing_string(
        option2_1,
        """
        <target domain="std" name="option" html_id="std-option-a-second-program.--version">
        <directive_argument><literal><text>--version, -v</text></literal></directive_argument>
        <target_identifier ids="['--version', 'a-second-program.--version']"><text>a-second-program --version</text></target_identifier>
        <target_identifier ids="['-v', 'a-second-program.-v']"><text>a-second-program -v</text></target_identifier>
        <paragraph><text>Display another program's version.</text></paragraph>
        </target>
    """,
    )

    # Test roles
    diagnostics = backend.diagnostics[page_id]
    assert len(diagnostics) == 1, diagnostics
    assert isinstance(diagnostics[0], AmbiguousTarget)

    # The following three roles all link to the same ".. option:: --config <filename>, -f <filename>" target
    # As such, they should have the SAME fileid, matching the html_id defined in that Target (option1_2).
    assert option1_2.html_id == "std-option-a-program.--config"
    roles = section.children[6].children
    check_ast_testing_string(
        roles[0].children[0].children[0],
        f"""
        <ref_role domain="std" name="option" target="a-program.-f" fileid="['a-program', '{option1_2.html_id}']">
        <literal><text>a-program -f</text></literal>
        </ref_role>
    """,
    )
    check_ast_testing_string(
        roles[1].children[0].children[0],
        f"""
        <ref_role domain="std" name="option" target="a-program.-f" fileid="['a-program', '{option1_2.html_id}']">
        <literal><text>-f</text></literal>
        </ref_role>
    """,
    )
    check_ast_testing_string(
        roles[2].children[0].children[0],
        f"""
        <ref_role domain="std" name="option" target="a-program.--config" fileid="['a-program', '{option1_2.html_id}']">
        <literal><text>--config</text></literal>
        </ref_role>
    """,
    )
    check_ast_testing_string(
        roles[3].children[0].children[0],
        """
        <ref_role domain="std" name="option" target="a-second-program.-v" fileid="['a-program', 'std-option-a-second-program.--version']">
        <literal><text>-v</text></literal>
        </ref_role>
    """,
    )
    check_ast_testing_string(
        roles[4].children[0].children[0],
        """
        <ref_role domain="std" name="option" target="a-program.-v" fileid="['a-program', 'std-option-a-program.--version']">
        <literal><text>a-program -v</text></literal>
        </ref_role>
    """,
    )


def test_doc_titles(backend: Backend) -> None:
    doc_id = FileId("folder/doc.txt")
    ast = backend.pages[doc_id].ast

    # Test relative path
    paragraph = ast.children[0]
    assert isinstance(paragraph, n.Paragraph)
    check_ast_testing_string(
        paragraph,
        """<paragraph><text>This doc role (</text><ref_role domain="std" name="doc" fileid="['../page1', '']"><text>Print this heading</text></ref_role><text>) should read "Print this heading".</text></paragraph>""",
    )

    # Test no heading in page
    paragraph = ast.children[1]
    assert isinstance(paragraph, n.Paragraph)
    check_ast_testing_string(
        paragraph,
        """<paragraph><text>This doc role (</text><ref_role domain="std" name="doc" fileid="['/page3', '']"></ref_role><text>) has no heading.</text></paragraph>""",
    )

    # Test doc with title specified inline
    paragraph = ast.children[2]
    assert isinstance(paragraph, n.Paragraph)
    check_ast_testing_string(
        paragraph,
        """<paragraph><text>This doc role (</text><ref_role domain="std" name="doc" fileid="['/page2', '']"><text>A Title</text></ref_role><text>) says "A Title" inline.</text></paragraph>""",
    )


def test_substitutions(backend: Backend) -> None:
    # Test substitutions as defined in snooty.toml
    page_id = FileId("page3.txt")
    ast = backend.pages[page_id].ast

    paragraph = ast.children[2]
    assert isinstance(paragraph, n.Paragraph)
    substitution_reference = paragraph.children[0]
    check_ast_testing_string(
        substitution_reference,
        """<substitution_reference name="service"><text>Atlas</text></substitution_reference>""",
    )

    substitution_reference = paragraph.children[2]
    check_ast_testing_string(
        substitution_reference,
        """<substitution_reference name="global-write-clusters"><text>Global </text><emphasis><text>Clusters</text></emphasis></substitution_reference>""",
    )

    # Verify that same substitution can be used multiple times on the same page
    paragraph = ast.children[4]
    assert isinstance(paragraph, n.Paragraph)
    substitution_reference = paragraph.children[0]
    check_ast_testing_string(
        substitution_reference,
        """<substitution_reference name="service"><text>Atlas</text></substitution_reference>""",
    )

    # Test substitution of empty string
    paragraph = ast.children[3]
    assert isinstance(paragraph, n.Paragraph)
    substitution_reference = paragraph.children[1]
    check_ast_testing_string(
        substitution_reference,
        """<substitution_reference name="blank"></substitution_reference>""",
    )

    # Test substitutions defined in-page
    paragraph = ast.children[6]
    assert isinstance(paragraph, n.Paragraph)
    substitution_reference = paragraph.children[0]
    check_ast_testing_string(
        substitution_reference,
        """<substitution_reference name="sub"><text>Diff</text></substitution_reference>""",
    )

    page_id = FileId("page4.txt")
    ast = backend.pages[page_id].ast
    assert isinstance(ast, n.Root)

    substution_definition = ast.children[2]
    assert isinstance(substution_definition, n.SubstitutionDefinition)
    check_ast_testing_string(
        substution_definition,
        """<substitution_definition name="sub"><text>Substitution</text></substitution_definition>""",
    )

    paragraph = ast.children[1]
    assert isinstance(paragraph, n.Paragraph)
    substitution_reference = paragraph.children[1]
    check_ast_testing_string(
        substitution_reference,
        """<substitution_reference name="sub"><text>Substitution</text></substitution_reference>""",
    )

    paragraph = ast.children[3]
    assert isinstance(paragraph, n.Paragraph)
    substitution_reference = paragraph.children[1]
    check_ast_testing_string(
        substitution_reference,
        """<substitution_reference name="sub"><text>Substitution</text></substitution_reference>""",
    )

    # Test substitution used in an include
    page_id = FileId("page5.txt")
    ast = backend.pages[page_id].ast
    assert isinstance(ast, n.Root)
    paragraph = ast.children[1]
    assert isinstance(paragraph, n.Paragraph)
    substitution_reference = paragraph.children[1]
    check_ast_testing_string(
        substitution_reference,
        """<substitution_reference name="included-sub"><text>ack</text></substitution_reference>""",
    )

    paragraph = ast.children[3]
    assert isinstance(paragraph, n.Paragraph)
    substitution_reference = paragraph.children[1]
    check_ast_testing_string(
        substitution_reference,
        """<substitution_reference name="included-sub"><text>included substitution</text></substitution_reference>""",
    )

    include = ast.children[2]
    assert isinstance(include, n.Directive)
    root = include.children[0]
    assert isinstance(root, n.Root)
    paragraph = root.children[1]
    assert isinstance(paragraph, n.Paragraph)
    substitution_reference = paragraph.children[1]
    check_ast_testing_string(
        substitution_reference,
        """<substitution_reference name="use-in-include"><text>Yes</text></substitution_reference>""",
    )

    # Test circular substitutionos
    page_id = FileId("circular.txt")

    diagnostics = backend.diagnostics[page_id]
    assert len(diagnostics) == 4

    ast = backend.pages[page_id].ast
    assert isinstance(ast, n.Root)
    paragraph = ast.children[2]
    assert isinstance(paragraph, n.Paragraph)
    substitution_reference = paragraph.children[0]
    check_ast_testing_string(
        substitution_reference,
        """<substitution_reference name="operation"><substitution_reference name="add"></substitution_reference></substitution_reference>""",
    )

    paragraph = ast.children[7]
    assert isinstance(paragraph, n.Paragraph)
    substitution_reference = paragraph.children[0]
    check_ast_testing_string(
        substitution_reference,
        """<substitution_reference name="baz"><substitution_reference name="foo"></substitution_reference></substitution_reference>""",
    )

    paragraph = ast.children[3]
    assert isinstance(paragraph, n.Paragraph)
    check_ast_testing_string(
        paragraph, "<paragraph><text>Testing content here.</text></paragraph>"
    )

    # Test nested substitutions
    page_id = FileId("nested.txt")
    ast = backend.pages[page_id].ast
    assert isinstance(ast, n.Root)
    paragraph = ast.children[2]
    assert isinstance(paragraph, n.Paragraph)
    substitution_reference = paragraph.children[0]
    check_ast_testing_string(
        substitution_reference,
        """<substitution_reference name="weather"><substitution_reference name="sun"><text>sun</text></substitution_reference></substitution_reference>""",
    )


def test_process_arguments(backend: Backend) -> None:
    """Ensure that directive arguments are ingested by postprocess layer"""
    note_id = FileId("note.txt")
    ast = backend.pages[note_id].ast
    note = ast.children[0]
    assert isinstance(note, n.Directive)

    print(ast_to_testing_string(note))
    check_ast_testing_string(
        note,
        """<directive name="note">
        <text>What's new in</text>
        <substitution_reference name="service"><text>Atlas</text></substitution_reference>
        <paragraph><text>Describe what's new.</text></paragraph></directive>""",
    )
