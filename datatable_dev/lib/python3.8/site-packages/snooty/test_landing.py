from pathlib import Path

import pytest

from . import n
from .parser import Project
from .test_project import Backend
from .types import BuildIdentifierSet, FileId
from .util_test import check_ast_testing_string


@pytest.fixture
def backend() -> Backend:
    backend = Backend()
    build_identifiers: BuildIdentifierSet = {}
    with Project(Path("test_data/test_landing"), backend, build_identifiers) as project:
        project.build()

    return backend


def test_queryable_fields(backend: Backend) -> None:
    page_id = FileId("index.txt")
    page = backend.pages[page_id]
    assert len(page.static_assets) == 2

    ast = page.ast
    section = ast.children[0]
    assert isinstance(section, n.Section)

    introduction = section.children[1]
    check_ast_testing_string(
        introduction,
        """<directive domain="landing" name="introduction"><paragraph><text>
   MongoDB is a distributed, object-oriented database that uses\nJSON-like documents to work with data in a more natural way.</text></paragraph></directive>""",
    )

    atf_image = section.children[2]
    check_ast_testing_string(
        atf_image,
        """<directive name="atf-image" checksum="71bf03ab0c5b8d46f0c03b77db6bd18a77d984d216c62c3519dfb45c162cd86b"><text>/images/pink.png</text></directive>""",
    )

    card_group = section.children[3]
    check_ast_testing_string(
        card_group,
        """<directive domain="landing" name="card-group">
        <directive domain="landing" name="card" headline="Run a self-managed database" cta="Get started with MongoDB" url="http://mongodb.com" icon="/images/purple.png" icon-alt="Alt text" tag="server" checksum="637c0eefb8026322bb5a563325a4ee70c53e2e7b9aecee576e2c576f2e3e1df6">
        <paragraph><text>Download and install the MongoDB database on your own\ninfrastructure.</text></paragraph>
        </directive>
        </directive>
        """,
    )

    section = section.children[4]
    assert isinstance(section, n.Section)

    cta = section.children[2]
    check_ast_testing_string(
        cta,
        """<directive domain="landing" name="cta"><paragraph><reference refuri="https://docs.mongodb.com/manual/introduction"><text>Read the Introduction to MongoDB</text></reference></paragraph></directive>""",
    )

    image = section.children[3]
    check_ast_testing_string(
        image,
        """<directive name="image" alt="Alt text label" class="fancypantscalloutpicturething" checksum="71bf03ab0c5b8d46f0c03b77db6bd18a77d984d216c62c3519dfb45c162cd86b"><text>/images/pink.png</text></directive>""",
    )
