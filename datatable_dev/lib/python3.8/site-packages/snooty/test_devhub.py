from pathlib import Path
from typing import Any, Dict, List, cast

import pytest

from .parser import Project
from .test_project import Backend
from .types import BuildIdentifierSet, FileId, SerializableType
from .util_test import ast_to_testing_string, check_ast_testing_string


@pytest.fixture
def backend() -> Backend:
    backend = Backend()
    build_identifiers: BuildIdentifierSet = {}
    with Project(Path("test_data/test_devhub"), backend, build_identifiers) as project:
        project.build()

    return backend


def simplify_dict(d: Dict[str, Any]) -> Dict[str, Any]:
    """Iterate over a dictionary and simplify child nodes into strings"""
    return {
        k: [ast_to_testing_string(n) for n in v] if k == "children" else v
        for k, v in d.items()
    }


def test_queryable_fields(backend: Backend) -> None:
    page_id = FileId("includes/authors/lastname-firstname.rst")
    page = backend.pages[page_id]
    query_fields: Dict[str, SerializableType] = page.query_fields
    assert len(page.static_assets) == 1

    page_id = FileId("index.txt")
    page = backend.pages[page_id]
    query_fields = page.query_fields
    assert len(page.static_assets) == 3
    assert query_fields is not None

    simple_authors: List[Dict[str, Any]] = [
        (simplify_dict(d)) for d in cast(Any, query_fields["author"])
    ]
    assert simple_authors == [
        {
            "name": "Firstname Lastname",
            "image": "/images/bio-name.jpg",
            "checksum": "324b32910cb1080451f033fea7f916c6d33ac851b868b4bca829a4b900a809d6",
            "children": ["<paragraph><text>Bio goes here.</text></paragraph>"],
        },
        {
            "name": "Eliot Horowitz",
            "image": "/images/bio-eliot.jpg",
            "location": "New York, NY",
            "company": "MongoDB Inc.",
            "title": "CTO",
            "website": "http://www.example.com",
            "twitter": "https://twitter.com/eliothorowitz",
            "github": "https://github.com/erh",
            "linkedin": "https://www.linkedin.com/in/eliothorowitz/",
            "checksum": "324b32910cb1080451f033fea7f916c6d33ac851b868b4bca829a4b900a809d6",
            "children": [
                "<paragraph><text>Eliot Horowitz is the CTO and Co-Founder of MongoDB. He wrote the core\ncode base for MongoDB starting in 2007, and subsequently built the\nengineering and product teams. Today, Eliot oversees those\nteams, and continues to drive technology innovations at MongoDB.</text></paragraph>"
            ],
        },
    ]
    assert query_fields["tags"] == ["foo", "bar", "baz"]
    assert query_fields["languages"] == ["nodejs", "java"]
    assert query_fields["products"] == ["Realm", "MongoDB"]
    # Incorrectly formatted date is omitted
    assert query_fields.get("pubdate") is None
    assert query_fields["updated-date"] == "2019-02-02"
    assert query_fields["atf-image"] == "/images/atf-images/generic/pattern-green.png"
    assert query_fields["type"] == "article, quickstart, how-to, video, live"
    assert query_fields["level"] == "beginner, intermediate, advanced"
    assert query_fields["slug"] == "/"

    related = cast(Any, query_fields["related"])
    check_ast_testing_string(
        related[0], "<literal><text>list of related articles</text></literal>"
    )
    check_ast_testing_string(
        related[1],
        """<ref_role domain="std" name="doc" fileid="['/path/to/article', '']"></ref_role>""",
    )
    check_ast_testing_string(
        related[2], """<literal><text>:doc:`/path/to/other/article`</text></literal>"""
    )

    meta_description = cast(Any, query_fields["meta-description"])
    check_ast_testing_string(
        meta_description[0],
        "<paragraph><text>meta description (160 characters or fewer)</text></paragraph>",
    )

    title = cast(Any, query_fields["title"])
    assert len(title) == 1
    check_ast_testing_string(title[0], "<text>h1 Article Title</text>")

    assert simplify_dict(cast(Any, query_fields["twitter"])) == {
        "site": "@mongodb",
        "creator": "@Lauren_Schaefer",
        "title": "Wordswordswords",
        "image": "/images/atf-images/generic/pink.png",
        "image-alt": "Description of the image",
        "checksum": "71bf03ab0c5b8d46f0c03b77db6bd18a77d984d216c62c3519dfb45c162cd86b",
        "children": [
            "<paragraph><text>Description of max 200 characters</text></paragraph>"
        ],
    }

    assert simplify_dict(cast(Any, query_fields["og"])) == {
        "url": "http://developer.mongodb.com/article/active-active-application-architectures",
        "type": "article",
        "title": "Active-Active Application Architectures",
        "image": "/images/atf-images/generic/purple.png",
        "checksum": "637c0eefb8026322bb5a563325a4ee70c53e2e7b9aecee576e2c576f2e3e1df6",
        "children": [
            "<paragraph><text>A brief description of the content, usually between 2 and 4\nsentences. This will displayed below the title of the post on\nFacebook.</text></paragraph>",
            "<paragraph><text>Supports multiple paragraphs.</text></paragraph>",
        ],
    }


def test_page_groups(backend: Backend) -> None:
    """Test that page groups are correctly filtered and cleaned."""
    page_groups: Dict[str, List[str]] = cast(Any, backend.metadata["pageGroups"])
    assert page_groups == {"Group 1": ["index", "index"]}
