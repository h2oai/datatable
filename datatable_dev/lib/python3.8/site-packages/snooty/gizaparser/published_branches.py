"""Models for giza's published_branches files.

These files define which branches should be built for the site, and allow for labeling branches
as published, active, stable, or upcoming. They also populate data for the version selection dropdown
on the docs front-end.
"""

from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from ..diagnostics import Diagnostic
from ..flutter import checked
from ..types import ProjectConfig, SerializableType
from .parse import parse


@checked
@dataclass
class PublishedBranchesVersion:
    published: List[str]
    active: List[str]
    stable: Optional[str]
    upcoming: Optional[str]

    def serialize(self) -> SerializableType:
        node: Dict[str, SerializableType] = {}

        node["published"] = self.published or None
        node["active"] = self.active or None
        node["stable"] = self.stable or None
        node["upcoming"] = self.upcoming or None

        return node


@checked
@dataclass
class PublishedBranchesGitBranches:
    manual: Optional[str]
    published: List[str]

    def serialize(self) -> SerializableType:
        node: Dict[str, SerializableType] = {}

        node["manual"] = self.manual or None
        node["published"] = self.published or None

        return node


@checked
@dataclass
class PublishedBranchesGit:
    branches: PublishedBranchesGitBranches

    def serialize(self) -> SerializableType:
        branches_node: Dict[str, SerializableType] = {}

        branches_node["branches"] = self.branches.serialize()

        return branches_node


@checked
@dataclass
class PublishedBranches:
    prefix: Optional[str]
    version: PublishedBranchesVersion
    git: PublishedBranchesGit

    def serialize(self) -> SerializableType:
        published_branches_node: Dict[str, SerializableType] = {}

        published_branches_node["version"] = self.version.serialize()
        published_branches_node["git"] = self.git.serialize()

        return published_branches_node


def parse_published_branches(
    path: Path, project_config: ProjectConfig, text: Optional[str] = None
) -> Tuple[Optional[PublishedBranches], List[Diagnostic]]:
    try:
        published_branches, _, diagnostics = parse(
            PublishedBranches, path, project_config, text
        )
        return published_branches[0], diagnostics
    except IndexError:
        return None, diagnostics
