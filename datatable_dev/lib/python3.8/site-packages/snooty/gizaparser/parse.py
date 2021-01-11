import logging
from pathlib import Path
from typing import List, Optional, Tuple, Type, TypeVar

import yaml
import yaml.resolver
import yaml.scanner
from yaml.composer import Composer

from ..diagnostics import Diagnostic, ErrorParsingYAMLFile, UnmarshallingError
from ..flutter import LoadError, check_type, mapping_dict
from ..types import ProjectConfig, SerializableType

_T = TypeVar("_T")
logger = logging.getLogger(__name__)


def load_yaml(
    path: Optional[Path], text: str
) -> Tuple[List[SerializableType], Optional[Diagnostic]]:
    class MyLoader(yaml.SafeLoader):
        def compose_node(self, parent: yaml.nodes.Node, index: int) -> yaml.nodes.Node:
            # the line number where the previous token has ended (plus empty lines)
            line = self.line
            node = Composer.compose_node(self, parent, index)
            node._start_line = line + 1
            return node

    def dict_constructor(loader: yaml.Loader, node: yaml.nodes.Node) -> mapping_dict:
        mapping = mapping_dict(loader.construct_pairs(node))
        mapping._start_line = node._start_line
        return mapping

    loader = MyLoader(text)
    loader.add_constructor(
        yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, dict_constructor
    )
    result: List[SerializableType] = []
    while True:
        try:
            data = loader.get_data()
        except yaml.error.MarkedYAMLError as err:
            lineno = err.problem_mark.line
            col = err.problem_mark.column
            return [], ErrorParsingYAMLFile(path, err.problem, (lineno, col))

        if data:
            result.append(data)
        else:
            break

    return result, None


def parse(
    ty: Type[_T], path: Path, project_config: ProjectConfig, text: Optional[str] = None
) -> Tuple[List[_T], str, List[Diagnostic]]:
    diagnostics: List[Diagnostic] = []
    if text is None:
        text, diagnostics = project_config.read(path)

    parsed_yaml, parse_diagnostic = load_yaml(path, text)
    if parse_diagnostic:
        diagnostics.append(parse_diagnostic)
        return ([], text, diagnostics)

    try:
        parsed = [check_type(ty, data) for data in parsed_yaml]
        return parsed, text, diagnostics
    except LoadError as err:
        mapping = err.bad_data if isinstance(err.bad_data, dict) else {}
        lineno = mapping._start_line if isinstance(mapping, mapping_dict) else 0
        return [], text, diagnostics + [UnmarshallingError(str(err), lineno)]
