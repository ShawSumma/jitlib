
import gen.ir as ir
import gen.types as types
import gen.fs as fs
from gen.text import indent, dedent
from gen.template import Replaces
from lark import Tree

def on_block_body(tree: Tree) -> str:
    ret = []
    for child in tree.children:
        if isinstance(child, Tree):
            ret.append('{\n')
            ret.append(indent(on_block_body(child)))
            ret.append('\n}')
        else:
            ret.append('\n')
            ret.append(dedent(child))
            ret.append('\n')
    return ''.join(ret).strip()

def on_block(tree: Tree) -> str:
    return ir.Run(on_block_body(tree))

def class_add_when(thing: ir.Class, tree: Tree) -> None:
    assert tree.data == 'when'

    [type, block] = tree.children

    type = str(type)

    match type:
        case 'enter':
            thing.init = on_block(block)
        case 'exit':
            thing.deinit = on_block(block)
        case _:
            raise Exception(f'{type=}')

def class_add_parse(thing: ir.Class, tree: Tree) -> None:
    assert tree.data == 'parse'
    assert thing.parse is None

    thing.parse = on_block(tree.children[0])

def scope_add_class(scope: ir.Scope, tree: Tree) -> None:
    assert tree.data == 'class'

    [name, base, *parts] = tree.children
    name = str(name)
    base = conv_type(base, scope)

    thing = scope.add_class(name, base)

    for part in parts:
        match part.data:
            case 'when':
                class_add_when(thing, part)
            case 'parse':
                class_add_parse(thing, part)
            case _:
                raise Exception(f'{part.data=}')

def type_add_suffix(type: types.Type, *rest: list[str]) -> None:
    for part in rest:
        match part:
            case '*':
                type = types.Ptr(type)
            case 'const':
                type = types.Const(type)
            case _:
                raise Exception(f'internal error, type: {part}')
    return type

def conv_type(tree: Tree, scope: ir.Scope) -> types.Type:
    parts = [str(child) for child in tree.children]
    match parts:
        case ['const', name, *rest]:
            return type_add_suffix(scope.type(name), 'const', *rest)
        case [name, *rest]:
            return type_add_suffix(scope.type(name), *rest)
        case _:
            text = ' '.join(parts)
            raise Exception(f'internal error, type: {text}')

def conv_arg(tree: Tree, scope: ir.Scope) -> ir.Var:
    [type, name] = tree.children
    type = conv_type(type, scope)
    name = str(name)

    return ir.Var(type, name)

def scope_add_op(scope: ir.Scope, tree: Tree) -> None:
    assert tree.data == 'op'

    [name, args, *impls] = tree.children
    name = str(name)
    args = [conv_arg(arg, scope) for arg in args.children]

    opcode = scope.add_opcode(name, args)

def interp_add_when(interp: ir.Interp, tree: Tree) -> None:
    [op, run] = tree.children

    match str(op):
        case 'enter':
            interp.enter = on_block(run)
        case 'exit':
            interp.exit = on_block(run)
        case _:
            raise NotImplementedError()

def interp_add_run(interp: ir.Interp, tree: Tree) -> None:
    [name, args, block] = tree.children
    name = str(name)
    args = [conv_arg(arg, interp.scope) for arg in args.children]
    block = on_block(block)
    arg_types = [arg.type for arg in args]

    for opcode in interp.scope.opcodes:
        if name != opcode.base:
            continue
        if arg_types != [arg.type for arg in opcode.args]:
            continue
        interp.body[opcode] = (args, block)
        return
    
    raise NotImplementedError()

def scope_add_interp(scope: ir.Scope, tree: Tree) -> None:
    assert tree.data == 'interp'

    [ret, name, args, lang, *parts] = tree.children

    ret = conv_type(ret, scope)
    name = str(name)
    args = [conv_arg(arg, scope) for arg in args.children]
    lang = str(lang)

    interp = scope.add_interp(ret, name, args, lang)

    for part in parts:
        match part.data:
            case 'when':
                interp_add_when(interp, part)
            case 'run':
                interp_add_run(interp, part)
            case _:
                raise NotImplementedError(f'{part.data=}')

def syntax_add_parser(syntax: ir.Syntax, tree: Tree) -> None:
    [type, block] = tree.children
    type = conv_type(type, syntax.scope)
    block = on_block(block)
    
    if type in syntax.parsers:
        raise Exception(f'duplicate in parser {syntax.name}: {type}')

    syntax.parsers[type] = block

def scope_add_syntax(scope: ir.Scope, tree: Tree) -> None:
    assert tree.data == 'syntax'

    [name, *parsers] = tree.children
    name = str(name)
    syntax = scope.add_syntax(name)

    for parser in parsers:
        syntax_add_parser(syntax, parser)

def conv_scope(tree: Tree) -> ir.Scope:
    assert tree.data == 'scope'
    
    name, *parts = tree.children
    name = str(name)
    
    scope = ir.Scope(name)

    for part in parts:
        if part.data == 'class':
            scope_add_class(scope, part)

    for part in parts:
        if part.data == 'op':
            scope_add_op(scope, part)

    for part in parts:
        if part.data == 'interp':
            scope_add_interp(scope, part)

    for part in parts:
        if part.data == 'syntax':
            scope_add_syntax(scope, part)
    
    return scope

def conv_start(tree: Tree) -> None:
    assert tree.data == 'start'
    
    for obj in tree.children:
        scope = conv_scope(obj)

        replaces = Replaces()

        scope.to(replaces)

        with fs.open(f'syntax.c') as in_file:
            syntax_base = in_file.read()
        with fs.open(f'scope.h') as in_file:
            header_base = in_file.read()

        with open(f'include/vmdef/{scope.name}.h', 'w') as out_file:
            out_file.write(replaces.exec(header_base))
        
        for syntax in scope.syntaxes:
            with open(f'src/vmdef/{syntax.name}.c', 'w') as out_file:
                syntax.to(replaces)
                out_file.write(replaces.exec(syntax_base))

        for interp in scope.interps:
            with fs.open(f'base/interp.{interp.lang}') as in_file:
                interp_base = in_file.read()
            
            with open(f'src/vmdef/{interp.name}.{interp.lang}', 'w') as out_file:
                interp.to(replaces)
                out_file.write(replaces.exec(interp_base))
