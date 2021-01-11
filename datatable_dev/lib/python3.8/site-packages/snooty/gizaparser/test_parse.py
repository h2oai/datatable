from pathlib import Path

from ..types import ProjectConfig
from .extracts import Extract
from .parse import parse


def test_invalid_yaml() -> None:
    project_config = ProjectConfig(Path("test_data"), "")
    pages, text, diagnostics = parse(
        Extract,
        Path(""),
        project_config,
        """
ref: troubleshooting-monitoring-agent-fails-to-collect-data
edition: onprem
content: |

   Fails to Collect Data
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Possible causes for this state:
""",
    )
    assert len(diagnostics) == 1
    assert diagnostics[0].start[0] == 6
