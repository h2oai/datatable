from dataclasses import dataclass
from typing import Dict, Optional

from ..flutter import checked
from .nodes import Inherit, Node


@checked
@dataclass
class ApiArg(Node):
    name: Optional[str]
    arg_name: Optional[str]
    description: Optional[str]
    interface: Optional[str]
    operation: Optional[str]
    optional: Optional[bool]
    position: Optional[int]
    type: Optional[str]
    pre: Optional[str]

    replacement: Optional[Dict[str, str]]
    inherit: Optional[Inherit]
