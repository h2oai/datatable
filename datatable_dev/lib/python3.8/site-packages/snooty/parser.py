import collections
import errno
import getpass
import logging
import multiprocessing
import os
import re
import subprocess
import threading
from copy import deepcopy
from dataclasses import dataclass
from functools import partial
from pathlib import Path, PurePath
from typing import (
    Any,
    Callable,
    Dict,
    Iterable,
    List,
    MutableSequence,
    Optional,
    Set,
    Tuple,
    cast,
)

import docutils.nodes
import docutils.utils
import networkx
import watchdog.events
from typing_extensions import Protocol

from . import gizaparser, n, rstparser, specparser, util, lsp
from .cache import Cache
from .diagnostics import (
    CannotOpenFile,
    Diagnostic,
    DocUtilsParseError,
    ExpectedImageArg,
    ExpectedPathArg,
    ImageSuggested,
    InvalidField,
    InvalidLiteralInclude,
    InvalidTableStructure,
    InvalidURL,
    MalformedGlossary,
    RemovedLiteralBlockSyntax,
    TabMustBeDirective,
    TodoInfo,
    UnexpectedIndentation,
    UnknownTabID,
    UnknownTabset,
)
from .gizaparser.nodes import GizaCategory
from .gizaparser.published_branches import PublishedBranches, parse_published_branches
from .openapi import OpenAPI
from .page import Page, PendingTask
from .postprocess import DevhubPostprocessor, Postprocessor
from .target_database import ProjectInterface, TargetDatabase
from .types import (
    BuildIdentifierSet,
    FileId,
    ProjectConfig,
    SerializableType,
    StaticAsset,
)
from .util import RST_EXTENSIONS

NO_CHILDREN = (n.SubstitutionReference,)
logger = logging.getLogger(__name__)


@dataclass
class _DefinitionListTerm(n.InlineParent):
    """A vate node used for internal book-keeping that should not be exported to the AST."""

    __slots__ = ()
    type = "definition_list_term"

    def verify(self) -> None:
        assert (
            False
        ), f"{self.__class__.__name__} is private and should have been removed from AST"


class PendingFigure(PendingTask):
    """Add an image's checksum."""

    def __init__(self, node: n.Directive, asset: StaticAsset) -> None:
        super().__init__(node)
        self.node: n.Directive = node
        self.asset = asset

    def __call__(
        self, diagnostics: List[Diagnostic], project: ProjectInterface
    ) -> None:
        """Compute this figure's checksum and store it in our node."""
        cache = project.expensive_operation_cache

        # Use the cached checksum if possible. Note that this does not currently
        # update the underlying asset: if the asset is used by the current backend,
        # the image will still have to be read.
        if self.node.options is None:
            self.node.options = {}
        options = self.node.options
        entry = cache[(self.asset.fileid, 0)]
        if entry is not None:
            assert isinstance(entry, str)
            options["checksum"] = entry
            return

        try:
            checksum = self.asset.get_checksum()
            options["checksum"] = checksum
            cache[(self.asset.fileid, 0)] = checksum
        except OSError as err:
            diagnostics.append(
                CannotOpenFile(self.asset.path, err.strerror, self.node.start[0])
            )


class JSONVisitor:
    """Node visitor that creates a JSON-serializable structure."""

    def __init__(
        self,
        project_config: ProjectConfig,
        docpath: PurePath,
        document: docutils.nodes.document,
    ) -> None:
        self.project_config = project_config
        self.docpath = docpath
        self.document = document
        self.state: List[n.Node] = []
        self.diagnostics: List[Diagnostic] = []
        self.static_assets: Set[StaticAsset] = set()
        self.pending: List[PendingTask] = []

    def dispatch_visit(self, node: docutils.nodes.Node) -> None:
        line = util.get_line(node)

        if isinstance(node, docutils.nodes.definition):
            return
        if isinstance(node, docutils.nodes.field_list):
            top = self.state[-1]
            if isinstance(top, n.Root):
                for field in node.children:
                    key = field.children[0].astext()
                    value = field.children[1].astext()
                    top.options[key] = value
                raise docutils.nodes.SkipNode()

            self.state.append(n.FieldList((line,), []))
            return
        elif isinstance(node, docutils.nodes.document):
            self.state.append(
                n.Root((0,), [], self.project_config.get_fileid(self.docpath), {})
            )
            return
        elif isinstance(node, docutils.nodes.field):
            field_name = node.children[0].astext()
            field_list = node.parent
            assert isinstance(field_list, docutils.nodes.field_list)
            rstobject = field_list.parent
            if isinstance(rstobject, rstparser.directive):
                try:
                    # Convert list of mixed strings and tuples into a key: value map
                    supported_fields = {
                        (f if isinstance(f, str) else f[0]): (
                            None if isinstance(f, str) else f[1]
                        )
                        for f in rstobject["fields"]
                    }

                    self.state.append(
                        n.Field((line,), [], field_name, supported_fields[field_name])
                    )
                    return
                except KeyError:
                    # Handle case where field is not included in directive's rstspec entry
                    assert isinstance(rstobject, rstparser.directive)
                    self.diagnostics.append(
                        InvalidField(
                            f"""Field {field_name} not supported by directive {rstobject["name"]}""",
                            util.get_line(node.children[0]),
                        )
                    )
                    raise docutils.nodes.SkipNode()
            else:
                # Handle case where :field: does not appear in a directive
                self.diagnostics.append(
                    InvalidField(
                        f"""Field {field_name} must be used in a valid directive""",
                        util.get_line(node.children[0]),
                    )
                )
        elif isinstance(node, docutils.nodes.field_name):
            raise docutils.nodes.SkipNode()
        elif isinstance(node, docutils.nodes.field_body):
            # Omit the field_body wrapper, but parse its children
            raise docutils.nodes.SkipDeparture()
        elif isinstance(node, rstparser.code):
            doc = n.Code(
                (line,),
                node["lang"] if "lang" in node else None,
                node["caption"] if "caption" in node else None,
                node["copyable"],
                node["emphasize_lines"] if "emphasize_lines" in node else None,
                node.astext(),
                node["linenos"],
            )
            top_of_state = self.state[-1]
            assert isinstance(top_of_state, n.Parent)
            top_of_state.children.append(doc)
            raise docutils.nodes.SkipNode()
        elif isinstance(node, docutils.nodes.block_quote):
            # We are uninterested in docutils blockquotes: they're too easy to accidentally
            # invoke. Treat them as an error.
            self.diagnostics.append(
                UnexpectedIndentation(util.get_line(node.children[0]))
            )
            raise docutils.nodes.SkipDeparture()
        elif isinstance(node, rstparser.target_directive):
            self.state.append(
                n.Target((line,), [], node["domain"], node["name"], None, None)
            )
        elif isinstance(node, rstparser.directive):
            directive = self.handle_directive(node, line)
            if directive:
                self.state.append(directive)
        elif isinstance(node, docutils.nodes.Text):
            self.state.append(n.Text((line,), str(node)))
            return
        elif isinstance(node, docutils.nodes.literal_block):
            self.diagnostics.append(
                RemovedLiteralBlockSyntax(util.get_line(node.children[0]))
            )
            raise docutils.nodes.SkipNode()
        elif isinstance(node, docutils.nodes.literal):
            self.state.append(n.Literal((line,), []))
            return
        elif isinstance(node, docutils.nodes.emphasis):
            self.state.append(n.Emphasis((line,), []))
            return
        elif isinstance(node, docutils.nodes.strong):
            self.state.append(n.Strong((line,), []))
            return
        elif isinstance(node, rstparser.ref_role):
            role_name = node["name"]
            flag = node["flag"] if "flag" in node else ""
            role: n.Role = n.RefRole(
                (line,), [], node["domain"], role_name, node["target"], flag, None, None
            )
            self.state.append(role)
            return
        elif isinstance(node, rstparser.role):
            role_name = node["name"]
            target = node["target"] if "target" in node else ""
            flag = node["flag"] if "flag" in node else ""

            if role_name == "doc":
                self.validate_doc_role(node)
                role = n.RefRole(
                    (line,), [], node["domain"], role_name, "", flag, (target, ""), None
                )
                self.state.append(role)
                return

            role = n.Role((line,), [], node["domain"], role_name, target, flag)
            self.state.append(role)
            return
        elif isinstance(node, docutils.nodes.target):
            assert (
                len(node["ids"]) <= 1
            ), f"Too many ids in this node: {self.docpath} {node}"
            if "refuri" in node:
                raise docutils.nodes.SkipNode()

            if not node["ids"]:
                self.diagnostics.append(InvalidURL(util.get_line(node)))
                raise docutils.nodes.SkipNode()

            node_id = node["names"][0]
            children: Any = [n.TargetIdentifier((line,), [], [node_id])]
            refuri = node["refuri"] if "refuri" in node else None
            self.state.append(n.Target((line,), children, "std", "label", refuri, None))
        elif isinstance(node, rstparser.target_identifier):
            self.state.append(n.TargetIdentifier((line,), [], node["ids"]))
        elif isinstance(node, docutils.nodes.definition_list):
            self.state.append(n.DefinitionList((line,), []))
        elif isinstance(node, docutils.nodes.definition_list_item):
            self.state.append(n.DefinitionListItem((line,), [], []))
        elif isinstance(node, docutils.nodes.bullet_list):
            self.state.append(n.ListNode((line,), [], n.ListEnumType.unordered, None))
        elif isinstance(node, docutils.nodes.enumerated_list):
            self.state.append(
                n.ListNode(
                    (line,),
                    [],
                    n.ListEnumType[node["enumtype"]],
                    node["start"] if "start" in node else None,
                )
            )
        elif isinstance(node, docutils.nodes.list_item):
            self.state.append(n.ListNodeItem((line,), []))
        elif isinstance(node, docutils.nodes.title):
            # Attach an anchor ID to this section
            assert node.parent
            title_id = util.make_html5_id(node.astext().strip()).lower()
            self.state.append(n.Heading((line,), [], title_id))
        elif isinstance(node, docutils.nodes.reference):
            self.state.append(
                n.Reference(
                    (line,),
                    [],
                    node["refuri"] if "refuri" in node else None,
                    node["refname"] if "refname" in node else None,
                )
            )
        elif isinstance(node, docutils.nodes.substitution_definition):
            try:
                name = node["names"][0]
                self.state.append(n.SubstitutionDefinition((line,), [], name))
            except IndexError:
                pass
        elif isinstance(node, docutils.nodes.substitution_reference):
            self.state.append(n.SubstitutionReference((line,), [], node["refname"]))
        elif isinstance(node, docutils.nodes.footnote):
            # Autonumbered footnotes do not have a refname
            name = node["names"] if "names" in node else None
            if isinstance(name, list):
                name = name[0] if name else None
            self.state.append(n.Footnote((line,), [], node["ids"][0], name))
        elif isinstance(node, docutils.nodes.footnote_reference):
            # Autonumbered footnotes do not have a refname
            refname = node["refname"] if "refname" in node else None
            self.state.append(n.FootnoteReference((line,), [], node["ids"][0], refname))
        elif isinstance(node, docutils.nodes.section):
            self.state.append(n.Section((line,), []))
        elif isinstance(node, docutils.nodes.paragraph):
            self.state.append(n.Paragraph((line,), []))
        elif isinstance(node, rstparser.directive_argument):
            self.state.append(n.DirectiveArgument((line,), []))
        elif isinstance(node, docutils.nodes.term):
            self.state.append(_DefinitionListTerm((line,), []))
        elif isinstance(node, docutils.nodes.line_block):
            self.state.append(n.LineBlock((line,), []))
        elif isinstance(node, docutils.nodes.line):
            self.state.append(n.Line((line,), []))
        elif isinstance(node, docutils.nodes.transition):
            self.state.append(n.Transition((line,)))
        elif isinstance(node, docutils.nodes.table):
            raise docutils.nodes.SkipNode()
        elif isinstance(node, (docutils.nodes.problematic, docutils.nodes.label)):
            raise docutils.nodes.SkipNode()
        elif isinstance(node, docutils.nodes.comment):
            self.state.append(n.Comment((line,), []))
        elif isinstance(node, docutils.nodes.system_message):
            level = int(node["level"])
            if level >= 2:
                level = Diagnostic.Level.from_docutils(level)
                msg = node[0].astext()
                diagnostic = DocUtilsParseError(msg, util.get_line(node))
                diagnostic.severity = level
                self.diagnostics.append(diagnostic)
            raise docutils.nodes.SkipNode()
        elif isinstance(node, rstparser.snooty_diagnostic):
            self.diagnostics.append(node["diagnostic"])
            return
        else:
            lineno = util.get_line(node)
            raise NotImplementedError(
                f"Unknown node type: {node.__class__.__name__} at {self.docpath}:{lineno}"
            )

    def dispatch_departure(self, node: docutils.nodes.Node) -> None:
        if len(self.state) == 1 or isinstance(node, docutils.nodes.definition):
            return

        popped = self.state.pop()

        top_of_state = self.state[-1]

        if isinstance(popped, _DefinitionListTerm):
            assert isinstance(
                top_of_state, n.DefinitionListItem
            ), "Definition list terms must be children of definition list items"
            top_of_state.term = popped.children
            return

        if not isinstance(top_of_state, NO_CHILDREN):
            if isinstance(top_of_state, n.Parent):
                top_of_state.children.append(popped)
            else:
                # This should not happen; unfortunately, it does: see DOCSP-7122.
                # Log some details so we can hopefully fix it when it happens next.
                logger.error(
                    "Malformed node in file %s: %s, %s",
                    self.docpath.as_posix(),
                    repr(top_of_state),
                    repr(popped),
                )

        if (
            isinstance(popped, n.Directive)
            and popped.options
            and "tabset" in popped.options
            and popped.options["tabset"] != "tab"
        ):
            self.handle_tabset(popped)

        elif (
            isinstance(popped, n.Directive)
            and f"{popped.domain}:{popped.name}" == ":glossary"
        ):

            definition_list = next(popped.get_child_of_type(n.DefinitionList), None)

            if definition_list is None:
                return

            if len(popped.children) != 1:
                self.diagnostics.append(MalformedGlossary(util.get_line(node)))
                return

            if popped.options.get("sorted", False):
                definition_list.children = sorted(
                    definition_list.children,
                    key=lambda DefinitionListItem: "".join(
                        term.get_text().casefold() for term in DefinitionListItem.term
                    ),
                )

            for item in definition_list.get_child_of_type(n.DefinitionListItem):
                term_text = "".join(term.get_text() for term in item.term)
                identifier = n.TargetIdentifier(item.start, [], [term_text])
                identifier.children = item.term[:]
                target = n.InlineTarget(item.start, [], "std", "term", None, None)
                target.children = [identifier]
                item.term.append(target)

    def handle_tabset(self, node: n.Directive) -> None:
        tabset = node.options["tabset"]
        line = node.start[0]
        # retrieve dictionary associated with this specific tabset
        try:
            tab_definitions_list = specparser.SPEC.tabs[tabset]
        except KeyError:
            self.diagnostics.append(UnknownTabset(tabset, line))
            return

        old_children = node.children
        new_children: List[n.Node] = []
        for child in old_children:
            if (not isinstance(child, n.Directive)) or child.name != "tab":
                self.diagnostics.append(
                    TabMustBeDirective(str(type(child).__class__.__name__), line)
                )
                continue

            tabid = child.options.get("tabid")
            if tabid is None:
                # Required options get warned about elsewhere, so no need to log an error
                continue

            if not isinstance(tabid, str):
                self.diagnostics.append(
                    UnknownTabID(
                        tabid,
                        tabset,
                        f"{tabid} is of type {str(type(tabid).__class__)}. Tab ids must be strings ",
                        line,
                    )
                )
                continue

            unknown_tabid = True
            # find matching title given id and insert directive_argument
            for t_idx, entry in enumerate(tab_definitions_list):
                if entry.id == tabid:
                    child.argument = [n.Text((line,), entry.title)]
                    unknown_tabid = False

            if unknown_tabid:
                self.diagnostics.append(
                    UnknownTabID(
                        tabid,
                        tabset,
                        f"{tabid} is not defined in rstspec.toml for this tabset",
                        line,
                    )
                )
                continue

            new_children.append(child)

        node.children = new_children

        # Sort tab directives based on order defined in rstspec.toml
        tabid_list = [tab.id for tab in tab_definitions_list]
        node.children = sorted(
            node.children,
            key=lambda x: tabid_list.index(cast(n.Directive, x).options["tabid"]),
        )

    def handle_directive(
        self, node: docutils.nodes.Node, line: int
    ) -> Optional[n.Node]:
        name = node["name"]
        domain = node["domain"]
        options = node["options"] or {}

        if name == "toctree":
            doc: n.Directive = n.TocTreeDirective(
                (line,), [], domain, name, [], options, node["entries"]
            )
            return doc

        doc = n.Directive((line,), [], domain, name, [], options)

        # Find and move the argument from the children to the "argument" field.
        argument: MutableSequence[Any] = []
        if node.children:
            index_of_argument = next(
                (
                    idx
                    for idx, value in enumerate(node.children)
                    if isinstance(value, rstparser.directive_argument)
                ),
                None,
            )
            if index_of_argument is not None:
                visitor = self.__make_child_visitor()
                node.children[index_of_argument].walkabout(visitor)
                top_of_visitor_state = visitor.state[-1]
                assert isinstance(top_of_visitor_state, n.Parent)
                argument = top_of_visitor_state.children
                del node.children[index_of_argument]

        doc.argument = argument

        argument_text = None
        try:
            argument_text = argument[0].value
        except (IndexError, AttributeError):
            pass

        key: str = f"{domain}:{name}"
        if name == "todo":
            todo_text = ["TODO"]
            if argument_text:
                todo_text.extend([": ", argument_text])
            TodoInfo("".join(todo_text), line)
            return None

        if name in {"figure", "image", "atf-image"}:
            if argument_text is None:
                self.diagnostics.append(ExpectedPathArg(name, line))
                return doc

            try:
                static_asset = self.add_static_asset(argument_text, upload=True)
                self.pending.append(PendingFigure(doc, static_asset))
            except OSError as err:
                self.diagnostics.append(
                    CannotOpenFile(argument_text, err.strerror, line)
                )

        elif name == "list-table":
            # Calculate the expected number of columns for this list-table structure.
            if not node.children:
                return doc

            expected_num_columns = 0
            if "widths" in options:
                widths = re.split(r"[,\s][\s]?", options["widths"])
                expected_num_columns = len(widths)
            bullet_list = node.children[0]
            for list_item in bullet_list.children:
                if expected_num_columns == 0 and list_item.children:
                    expected_num_columns = len(list_item.children[0].children)
                for bullets in list_item.children:
                    self.validate_list_table(bullets, expected_num_columns)

        elif name == "openapi":
            if argument_text is None:
                self.diagnostics.append(ExpectedPathArg(name, line))
                return doc

            openapi_fileid, filepath = util.reroot_path(
                FileId(argument_text), self.docpath, self.project_config.source_path
            )

            try:
                with open(filepath) as f:
                    openapi = OpenAPI.load(f)

                def create_page() -> Tuple[Page, EmbeddedRstParser]:
                    # Create dummy page in order to use EmbeddedRstParser
                    page = Page.create(
                        filepath, None, "", n.Root((-1,), [], openapi_fileid, {})
                    )
                    diagnostics: Dict[PurePath, List[Diagnostic]] = {}
                    return (
                        page,
                        EmbeddedRstParser(
                            self.project_config,
                            page,
                            diagnostics.setdefault(filepath, []),
                        ),
                    )

                openapi_ast, diagnostics = openapi.to_ast(filepath, create_page)
                self.diagnostics.extend(diagnostics)
                doc.children.extend(openapi_ast)

            except OSError as err:
                self.diagnostics.append(
                    CannotOpenFile(argument_text, err.strerror, line)
                )
                return doc

        elif name == "literalinclude":
            if argument_text is None:
                self.diagnostics.append(ExpectedPathArg(name, line))
                return doc

            _, filepath = util.reroot_path(
                FileId(argument_text), self.docpath, self.project_config.source_path
            )

            # Attempt to read the literally included file
            try:
                with open(filepath) as file:
                    text = file.read()

            except OSError as err:
                self.diagnostics.append(
                    CannotOpenFile(argument_text, err.strerror, line)
                )
                return doc

            lines = text.split("\n")

            def _locate_text(text: str) -> int:
                """
                Searches the literally-included file ('lines') for the specified text. If no such text is found,
                add an InvalidLiteralInclude diagnostic.
                """
                assert isinstance(text, str)
                loc = next((idx for idx, line in enumerate(lines) if text in line), -1)
                if loc < 0:
                    self.diagnostics.append(
                        InvalidLiteralInclude(f'"{text}" not found in {filepath}', line)
                    )
                return loc

            # Locate the start_after query
            start_after = 0
            if "start-after" in options:
                start_after_text = options["start-after"]
                start_after = _locate_text(start_after_text)
                # Only increment start_after if text is specified, to avoid capturing the start_after_text
                start_after += 1

            # ...now locate the end_before query
            end_before = len(lines)
            if "end-before" in options:
                end_before_text = options["end-before"]
                end_before = _locate_text(end_before_text)

            # Check that start_after_text precedes end_before_text (and end_before exists)
            if start_after >= end_before >= 0:
                self.diagnostics.append(
                    InvalidLiteralInclude(
                        f'"{end_before_text}" precedes "{start_after_text}" in {filepath}',
                        line,
                    )
                )

            # If we failed to locate end_before text, default to the end-of-file
            if end_before == -1:
                end_before = len(lines)

            # Capture the original file-length before splicing it
            len_file = len(lines)
            lines = lines[start_after:end_before]

            dedent = 0
            if "dedent" in options:
                # Dedent is specified as a flag
                if isinstance(options["dedent"], bool):
                    # Deduce a reasonable dedent
                    try:
                        dedent = min(
                            len(line) - len(line.lstrip())
                            for line in lines
                            if len(line.lstrip()) > 0
                        )
                    except ValueError:
                        # Handle the (unlikely) case where there are no non-empty lines
                        dedent = 0
                # Dedent is specified as a nonnegative integer (number of characters):
                # Note: since boolean is a subtype of int, this conditonal must follow the
                # above bool-type conditional.
                elif isinstance(options["dedent"], int):
                    dedent = options["dedent"]
                else:
                    self.diagnostics.append(
                        InvalidLiteralInclude(
                            f'Dedent "{dedent}" of type {type(dedent)}; expected nonnegative integer or flag',
                            line,
                        )
                    )
                    return doc

            lines = [line[dedent:] for line in lines]

            emphasize_lines = None
            if "emphasize-lines" in options:
                try:
                    emphasize_lines = rstparser.parse_linenos(
                        options["emphasize-lines"], len_file
                    )
                except ValueError as err:
                    self.diagnostics.append(
                        InvalidLiteralInclude(
                            f"Invalid emphasize-lines specification caused: {err}", line
                        )
                    )

            span = (line,)
            language = options["language"] if "language" in options else ""
            caption = options["caption"] if "caption" in options else None
            copyable = "copyable" not in options or options["copyable"] == "True"
            selected_content = "\n".join(lines)
            linenos = "linenos" in options

            code = n.Code(
                span,
                language,
                caption,
                copyable,
                emphasize_lines,
                selected_content,
                linenos,
            )

            doc.children.append(code)

        elif name == "include":
            if argument_text is None:
                self.diagnostics.append(ExpectedPathArg(name, util.get_line(node)))
                return doc

            fileid, path = util.reroot_path(
                FileId(argument_text), self.docpath, self.project_config.source_path
            )

            # Validate if file exists
            if not path.is_file():
                # Check if file is snooty-generated
                if (
                    fileid.match("steps/*.rst")
                    or fileid.match("extracts/*.rst")
                    or fileid.match("release/*.rst")
                    or fileid == FileId("includes/hash.rst")
                ):
                    pass
                else:
                    self.diagnostics.append(
                        CannotOpenFile(
                            argument_text,
                            os.strerror(errno.ENOENT),
                            util.get_line(node),
                        )
                    )

        elif name == "cardgroup-card":
            image_argument = options.get("image", None)

            if image_argument is None:
                self.diagnostics.append(
                    ExpectedImageArg(
                        f'"{name}" expected an image argument', util.get_line(node)
                    )
                )
                return doc

            try:
                static_asset = self.add_static_asset(image_argument, upload=True)
                self.pending.append(PendingFigure(doc, static_asset))
            except OSError as err:
                self.diagnostics.append(
                    CannotOpenFile(image_argument, err.strerror, util.get_line(node))
                )

        elif name in {"pubdate", "updated-date"}:
            if "date" in node:
                doc.options["date"] = node["date"]

        elif key in {"devhub:author", ":og", ":twitter"}:
            # Grab image from options array and save as static asset
            image_argument = options.get("image")

            if not image_argument:
                # Warn writers that an image is suggested, but do not require
                self.diagnostics.append(ImageSuggested(name, util.get_line(node)))
            else:
                try:
                    static_asset = self.add_static_asset(image_argument, upload=True)
                    self.pending.append(PendingFigure(doc, static_asset))
                except OSError as err:
                    self.diagnostics.append(
                        CannotOpenFile(
                            image_argument, err.strerror, util.get_line(node)
                        )
                    )
        elif key in {"landing:card"}:
            image_argument = options.get("icon")

            if image_argument:
                try:
                    static_asset = self.add_static_asset(image_argument, upload=True)
                    self.pending.append(PendingFigure(doc, static_asset))
                except OSError as err:
                    self.diagnostics.append(
                        CannotOpenFile(
                            image_argument, err.strerror, util.get_line(node)
                        )
                    )

        return doc

    def validate_doc_role(self, node: docutils.nodes.Node) -> None:
        """Validate target for doc role"""
        resolved_target_path = util.add_doc_target_ext(
            node["target"], self.docpath, self.project_config.source_path
        )

        if not resolved_target_path.is_file():
            self.diagnostics.append(
                CannotOpenFile(
                    resolved_target_path, os.strerror(errno.ENOENT), util.get_line(node)
                )
            )

    def validate_list_table(
        self, node: docutils.nodes.Node, expected_num_columns: int
    ) -> None:
        """Validate list-table structure"""
        if (
            isinstance(node, docutils.nodes.bullet_list)
            and len(node.children) != expected_num_columns
        ):
            msg = (
                f'Expected "{expected_num_columns}" columns, saw "{len(node.children)}"'
            )
            self.diagnostics.append(
                InvalidTableStructure(msg, util.get_line(node) + len(node.children) - 1)
            )
            return

    def add_static_asset(self, raw_path: str, upload: bool) -> StaticAsset:
        fileid, path = util.reroot_path(
            FileId(raw_path), self.docpath, self.project_config.source_path
        )
        static_asset = StaticAsset.load(raw_path, fileid, path, upload)
        self.static_assets.add(static_asset)
        return static_asset

    def add_diagnostics(self, diagnostics: Iterable[Diagnostic]) -> None:
        self.diagnostics.extend(diagnostics)

    def __make_child_visitor(self) -> "JSONVisitor":
        visitor = type(self)(self.project_config, self.docpath, self.document)
        visitor.diagnostics = self.diagnostics
        visitor.static_assets = self.static_assets
        visitor.pending = self.pending
        return visitor


class InlineJSONVisitor(JSONVisitor):
    """A JSONVisitor subclass which does not emit block nodes."""

    def dispatch_visit(self, node: docutils.nodes.Node) -> None:
        if isinstance(node, docutils.nodes.Body) and not isinstance(
            node, (docutils.nodes.Inline, docutils.nodes.system_message)
        ):
            return

        JSONVisitor.dispatch_visit(self, node)

    def dispatch_departure(self, node: docutils.nodes.Node) -> None:
        if isinstance(node, docutils.nodes.Body) and not isinstance(
            node, (docutils.nodes.Inline, docutils.nodes.system_message)
        ):
            return

        JSONVisitor.dispatch_departure(self, node)


def parse_rst(
    parser: rstparser.Parser[JSONVisitor], path: Path, text: Optional[str] = None
) -> Tuple[Page, List[Diagnostic]]:
    visitor, text = parser.parse(path, text)

    top_of_state = visitor.state[-1]
    assert isinstance(top_of_state, n.Parent)
    page = Page.create(path, None, text, top_of_state)
    page.static_assets = visitor.static_assets
    page.pending_tasks = visitor.pending
    return (page, visitor.diagnostics)


@dataclass
class EmbeddedRstParser:
    __slots__ = ("project_config", "page", "diagnostics")

    project_config: ProjectConfig
    page: Page
    diagnostics: List[Diagnostic]

    def parse_block(self, rst: str, lineno: int) -> MutableSequence[n.Node]:
        # Crudely make docutils line numbers match
        text = "\n" * lineno + rst.strip()
        parser = rstparser.Parser(self.project_config, JSONVisitor)
        visitor, _ = parser.parse(self.page.source_path, text)
        top_of_state = visitor.state[-1]
        children: MutableSequence[n.Node] = top_of_state.children  # type: ignore

        self.diagnostics.extend(visitor.diagnostics)
        self.page.static_assets.update(visitor.static_assets)
        self.page.pending_tasks.extend(visitor.pending)

        return children

    def parse_inline(self, rst: str, lineno: int) -> MutableSequence[n.InlineNode]:
        # Crudely make docutils line numbers match
        text = "\n" * lineno + rst.strip()
        parser = rstparser.Parser(self.project_config, InlineJSONVisitor)
        visitor, _ = parser.parse(self.page.source_path, text)
        top_of_state = visitor.state[-1]
        children: MutableSequence[n.InlineNode] = top_of_state.children  # type: ignore

        self.diagnostics.extend(visitor.diagnostics)
        self.page.static_assets.update(visitor.static_assets)
        self.page.pending_tasks.extend(visitor.pending)

        return children


def get_giza_category(path: PurePath) -> str:
    """Infer the Giza category of a YAML file."""
    return path.name.split("-", 1)[0]


class ProjectBackend(Protocol):
    def on_progress(self, progress: int, total: int, message: str) -> None:
        ...

    def on_diagnostics(self, path: FileId, diagnostics: List[Diagnostic]) -> None:
        ...

    def on_update(
        self,
        prefix: List[str],
        build_identifiers: BuildIdentifierSet,
        page_id: FileId,
        page: Page,
    ) -> None:
        ...

    def on_update_metadata(
        self,
        prefix: List[str],
        build_identifiers: BuildIdentifierSet,
        field: Dict[str, SerializableType],
    ) -> None:
        ...

    def on_delete(self, page_id: FileId, build_identifiers: BuildIdentifierSet) -> None:
        ...

    def flush(self) -> None:
        ...


class PageDatabase:
    """A database of FileId->Page mappings that ensures the postprocessing pipeline
       is run correctly. Raw parsed pages are added, flush() is called, then postprocessed
       pages can be accessed."""

    def __init__(self, postprocessor_factory: Callable[[], Postprocessor]) -> None:
        self.postprocessor_factory = postprocessor_factory
        self.parsed: Dict[FileId, Page] = {}
        self.__postprocessed: Dict[FileId, Page] = {}
        self.__changed_pages: Set[FileId] = set()

    def __setitem__(self, key: FileId, value: Page) -> None:
        """Set a raw parsed page."""
        self.parsed[key] = value
        self.__changed_pages.add(key)

    def __getitem__(self, key: FileId) -> Page:
        """If the postprocessor has been run since modifications were made, fetch a postprocessed page."""
        assert not self.__changed_pages
        return self.__postprocessed[key]

    def __contains__(self, key: FileId) -> bool:
        """Check if a given page exists in the parsed set."""
        return key in self.parsed

    def values(self) -> Iterable[Page]:
        """Iterate over postprocessed pages."""
        assert not self.__changed_pages
        return self.__postprocessed.values()

    def items(self) -> Iterable[Tuple[FileId, Page]]:
        """Iterate over the postprocessed (FileId, Page) set."""
        assert not self.__changed_pages
        return self.__postprocessed.items()

    def flush(
        self,
    ) -> Tuple[Dict[str, SerializableType], Dict[FileId, List[Diagnostic]]]:
        """Run the postprocessor if and only if any pages have changed, and return postprocessing results."""
        if not self.__changed_pages:
            return {}, {}

        postprocessor = self.postprocessor_factory()

        with util.PerformanceLogger.singleton().start("copy"):
            copied_pages = {k: util.fast_deep_copy(v) for k, v in self.parsed.items()}

        with util.PerformanceLogger.singleton().start("postprocessing"):
            post_metadata, post_diagnostics = postprocessor.run(copied_pages)

        self.__postprocessed = postprocessor.pages
        self.__changed_pages.clear()

        return post_metadata, post_diagnostics


class _Project:
    """Internal representation of a Snooty project with no data locking."""

    def __init__(
        self,
        root: Path,
        backend: ProjectBackend,
        filesystem_watcher: util.FileWatcher,
        build_identifiers: BuildIdentifierSet,
    ) -> None:
        root = root.resolve(strict=True)
        self.config, config_diagnostics = ProjectConfig.open(root)
        self.targets = TargetDatabase.load(self.config)

        if config_diagnostics:
            backend.on_diagnostics(
                FileId(self.config.config_path.relative_to(root)), config_diagnostics
            )

        self.parser = rstparser.Parser(self.config, JSONVisitor)
        self.backend = backend
        self.filesystem_watcher = filesystem_watcher
        self.build_identifiers = build_identifiers

        self.yaml_mapping: Dict[str, GizaCategory[Any]] = {
            "steps": gizaparser.steps.GizaStepsCategory(self.config),
            "extracts": gizaparser.extracts.GizaExtractsCategory(self.config),
            "release": gizaparser.release.GizaReleaseSpecificationCategory(self.config),
        }

        # For each repo-wide substitution, parse the string and save to our project config
        inline_parser = rstparser.Parser(self.config, InlineJSONVisitor)
        substitution_nodes: Dict[str, List[n.InlineNode]] = {}
        for k, v in self.config.substitutions.items():
            page, substitution_diagnostics = parse_rst(inline_parser, root, v)
            substitution_nodes[k] = list(
                deepcopy(child) for child in page.ast.children  # type: ignore
            )

            if substitution_diagnostics:
                backend.on_diagnostics(
                    self.config.get_fileid(self.config.config_path),
                    substitution_diagnostics,
                )

        self.config.substitution_nodes = substitution_nodes

        username = getpass.getuser()
        try:
            branch = subprocess.check_output(
                ["git", "rev-parse", "--abbrev-ref", "HEAD"],
                cwd=root,
                encoding="utf-8",
                stderr=subprocess.PIPE,
            ).strip()
        except subprocess.CalledProcessError as err:
            logger.info("git error getting branch name: %s", err.stderr)
            branch = "current"

        self.prefix = [self.config.name, username, branch]

        self.pages = PageDatabase(
            lambda: DevhubPostprocessor(self.config, self.targets)
            if self.config.default_domain == "devhub"
            else Postprocessor(self.config, self.targets)
        )

        self.asset_dg: "networkx.DiGraph[FileId]" = networkx.DiGraph()
        self.expensive_operation_cache: Cache[FileId] = Cache()

        published_branches, published_branches_diagnostics = self.get_parsed_branches()
        if published_branches:
            self.backend.on_update_metadata(
                self.prefix,
                self.build_identifiers,
                {"publishedBranches": published_branches.serialize()},
            )

        if published_branches_diagnostics:
            backend.on_diagnostics(
                self.config.get_fileid(self.config.config_path),
                published_branches_diagnostics,
            )

    def get_parsed_branches(
        self,
    ) -> Tuple[Optional[PublishedBranches], List[Diagnostic]]:
        try:
            path = self.config.root
            return parse_published_branches(
                path.joinpath("published-branches.yaml"), self.config
            )
        except FileNotFoundError:
            pass
        return None, []

    def get_full_path(self, fileid: FileId) -> Path:
        return self.config.source_path.joinpath(fileid)

    def get_line_content(self, path: Path, line: int) -> str:
        """Update page file (.txt) with current text and return a line"""
        # Get line content of page at specific line number
        fileid = self.config.get_fileid(path)
        self.pages.flush()
        page = self.pages[fileid]
        lines = page.source.splitlines()
        return lines[line]

    def get_page_ast(self, path: Path) -> n.Node:
        """Update page file (.txt) with current text and return fully populated page AST"""
        # Get incomplete AST of page
        fileid = self.config.get_fileid(path)
        post_metadata, post_diagnostics = self.pages.flush()
        for fileid, diagnostics in post_diagnostics.items():
            self.backend.on_diagnostics(fileid, diagnostics)
        page = self.pages[fileid]

        assert isinstance(page.ast, n.Parent)
        return page.ast

    def get_project_name(self) -> str:
        return self.config.name

    def get_project_title(self) -> str:
        return self.config.title

    def update(self, path: Path, optional_text: Optional[str] = None) -> None:
        diagnostics: Dict[PurePath, List[Diagnostic]] = {path: []}
        prefix = get_giza_category(path)
        _, ext = os.path.splitext(path)
        pages: List[Page] = []
        if ext in RST_EXTENSIONS:
            page, page_diagnostics = parse_rst(self.parser, path, optional_text)
            pages.append(page)
            diagnostics[path] = page_diagnostics
        elif ext == ".yaml" and prefix in self.yaml_mapping:
            file_id = os.path.basename(path)
            giza_category = self.yaml_mapping[prefix]
            needs_rebuild = set((file_id,)).union(
                *(
                    category.dg.predecessors(file_id)
                    for category in self.yaml_mapping.values()
                )
            )
            logger.debug("needs_rebuild: %s", ",".join(needs_rebuild))
            for file_id in needs_rebuild:
                file_diagnostics: List[Diagnostic] = []
                try:
                    giza_node = giza_category.reify_file_id(file_id, diagnostics)
                except KeyError:
                    logging.warn("No file found in registry: %s", file_id)
                    continue

                steps, text, parse_diagnostics = giza_category.parse(
                    path, optional_text
                )
                file_diagnostics.extend(parse_diagnostics)

                def create_page(filename: str) -> Tuple[Page, EmbeddedRstParser]:
                    page = Page.create(
                        giza_node.path,
                        filename,
                        text,
                        n.Root((-1,), [], self.config.get_fileid(FileId(filename)), {}),
                    )
                    return (
                        page,
                        EmbeddedRstParser(self.config, page, file_diagnostics),
                    )

                giza_category.add(path, text, steps)
                pages = giza_category.to_pages(
                    giza_node.path, create_page, giza_node.data
                )
                path = giza_node.path
                diagnostics.setdefault(path).extend(file_diagnostics)
        else:
            raise ValueError("Unknown file type: " + str(path))

        for source_path, diagnostic_list in diagnostics.items():
            self.backend.on_diagnostics(
                self.config.get_fileid(source_path), diagnostic_list
            )

        for page in pages:
            self._page_updated(page, diagnostic_list)
            fileid = self.config.get_fileid(page.fake_full_path())
            self.backend.on_update(self.prefix, self.build_identifiers, fileid, page)
        self.backend.flush()

    def delete(self, path: PurePath) -> None:
        file_id = os.path.basename(path)
        for giza_category in self.yaml_mapping.values():
            del giza_category[file_id]

        self.backend.on_delete(self.config.get_fileid(path), self.build_identifiers)

    def queryFileNames(self) -> List[Dict]:
        completions = []
        paths = util.get_files(self.config.source_path, RST_EXTENSIONS)
        for path in paths:
            file, ext = os.path.splitext(path.relative_to(self.config.source_path))
            completions.append({
                "file": file,
                "path": str(path.absolute())
            })
        return completions

    def build(
        self, max_workers: Optional[int] = None, postprocess: bool = True
    ) -> None:
        all_yaml_diagnostics: Dict[PurePath, List[Diagnostic]] = {}
        pool = multiprocessing.Pool(max_workers)
        with util.PerformanceLogger.singleton().start("parse rst"):
            try:
                paths = util.get_files(self.config.source_path, RST_EXTENSIONS)
                logger.debug("Processing rst files")
                results = pool.imap_unordered(partial(parse_rst, self.parser), paths)
                for page, diagnostics in results:
                    self._page_updated(page, diagnostics)
            finally:
                # We cannot use the multiprocessing.Pool context manager API due to the following:
                # https://pytest-cov.readthedocs.io/en/latest/subprocess-support.html#if-you-use-multiprocessing-pool
                pool.close()
                pool.join()

        # Categorize our YAML files
        logger.debug("Categorizing YAML files")
        categorized: Dict[str, List[Path]] = collections.defaultdict(list)
        for path in util.get_files(self.config.source_path, (".yaml",)):
            prefix = get_giza_category(path)
            if prefix in self.yaml_mapping:
                categorized[prefix].append(path)

        # Initialize our YAML file registry
        for prefix, giza_category in self.yaml_mapping.items():
            logger.debug("Parsing %s YAML", prefix)
            for path in categorized[prefix]:
                steps, text, diagnostics = giza_category.parse(path)
                all_yaml_diagnostics[path] = diagnostics
                giza_category.add(path, text, steps)

        # Now that all of our YAML files are loaded, generate a page for each one
        for prefix, giza_category in self.yaml_mapping.items():
            logger.debug("Processing %s YAML: %d nodes", prefix, len(giza_category))
            for file_id, giza_node in giza_category.reify_all_files(
                all_yaml_diagnostics
            ):

                def create_page(filename: str) -> Tuple[Page, EmbeddedRstParser]:
                    page = Page.create(
                        giza_node.path,
                        filename,
                        giza_node.text,
                        n.Root((-1,), [], self.config.get_fileid(FileId(filename)), {}),
                    )
                    return (
                        page,
                        EmbeddedRstParser(
                            self.config,
                            page,
                            all_yaml_diagnostics.setdefault(giza_node.path, []),
                        ),
                    )

                for page in giza_category.to_pages(
                    giza_node.path, create_page, giza_node.data
                ):
                    self._page_updated(
                        page, all_yaml_diagnostics.get(page.source_path, [])
                    )

        if postprocess:
            post_metadata, post_diagnostics = self.pages.flush()

            static_files = {
                "objects.inv": self.targets.generate_inventory("").dumps(
                    self.config.name, ""
                )
            }
            post_metadata["static_files"] = static_files

            with util.PerformanceLogger.singleton().start("commit"):
                for fileid, page in self.pages.items():
                    self.backend.on_update(
                        self.prefix, self.build_identifiers, fileid, page
                    )
                self.backend.flush()

            for fileid, diagnostics in post_diagnostics.items():
                self.backend.on_diagnostics(fileid, diagnostics)

            self.backend.on_update_metadata(
                self.prefix, self.build_identifiers, post_metadata
            )

    def _page_updated(self, page: Page, diagnostics: List[Diagnostic]) -> None:
        """Update any state associated with a parsed page."""
        # Finish any pending tasks
        page.finish(diagnostics, self)

        # Synchronize our asset watching
        old_assets: Set[StaticAsset] = set()
        removed_assets: Set[StaticAsset] = set()
        fileid = self.config.get_fileid(page.fake_full_path())

        logger.debug("Updated: %s", fileid)

        if fileid in self.pages:
            old_page = self.pages.parsed[fileid]
            old_assets = old_page.static_assets
            removed_assets = old_page.static_assets.difference(page.static_assets)

        new_assets = page.static_assets.difference(old_assets)
        for asset in new_assets:
            try:
                self.filesystem_watcher.watch_file(asset.path)
            except OSError as err:
                # Missing static asset directory: don't process it. We've already raised a
                # diagnostic to the user.
                logger.debug(f"Failed to set up watch: {err}")
                page.static_assets.remove(asset)
        for asset in removed_assets:
            self.filesystem_watcher.end_watch(asset.path)

        # Update dependents
        try:
            self.asset_dg.remove_node(self.config.get_fileid(page.source_path))
        except networkx.exception.NetworkXError:
            pass
        self.asset_dg.add_edges_from(
            (
                self.config.get_fileid(page.source_path),
                self.config.get_fileid(asset.path),
            )
            for asset in page.static_assets
        )

        # Report to our backend
        self.pages[fileid] = page
        self.backend.on_diagnostics(
            self.config.get_fileid(page.source_path), diagnostics
        )

    def on_asset_event(self, ev: watchdog.events.FileSystemEvent) -> None:
        asset_path = self.config.get_fileid(Path(ev.src_path))

        # Revoke any caching that might have been performed on this file
        try:
            del self.expensive_operation_cache[asset_path]
        except KeyError:
            pass

        # Rebuild any pages depending on this asset
        for page_id in list(self.asset_dg.predecessors(asset_path)):
            self.update(self.pages[page_id].source_path)


class Project:
    """A Snooty project, providing high-level operations on a project such as
       requesting a rebuild, and updating a file based on new contents.

       This class's public methods are thread-safe."""

    __slots__ = ("_project", "_lock", "_filesystem_watcher")

    def __init__(
        self, root: Path, backend: ProjectBackend, build_identifiers: BuildIdentifierSet
    ) -> None:
        self._filesystem_watcher = util.FileWatcher(self._on_asset_event)
        self._project = _Project(
            root, backend, self._filesystem_watcher, build_identifiers
        )
        self._lock = threading.Lock()
        self._filesystem_watcher.start()

    @property
    def config(self) -> ProjectConfig:
        return self._project.config

    def get_full_path(self, fileid: FileId) -> Path:
        # We don't need to obtain a lock because this method only operates on
        # _Project.root, which never changes after creation.
        return self._project.get_full_path(fileid)

    def get_line_content(self, path: Path, position) -> str:
        """Return line content of page with updated text"""
        with self._lock:
            return self._project.get_line_content(path, position)

    def get_page_ast(self, path: Path) -> n.Node:
        """Return complete AST of page with updated text"""
        with self._lock:
            return self._project.get_page_ast(path)

    def get_project_name(self) -> str:
        return self._project.get_project_name()

    def update(self, path: Path, optional_text: Optional[str] = None) -> None:
        """Re-parse a file, optionally using the provided text rather than reading the file."""
        with self._lock:
            self._project.update(path, optional_text)

    def delete(self, path: PurePath) -> None:
        """Mark a path as having been deleted."""
        with self._lock:
            self._project.delete(path)

    def build(
        self, max_workers: Optional[int] = None, postprocess: bool = True
    ) -> None:
        """Build the full project."""
        with self._lock:
            self._project.build(max_workers, postprocess)

    def queryFileNames(self) -> List[Dict]:
        return self._project.queryFileNames()

    def stop_monitoring(self) -> None:
        """Stop the filesystem monitoring thread associated with this project."""
        self._filesystem_watcher.stop(join=True)

    def _on_asset_event(self, ev: watchdog.events.FileSystemEvent) -> None:
        with self._lock:
            self._project.on_asset_event(ev)

    def __enter__(self) -> "Project":
        return self

    def __exit__(self, *args: object) -> None:
        self.stop_monitoring()
