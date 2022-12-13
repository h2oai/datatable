from __future__ import annotations

from typing import TYPE_CHECKING

from poetry.plugins.application_plugin import ApplicationPlugin

from poetry_plugin_export.command import ExportCommand


if TYPE_CHECKING:
    from poetry.console.application import Application
    from poetry.console.commands.command import Command


class ExportApplicationPlugin(ApplicationPlugin):
    @property
    def commands(self) -> list[type[Command]]:
        return [ExportCommand]

    def activate(self, application: Application) -> None:
        # Removing the existing export command to avoid an error
        # until Poetry removes the export command
        # and uses this plugin instead.

        # If you're checking this code out to get inspiration
        # for your own plugins: DON'T DO THIS!
        if application.command_loader.has("export"):
            del application.command_loader._factories["export"]

        super().activate(application=application)
