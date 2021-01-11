from dataclasses import dataclass
from pathlib import Path
from typing import Callable, List, MutableSequence, Optional, Sequence, Tuple, Union

from .. import n
from ..diagnostics import Diagnostic
from ..flutter import checked
from ..page import Page
from ..types import EmbeddedRstParser
from .nodes import GizaCategory, HeadingMixin, Inheritable
from .parse import parse


@checked
@dataclass
class Action(HeadingMixin):
    """An action that a user must take."""

    code: Optional[str]
    copyable: Optional[bool]
    content: Optional[str]
    language: Optional[str]
    post: Optional[str]
    pre: Optional[str]

    def render(self, rst_parser: EmbeddedRstParser) -> List[n.Node]:
        all_nodes: List[n.Node] = []
        heading_nodes = self.render_heading(rst_parser)

        if heading_nodes:
            nodes_to_append_children: MutableSequence[n.Node] = []
            section = n.Section((self.line,), nodes_to_append_children)  # type: ignore
            section.children = nodes_to_append_children
            all_nodes.append(section)

            nodes_to_append_children.extend(heading_nodes)
        else:
            nodes_to_append_children = all_nodes

        if self.pre:
            result = rst_parser.parse_block(self.pre, self.line)
            nodes_to_append_children.extend(result)

        if self.code:
            nodes_to_append_children.append(
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

        if self.content:
            result = rst_parser.parse_block(self.content, self.line)
            nodes_to_append_children.extend(result)

        if self.post:
            result = rst_parser.parse_block(self.post, self.line)
            nodes_to_append_children.extend(result)

        return all_nodes


@checked
@dataclass
class Step(Inheritable, HeadingMixin):
    stepnum: Optional[int]
    content: Optional[str]
    post: Optional[str]
    pre: Optional[str]
    level: Optional[int]
    optional: Optional[bool]
    edition: Union[List[str], str, None]

    action: Union[List[Action], Action, None]

    def render(self, page: Page, rst_parser: EmbeddedRstParser) -> n.Node:
        children: MutableSequence[n.Node] = []
        root = n.Section((self.line,), children)  # type: ignore

        children.extend(self.render_heading(rst_parser))

        if self.pre:
            result = rst_parser.parse_block(self.pre, self.line)
            children.extend(result)

        if self.action:
            actions = [self.action] if isinstance(self.action, Action) else self.action
            for action in actions:
                result = action.render(rst_parser)
                children.extend(result)

        if self.content:
            result = rst_parser.parse_block(self.content, self.line)
            children.extend(result)

        if self.post:
            result = rst_parser.parse_block(self.post, self.line)
            children.extend(result)

        return root


def step_to_page(page: Page, step: Step, rst_parser: EmbeddedRstParser) -> n.Directive:
    rendered = step.render(page, rst_parser)
    directive = n.Directive((step.line,), [], "", "step", [], {})
    directive.children = [rendered]
    return directive


class GizaStepsCategory(GizaCategory[Step]):
    def parse(
        self, path: Path, text: Optional[str] = None
    ) -> Tuple[Sequence[Step], str, List[Diagnostic]]:
        return parse(Step, path, self.project_config, text)

    def to_pages(
        self,
        source_path: Path,
        page_factory: Callable[[str], Tuple[Page, EmbeddedRstParser]],
        steps: Sequence[Step],
    ) -> List[Page]:
        output_filename = source_path.with_suffix(".rst").name
        output_filename = output_filename[len("steps-") :]
        page, rst_parser = page_factory(output_filename)
        page.category = "steps"
        source_fileid = self.project_config.get_fileid(source_path)
        steps_directive = n.Directive((0,), [], "", "steps", [], {})
        steps_directive.children = [
            step_to_page(page, step, rst_parser) for step in steps
        ]
        page.ast = n.Root((0,), [], source_fileid, {})
        page.ast.children.append(steps_directive)
        return [page]
