from typing import Any, Callable, Dict, List, Tuple, Match
import re
import sys

# Prevent Python 2 from converting our nice clean unicode strings back
# to byte bags.
str_func = 'str' if sys.version_info > (3,) else 'unicode'

TOKEN_COMMENT = sys.intern('comment')
TOKEN_ELSE = sys.intern('else')
TOKEN_END = sys.intern('end')
TOKEN_FOR = sys.intern('for')
TOKEN_FORMAT = sys.intern('format')
TOKEN_IF = sys.intern('if')
TOKEN_LITERAL = sys.intern('literal')
TOKEN_SUB = sys.intern('sub')


def escape(s: str) -> str:
    """Escape HTML entity characters."""
    s = s.replace('&', '&amp;')
    s = s.replace('<', '&lt;')
    s = s.replace('>', '&gt;')
    s = s.replace('"', '&quot;')
    return s.replace('\'', '&#x27;')


def eat_error(f: Callable[[], Any]) -> Any:
    """Call f(), returning an empty string on error."""
    try:
        return f()
    except (IndexError, KeyError):
        return ''


class VariableStack:
    """Tracks variable scopes and loop counter depth."""
    __slots__ = ('stack', 'loop_counter')

    def __init__(self) -> None:
        self.stack = []  # type: List[str]
        self.loop_counter = 0

    def __contains__(self, value: str) -> bool:
        if value == 'i' and self.loop_counter > 0:
            return True

        return value in self.stack


class Template:
    """A compiled Fett template."""
    PAT = re.compile(r'\{\{[^{]*\}\}')
    PAT_TAGS = re.compile(r'<[^>]+>')
    ONLY_WHITESPACE_LEFT = re.compile(r'[^\S\n]*(?:\n|$)')
    ONLY_WHITESPACE_RIGHT = re.compile(r'(?:\n|^)[^\S\n]*$')
    NAME_PAT = re.compile(r'^[a-zA-Z0-9_]+$')
    FILTER_PAT = re.compile(r'([a-zA-Z0-9_]+)(?:\(([^)]+)\))?')
    FILTERS = {
        'odd': lambda x: int(x) % 2 != 0,
        'even': lambda x: int(x) % 2 == 0,
        'car': lambda x: x[0],
        'cdr': lambda x: x[1:],
        'dec': lambda x: int(x) - 1,
        'len': lambda x: len(x),
        'strip': lambda x: str(x).strip(),
        'inc': lambda x: int(x) + 1,
        'negative': lambda x: int(x) < 0,
        'not': lambda x: not x,
        'timesNegOne': lambda x: -int(x),
        'positive': lambda x: int(x) > 0,
        'split': lambda x: str(x).split(),
        'escape': escape,
        'striptags': lambda x: Template.PAT_TAGS.sub('', x),
        'zero': lambda x: x == 0,

        'upperCase': lambda x: str(x).upper(),
        'lowerCase': lambda x: str(x).lower(),

        'plus': lambda x, n: int(x) + int(n),
        'minus': lambda x, n: int(x) - int(n),
        'equal': lambda x1, x2: str(x1) == str(x2),
        'lessThan': lambda x, n: int(x) < int(n),
        'lessThanOrEqual': lambda x, n: int(x) <= int(n),
        'greaterThan': lambda x, n: int(x) > int(n),
        'greaterThanOrEqual': lambda x, n: int(x) >= int(n),
    }  # type: Dict[str, Any]

    def __init__(self, template: str) -> None:
        self.program_source = ''

        tasks = []  # type: List[Tuple[str, ...]]
        depth = 0
        a = 0
        b = 0
        for match in self.PAT.finditer(template):
            tag = match.group(0)[2:-2].strip()
            components = tag.split(None, 4)

            # Trim standalone tag whitespace
            start_of_line = max(0, template.rfind('\n', 0, match.start()))
            line_start = self.ONLY_WHITESPACE_RIGHT.match(template,
                                                          start_of_line,
                                                          match.start())
            line_end = self.ONLY_WHITESPACE_LEFT.match(template, match.end())
            if line_start and line_end and not self.is_interpolation(tag):
                skip_newline = 0 if line_start.start() == 0 else 1
                literal = template[a:line_start.start() + skip_newline]
                if literal:
                    tasks.append((TOKEN_LITERAL, literal))

                a = line_end.end()
                b = line_end.end()
            else:
                b = match.start()
                if a != b:
                    literal = template[a:b]
                    if literal:
                        tasks.append((TOKEN_LITERAL, literal))
                (a, b) = (match.end(), match.end())

            # Comment
            if tag.startswith('#'):
                tasks.append((TOKEN_COMMENT, tag))
                continue

            if len(components) >= 4 \
               and components[0] == 'for' \
               and components[2] == 'in':
                expr = ' '.join(components[3:])
                tasks.append((TOKEN_FOR, components[1], expr))
                depth += 1
            elif len(components) >= 2 and components[0] == 'if':
                tasks.append((TOKEN_IF, ' '.join(components[1:])))
                depth += 1
            elif components and components[0] == 'else':
                tasks.append((TOKEN_ELSE,))
            elif components and components[0] == 'end':
                tasks.append((TOKEN_END,))
                depth -= 1
            elif len(components) == 2 and components[0] == 'format':
                indentation = self.get_indentation(template, match)
                tasks.append((TOKEN_FORMAT, components[1], indentation))
            else:
                indentation = self.get_indentation(template, match)
                tasks.append((TOKEN_SUB, tag, indentation))

        literal = template[b:]
        if literal:
            tasks.append((TOKEN_LITERAL, literal))

        if depth > 0:
            raise ValueError('Expected "end"')

        if depth < 0:
            raise ValueError('Umatched "end"')

        try:
            self.program = self._compile(tasks)
        except SyntaxError as err:
            raise err.__class__('{}. Source:\n{}'.format(str(err),
                                                         self.program_source))

    def render(self, data: Dict[str, Any]) -> str:
        """Render this compiled template into a string using the given data."""
        env = self.FILTERS.copy()
        env['__eat_error__'] = eat_error
        env['miniformat'] = self.miniformat

        gobj = {}  # type: Dict[str, Any]
        program = eval(self.program, env, gobj)
        program = gobj['run']

        try:
            generator = program(data)
            if generator:
                return ''.join(generator)

            return ''
        except Exception as err:
            raise err.__class__('{}. Source:\n{}'.format(str(err),
                                                         self.program_source))

    def _compile(self, tasks: List[Tuple[str, ...]]) -> Any:
        indent = 4
        need_pass = False
        local_stack = VariableStack()
        stack = []  # type: List[Tuple[str, ...]]
        program = ['def run(__data__):']

        for task in tasks:
            if task[0] is TOKEN_LITERAL:
                program.append(indent * ' ' + 'yield ' + repr(task[1]))
                need_pass = False
            elif task[0] is TOKEN_FOR:
                iter_name = self.vet_name(task[1])
                attr = self.transform_expr(task[2], local_stack)
                program.append('{}for i, {} in enumerate({}):'
                               .format(' ' * indent, iter_name, attr))
                stack.append((TOKEN_FOR, task[2]))
                local_stack.stack.append(task[1])
                local_stack.loop_counter += 1
                need_pass = True
                indent += 4
            elif task[0] is TOKEN_IF:
                attr = self.transform_expr(task[1], local_stack)
                program.append('{}if {}:'.format(' ' * indent, attr))
                stack.append((TOKEN_IF,))
                need_pass = True
                indent += 4
            elif task[0] is TOKEN_ELSE:
                if not stack:
                    raise ValueError('"else" without matching "for" or "if"')

                if need_pass:
                    program.append('{}pass'.format(' ' * indent))

                if stack[-1][0] is TOKEN_FOR:
                    attr = self.transform_expr(stack[-1][1], local_stack)
                    program.append('{}if not {}:'
                                   .format(' ' * (indent - 4), attr))
                elif stack[-1][0] is TOKEN_IF:
                    program.append('{}else:'.format(' ' * (indent - 4)))
                else:
                    assert False

                need_pass = True
            elif task[0] is TOKEN_END:
                if stack[-1][0] is TOKEN_FOR:
                    local_stack.stack.pop()
                    local_stack.loop_counter -= 1

                stack.pop()

                if need_pass:
                    program.append('{}pass'.format(' ' * indent))
                    need_pass = False

                indent -= 4
            elif task[0] is TOKEN_SUB:
                getter = self.transform_expr(task[1], local_stack)
                program.append('{}yield {}({}).replace("\\n", "\\n" + {})'
                               .format(' ' * indent, str_func, getter,
                                       repr(task[2])))
                need_pass = False
            elif task[0] is TOKEN_FORMAT:
                getter = self.transform_expr(task[1], local_stack)
                program.append('{}yield miniformat({}({}), __data__)'
                               '.replace("\\n", "\\n" + {})'
                               .format(' ' * indent, str_func, getter,
                                       repr(task[2])))
                need_pass = False

        self.program_source = '\n'.join(program)
        return compile(self.program_source, 'renderer', 'exec')

    @classmethod
    def is_interpolation(cls, tag: str) -> bool:
        """Check if a tag is interpolating or just a block structure
           or comment"""
        return not (tag.startswith('if ') or tag.startswith('for ') or
                    tag.startswith('#') or tag in ('else', 'end'))

    @classmethod
    def transform_expr(cls, expr: str, unmangle: VariableStack) -> str:
        """Transform a space-delimited sequence of tokens into a chained
           sequence of function calls."""
        if expr.startswith('`'):
            end_literal_index = expr.find('`', 1)
            if end_literal_index < 0:
                raise ValueError('Unmatched literal: ' + expr)
            result = repr(expr[1:end_literal_index])
            components_string = expr[end_literal_index + 1:].strip()
        else:
            parts = expr.split(' ', 1)
            result = parts[0]
            if not result:
                return ''

            components_string = '' if len(parts) == 1 else parts[1]
            result = cls.dot_to_subscript(result, unmangle)

        for component in cls.FILTER_PAT.finditer(components_string):
            filter_name, args = component.groups()
            if filter_name not in cls.FILTERS:
                raise ValueError('Unknown filter: ' + filter_name)

            if args is not None:
                result = '{}({}, {})'.format(filter_name, result, repr(args))
            else:
                result = '{}({})'.format(filter_name, result)

        return result

    @classmethod
    def dot_to_subscript(cls, name: str, unmangle: VariableStack) -> str:
        """Transform a.b.c into data['a']['b']['c']."""
        path = name.split('.')

        from_local = False
        if len(path) >= 1 and unmangle and path[0] in unmangle:
            from_local = True

        if from_local:
            if len(path) > 1:
                return path[0] + ''.join(['[{}]'.format(repr(x))
                                          for x in path[1:]])

            return name

        result = ''.join(['[{}]'.format(repr(cls.vet_name(x))) for x in path])
        return '__eat_error__(lambda: __data__' + result + ')'

    @classmethod
    def vet_name(cls, name: str) -> str:
        """Check if a name (field name or iteration variable) is legal."""
        if not cls.NAME_PAT.match(name):
            raise ValueError('Illegal name: ' + repr(name))

        return name

    @classmethod
    def get_indentation(cls, template: str, match: Match) -> str:
        """Get the indentation string at a given point in the template."""
        start_of_line = template.rfind('\n', 0, match.start()) + 1
        indentation = []
        while template[start_of_line] in (' ', '\t'):
            indentation.append(template[start_of_line])
            start_of_line += 1

        return ''.join(indentation)

    @classmethod
    def miniformat(cls, template: str, data: Any) -> str:
        """Simple string formatting routine."""
        def handle(match: Match) -> Any:
            cursor = data
            inner = match.group(0)[2:-2].strip()
            if not inner:
                return ''

            components = inner.split(' ')
            path = components[0].split('.')[::-1]

            components = components[1:]
            while path:
                element = cls.vet_name(path.pop().strip())
                cursor = cursor[element]

            while components:
                component = components.pop()
                if component not in cls.FILTERS:
                    raise ValueError('Unknown filter: ' + component)

                cursor = cls.FILTERS[component](cursor)

            return str(cursor)

        return cls.PAT.sub(lambda match: eat_error(lambda: handle(match)),
                           template)
