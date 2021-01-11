from typing import Any, Callable, Dict, Iterable, List, Optional, Set, Tuple, Union

from . import n
from .page import Page
from .types import FileId


class FileIdStack:
    """A stack which tracks file inclusion history, allowing a postprocessor
       pass to know at any point both the page where processing started, as well
       as what file is currently being processed."""

    __slots__ = ("_stack",)

    def __init__(self, initial_stack: Optional[List[FileId]] = None) -> None:
        self._stack: List[FileId] = initial_stack if initial_stack is not None else []

    def pop(self) -> None:
        self._stack.pop()

    def append(self, fileid: FileId) -> None:
        self._stack.append(fileid)

    def clear(self) -> None:
        self._stack.clear()

    @property
    def root(self) -> FileId:
        return self._stack[0]

    @property
    def current(self) -> FileId:
        return self._stack[-1]


class EventListeners:
    """Manage the listener functions associated with an event-based parse operation"""

    def __init__(self) -> None:
        self._universal_listeners: Set[Callable[..., Any]] = set()
        self._event_listeners: Dict[str, Set[Callable[..., Any]]] = {}

    def add_universal_listener(self, listener: Callable[..., Any]) -> None:
        """Add a listener to be called on any event"""
        self._universal_listeners.add(listener)

    def add_event_listener(self, event: str, listener: Callable[..., Any]) -> None:
        """Add a listener to be called when a particular type of event occurs"""
        event = event.upper()
        listeners: Set[Callable[..., Any]] = self._event_listeners.get(event, set())
        listeners.add(listener)
        self._event_listeners[event] = listeners

    def get_event_listeners(self, event: str) -> Set[Callable[..., Any]]:
        """Return all listeners of a particular type"""
        event = event.upper()
        return self._event_listeners.get(event, set())

    def fire(
        self,
        event: str,
        fileid: FileIdStack,
        *args: Union[n.Node, Page],
        **kwargs: Union[n.Node, Page],
    ) -> None:
        """Iterate through all universal listeners and all listeners of the specified type and call them"""
        for listener in self.get_event_listeners(event):
            listener(fileid, *args, **kwargs)

        for listener in self._universal_listeners:
            listener(fileid, *args, **kwargs)


class EventParser(EventListeners):
    """Initialize an event-based parse on a python dictionary"""

    PAGE_START_EVENT = "page_start"
    PAGE_END_EVENT = "page_end"
    OBJECT_START_EVENT = "object_start"
    OBJECT_END_EVENT = "object_end"

    def __init__(self) -> None:
        super(EventParser, self).__init__()
        self.fileid_stack = FileIdStack()

    def consume(self, d: Iterable[Tuple[FileId, Page]]) -> None:
        """Initializes a parse on the provided key-value map of pages"""
        for filename, page in d:
            self._on_page_enter_event(page, filename)
            self._iterate(page.ast, filename)
            self._on_page_exit_event(page, filename)

            self.fileid_stack.clear()

    def _iterate(self, d: n.Node, filename: FileId) -> None:
        if isinstance(d, n.Root):
            self.fileid_stack.append(d.fileid)

        self._on_object_enter_event(d, filename)

        if isinstance(d, n.Parent):
            if isinstance(d, n.DefinitionListItem):
                for child in d.term:
                    self._iterate(child, filename)

            if isinstance(d, n.Directive):
                for arg in d.argument:
                    self._iterate(arg, filename)

            for child in d.children:
                self._iterate(child, filename)

        self._on_object_exit_event(d, filename)

        if isinstance(d, n.Root):
            self.fileid_stack.pop()

    def _on_page_enter_event(self, page: Page, filename: FileId) -> None:
        """Called when an array is first encountered in tree"""
        self.fire(self.PAGE_START_EVENT, FileIdStack([filename]), page=page)

    def _on_page_exit_event(self, page: Page, filename: FileId) -> None:
        """Called when an array is first encountered in tree"""
        self.fire(self.PAGE_END_EVENT, FileIdStack([filename]), page=page)

    def _on_object_enter_event(self, node: n.Node, filename: FileId) -> None:
        """Called when an object is first encountered in tree"""
        self.fire(self.OBJECT_START_EVENT, self.fileid_stack, node=node)

    def _on_object_exit_event(self, node: n.Node, filename: FileId) -> None:
        """Called when an object is first encountered in tree"""
        self.fire(self.OBJECT_END_EVENT, self.fileid_stack, node=node)
