from pathlib import Path

from . import rstparser
from .parser import JSONVisitor, parse_rst
from .types import FileId, ProjectConfig
from .util_test import check_ast_testing_string


def test_legacy_guides() -> None:
    root = Path("test_data")
    legacy_path = Path(root).joinpath(Path("guides/test_legacy_guides.rst"))
    new_path = Path(root).joinpath(Path("guides/test_guides.rst"))
    project_config = ProjectConfig(root, "", source="./")
    parser = rstparser.Parser(project_config, JSONVisitor)
    legacy_page, legacy_diagnostics = parse_rst(parser, legacy_path, None)
    new_page, new_diagnostics = parse_rst(parser, new_path, None)

    legacy_page.finish(legacy_diagnostics)
    new_page.finish(new_diagnostics)

    def make_correct(fileid: FileId) -> str:
        return "".join(
            (
                f'<root fileid="{fileid.as_posix()}" guide="">',
                '<section><heading id="sample-app"><text>Sample App</text></heading>',
                '<directive name="author"><text>MongoDB</text></directive>',
                '<directive name="category"><text>Getting Started</text></directive>',
                '<directive name="languages"><list enumtype="unordered">',
                "<listItem><paragraph><text>shell</text></paragraph></listItem>",
                "<listItem><paragraph><text>java-sync</text></paragraph></listItem>",
                "</list></directive>",
                '<directive name="level"><text>beginner</text></directive>',
                '<directive name="product_version"><text>4.0</text></directive>',
                '<directive name="result_description"><paragraph><text>In this guide...</text>',
                "</paragraph></directive>",
                '<directive name="time"><text>15</text></directive>',
                '<directive name="prerequisites">',
                "<paragraph><text>Before you start this Guide...</text></paragraph>",
                "<paragraph><text>The ",
                '</text><reference refuri="https://docs.mongodb.com/stitch/authentication/google/">',
                "<text>Google Authentication Page</text></reference><text>.</text></paragraph></directive>",
                '<directive name="check_your_environment"><paragraph><text>stuff</text></paragraph>',
                '</directive><directive name="procedure"><paragraph><text>stuff</text></paragraph>',
                '</directive><directive name="summary"><paragraph><text>stuff</text></paragraph>',
                '</directive><directive name="whats_next"><paragraph><text>stuff</text></paragraph>',
                "</directive>",
                '<directive name="guide-index">',
                '<directive name="card"><text>server/introduction</text></directive>',
                '<directive name="card"><text>server/auth</text></directive>',
                '<directive name="multi-card"><text>MongoDB in the Cloud</text>',
                '<list enumtype="unordered"><listItem><paragraph><text>cloud/atlas</text></paragraph></listItem>',
                "<listItem><paragraph><text>cloud/connectionstring</text></paragraph></listItem></list>",
                "</directive>",
                '<directive name="multi-card"><text>Migrate to MongoDB Atlas</text>',
                '<list enumtype="unordered"><listItem><paragraph><text>cloud/migrate-from-aws-to-atlas</text></paragraph>',
                "</listItem></list></directive>",
                '<directive name="card"><text>server/import</text></directive>',
                '<directive name="card"><text>server/drivers</text></directive>',
                '<directive name="multi-card">',
                '<text>CRUD Guides: Create, Read, Update, and Delete Data</text><list enumtype="unordered">',
                "<listItem><paragraph><text>server/insert</text></paragraph></listItem>",
                "<listItem><paragraph><text>server/read</text></paragraph></listItem>",
                "</list></directive>",
                '<directive name="card"><text>stitch/react_googleauth</text></directive></directive>',
                "</section>",
                "</root>",
            )
        )

    # Ensure that legacy syntax and new syntax create the same output
    check_ast_testing_string(
        legacy_page.ast, make_correct(FileId("guides/test_legacy_guides.rst"))
    )
    check_ast_testing_string(
        new_page.ast, make_correct(FileId("guides/test_guides.rst"))
    )
