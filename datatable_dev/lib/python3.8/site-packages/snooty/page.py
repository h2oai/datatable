from dataclasses import dataclass, field
from pathlib import Path, PurePath
from typing import Dict, List, Optional, Set

from . import n
from .diagnostics import Diagnostic
from .target_database import EmptyProjectInterface, ProjectInterface
from .types import FileId, StaticAsset


class PendingTask:
    """A thunk which will be executed in the main process after the full tree is
       constructed. This should primarily be used to execute tasks which may need
       to mutate state from the main process (e.g. caches or dependency graphs)."""

    def __init__(self, node: n.Node) -> None:
        self.node = node

    def __call__(
        self, diagnostics: List[Diagnostic], project: ProjectInterface
    ) -> None:
        """Perform an action in the main process once the tree has been built."""
        pass


@dataclass
class Page:
    source_path: Path
    output_filename: str
    source: str
    ast: n.Parent[n.Node]
    static_assets: Set[StaticAsset] = field(default_factory=set)
    pending_tasks: List[PendingTask] = field(default_factory=list)
    category: Optional[str] = None
    query_fields: Dict[str, n.SerializableType] = field(default_factory=dict)

    @classmethod
    def create(
        self,
        source_path: Path,
        output_filename: Optional[str],
        source: str,
        ast: Optional[n.Parent[n.Node]] = None,
    ) -> "Page":
        if output_filename is None:
            output_filename = source_path.name

        if ast is None:
            ast = n.Root((0,), [], FileId(source_path), {})

        return Page(source_path, output_filename, source, ast)

    def fake_full_path(self) -> PurePath:
        """Return a fictitious path (hopefully) uniquely identifying this output artifact."""
        if self.category:
            # Giza wrote out yaml file artifacts under a directory. e.g. steps-foo.yaml becomes
            # steps/foo.rst
            return self.source_path.parent.joinpath(
                PurePath(self.category), self.output_filename
            )
        return self.source_path

    def finish(
        self, diagnostics: List[Diagnostic], project: Optional[ProjectInterface] = None
    ) -> None:
        """Finish all pending tasks for this page. This should be run in the main process."""
        for task in self.pending_tasks:
            task(
                diagnostics, project if project is not None else EmptyProjectInterface()
            )

        self.pending_tasks.clear()
