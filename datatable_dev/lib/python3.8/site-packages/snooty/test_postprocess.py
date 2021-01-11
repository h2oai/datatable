"""An alternative and more granular approach to writing postprocessing tests.
   Eventually most postprocessor tests should probably be moved into this format."""

from pathlib import Path

from . import diagnostics
from .diagnostics import (
    DuplicateDirective,
    ExpectedTabs,
    MissingTab,
    TabMustBeDirective,
    TargetNotFound,
)
from .types import FileId
from .util_test import ast_to_testing_string, check_ast_testing_string, make_test


# ensure that broken links still generate titles
def test_broken_link() -> None:
    with make_test(
        {
            Path(
                "source/param.txt"
            ): """
======
$title
======

.. parameter:: $title

The :parameter:`title` stuff works


"""
        }
    ) as result:
        diagnostics = result.diagnostics[FileId("param.txt")]
        assert len(diagnostics) == 1
        assert isinstance(diagnostics[0], TargetNotFound)

        page = result.pages[FileId("param.txt")]
        check_ast_testing_string(
            page.ast,
            """
<root fileid="param.txt">
    <section>
        <heading id="-title">
            <text>$title</text>
        </heading>
        <target domain="mongodb" name="parameter" html_id="mongodb-parameter-param.-title">
            <directive_argument>
                <literal><text>$title</text></literal>
            </directive_argument>
            <target_identifier ids="['param.$title']">
                <text>$title</text>
            </target_identifier>
        </target>
        <paragraph>
            <text>The </text>
            <ref_role domain="mongodb" name="parameter" target="param.title">
                <literal><text>title</text></literal>
            </ref_role>
            <text> stuff works</text>
        </paragraph>
    </section>
</root>
                """,
        )


def test_case_sensitive_labels() -> None:
    with make_test(
        {
            Path(
                "source/index.txt"
            ): """
.. _a-MixedCase-Label:

============
Main Heading
============

:ref:`a-mixedcase-label`

:ref:`a-MixedCase-Label`
"""
        }
    ) as result:
        assert [
            "a-mixedcase-label" in diagnostic.message
            for diagnostic in result.diagnostics[FileId("index.txt")]
        ] == [True], "Incorrect diagnostics raised"
        page = result.pages[FileId("index.txt")]
        print(ast_to_testing_string(page.ast))
        check_ast_testing_string(
            page.ast,
            """
<root fileid="index.txt">
    <target domain="std" name="label" html_id="std-label-a-MixedCase-Label">
        <target_identifier ids="['a-MixedCase-Label']"><text>Main Heading</text></target_identifier>
    </target>
    <section>
        <heading id="main-heading"><text>Main Heading</text></heading>
        <paragraph><ref_role domain="std" name="label" target="a-mixedcase-label"><text>a-mixedcase-label</text></ref_role></paragraph>
        <paragraph>
            <ref_role domain="std" name="label" target="a-MixedCase-Label" fileid="['index', 'std-label-a-MixedCase-Label']">
                <text>Main Heading</text>
            </ref_role>
        </paragraph>
    </section>
</root>
        """,
        )


def test_same_page_target_resolution() -> None:
    with make_test(
        {
            Path(
                "source/program1.txt"
            ): """
=========
Program 1
=========

.. program:: program1

.. option:: --verbose

   Verbose

.. include:: /includes/fact.rst
""",
            Path(
                "source/program2.txt"
            ): """
=========
Program 2
=========

.. program:: program2

.. option:: --verbose

   Verbose

.. include:: /includes/fact.rst
""",
            Path(
                "source/includes/fact.rst"
            ): """
:option:`--verbose`

:option:`program1 --verbose`

:option:`program2 --verbose`
""",
        }
    ) as result:
        assert not [
            diagnostics for diagnostics in result.diagnostics.values() if diagnostics
        ], "Should not raise any diagnostics"
        page = result.pages[FileId("program1.txt")]
        check_ast_testing_string(
            page.ast,
            """
<root fileid="program1.txt"><section>
    <heading id="program-1"><text>Program 1</text></heading>

    <target domain="std" name="program" html_id="std-program-program1">
        <directive_argument>
            <literal><text>program1</text></literal>
        </directive_argument>
        <target_identifier ids="['program1']"><text>program1</text></target_identifier>
    </target>

    <target domain="std" name="option" html_id="std-option-program1.--verbose">
        <directive_argument><literal><text>--verbose</text></literal></directive_argument>
        <target_identifier ids="['--verbose', 'program1.--verbose']"><text>program1 --verbose</text></target_identifier>
        <paragraph><text>Verbose</text></paragraph>
    </target>

    <directive name="include">
        <text>/includes/fact.rst</text>
        <root fileid="includes/fact.rst">
            <paragraph>
                <ref_role domain="std" name="option" target="program1.--verbose" fileid="['program1', 'std-option-program1.--verbose']">
                    <literal><text>--verbose</text></literal>
                </ref_role>
            </paragraph>

            <paragraph>
                <ref_role domain="std" name="option" target="program1.--verbose" fileid="['program1', 'std-option-program1.--verbose']">
                    <literal><text>program1 --verbose</text></literal>
                </ref_role>
            </paragraph>

            <paragraph>
                <ref_role domain="std" name="option" target="program2.--verbose" fileid="['program2', 'std-option-program2.--verbose']">
                    <literal><text>program2 --verbose</text></literal>
                </ref_role>
            </paragraph>
        </root>
    </directive>
</section></root>
        """,
        )

        page = result.pages[FileId("program2.txt")]
        check_ast_testing_string(
            page.ast,
            """
<root fileid="program2.txt"><section>
    <heading id="program-2"><text>Program 2</text></heading>

    <target domain="std" name="program" html_id="std-program-program2">
        <directive_argument>
            <literal><text>program2</text></literal>
        </directive_argument>
        <target_identifier ids="['program2']"><text>program2</text></target_identifier>
    </target>

    <target domain="std" name="option" html_id="std-option-program2.--verbose">
        <directive_argument><literal><text>--verbose</text></literal></directive_argument>
        <target_identifier ids="['--verbose', 'program2.--verbose']"><text>program2 --verbose</text></target_identifier>
        <paragraph><text>Verbose</text></paragraph>
    </target>

    <directive name="include">
        <text>/includes/fact.rst</text>
        <root fileid="includes/fact.rst">
            <paragraph>
                <ref_role domain="std" name="option" target="program2.--verbose" fileid="['program2', 'std-option-program2.--verbose']">
                    <literal><text>--verbose</text></literal>
                </ref_role>
            </paragraph>

            <paragraph>
                <ref_role domain="std" name="option" target="program1.--verbose" fileid="['program1', 'std-option-program1.--verbose']">
                    <literal><text>program1 --verbose</text></literal>
                </ref_role>
            </paragraph>

            <paragraph>
                <ref_role domain="std" name="option" target="program2.--verbose" fileid="['program2', 'std-option-program2.--verbose']">
                    <literal><text>program2 --verbose</text></literal>
                </ref_role>
            </paragraph>
        </root>
    </directive>
</section></root>
        """,
        )


def test_language_selector() -> None:
    with make_test(
        {
            Path(
                "source/tabs.txt"
            ): """
.. tabs-pillstrip:: languages

.. tabs-drivers::
   :hidden:

   .. tab::
      :tabid: shell

      Shell

   .. tab::
      :tabid: python

      Python
"""
        }
    ) as result:
        for d in result.diagnostics[FileId("tabs.txt")]:
            print(d.message)
        assert [
            "deprecated" in diagnostic.message
            for diagnostic in result.diagnostics[FileId("tabs.txt")]
        ] == [True], "Incorrect diagnostics raised"
        page = result.pages[FileId("tabs.txt")]
        print(ast_to_testing_string(page.ast))
        check_ast_testing_string(
            page.ast,
            """
<root fileid="tabs.txt" selectors="{'drivers': {'shell': [{'type': 'text', 'position': {'start': {'line': 3}}, 'value': 'Mongo Shell'}], 'python': [{'type': 'text', 'position': {'start': {'line': 3}}, 'value': 'Python'}]}}">
<directive name="tabs-pillstrip"><text>languages</text></directive>
<directive name="tabs" hidden="True" tabset="drivers">
<directive name="tab" tabid="shell"><text>Mongo Shell</text>
<paragraph><text>Shell</text></paragraph>
</directive>
<directive name="tab" tabid="python"><text>Python</text>
<paragraph><text>Python</text></paragraph>
</directive>
</directive>
</root>
            """,
        )

    # Ensure that diagnostic is output when some languages don't appear in every tabset
    with make_test(
        {
            Path(
                "source/tabs-two.txt"
            ): """
.. tabs-selector:: drivers

.. tabs-drivers::
   :hidden:

   .. tab::
      :tabid: c

      C

   .. tab::
      :tabid: nodejs

      Node.js

.. tabs-drivers::
   :hidden:

   .. tab::
      :tabid: c

      C

.. tabs-drivers::
   :hidden:

   .. tab::
      :tabid: python

      Python
"""
        }
    ) as result:
        assert [
            "nodejs" in d.message
            and "c" in d.message
            and "python" in d.message
            and type(d) == MissingTab
            for d in result.diagnostics[FileId("tabs-two.txt")]
        ] == [True], "Incorrect diagnostics raised"
        page = result.pages[FileId("tabs-two.txt")]
        print(ast_to_testing_string(page.ast))
        check_ast_testing_string(
            page.ast,
            """
<root fileid="tabs-two.txt" selectors="{'drivers': {'nodejs': [{'type': 'text', 'position': {'start': {'line': 3}}, 'value': 'Node.js'}], 'c': [{'type': 'text', 'position': {'start': {'line': 3}}, 'value': 'C'}]}}">
<directive name="tabs-selector"><text>drivers</text></directive>
<directive name="tabs" hidden="True" tabset="drivers">
<directive name="tab" tabid="nodejs"><text>Node.js</text>
<paragraph><text>Node.js</text></paragraph>
</directive>
<directive name="tab" tabid="c"><text>C</text>
<paragraph><text>C</text></paragraph>
</directive>
</directive>
<directive name="tabs" hidden="True" tabset="drivers">
<directive name="tab" tabid="c"><text>C</text>
<paragraph><text>C</text></paragraph>
</directive>
</directive>
<directive name="tabs" hidden="True" tabset="drivers">
<directive name="tab" tabid="python"><text>Python</text>
<paragraph><text>Python</text></paragraph>
</directive>
</directive>
</root>
            """,
        )

    # Ensure that diagnostic is output when tabs are missing from page with language selector
    with make_test(
        {
            Path(
                "source/tabs-three.txt"
            ): """
.. tabs-selector:: drivers
"""
        }
    ) as result:
        assert [
            type(diagnostic) == ExpectedTabs
            for diagnostic in result.diagnostics[FileId("tabs-three.txt")]
        ] == [True], "Incorrect diagnostics raised"
        page = result.pages[FileId("tabs-three.txt")]
        print(ast_to_testing_string(page.ast))
        check_ast_testing_string(
            page.ast,
            """
<root fileid="tabs-three.txt">
<directive name="tabs-selector"><text>drivers</text></directive>
</root>
            """,
        )

    # Ensure postprocessor doesn't fail when no argument is specified
    with make_test(
        {
            Path(
                "source/tabs-four.txt"
            ): """
.. tabs-selector::

.. tabs-drivers::
   :hidden:

   .. tab::
      :tabid: java-sync

      Java (sync)
"""
        }
    ) as result:
        assert not [
            diagnostics for diagnostics in result.diagnostics.values() if diagnostics
        ], "Should not raise any diagnostics"
        page = result.pages[FileId("tabs-four.txt")]
        print(ast_to_testing_string(page.ast))
        check_ast_testing_string(
            page.ast,
            """
<root fileid="tabs-four.txt">
<directive name="tabs-selector" />
<directive name="tabs" hidden="True" tabset="drivers">
<directive name="tab" tabid="java-sync"><text>Java (Sync)</text>
<paragraph><text>Java (sync)</text></paragraph>
</directive>
</directive>
</root>
            """,
        )

    # Ensure non-drivers tabset works properly
    with make_test(
        {
            Path(
                "source/tabs-five.txt"
            ): """
.. tabs-selector:: platforms

.. tabs-platforms::
   :hidden:

   .. tab::
      :tabid: windows

      Windows

   .. tab::
      :tabid: macos

      macOS

   .. tab::
      :tabid: linux

      Linux
"""
        }
    ) as result:
        assert not [
            diagnostics for diagnostics in result.diagnostics.values() if diagnostics
        ], "Should not raise any diagnostics"
        page = result.pages[FileId("tabs-five.txt")]
        print(ast_to_testing_string(page.ast))
        check_ast_testing_string(
            page.ast,
            """
<root fileid="tabs-five.txt" selectors="{'platforms': {'windows': [{'type': 'text', 'position': {'start': {'line': 3}}, 'value': 'Windows'}], 'macos': [{'type': 'text', 'position': {'start': {'line': 3}}, 'value': 'macOS'}], 'linux': [{'type': 'text', 'position': {'start': {'line': 3}}, 'value': 'Linux'}]}}">
<directive name="tabs-selector"><text>platforms</text></directive>
<directive name="tabs" hidden="True" tabset="platforms">
<directive name="tab" tabid="windows"><text>Windows</text>
<paragraph><text>Windows</text></paragraph>
</directive>
<directive name="tab" tabid="macos"><text>macOS</text>
<paragraph><text>macOS</text></paragraph>
</directive>
<directive name="tab" tabid="linux"><text>Linux</text>
<paragraph><text>Linux</text></paragraph>
</directive>
</directive>
</root>
            """,
        )

    # Ensure a second tabs-selector directive doesnt' overwrite the first tabset
    with make_test(
        {
            Path(
                "source/tabs-six.txt"
            ): """
.. tabs-selector:: drivers

.. tabs-drivers::

   .. tab::
      :tabid: java-sync

      Java (sync)

   .. tab::
      :tabid: python

      Python tab

.. tabs-selector:: drivers

.. tabs-drivers::

   .. tab::
      :tabid: java-sync

      Java (sync)
"""
        }
    ) as result:
        diagnostics = result.diagnostics[FileId("tabs-six.txt")]
        assert isinstance(diagnostics[0], DuplicateDirective)
        assert isinstance(diagnostics[1], MissingTab)
        page = result.pages[FileId("tabs-six.txt")]
        print(ast_to_testing_string(page.ast))
        check_ast_testing_string(
            page.ast,
            """
<root fileid="tabs-six.txt" selectors="{'drivers': {'python': [{'type': 'text', 'position': {'start': {'line': 3}}, 'value': 'Python'}], 'java-sync': [{'type': 'text', 'position': {'start': {'line': 3}}, 'value': 'Java (Sync)'}]}}">
<directive name="tabs-selector"><text>drivers</text></directive>
<directive name="tabs" tabset="drivers">
<directive name="tab" tabid="python"><text>Python</text>
<paragraph><text>Python tab</text></paragraph>
</directive>
<directive name="tab" tabid="java-sync"><text>Java (Sync)</text>
<paragraph><text>Java (sync)</text></paragraph>
</directive>
</directive>
<directive name="tabs-selector"><text>drivers</text></directive>
<directive name="tabs" tabset="drivers">
<directive name="tab" tabid="java-sync"><text>Java (Sync)</text>
<paragraph><text>Java (sync)</text></paragraph>
</directive>
</directive>
</root>
            """,
        )

    # Ensure we gracefully handle invalid tabs
    with make_test(
        {
            Path(
                "source/tabs-six.txt"
            ): """
.. tabs-drivers::

   .. tab::
      :tabid: java-sync

      Java (sync)

   .. tip::

      A tip
"""
        }
    ) as result:
        assert [type(d) for d in result.diagnostics[FileId("tabs-six.txt")]] == [
            TabMustBeDirective
        ]


def test_correct_diagnostic_path() -> None:
    with make_test(
        {
            Path(
                "source/index.txt"
            ): """
.. _a-MixedCase-Label:

============
Main Heading
============

.. include:: /includes/fact.rst
""",
            Path(
                "source/includes/fact.rst"
            ): """
:ref:`missing-ref`
""",
        }
    ) as result:
        assert {
            k: [type(diag) for diag in v] for k, v in result.diagnostics.items() if v
        } == {
            FileId("includes/fact.rst"): [diagnostics.TargetNotFound]
        }, "Incorrect diagnostics raised"


def test_subcommands() -> None:
    with make_test(
        {
            Path(
                "source/index.txt"
            ): """
=========
Program 1
=========

.. program:: realmcli

.. option:: --verbose

   Verbose

.. option:: --version

   Version

.. program:: realmcli import

.. option:: --verbose

   Verbose

.. program:: realmcli export

.. option:: --verbose

   Verbose

.. program:: realmcli export just-silly-now

.. option:: --verbose

   Verbose

.. program:: a-program

.. option:: --verbose

   Verbose

""",
            Path(
                "source/links.txt"
            ): """
* :option:`realmcli export --verbose`
* :option:`realmcli export just-silly-now --verbose`
* :option:`realmcli import --verbose`
* :option:`realmcli --verbose`
""",
        }
    ) as result:
        assert not [
            diagnostics for diagnostics in result.diagnostics.values() if diagnostics
        ], "Should not raise any diagnostics"
        check_ast_testing_string(
            result.pages[FileId("links.txt")].ast,
            """
<root fileid="links.txt">
<list enumtype="unordered">
    <listItem>
        <paragraph>
            <ref_role domain="std" name="option" target="realmcli export.--verbose" fileid="['index', 'std-option-realmcli-export.--verbose']">
                <literal><text>realmcli export --verbose</text></literal>
            </ref_role>
        </paragraph>
    </listItem>
    <listItem>
        <paragraph>
            <ref_role domain="std" name="option" target="realmcli export just-silly-now.--verbose" fileid="['index', 'std-option-realmcli-export-just-silly-now.--verbose']">
                <literal><text>realmcli export just-silly-now --verbose</text></literal>
            </ref_role>
        </paragraph>
    </listItem>
    <listItem>
        <paragraph>
            <ref_role domain="std" name="option" target="realmcli import.--verbose" fileid="['index', 'std-option-realmcli-import.--verbose']">
                <literal><text>realmcli import --verbose</text></literal>
            </ref_role>
        </paragraph>
    </listItem>
    <listItem>
        <paragraph>
            <ref_role domain="std" name="option" target="realmcli.--verbose" fileid="['index', 'std-option-realmcli.--verbose']">
                <literal><text>realmcli --verbose</text></literal>
            </ref_role>
        </paragraph>
    </listItem>
</list>
</root>
    """,
        )

    # Ensure that the correct HTML IDs exist
    index_page = result.pages[FileId("index.txt")]
    index_page_ast = ast_to_testing_string(index_page.ast)
    for html_id in (
        "std-option-realmcli.--verbose",
        "std-option-realmcli-export.--verbose",
        "std-option-realmcli-export-just-silly-now.--verbose",
        "std-option-realmcli-import.--verbose",
    ):
        assert f'"{html_id}"' in index_page_ast, f"missing {html_id}"


def test_heading_id_unique() -> None:
    with make_test(
        {
            Path(
                "source/index.txt"
            ): """
==========
Index Page
==========

A Heading
---------

.. include:: /includes/fact.rst

.. include:: /includes/fact.rst
""",
            Path(
                "source/includes/fact.rst"
            ): """
A Heading
~~~~~~~~~
        """,
        }
    ) as result:
        for d in result.diagnostics[FileId("index.txt")]:
            print(d.message)
        assert not result.diagnostics[FileId("index.txt")]
        page = result.pages[FileId("index.txt")]
        check_ast_testing_string(
            page.ast,
            """
<root fileid="index.txt">
<section>
    <heading id="index-page"><text>Index Page</text></heading>
    <section>
        <heading id="a-heading"><text>A Heading</text></heading>
        <directive name="include"><text>/includes/fact.rst</text>
            <root fileid="includes/fact.rst">
                <section>
                    <heading id="a-heading-1"><text>A Heading</text></heading>
                </section>
            </root>
        </directive>
        <directive name="include"><text>/includes/fact.rst</text>
            <root fileid="includes/fact.rst">
                <section>
                    <heading id="a-heading-2"><text>A Heading</text></heading>
                </section>
            </root>
        </directive>
    </section>
</section></root>""",
        )


def test_include_subset() -> None:
    with make_test(
        {
            # Splice on comments
            Path(
                "source/program1.txt"
            ): """
.. include:: /includes/included.rst
   :start-after: start-comment
   :end-before: end-comment
""",
            # Splice on labels
            Path(
                "source/program2.txt"
            ): """
.. include:: /includes/included.rst
   :start-after: start-label
   :end-before: end-label
""",
            Path(
                "source/includes/included.rst"
            ): """
.. start-comment

Section Heading
---------------

Paragraph.

.. end-comment

.. _start-label:

Section Heading
---------------

Paragraph

.. _end-label:
""",
        }
    ) as result:
        assert not [
            diagnostics for diagnostics in result.diagnostics.values() if diagnostics
        ], "Should not raise any diagnostics"
        page = result.pages[FileId("program1.txt")]
        check_ast_testing_string(
            page.ast,
            """
<root fileid="program1.txt">
    <directive name="include" start-after="start-comment" end-before="end-comment">
        <text>/includes/included.rst</text>
        <root fileid="includes/included.rst">
            <comment><text>start-comment</text></comment>
            <section>
                <heading id="section-heading">
                    <text>Section Heading</text>
                </heading>
                <paragraph>
                    <text>Paragraph.</text>
                </paragraph>
                <comment>
                    <text>end-comment</text>
                </comment>
            </section>
        </root>
    </directive>
</root>
        """,
        )

        page = result.pages[FileId("program2.txt")]
        check_ast_testing_string(
            page.ast,
            """
<root fileid="program2.txt">
    <directive name="include" start-after="start-label" end-before="end-label">
        <text>/includes/included.rst</text>
        <root fileid="includes/included.rst">
            <section>
                <target domain="std" name="label" html_id="std-label-start-label">
                    <target_identifier ids="['start-label']">
                        <text>Section Heading</text>
                    </target_identifier>
                </target>
            </section>
            <section>
                <heading id="section-heading">
                    <text>Section Heading</text>
                </heading>
                <paragraph>
                    <text>Paragraph</text>
                </paragraph>
                <target domain="std" name="label" html_id="std-label-end-label">
                    <target_identifier ids="['end-label']"/>
                </target>
            </section>
        </root>
    </directive>
</root>
        """,
        )


def test_include_subset_fails() -> None:
    with make_test(
        {
            # Splice on text in reverse order
            Path(
                "source/program1.txt"
            ): """
.. include:: /includes/included.rst
   :start-after: end-text
   :end-before: start-text
""",
            # Can't find start text
            Path(
                "source/program2.txt"
            ): """
.. include:: /includes/included.rst
   :start-after: fake-start-text
   :end-before: end-text
""",
            # Can't find end text
            Path(
                "source/program3.txt"
            ): """
.. include:: /includes/included.rst
   :start-after: start-text
   :end-before: fake-end-text
""",
            Path(
                "source/includes/included.rst"
            ): """
.. start-comment

Section Heading
---------------

Paragraph.

.. end-comment

start-text

end-text

.. _start-label:

Section Heading
---------------

Paragraph

.. _end-label:
""",
        }
    ) as result:
        assert (
            len(
                [
                    diagnostics
                    for diagnostics in result.diagnostics.values()
                    if diagnostics
                ]
            )
            == 3
        ), "Should raise 3 diagnostics"
