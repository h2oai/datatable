from dataclasses import dataclass
from typing import Dict, Optional

from ..flutter import checked
from .nodes import Inherit, Node


@checked
@dataclass
class OptionInherit(Inherit):
    program: Optional[str]


@checked
@dataclass
class Option(Node):
    program: Optional[str]
    name: Optional[str]

    command: Optional[str]
    aliases: Optional[str]
    args: Optional[str]
    default: Optional[str]
    description: Optional[str]
    directive: Optional[str]
    optional: Optional[bool]
    post: Optional[str]
    pre: Optional[str]
    type: Optional[str]

    replacement: Optional[Dict[str, str]]
    inherit: Optional[OptionInherit]
