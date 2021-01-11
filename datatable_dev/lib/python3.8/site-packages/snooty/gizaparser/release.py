from dataclasses import dataclass
from pathlib import Path
from typing import Callable, List, Optional, Sequence, Tuple

from .. import n
from ..diagnostics import Diagnostic, MissingRef
from ..flutter import checked
from ..page import Page
from ..types import EmbeddedRstParser
from .nodes import GizaCategory, Inheritable
from .parse import parse


@checked
@dataclass
class ReleaseSpecification(Inheritable):
    pre: Optional[str]
    copyable: Optional[bool]
    language: Optional[str]
    code: Optional[str]

    def render(self, page: Page, rst_parser: EmbeddedRstParser) -> List[n.Node]:
        children: List[n.Node] = []
        if self.pre:
            result = rst_parser.parse_block(self.pre, self.line)
            children.extend(result)

        if self.code:
            children.append(
                n.Code(
                    (self.line,),
                    self.language,
                    None,
                    True if self.copyable is None else self.copyable,
                    None,
                    self.code,
                    False,
                )
            )
        return children


class GizaReleaseSpecificationCategory(GizaCategory[ReleaseSpecification]):
    def parse(
        self, path: Path, text: Optional[str] = None
    ) -> Tuple[Sequence[ReleaseSpecification], str, List[Diagnostic]]:
        nodes, text, diagnostics = parse(
            ReleaseSpecification, path, self.project_config, text
        )

        def report_missing_ref(node: ReleaseSpecification) -> bool:
            diagnostics.append(MissingRef("release specifications", node.line))
            return False

        # All nodes must have an explicitly-defined ref ID
        release_specifications = [
            node for node in nodes if node.ref or report_missing_ref(node)
        ]
        return release_specifications, text, diagnostics

    def to_pages(
        self,
        source_path: Path,
        page_factory: Callable[[str], Tuple[Page, EmbeddedRstParser]],
        nodes: Sequence[ReleaseSpecification],
    ) -> List[Page]:
        pages: List[Page] = []
        source_fileid = self.project_config.get_fileid(source_path)

        for node in nodes:
            assert node.ref is not None
            if node.ref.startswith("_"):
                continue

            page, rst_parser = page_factory(f"{node.ref}.rst")
            page.category = "release"

            rendered = node.render(page, rst_parser)
            release_directive = n.Directive(
                (node.line,), [], "", "release_specification", [], {}
            )
            release_directive.children = rendered

            page.ast = n.Root((0,), [], source_fileid, {})
            page.ast.children.append(release_directive)

            pages.append(page)

        return pages
