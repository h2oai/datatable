#!/usr/bin/env python
#-------------------------------------------------------------------------------
# Copyright 2019 H2O.ai
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#-------------------------------------------------------------------------------
"""
Sphinx directive to embed datatable Frames into generated RST
documentation.

In order to use this module, first add its name to the `extensions`
list in your documentations's "conf.py" file:

    extensions = [
        ...,
        'datatable.sphinxext.dtframe_directive'
    ]

After that, you can use the `.. dtframe::` directive in your .rst
files. For example:

    .. dtframe::
        :names: Key, Value, ..., Last
        :types: str32, int32, ..., float64
        :shape: (9, 7)

        0,cat,1,...,2.3
        1,apple,2,...,0.0e+0
        2,ocean,NA,...,NA
        3,oak,-22,...,inf
        ...
        7,saturn,23,...,-3.45
        8,</table>,0,...,-2.33e-101

The following rules apply:

    - Lines `:names:`, `:types:` and `:shape:` are required.

    - Option `:types:` lists the stypes of all columns in the frame.
      This is a comma-separated list, optionally surrounded in square
      brackets or parentheses.

      One of the types may be "...", indicating a place where some of
      the columns were omitted. In this case the `:names:` list must
      also have "..." at the same position, and similarly all data
      rows.

    - Option `:names:` lists the column names in the frame. This is a
      comma-separated list, which can also be surrounded in square
      brackets. The names may also be quoted. If a name contains
      quotes, they should be escaped (using either \\, or doubling
      the quote).

      The `:names:` list should have the same number of entries as
      the `:types:` list.

    - Option `:shape:` is a 2-tuple of (nrows, ncols).

    - Each data row must start with a row index, and then have the
      same number of elements as lists `:types:` and `:names:`.

      One of the rows (but not the first one) may contain a single
      "...". This indicates the place where some rows were omitted,
      and will be rendered accordingly.

    - All non-quoted `NA` entries will be rendered as special "NA"
      tokens in the output.

"""
import re
from docutils import nodes
from docutils.parsers.rst import directives
from docutils.parsers.rst import Directive

stype2ltype = {
    "bool8": "bool",
    "int8": "int",
    "int16": "int",
    "int32": "int",
    "int64": "int",
    "float32": "real",
    "float64": "real",
    "str32": "str",
    "str64": "str",
    "obj64": "obj"
}
stype2width = {
    "bool8": 1,
    "int8": 1,
    "int16": 2,
    "int32": 4,
    "int64": 8,
    "float32": 4,
    "float64": 8,
    "str32": 4,
    "str64": 8,
    "obj64": 8
}

def comma_separated(n):
    """Render number `n` as a comma-separated string"""
    if n < 0:
        return "-" + comma_separated(-n)
    if n < 10000:
        return str(n)
    else:
        nstr = ""
        while n >= 1000:
            nstr = ",%03d%s" % (n % 1000, nstr)
            n = n // 1000
        return str(n) + nstr


#-------------------------------------------------------------------------------
# Parsers
#-------------------------------------------------------------------------------

def check_parens(p1, p2):
    if p1 == "" and p2 == "": return True
    if p1 == "" or p2 == "": return False
    d = ord(p2) - ord(p1)
    return d == 1 or d == 2


def parse_csv_line(line):
    WHITESPACE = " \t\n\r"
    tokens = []
    # Skip the whitespace at the beginning/end
    istart = 0
    iend = len(line)
    while istart < iend and line[istart] in WHITESPACE:
        istart += 1
    while istart < iend and line[iend - 1] in WHITESPACE:
        iend -= 1
    # Iterate over the string, parsing the tokens
    i = istart
    token_pending = True
    while i < iend:
        ch = line[i]
        i += 1
        if ch == "'" or ch == '"':
            # Parse a quoted token
            token_pending = False
            quote = ch
            chars = []
            quote_ended = False
            while i < iend:
                ch = line[i]
                i += 1
                if ch == quote:
                    if i < iend and line[i] == quote:
                        i += 1
                        chars.append(quote)
                    else:
                        quote_ended = True
                        break
                elif ch == '\\':
                    if i < iend:
                        chars.append(line[i])
                        i += 1
                    else:
                        raise ValueError("Unterminated escape character `\\` "
                                         "in line %r" % line)
                else:
                    chars.append(ch)
            if not quote_ended:
                raise ValueError("Non-terminated quote in line %r" % line)
            while i < iend and line[i] in WHITESPACE:
                i += 1
            if i < iend:
                if line[i] == ',':
                    i += 1
                    token_pending = True
                else:
                    raise ValueError("Expected a comma after a quoted string "
                                     "at pos %d in line %r" % (i, line))
            tokens.append("".join(chars))
        elif ch in WHITESPACE:
            pass
        elif ch == ",":
            tokens.append("")
        else:
            # Parse a non-quoted token
            token_pending = False
            token_start = i - 1
            while i < iend and line[i] != ',':
                i += 1
            token_end = i
            while token_end > token_start and line[token_end-1] in WHITESPACE:
                token_end -= 1
            tok = line[token_start:token_end]
            if tok == "NA":
                tok = None
            tokens.append(tok)
            if i < iend:
                assert line[i] == ','
                i += 1
                token_pending = True
    if token_pending:
        tokens.append("")
    return tokens


def parse_shape(s):
    """
    Parser for the :shape: option, which must be a tuple
    `NROWS, NCOLS`, possibly surrounded with parentheses or square
    brackets.
    """
    if not s:
        raise ValueError("Option :shape: is required")
    mm = re.fullmatch(r"\s*([\[\(]?)\s*(\d+)\s*,\s*(\d+)\s*([\]\)]?)\s*", s)
    if not mm:
        raise ValueError("Invalid shape value: %r" % s)
    p1, str_rows, str_cols, p2  = mm.groups()
    if not check_parens(p1, p2):
        raise ValueError("Mismatched parentheses in :shape: %r" % s)
    return (int(str_rows), int(str_cols))


def parse_types(s):
    """
    Parser for the :types: option
    """
    if s is None:
        raise ValueError("Option :types: is required")
    mm = re.fullmatch(r"\s*([\[\(]?)(.*?)([\]\)]?)\s*", s, re.DOTALL)
    assert mm, "parse_types regular expression failed for %r" % s
    p1, middle, p2 = mm.groups()
    if not check_parens(p1, p2):
        raise ValueError("Mismatched parentheses in :types: %r (%r, %r)" % (s, p1, p2))
    items = parse_csv_line(middle)
    if items == [""]:
        items = []
    ellipsis_found = False
    for i, item in enumerate(items):
        if item == "...":
            if ellipsis_found:
                raise ValueError("More than one ellipsis in :types: is not "
                                 "allowed")
            ellipsis_found = True
            items[i] = Ellipsis
            continue
        if item not in stype2ltype:
            raise ValueError("Invalid stype %r at index %d in :types:"
                             % (item, i))
    return items


def parse_names(s):
    if s is None:
        raise ValueError("Option :names: is required")
    mm = re.fullmatch(r"\s*([\[\(]?)(.*?)([\]\)]?)\s*", s, re.DOTALL)
    assert mm, "parse_types regular expression failed for %r?" % s
    p1, middle, p2 = mm.groups()
    if not check_parens(p1, p2):
        raise ValueError("Mismatched parentheses in :names: %r" % s)
    names = parse_csv_line(middle)
    if names == [""]:
        names = []
    return names



#-------------------------------------------------------------------------------
# Nodes
#-------------------------------------------------------------------------------

class div_node(nodes.General, nodes.Element): pass

def visit_div(self, node):
    self.body.append(self.starttag(node, "div"))

def depart_div(self, node):
    self.body.append("</div>")



class table_node(nodes.Element): pass

def visit_table(self, node):
    self.body.append(self.starttag(node, "table"))

def depart_table(self, node):
    self.body.append("</table>")



class td_node(nodes.Element):
    def __init__(self, text="", **attrs):
        super().__init__(**attrs)
        if text:
            self += nodes.Text(text)

def visit_td(self, node):
    self.body.append(self.starttag(node, "td", suffix=""))

def depart_td(self, node):
    self.body.append("</td>")



class th_node(nodes.Element):
    def __init__(self, text="", **attrs):
        super().__init__(**attrs)
        if text:
            self += nodes.Text(text)

def visit_th(self, node):
    self.body.append(self.starttag(node, "th", suffix=""))

def depart_th(self, node):
    self.body.append("</th>")




#-------------------------------------------------------------------------------
# DtframeDirective
#-------------------------------------------------------------------------------

class DtframeDirective(Directive):
    has_content = True
    required_arguments = 0
    optional_arguments = 0
    option_spec = {
        "shape": parse_shape,
        "types": parse_types,
        "names": parse_names,
        "output": directives.flag,  # temporary
    }

    def run(self):
        shape, types, names = self._parse_options()
        frame_data = self._parse_table(types, names)

        root_node = div_node(classes=["datatable"])
        root_node += self._make_table(names, types, frame_data)
        root_node += self._make_footer(shape)
        if "output" in self.options:
            div = div_node(classes=["output-cell"])
            div += root_node
            return [div]
        else:
            return [root_node]


    def _parse_options(self):
        if "shape" not in self.options: parse_shape(None)
        if "types" not in self.options: parse_types(None)
        if "names" not in self.options: parse_names(None)
        shape = self.options["shape"]
        types = self.options["types"]
        names = self.options["names"]
        if len(names) != len(types):
            raise ValueError("Number of values in :names: is %d, while the "
                             "number of values in :types: is %d"
                             % (len(names), len(types)))
        return shape, types, names


    def _parse_table(self, types, names):
        ncols = len(types)
        frame_data = []
        for i, line in enumerate(self.content):
            if line == "...":
                if i == 0:
                    raise ValueError("First row cannot be Ellipsis")
                frame_data.append(Ellipsis)
                continue
            tokens = parse_csv_line(line)
            if len(tokens) != ncols + 1:
                raise ValueError("Expected %d entries in line %d of input, "
                                 "instead found %d"
                                 % (ncols + 1, i + 1, len(tokens)))
            frame_data.append(tokens)
        for i in range(ncols):
            if types[i] is Ellipsis:
                if names[i] == "...":
                    names[i] = Ellipsis
                if names[i] is not Ellipsis:
                    raise ValueError("Inconsistent position of the ellipsis "
                                     "column: types[%d] = ..., whereas "
                                     "names[%d] = %r" % (i, i, names[i]))
                for j, row in enumerate(frame_data):
                    if row is Ellipsis:
                        continue
                    if row[i+1] == "...":
                        row[i+1] = Ellipsis
                    else:
                        raise ValueError("Inconsistent position of the ellipsis"
                                         " column: types[%d] = ..., whereas "
                                         "data[%d][%d] = %r"
                                         % (i, j, i+1, row[i+1]))
                break
        return frame_data


    def _make_table(self, names, types, data):
        table = table_node(classes=["frame"])
        thead = nodes.thead()
        thead += self._make_column_names_row(names)
        thead += self._make_column_types_row(types)
        table += thead
        table += self._make_table_body(types, data)
        return table


    def _make_column_names_row(self, names):
        row = nodes.row(classes=["colnames"])
        row += td_node(classes=["row_index"])
        for name in names:
            classes = []
            if name is Ellipsis:
                name = "\u2026"
                classes.append("vellipsis")
            row += th_node(classes=classes, text=name)
        return row


    def _make_column_types_row(self, types):
        row = nodes.row(classes=["coltypes"])
        row += td_node(classes=["row_index"])
        for stype in types:
            if stype is Ellipsis:
                row += th_node()
            else:
                row += th_node(classes=[stype2ltype[stype]], title=stype,
                               text="\u25AA" * stype2width[stype])
        return row


    def _make_table_body(self, types, data):
        body = nodes.tbody()
        if not data:
            return body
        ellipsis_column = -1
        is_numeric = [False] * len(data[0])
        for i, entry in enumerate(data[0]):
            if entry is Ellipsis:
                ellipsis_column = i
            if i > 0 and types[i-1] is not Ellipsis:
                lt = stype2ltype[types[i-1]]
                if lt == "int" or lt == "real":
                    is_numeric[i] = True
        for datarow in data:
            row_node = nodes.row()
            body += row_node
            if datarow is Ellipsis:
                for i in range(len(data[0])):
                    html_class = "row_index" if i == 0 else "hellipsis"
                    text = "\u22F1" if i == ellipsis_column else "\u22EE"
                    row_node += td_node(classes=[html_class], text=text)
            else:
                for i, text in enumerate(datarow):
                    classes = []
                    if i == 0:
                        classes = ["row_index"]
                        text = comma_separated(int(text))
                    elif i == ellipsis_column:
                        classes = ["vellipsis"]
                        text = "\u2026"
                    elif text is None:
                        classes = ["na"]
                        text = "NA"
                    elif is_numeric[i]:
                        text = text.replace("-", "\u2212")
                    assert isinstance(text, str)
                    row_node += td_node(classes=classes, text=text)
        return body


    def _make_footer(self, shape):
        footer_node = div_node(classes=["footer"])
        dim_node = div_node(classes=["frame_dimensions"])
        shape_text = ("%s row%s × %s column%s"
                      % (comma_separated(shape[0]),
                         "" if shape[0] == 1 else "s",
                         comma_separated(shape[1]),
                         "" if shape[1] == 1 else "s"))
        dim_node += nodes.Text(shape_text)
        footer_node += dim_node
        return footer_node




#-------------------------------------------------------------------------------
# Set-up
#-------------------------------------------------------------------------------

def html_page_context(app, pagename, templatename, context, doctree):
    """
    Handler for the html-page-context event. This handler is used to inject
    a CSS declaration into the generated page.
    """
    style = """
        .rst-content dl dd .datatable { line-height: normal; }
        .rst-content .datatable table.frame,
        .datatable table.frame {
            border: none;
            border-collapse: collapse;
            border-spacing: 0;
            color: #222;
            cursor: default;
            font-size: 12px;
            margin: 0 !important;
        }
        .datatable table.frame thead {
            border-bottom: none;
            vertical-align: bottom;
        }
        .datatable table.frame thead tr.coltypes th {
            color: #FFFFFF;
            line-height: 6px;
            padding: 0 0.5em;
        }
        .datatable .bool { background: #DDDD99; }
        .datatable .obj  { background: #565656; }
        .datatable .int  { background: #5D9E5D; }
        .datatable .real { background: #4040CC; }
        .datatable .str  { background: #CC4040; }
        .datatable table.frame td,
        .datatable table.frame thead th,
        .datatable table.frame tr {
            border: none;
            max-width: none;
            padding: 0.5em 0.5em;
            text-align: right;
            vertical-align: middle;
        }
        .datatable table.frame thead tr th:nth-child(2) {
            padding-left: 12px;
        }
        .datatable table.frame tbody tr:nth-child(even) {
            background-color: #F5F5F5;
        }
        .datatable table.frame.docutils tbody tr:nth-child(odd) td {
            background-color: #FFFFFF;
        }
        .datatable table.frame td.row_index {
            color: rgba(0, 0, 0, 0.38);
            background: #EEEEEE;
            font-size: 9px;
            border-right: 1px solid #BDBDBD;
        }
        .datatable table.frame tr.coltypes td.row_index {
            background: #BDBDBD;
        }
        .datatable table.frame td.hellipsis {
            color: #E0E0E0;
        }
        .datatable table.frame td.vellipsis {
            background: #FFFFFF;
            color: #E0E0E0;
        }
        .datatable table.frame td.na {
            color: #E0E0E0;
            font-size: 80%;
        }
        .datatable .footer {
            font-size: 9px;
        }
        .datatable .footer .frame_dimensions {
            background: #EEEEEE;
            border-top: 1px solid #BDBDBD;
            color: rgba(0, 0, 0, 0.38);
            display: inline-block;
            opacity: 0.6;
            padding: 1px 10px 1px 5px;
        }
    """
    context['body'] = ('<style type="text/css" id="dtcss">' + style + '</style>\n' +
                       context.get('body', ""))


def setup(app):
    app.add_directive("dtframe", DtframeDirective)
    app.add_node(div_node, html=(visit_div, depart_div))
    app.add_node(table_node, html=(visit_table, depart_table))
    app.add_node(td_node, html=(visit_td, depart_td))
    app.add_node(th_node, html=(visit_th, depart_th))
    app.connect('html-page-context', html_page_context)
    return {"parallel_read_safe": True, "parallel_write_safe": True}
