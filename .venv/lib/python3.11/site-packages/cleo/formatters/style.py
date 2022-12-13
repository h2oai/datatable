from __future__ import annotations

from cleo.color import Color


class Style:
    def __init__(
        self,
        foreground: str | None = None,
        background: str | None = None,
        options: list[str] | None = None,
    ) -> None:
        self._foreground = foreground or ""
        self._background = background or ""
        self._options = options or []

        self._color = Color(self._foreground, self._background, self._options)

    def foreground(self, foreground: str) -> Style:
        self._color = Color(foreground, self._background, self._options)
        self._foreground = foreground

        return self

    def background(self, background: str) -> Style:
        self._color = Color(self._foreground, background, self._options)
        self._background = background

        return self

    def bold(self, bold: bool = True) -> Style:
        return self.set_option("bold") if bold else self.unset_option("bold")

    def dark(self, dark: bool = True) -> Style:
        return self.set_option("dark") if dark else self.unset_option("dark")

    def underlines(self, underlined: bool = True) -> Style:
        return (
            self.set_option("underline")
            if underlined
            else self.unset_option("underline")
        )

    def italic(self, italic: bool = True) -> Style:
        return self.set_option("italic") if italic else self.unset_option("italic")

    def blinking(self, blinking: bool = True) -> Style:
        return self.set_option("blink") if blinking else self.unset_option("blink")

    def inverse(self, inverse: bool = True) -> Style:
        return self.set_option("reverse") if inverse else self.unset_option("reverse")

    def hidden(self, hidden: bool = True) -> Style:
        return self.set_option("conceal") if hidden else self.unset_option("conceal")

    def set_option(self, option: str) -> Style:
        self._options.append(option)
        self._color = Color(self._foreground, self._background, self._options)

        return self

    def unset_option(self, option: str) -> Style:
        if option in self._options:
            index = self._options.index(option)
            del self._options[index]
            self._color = Color(self._foreground, self._background, self._options)
        return self

    def apply(self, text: str) -> str:
        return self._color.apply(text)
