from __future__ import annotations

import hashlib
import inspect
import os
import posixpath
import re
import subprocess

from cleo import helpers
from cleo._compat import shell_quote
from cleo.commands.command import Command
from cleo.commands.completions.templates import TEMPLATES


class CompletionsCommand(Command):

    name = "completions"
    description = "Generate completion scripts for your shell."

    arguments = [
        helpers.argument(
            "shell", "The shell to generate the scripts for.", optional=True
        )
    ]
    options = [
        helpers.option(
            "alias", None, "Alias for the current command.", flag=False, multiple=True
        )
    ]

    SUPPORTED_SHELLS = ("bash", "zsh", "fish")

    hidden = True

    help = """
One can generate a completion script for `<options=bold>{script_name}</>` \
that is compatible with a given shell. The script is output on \
`<options=bold>stdout</>` allowing one to re-direct \
the output to the file of their choosing. Where you place the file will \
depend on which shell, and which operating system you are using. Your \
particular configuration may also determine where these scripts need \
to be placed.

Here are some common set ups for the three supported shells under \
Unix and similar operating systems (such as GNU/Linux).

<options=bold>BASH</>:

Completion files are commonly stored in `<options=bold>/etc/bash_completion.d/</>`

Run the command:

`<options=bold>{script_name} {command_name} bash >\
 /etc/bash_completion.d/{script_name}.bash-completion</>`

This installs the completion script. You may have to log out and log \
back in to your shell session for the changes to take effect.

<options=bold>FISH</>:

Fish completion files are commonly stored in\
`<options=bold>$HOME/.config/fish/completions</>`

Run the command:

`<options=bold>{script_name} {command_name} fish > \
~/.config/fish/completions/{script_name}.fish</>`

This installs the completion script. You may have to log out and log \
back in to your shell session for the changes to take effect.

<options=bold>ZSH</>:

ZSH completions are commonly stored in any directory listed in your \
`<options=bold>$fpath</>` variable. To use these completions, you must either add the \
generated script to one of those directories, or add your own \
to this list.

Adding a custom directory is often the safest best if you're unsure \
of which directory to use. First create the directory, for this \
example we'll create a hidden directory inside our `<options=bold>$HOME</>` directory

`<options=bold>mkdir ~/.zfunc</>`

Then add the following lines to your `<options=bold>.zshrc</>` \
just before `<options=bold>compinit</>`

`<options=bold>fpath+=~/.zfunc</>`

Now you can install the completions script using the following command

`<options=bold>{script_name} {command_name} zsh > ~/.zfunc/_{script_name}</>`

You must then either log out and log back in, or simply run

`<options=bold>exec zsh</>`

For the new completions to take affect.

<options=bold>CUSTOM LOCATIONS</>:

Alternatively, you could save these files to the place of your choosing, \
such as a custom directory inside your $HOME. Doing so will require you \
to add the proper directives, such as `source`ing inside your login \
script. Consult your shells documentation for how to add such directives.
"""

    def handle(self) -> int:
        shell = self.argument("shell")
        if not shell:
            shell = self.get_shell_type()

        if shell not in self.SUPPORTED_SHELLS:
            raise ValueError(
                f'[shell] argument must be one of {", ".join(self.SUPPORTED_SHELLS)}'
            )

        self.line(self.render(shell))

        return 0

    def render(self, shell: str) -> str:
        if shell == "bash":
            return self.render_bash()
        if shell == "zsh":
            return self.render_zsh()
        if shell == "fish":
            return self.render_fish()

        raise RuntimeError(f"Unrecognized shell: {shell}")

    def _get_script_name_and_path(self) -> tuple[str, str]:
        # FIXME: when generating completions via `python -m script completions`,
        # we incorrectly infer `script_name` as `__main__.py`
        script_name = self._io.input.script_name or inspect.stack()[-1][1]
        script_path = posixpath.realpath(script_name)
        script_name = os.path.basename(script_path)

        return script_name, script_path

    def render_bash(self) -> str:
        script_name, script_path = self._get_script_name_and_path()
        aliases = [script_name, script_path, *self.option("alias")]
        function = self._generate_function_name(script_name, script_path)

        # Global options
        assert self.application
        opts = [
            f"--{opt.name}"
            for opt in sorted(self.application.definition.options, key=lambda o: o.name)
        ]

        # Commands + options
        cmds = []
        cmds_opts = []
        for cmd in sorted(self.application.all().values(), key=lambda c: c.name or ""):
            if cmd.hidden or not cmd.enabled or not cmd.name:
                continue
            command_name = shell_quote(cmd.name) if " " in cmd.name else cmd.name
            cmds.append(command_name)
            options = " ".join(
                f"--{opt.name}".replace(":", "\\:")
                for opt in sorted(cmd.definition.options, key=lambda o: o.name)
            )
            cmds_opts += [
                f"            ({command_name})",
                f'            opts="${{opts}} {options}"',
                "            ;;",
                "",  # newline
            ]

        return TEMPLATES["bash"] % {
            "script_name": script_name,
            "function": function,
            "opts": " ".join(opts),
            "cmds": " ".join(cmds),
            "cmds_opts": "\n".join(cmds_opts[:-1]),  # trim trailing newline
            "compdefs": "\n".join(
                f"complete -o default -F {function} {alias}" for alias in aliases
            ),
        }

    def render_zsh(self) -> str:
        script_name, script_path = self._get_script_name_and_path()
        aliases = [script_path, *self.option("alias")]
        function = self._generate_function_name(script_name, script_path)

        def sanitize(s: str) -> str:
            return self._io.output.formatter.remove_format(s)

        # Global options
        assert self.application
        opts = [
            self._zsh_describe(f"--{opt.name}", sanitize(opt.description))
            for opt in sorted(self.application.definition.options, key=lambda o: o.name)
        ]

        # Commands + options
        cmds = []
        cmds_opts = []
        for cmd in sorted(self.application.all().values(), key=lambda c: c.name or ""):
            if cmd.hidden or not cmd.enabled or not cmd.name:
                continue
            command_name = shell_quote(cmd.name) if " " in cmd.name else cmd.name
            cmds.append(self._zsh_describe(command_name, sanitize(cmd.description)))
            options = " ".join(
                self._zsh_describe(f"--{opt.name}", sanitize(opt.description))
                for opt in sorted(cmd.definition.options, key=lambda o: o.name)
            )
            cmds_opts += [
                f"            ({command_name})",
                f"            opts+=({options})",
                "            ;;",
                "",  # newline
            ]

        return TEMPLATES["zsh"] % {
            "script_name": script_name,
            "function": function,
            "opts": " ".join(opts),
            "cmds": " ".join(cmds),
            "cmds_opts": "\n".join(cmds_opts[:-1]),  # trim trailing newline
            "compdefs": "\n".join(f"compdef {function} {alias}" for alias in aliases),
        }

    def render_fish(self) -> str:
        script_name, script_path = self._get_script_name_and_path()
        function = self._generate_function_name(script_name, script_path)

        def sanitize(s: str) -> str:
            return self._io.output.formatter.remove_format(s).replace("'", "\\'")

        # Global options
        assert self.application
        opts = [
            f"complete -c {script_name} -n '__fish{function}_no_subcommand' "
            f"-l {opt.name} -d '{sanitize(opt.description)}'"
            for opt in sorted(self.application.definition.options, key=lambda o: o.name)
        ]

        # Commands + options
        cmds = []
        cmds_opts = []
        cmds_names = []
        for cmd in sorted(self.application.all().values(), key=lambda c: c.name or ""):
            if cmd.hidden or not cmd.enabled or not cmd.name:
                continue
            command_name = shell_quote(cmd.name) if " " in cmd.name else cmd.name
            cmds.append(
                f"complete -c {script_name} -f -n '__fish{function}_no_subcommand' "
                f"-a {command_name} -d '{sanitize(cmd.description)}'"
            )
            cmds_opts += [
                f"# {command_name}",
                *[
                    f"complete -c {script_name} -A "
                    f"-n '__fish_seen_subcommand_from {command_name}' "
                    f"-l {opt.name} -d '{sanitize(opt.description)}'"
                    for opt in sorted(cmd.definition.options, key=lambda o: o.name)
                ],
                "",  # newline
            ]
            cmds_names.append(command_name)

        return TEMPLATES["fish"] % {
            "script_name": script_name,
            "function": function,
            "opts": "\n".join(opts),
            "cmds": "\n".join(cmds),
            "cmds_opts": "\n".join(cmds_opts[:-1]),  # trim trailing newline
            "cmds_names": " ".join(cmds_names),
        }

    def get_shell_type(self) -> str:
        shell = os.getenv("SHELL")
        if not shell:
            raise RuntimeError(
                "Could not read SHELL environment variable. "
                "Please specify your shell type by passing it as the first argument."
            )

        return os.path.basename(shell)

    def _generate_function_name(self, script_name: str, script_path: str) -> str:
        sanitized_name = self._sanitize_for_function_name(script_name)
        md5_hash = hashlib.md5(script_path.encode()).hexdigest()[0:16]
        return f"_{sanitized_name}_{md5_hash}_complete"

    def _sanitize_for_function_name(self, name: str) -> str:
        name = name.replace("-", "_")

        return re.sub("[^A-Za-z0-9_]+", "", name)

    def _zsh_describe(self, value: str, description: str | None = None) -> str:
        value = '"' + value.replace(":", "\\:")
        if description:
            description = re.sub(
                r'(["\'#&;`|*?~<>^()\[\]{}$\\\x0A\xFF])', r"\\\1", description
            )
            value += ":" + subprocess.list2cmdline([description]).strip('"')

        value += '"'

        return value
