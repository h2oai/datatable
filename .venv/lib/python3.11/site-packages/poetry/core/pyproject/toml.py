from __future__ import annotations

from typing import TYPE_CHECKING
from typing import Any

from tomlkit.api import table


if TYPE_CHECKING:
    from pathlib import Path

    from tomlkit.toml_document import TOMLDocument

    from poetry.core.pyproject.tables import BuildSystem
    from poetry.core.toml import TOMLFile


class PyProjectTOML:
    def __init__(self, path: str | Path) -> None:
        from poetry.core.toml import TOMLFile

        self._file = TOMLFile(path=path)
        self._data: TOMLDocument | None = None
        self._build_system: BuildSystem | None = None

    @property
    def file(self) -> TOMLFile:
        return self._file

    @property
    def data(self) -> TOMLDocument:
        from tomlkit.toml_document import TOMLDocument

        if self._data is None:
            if not self._file.exists():
                self._data = TOMLDocument()
            else:
                self._data = self._file.read()

        return self._data

    def is_build_system_defined(self) -> bool:
        return self._file.exists() and "build-system" in self.data

    @property
    def build_system(self) -> BuildSystem:
        from poetry.core.pyproject.tables import BuildSystem

        if self._build_system is None:
            build_backend = None
            requires = None

            if not self._file.exists():
                build_backend = "poetry.core.masonry.api"
                requires = ["poetry-core"]

            container = self.data.get("build-system", {})
            self._build_system = BuildSystem(
                build_backend=container.get("build-backend", build_backend),
                requires=container.get("requires", requires),
            )

        return self._build_system

    @property
    def poetry_config(self) -> dict[str, Any]:
        from tomlkit.exceptions import NonExistentKey

        try:
            tool = self.data["tool"]
            assert isinstance(tool, dict)
            config = tool["poetry"]
            assert isinstance(config, dict)
            return config
        except NonExistentKey as e:
            from poetry.core.pyproject.exceptions import PyProjectException

            raise PyProjectException(
                f"[tool.poetry] section not found in {self._file}"
            ) from e

    def is_poetry_project(self) -> bool:
        from poetry.core.pyproject.exceptions import PyProjectException

        if self.file.exists():
            try:
                _ = self.poetry_config
                return True
            except PyProjectException:
                pass

        return False

    def __getattr__(self, item: str) -> Any:
        return getattr(self.data, item)

    def save(self) -> None:
        data = self.data

        if self._build_system is not None:
            if "build-system" not in data:
                data["build-system"] = table()

            build_system = data["build-system"]
            assert isinstance(build_system, dict)

            build_system["requires"] = self._build_system.requires
            build_system["build-backend"] = self._build_system.build_backend

        self.file.write(data=data)

    def reload(self) -> None:
        self._data = None
        self._build_system = None
