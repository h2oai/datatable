import re
from dataclasses import dataclass
from typing import Callable, Dict, Iterable, List, Optional, Tuple

import docutils.nodes
import docutils.parsers.rst.directives
import fett

from .rstparser import BaseDocutilsDirective

LEADING_WHITESPACE = re.compile(r"^\n?(\x20+)")
LEGACY_GUIDES_TEMPLATE = fett.Template(
    """
:guide:

====================================================================================================
{{ title }}
====================================================================================================

.. author:: {{ author }}

.. category:: {{ type }}

{{ if languages }}

.. languages::

   {{ for language in languages }}
   * {{ language }}
   {{ end }}
{{ end }}

.. level:: {{ level }}

{{ if product_version }}
.. product_version:: {{ product_version }}
{{ end }}

.. result_description::

   {{ result_description }}

.. time:: {{ time }}

.. prerequisites::

   {{ prerequisites }}

{{ if check_your_environment }}
.. check_your_environment::

   {{ check_your_environment }}
{{ end }}

.. procedure::

   {{ procedure }}

.. summary::

   {{ summary }}

{{ if whats_next }}
.. whats_next::

   {{ whats_next }}
{{ end }}

{{ if seealso }}
.. seealso::

   {{ seealso }}
{{ end }}
"""
)

LEGACY_GUIDES_INDEX_TEMPLATE = fett.Template(
    """
.. guide-index::

   {{ for card in cards }}
   {{ if card.jumbo }}

   .. multi-card:: {{ card.title }}

      {{ for guide in card.guides }}
      * {{ guide }}
      {{ end }}

   {{ else }}

   .. card:: {{ card.guides car }}
   {{ end }}
   {{ end }}
"""
)


class ParseError(Exception):
    def __init__(self, msg: str, lineno: int) -> None:
        super(ParseError, self).__init__(msg)
        self.lineno = lineno


def _parse_indentation(lines: Iterable[str]) -> Iterable[Tuple[bool, int, str]]:
    """For each line, yield the tuple (indented, lineno, text)."""
    indentation = 0

    for lineno, line in enumerate(lines):
        line = line.replace("\t", "    ")
        line_indentation_match = LEADING_WHITESPACE.match(line)

        if line_indentation_match is None:
            yield (False, lineno, line)
            indentation = 0
        else:
            line_indentation = len(line_indentation_match.group(0))
            if indentation == 0:
                indentation = line_indentation

            if line_indentation < indentation:
                raise ParseError("Improper dedent", lineno)
            line_indentation = min(indentation, line_indentation)
            yield (True, lineno, line[line_indentation:])


def _parse_keys(lines: Iterable[str]) -> Dict[str, str]:
    """Legacy guide content parser."""
    result: Dict[str, str] = {}
    in_key = True

    pending_key = ""
    pending_value: List[str] = []

    # This is a 2-state machine
    for is_indented, lineno, line in _parse_indentation(lines):
        if not in_key:
            if not is_indented:
                if not line:
                    pending_value.append("")
                    continue

                # Switch to in_key
                result[pending_key] = "\n".join(pending_value).strip()
                pending_value = []
                pending_key = ""
                in_key = True
            else:
                pending_value.append(line)

        if in_key:
            if is_indented:
                raise ParseError("Unexpected indentation", lineno)

            parts = line.split(":", 1)
            if line.strip() and len(parts) != 2:
                raise ParseError("Expected key", lineno)

            pending_key = parts[0].strip()
            value = parts[1].strip()
            if value:
                pending_value.append(value)

            in_key = False

    if pending_value:
        result[pending_key] = "\n".join(pending_value).strip()

    return result


class LegacyGuideDirective(docutils.parsers.rst.Directive):
    required_arguments = 0
    optional_arguments = 0
    has_content = True
    final_argument_whitespace = True

    guide_keys: Dict[str, Callable[[str], object]] = {
        "title": str,
        "languages": lambda l: l.split(),
        "author": str,
        "type": str,
        "level": str,
        "product_version": str,
        "result_description": str,
        "time": int,
        "prerequisites": str,
        "check_your_environment": str,
        "considerations": str,
        "procedure": str,
        "verify": str,
        "summary": str,
        "whats_next": str,
        "seealso": str,
    }

    optional_keys = {
        "check_your_environment",
        "considerations",
        "verify",
        "languages",
        "whats_next",
        "seealso",
    }

    def run(self) -> List[docutils.nodes.Node]:
        messages: List[docutils.nodes.Node] = []

        try:
            options = _parse_keys(self.content)
        except ParseError as err:
            return [
                self.state.document.reporter.error(
                    str(err), line=(self.lineno + err.lineno + 1)
                )
            ]

        # Validate keys
        result: Dict[str, object] = {}
        for key, validation_function in self.guide_keys.items():
            if key not in options:
                if key in self.optional_keys:
                    options[key] = ""
                else:
                    messages.append(
                        self.state.document.reporter.warning(
                            f"Missing required guide option: {key}", line=self.lineno
                        )
                    )
                    continue

            try:
                result[key] = validation_function(options[key])
            except ValueError as err:
                message = f"Invalid guide option value: {key}: {err}"
                return [self.state.document.reporter.error(message, line=self.lineno)]

        try:
            rendered = LEGACY_GUIDES_TEMPLATE.render(result)
        except Exception as error:
            raise self.severe(f"Failed to render template: {error}")

        rendered_lines = docutils.statemachine.string2lines(
            rendered, 4, convert_whitespace=True
        )
        self.state_machine.insert_input(rendered_lines, "")

        return messages


@dataclass
class Card:
    title: str
    jumbo: bool
    guides: List[str]

    def __getitem__(self, key: str) -> object:
        result: object = getattr(self, key)
        return result


class LegacyGuideIndexDirective(BaseDocutilsDirective):
    required_arguments = 0
    optional_arguments = 0
    has_content = True
    final_argument_whitespace = True

    def run(self) -> List[docutils.nodes.Node]:
        if not self.content or any(".. " in line for line in self.content):
            return super().run()

        cards: List[Card] = []
        previous_line: Optional[str] = None
        pending_card: List[Optional[Card]] = [None]

        def handle_single_guide() -> None:
            if pending_card[0] is None:
                assert previous_line is not None
                cards.append(Card("", False, [previous_line]))
            else:
                cards.append(Card(pending_card[0].title, True, pending_card[0].guides))
                pending_card[0] = None

        for is_indented, lineno, line in _parse_indentation(self.content):
            if not line:
                continue

            if is_indented:
                # A card containing multiple guides
                if pending_card[0] is None:
                    assert previous_line is not None
                    pending_card[0] = Card(previous_line, False, [])

                assert pending_card[0] is not None
                pending_card[0].guides.append(line)
            elif previous_line is not None:
                # A card containing a single guide
                handle_single_guide()

            previous_line = line

        if previous_line is not None:
            handle_single_guide()

        data = {"cards": cards}

        try:
            rendered = LEGACY_GUIDES_INDEX_TEMPLATE.render(data)
        except Exception as error:
            raise self.severe(f"Failed to render template: {error}")

        rendered_lines = docutils.statemachine.string2lines(
            rendered, 4, convert_whitespace=True
        )
        self.state_machine.insert_input(rendered_lines, "")
        return []
