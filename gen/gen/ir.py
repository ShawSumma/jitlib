
import gen.types as types
from gen.text import dedent
from gen.template import Replaces
from gen.ctype import C_TYPES
from gen.text import indent
from typing import Callable, TypeVar, TypeVarTuple

class Scoped:
    scope: 'Scope'

    def __init__(self, scope: 'Scope') -> None:
        assert isinstance(scope, Scope)
        self.scope = scope

    def type(self, base: str) -> types.Type:
        return self.scope.type(f'{self.scope.name}_{base}')
    
    def var(self, base: str) -> str:
        return self.scope.var(f'{self.scope.name}_{base}')


class Var:
    type: types.Type
    name: str

    def __init__(self, type: types.Type, name: str):
        self.type = type
        self.name = name

    def __hash__(self):
        return hash(self.name)

    def __eq__(self, other: object) -> bool:
        return isinstance(other, Var) and self.name == other.name

    def __str__(self) -> str:
        return f'{self.type} {self.name}'

class Opcode(Scoped):
    suffix: str
    base: str
    args: list[Var]

    def __init__(self, scope: 'Scope', base: str, args: list[Var]) -> None:
        super().__init__(scope)
        self.suffix = ''
        self.base = base
        self.args = args

    def __eq__(self, other: object) -> bool:
        return isinstance(other, Opcode) and self.base == other.base and self.args == other.args
    
    def __hash__(self) -> int:
        return hash((self.base, *(arg.type for arg in self.args)))
    
    def __str__(self) -> str:
        return self.name

    @property
    def name_suffix(self) -> str:
        same_name: set[Opcode] = set()
        for opcode in self.scope.opcodes:
            if self.base == opcode.base:
                same_name.add(opcode)
        if len(same_name) == 1:
            return self.base
        
        by_arg_size: dict[int, set[Opcode]] = {}
        for opcode in same_name:
            size = len(opcode.args)
            if size not in by_arg_size:
                by_arg_size[size] = set()
            by_arg_size[size].add(opcode)
        
        for num_args, opcodes in by_arg_size.items():
            if len(opcodes) == 1:
                opcode.suffix = str(num_args)
            for id, opcode in enumerate(opcodes):
                if opcode.suffix == '':
                    opcode.suffix = f'{num_args}_v{id}'
        
        return f'{self.base}{self.suffix}'

    @property
    def name(self) -> str:
        return self.scope.var(f'op_{self.name_suffix}')

class Run:
    code: str

    def __init__(self, code: str) -> None:
        self.code = dedent(code)

    def __hash__(self) -> int:
        return hash(self.code)

    def __eq__(self, other: object) -> bool:
        return isinstance(other, Run) and self.code == other.code

    def __str__(self) -> str:
        return self.code

class Interp(Scoped):
    base: str
    ret: types.Type
    args: list[Var]
    body: dict[Opcode, tuple[list[Var], Run]]
    globals: list[str]
    enter: Run | None
    exit: Run | None

    def __init__(self, scope: 'Scope', ret: types.Type, base: str, args: list[Var]) -> None:
        super().__init__(scope)

        assert isinstance(ret, (types.Ptr, types.Name, types.Const))
        assert isinstance(base, str)
        assert isinstance(args, list)
        assert all(isinstance(arg, Var) for arg in args)
        
        self.base = base
        self.ret = ret
        self.args = args
        self.body = {}

        self.globals = []

        self.enter = None
        self.exit = None

    @property
    def name(self) -> str:
        return self.scope.var(self.base)

    def sig(self) -> str:
        return f'{self.ret} {self.name}'

    def __hash__(self) -> int:
        return hash(self.name)

    def __eq__(self, other: object) -> bool:
        return isinstance(other, Interp) and self.scope == other.scope and self.name == other.name

    def to(self, replaces: Replaces) -> None:
        params = [
            f'{self.scope.type('buffer')} buf',
            *(str(arg) for arg in self.args),
        ]

        cases = []
        for opcode, (args, run) in self.body.items():
            case = []
            case.append(f'case {opcode.name.upper()}: {{')
            for arg in args:
                case.append(f'    {arg.type} {arg.name} = next({arg.type});')
            case.append(indent(run.code))
            case.append(f'    break;')
            case.append(f'}}')
            cases.append('\n'.join(case))

        replaces.replace('interp.globals', '\n'.join(self.globals))
        replaces.replace('interp.bytecode.mem', 'buf.mem')
        replaces.replace('interp.bytecode.len', 'buf.len')
        
        replaces.replace('interp.ret', str(self.ret))
        replaces.replace('interp.name', self.name)
        replaces.replace('interp.params', ', '.join(params))
        if self.enter is None:
            replaces.replace('interp.enter', '')
        else:
            replaces.replace('interp.enter', self.enter.code)
        if self.exit is None:
            replaces.replace('interp.exit', '')
        else:
            replaces.replace('interp.exit', self.exit.code)
        replaces.replace('interp.index', f'{self.name}_index')

        replaces.replace('interp.cases', '\n'.join(cases))
    
class Class(Scoped):
    name: str
    base: types.Type
    cons: dict[types.Type, Run]
    parse: Run | None
    init: Run | None
    deinit: Run | None

    def __init__(self, scope: 'Scope', name: str, base: types.Type) -> None:
        super().__init__(scope)
        self.name = name
        self.base = base
        self.cons = {}
        self.parse = None
        self.init = None
        self.deinit = None

    def __hash__(self) -> int:
        return hash(self.name)
    
    def __eq__(self, other: object) -> bool:
        return isinstance(other, Class) and self.name == other.name

    def __str__(self) -> str:
        return NotImplemented
    
    def to_type(self) -> types.Type:
        return self.scope.type(self.name)

class Syntax(Scoped):
    base: str
    parsers: dict[types.Type, Run]

    def __init__(self, scope: 'Scope', base: str) -> None:
        super().__init__(scope)
        self.parsers = {}
        self.base = base

    @property
    def name(self) -> None:
        return self.scope.var(self.base)

    def to(self, replaces: Replaces) -> None:
        parsers = []
        
        for opcode in self.scope.opcodes:
            parser = []
            parser.append(f'if (argc == {len(opcode.args)} && !strcmp("{opcode.base}", name)) {{')
            args = ['out']
            for index, arg in enumerate(opcode.args):
                args.append(f'arg{index}')
                parser.append(indent(f'{arg.type} arg{index};'))
                reps = self.scope.user.copy()
                reps.replace('parse.out', f'arg{index}')
                reps.replace('parse.str', f'argv[{index}]')
                res = reps.exec(self.parsers[arg.type].code)
                parser.append(indent('{'))
                parser.append(indent(indent(res)))
                parser.append(indent('}'))
            builder = self.scope.var(f'buffer_{opcode.name_suffix}')
            args = ', '.join(args)
            parser.append(indent(f'{builder}({args});'))
            parser.append(indent(f'goto next;'))
            parser.append(f'}}')
            parsers.append('\n'.join(parser))

        replaces.replace('syntax.buffer.type', self.scope.var('buffer'))

        replaces.replace('syntax.name', self.name)
        replaces.replace('syntax.opcodes', '\n'.join(parsers))
        
        replaces.replace('syntax.buf.size', '128')
        replaces.replace('syntax.max.args', str(max(len(op.args) for op in self.scope.opcodes)))
        replaces.replace('syntax.buf.size', '128')

R = TypeVar('T')
A = TypeVarTuple('A')

def adder(func: Callable[['Scope'], R]) -> Callable[['Scope', *A], R]:
    cls = func.__annotations__['obj']
    def real(self, *args, **kwargs) -> R:
        obj = cls(self, *args, **kwargs)
        func(self, obj)
        return obj
    return real

class Scope:
    name: str
    lang: str

    opcodes: list[Opcode]
    classes: list[Class]
    interps: list[Interp]
    syntaxes: list[Syntax]
    
    context: list[Var]
    includes: list[str]
    globals: list[str]
    header: list[str]

    _user: Replaces | None

    def __init__(self, name: str, lang: str) -> None:
        self.name = name
        self.lang = lang

        self.opcodes = []
        self.classes = []
        self.interps = []
        self.syntaxes = []

        self.context = []
        self.includes = []
        self.globals = []
        self.header = []

        self._user = None

    @property
    def user(self):
        if self._user is None:
            user = Replaces()
            for cls in self.classes:
                user.replace(f'class.{cls.name}', str(cls.to_type()))

            self._user = user

        return self._user

    @adder
    def add_opcode(self, obj: Opcode) -> None:
        self.opcodes.append(obj)

    @adder
    def add_class(self, obj: Class) -> None:
        self.classes.append(obj)

    @adder
    def add_interp(self, obj: Interp) -> None:
        self.interps.append(obj)

    @adder
    def add_syntax(self, obj: Syntax) -> None:
        self.syntaxes.append(obj)

    def class_dict(self) -> dict[types.Type, Class]:
        return {cls.to_type(): cls for cls in self.classes}

    def check(self) -> None:
        want = set(self.class_dict().keys())

        for syntax in self.syntaxes:
            has = set(syntax.parsers.keys())
            extra = ', '.join(str(i) for i in has - want)
            missing = ', '.join(str(i) for i in want - has)
            if extra and missing:
                raise TypeError(f'parser missing {missing}; parser extra {extra}')
            if extra:
                raise TypeError(f'parser extra {extra}')
            if missing:
                raise TypeError(f'parser extra {missing}')

    def to(self, replaces: Replaces) -> None:
        self.check()

        context_type = self.type('contxt')
        buffer_type = self.type('buffer')
        opcode_type = self.type('opcode')

        syntax_decls = []
        for syntax in self.syntaxes:
            syntax_decls.append(f'bool {syntax.name}({context_type} *ctx, {buffer_type} *out, ptrdiff_t len, const char *str);')

        opcode_list = []
        for opcode in self.opcodes:
            opcode_list.append(f'{opcode.name.upper()},')

        class_decls = []
        for cls in self.classes:
            cls_name = cls.to_type()
            class_decls.append(f'typedef {cls.base} {cls_name};')

        builders = []
        for opcode in self.opcodes:
            func_name = self.var(f'buffer_{opcode.name_suffix}')
            params_list = [f'{buffer_type} *buffer']
            params_list.extend(f'{arg.type} {arg.name}' for arg in opcode.args)
            params = ', '.join(params_list)

            builder = []
            builder.append(f'static inline void {func_name}({params})' + '{')
            builder.append(f'    {self.opcode_type} op = {opcode.name.upper()};')
            builder.append(f'    {self.var('buffer_push')}(buffer, sizeof({self.opcode_type}), &op);')
            for arg in opcode.args:
                builder.append(f'    {self.var('buffer_push')}(buffer, sizeof({arg.type}), &{arg.name});')
            builder.append('}')
            builders.append('\n'.join(builder))

        interp_decls = []
        for interp in self.interps:
            ret = interp.ret
            arg_types = [str(arg.type) for arg in interp.args]
            arg_names = [arg.name for arg in interp.args]

            func_name = interp.name

            arg_types.insert(0, f'{context_type} *')
            arg_names.insert(0, 'context')

            arg_types.insert(1, buffer_type)
            arg_names.insert(1, 'buffer')

            arg_strs = ', '.join(f'{type} {name}' for type, name in zip(arg_types, arg_names))

            interp_decls.append(f'{ret} {func_name}({arg_strs});')

        includes = []
        includes.append(f'#include <vmdef/{self.name}.h>')

        context_fields = [f'{var.type} {var.name};' for var in self.context]

        if len(context_fields) == 0:
            context_fields.append('char empty_struct[1];')

        replaces.replace('scope.name', self.name)
        replaces.replace('scope.globals', '\n'.join(self.globals))
        replaces.replace('scope.header', '\n'.join(self.header))

        replaces.replace('scope.context.type', str(context_type))
        replaces.replace('scope.context.fields', '\n'.join(context_fields))

        replaces.replace('scope.buffer.type', str(buffer_type))
        replaces.replace('scope.buffer.new', self.var('buffer_new'))
        replaces.replace('scope.buffer.free', self.var('buffer_free'))
        replaces.replace('scope.buffer.push', self.var('buffer_push'))
        
        replaces.replace('scope.builders', '\n\n'.join(builders))
        replaces.replace('scope.syntax.decls', '\n'.join(syntax_decls))
        replaces.replace('scope.class.decls', '\n'.join(class_decls))
        
        replaces.replace('scope.opcodes.list', '\n'.join(opcode_list))
        replaces.replace('scope.interp.decls', '\n'.join(interp_decls))

        replaces.replace('scope.opcode.type', self.opcode_type)
        replaces.replace('scope.includes', '\n'.join(includes))

    @property
    def opcode_type(self) -> str:
        if len(self.opcodes) <= 2**8:
            return 'uint8_t'
        elif len(self.opcodes) <= 2**16:
            return 'uint16_t'
        elif len(self.opcodes) <= 2**32:
            return 'uint32_t'
        else:
            return 'uint64_t'

    def type(self, base: str) -> types.Type:
        assert isinstance(base, str)

        if base in C_TYPES:
            return types.Name(base)

        return types.Name(f'{self.name}_{base}_t')

    def var(self, base: str) -> str:
        return f'{self.name}_{base}'
