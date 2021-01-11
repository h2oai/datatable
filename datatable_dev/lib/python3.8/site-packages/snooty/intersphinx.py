"""Intersphinx inventories allow different Sphinx projects to refer to targets
   defined in other projects, and export their targets to other projects.

   This module is responsible for loading and parsing these inventories."""

import datetime
import logging
import re
import urllib.parse
import zlib
from dataclasses import dataclass, field
from email.utils import formatdate
from pathlib import Path
from time import mktime
from typing import Dict, List, NamedTuple, Optional, Tuple

import requests

__all__ = ("TargetDefinition", "Inventory")
DEFAULT_CACHE_DIR = Path.home().joinpath(".cache", "snooty")
INVENTORY_PATTERN = re.compile(r"(?x)(.+?)\s+(\S*:\S*)\s+(-?\d+)\s(\S*)\s+(.*)")
logger = logging.Logger(__name__)


class TargetDefinition(NamedTuple):
    """A definition of a reStructuredText link target."""

    name: str
    role: Tuple[str, str]
    priority: int
    uri_base: str
    uri: str
    display_name: Optional[str]

    @property
    def domain_and_role(self) -> str:
        return f"{self.role[0]}:{self.role[1]}"


@dataclass
class Inventory:
    """An inventory of a project's link target definitions."""

    base_url: str
    targets: Dict[str, TargetDefinition] = field(default_factory=dict)

    def __len__(self) -> int:
        return len(self.targets)

    def __contains__(self, target: str) -> bool:
        return target in self.targets

    def get(self, target: str) -> Optional[TargetDefinition]:
        return self.targets.get(target)

    def __getitem__(self, target: str) -> TargetDefinition:
        return self.targets[target]

    def dumps(self, name: str, version: str) -> bytes:
        """Serialize this inventory in the Intersphinx inventory format, and
           return the resulting bytes."""
        # Newlines break the (fragile) format; let's just be safe and make sure we're not smuggling any in.
        if "\n" in name:
            raise ValueError(name)
        if "\n" in version:
            raise ValueError(version)

        buffer: List[bytes] = [
            bytes(
                f"# Sphinx inventory version 2\n# Project: {name}\n# Version: {version}\n# The remainder of this file is compressed using zlib.\n",
                "utf-8",
            )
        ]

        lines: List[str] = []
        for target in self.targets.values():
            lines.append(
                " ".join(
                    (
                        target.name,
                        ":".join(target.role),
                        str(target.priority),
                        target.uri_base,
                        "-" if target.display_name is None else target.display_name,
                    )
                )
            )

        # giza/intermanual expects a terminating newline
        lines.append("")

        buffer.append(zlib.compress(bytes("\n".join(lines), "utf-8"), 9))

        return b"".join(buffer)

    @classmethod
    def parse(cls, base_url: str, text: bytes) -> "Inventory":
        """Parse an intersphinx inventory from the given URL prefix and raw inventory contents."""
        # Intersphinx always has 4 lines of ASCII before the payload.
        start_index = 0
        for i in range(4):
            start_index = text.find(b"\n", start_index) + 1

        decompressed = str(zlib.decompress(text[start_index:]), "utf-8")
        inventory = cls(base_url)
        for line in decompressed.split("\n"):
            if not line.strip():
                continue

            match = INVENTORY_PATTERN.match(line.rstrip())
            if match is None:
                logger.info(f"Invalid intersphinx line: {line}")
                continue

            name, domain_and_role, raw_priority, uri, raw_dispname = match.groups()

            # This is hard-coded in Sphinx as well. Support this name for compatibility.
            if domain_and_role == "std:cmdoption":
                domain_and_role = "std:option"

            uri_base = uri
            if uri.endswith("$"):
                uri = uri[:-1] + name

            # The spec says that only {dispname} can contain spaces. In practice, this is a lie.
            # Just silently skip invalid lines.
            try:
                priority = int(raw_priority)
            except ValueError:
                logger.debug(f"Invalid priority in intersphinx inventory: {line}")
                continue

            domain, role = domain_and_role.split(":", 1)

            # "If {dispname} is identical to {name}, it is stored as -"
            dispname = None if raw_dispname == "-" else raw_dispname

            target_definition = TargetDefinition(
                name, (domain, role), priority, uri_base, uri, dispname
            )
            inventory.targets[f"{domain_and_role}:{name}"] = target_definition

        return inventory


def fetch_inventory(url: str, cache_dir: Path = DEFAULT_CACHE_DIR) -> Inventory:
    """Fetch an intersphinx inventory, or use a locally cached copy if it is still valid."""
    logger.debug(f"Fetching inventory: {url}")

    base_url = url.rsplit("/", 1)[0]
    base_url.rstrip("/")
    base_url += "/"

    # Make our user's cache directory if it doesn't exist
    parsed_url = urllib.parse.urlparse(url)
    filename = "".join(
        char for char in parsed_url.netloc + parsed_url.path if char.isalnum()
    )
    cache_dir.mkdir(parents=True, exist_ok=True)
    inventory_path = cache_dir.joinpath(filename)

    # Only re-request if more than an hour old
    request_headers: Dict[str, str] = {}
    mtime: Optional[datetime.datetime] = None
    try:
        mtime = datetime.datetime.fromtimestamp(inventory_path.stat().st_mtime)
    except FileNotFoundError:
        pass

    if mtime is not None:
        if (datetime.datetime.now() - mtime) < datetime.timedelta(hours=1):
            request_headers["If-Modified-Since"] = formatdate(
                mktime(mtime.timetuple()), usegmt=True
            )

    res = requests.get(url, headers=request_headers)
    res.raise_for_status()
    if res.status_code == 304:
        return Inventory.parse(base_url, inventory_path.read_bytes())

    with open(inventory_path, "wb") as f:
        f.write(res.content)

    return Inventory.parse(base_url, res.content)
