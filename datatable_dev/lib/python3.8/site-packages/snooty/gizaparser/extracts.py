from dataclasses import dataclass
from pathlib import Path
from typing import Callable, List, Optional, Sequence, Tuple

from .. import n
from ..diagnostics import Diagnostic, MissingRef
from ..flutter import checked
from ..page import Page
from ..types import EmbeddedRstParser
from .nodes import GizaCategory, HeadingMixin, Inheritable
from .parse import parse


@checked
@dataclass
class Extract(Inheritable, HeadingMixin):
    content: Optional[str]
    only: Optional[str]

    def render(self, page: Page, rst_parser: EmbeddedRstParser) -> List[n.Node]:
        if self.only is not None:
            raise NotImplementedError('extracts: "only" not implemented')

        children: List[n.Node] = []
        children.extend(self.render_heading(rst_parser))
        if self.content:
            children.extend(rst_parser.parse_block(self.content, self.line))

        return children


class GizaExtractsCategory(GizaCategory[Extract]):
    def parse(
        self, path: Path, text: Optional[str] = None
    ) -> Tuple[Sequence[Extract], str, List[Diagnostic]]:
        extracts, text, diagnostics = parse(Extract, path, self.project_config, text)

        def report_missing_ref(extract: Extract) -> bool:
            diagnostics.append(MissingRef("extracts", extract.line))
            return False

        # All extracts must have an explicitly-defined ref ID
        extracts = [
            extract
            for extract in extracts
            if extract.ref or report_missing_ref(extract)
        ]
        return extracts, text, diagnostics

    def to_pages(
        self,
        source_path: Path,
        page_factory: Callable[[str], Tuple[Page, EmbeddedRstParser]],
        extracts: Sequence[Extract],
    ) -> List[Page]:
        pages: List[Page] = []
        source_fileid = self.project_config.get_fileid(source_path)

        for extract in extracts:
            assert extract.ref is not None
            if extract.ref.startswith("_"):
                continue

            page, rst_parser = page_factory(f"{extract.ref}.rst")
            page.category = "extracts"
            rendered = extract.render(page, rst_parser)
            extract_directive = n.Directive((extract.line,), [], "", "extract", [], {})
            extract_directive.children = rendered
            page.ast = n.Root((0,), [], source_fileid, {})
            page.ast.children.append(extract_directive)
            pages.append(page)

        return pages
