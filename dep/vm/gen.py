
from lark import Lark, Tree
from dataclasses import dataclass
import sys
import re
import os

def indent(text, tab = ' ' * 4):
    return '\n'.join(tab + line for line in text.split('\n'))

def dedent(text):
    return '\n'.join(line.strip() for line in text.split('\n'))

@dataclass
class Arg:
    raw: str
    chr: str
    name: str
    type: str

@dataclass
class Op:
    name: str
    args: list[Arg]
    impl: str

class Emit:
    def __init__(self):
        self.parsers = {}
        self.replaces = {}

    def opname(self, op):
        return f'{self.scope_name}_OP_{self.func(op)}'.upper()
    
    def func(self, op):
        if len(op.args) == 0:
            return op.name
        else:
            args = ''.join(arg.raw for arg in op.args)
            return f'{op.name}_{args}'

    def reset(self):
        self.scope_name = None
        self.pre_txt = ''
        self.ops_def = {}
        self.post_txt = ''
        self.globals = []

    def replacer(self, mat):
        ind = mat.group(1)
        name = mat.group(2)
        match name.split('.'):
            case ['scope']:
                return ind + self.scope_name
            
            case ['ops']:
                names = [self.opname(op) + ',' for op in self.ops_def.values()]
                return indent('\n'.join(names), ind)
            
            case ['builders']:
                funcs = []
                for op in self.ops_def.values():
                    funcs.append(f'static inline void {self.scope_name}_{self.func(op)}(')
                    funcs.append(f'{self.scope_name}_buf_t *buf_')
                    funcs.append(''.join(f', {arg.type} {arg.name}' for arg in op.args))
                    funcs.append(') {\n')
                    funcs.append(f'    {self.scope_name}_opcode_t op_ = {self.opname(op)};\n')
                    funcs.append(f'    {self.scope_name}_buf_push(buf_, sizeof({self.scope_name}_opcode_t), &op_);\n')
                    for arg in op.args:
                        funcs.append(f'    {self.scope_name}_buf_push(buf_, sizeof({arg.type}), &{arg.name});\n')
                    funcs.append('}\n\n')
                return indent(''.join(funcs), ind).strip()
            
            case ['pre']:
                return indent(''.join(self.pre_txt), ind)

            case ['post']:
                return indent(''.join(self.post_txt), ind)

            case ['next']:
                return ind + f'{self.scope_name}_next'

            case ['cases']:
                txt = []
                for op in self.ops_def.values():
                    txt.append('case ')
                    txt.append(self.opname(op))
                    txt.append(': {\n')
                    for arg in op.args:
                        txt.append(f'    {arg.type} {arg.name} = {self.scope_name}_next({arg.type});\n')
                    txt.append(self.replace(indent(op.impl)))
                    txt.append('\n} break;\n\n')
                return indent(''.join(txt).strip(), ind) + '\n'

            case ['globals']:
                return self.replace('\n'.join(self.globals))

            case ['includes']:
                txt = f'''
                    
                    #include <vmdef/{self.scope_name}.h>

                '''

                return indent(dedent(txt.strip()), ind)
            
            case ['parsers']:
                res = []

                max_name = 1 + max(len(op.name) for op in self.ops_def.values())

                res.append(f'char op_name[{max_name}];\n')
                res.append(f'read_name({max_name}, &op_name[0], &str);\n')

                for op in self.ops_def.values():
                    res.append(f'if (!strcmp(op_name, "{op.name}"))')
                    res.append(' {\n')
                    for arg in op.args:
                        res.append(f'    {arg.type} val_{arg.name};\n\n')
                    res.append(f'    const char *base = str;\n')
                    for arg in op.args:
                        res.append(f'    read_arg_t arg_{arg.name} = read_arg(base);\n')
                        res.append(f'    if(arg_{arg.name}.type != \'{arg.chr}\' || arg_{arg.name}.base == NULL) goto not_{self.func(op)};\n')
                        res.append(f'    base = arg_{arg.name}.next;\n')
                    self.replaces['fail'] = f'not_{self.func(op)}'
                    for arg in op.args:
                        res.append('    {\n')
                        self.replaces['str'] = f'arg_{arg.name}.base'
                        self.replaces['out'] = f'val_{arg.name}'
                        res.append(
                            self.replace(
                                indent(indent(self.parsers[arg.type]))
                            )
                        )
                        res.append('\n')
                        res.append('    }\n')
                    del self.replaces['fail']
                    args = ['buf']
                    for arg in op.args:
                        args.append(f'val_{arg.name}')
                    args = ', '.join(args)
                    res.append('\n')
                    res.append(f'    interp_{self.func(op)}({args});\n')
                    if len(op.args) == 0:
                        res.append('    goto next;\n')
                    else:
                        res.append('    if (read_done(& base)) {\n')
                        res.append('        str = base;\n')
                        res.append('        goto next;\n')
                        res.append('    }\n')
                    res.append('}\n')
                    res.append(f'not_{self.func(op)}:;\n')

                return indent(''.join(res), ind)
            
            case _:
                if name in self.replaces:
                    return ind + self.replaces[name]
                
                return ind + f'__{name}__'

    def replace(self, txt):
        return re.sub('( *)\\$([a-z.]+)', self.replacer, txt)

    def template(self, path):
        with open(path) as f:
            txt = f.read()

        return self.replace(txt)
    
    def on_scope(self, tree):
        self.reset()
        [name, *tops] = tree.children
        name = str(name)
        self.scope_name  = name
        for top in tops:
            match top.data:
                case 'pre':
                    self.on_pre(top)
                case 'post':
                    self.on_post(top)
                case 'global':
                    self.on_global(top)
                case 'op':
                    self.on_op(top)
        self.scopes[f'src/vmdef/{name}/run.dasc'] = self.template('dep/vm/run.template.dasc')
        self.scopes[f'src/vmdef/{name}/asm.c'] = self.template('dep/vm/asm.template.c')
        self.scopes[f'include/vmdef/{name}.h'] = self.template('dep/vm/template.h')

    def emit(self, tree):
        self.scopes = {}
        for elem in tree.children:
            match elem.data:
                case 'scope':
                    self.on_scope(elem)
                case 'parse':
                    self.on_parse(elem)
        return self.scopes
    
    def on_parse(self, tree):
        [name, block] = tree.children
        name = str(name)
        self.parsers[name] = self.on_block(block)

    def on_arg(self, arg):
        [raw, name, type] = arg.children
        base = str(raw)
        match base:
            case '$':
                raw = 'a'
            case '#':
                raw = 'n'
            case _:
                raw = str(raw)
        return Arg(raw, base, str(name), str(type))

    def on_block_body(self, tree):
        ret = []
        for child in tree.children:
            if isinstance(child, Tree):
                ret.append('{\n')
                ret.append(indent(self.on_block_body(child)))
                ret.append('\n}')
            else:
                ret.append('\n')
                ret.append(dedent(child))
                ret.append('\n')
        return ''.join(ret).strip()
    
    def on_block(self, tree):
        return self.on_block_body(tree)

    def on_op(self, tree):
        [name, *args, block] = tree.children
        name = str(name)
        args = [self.on_arg(arg) for arg in args]
        key = (name, *(arg.raw for arg in args))
        assert key not in self.ops_def, f"already defined: {name}"
        d = self.on_block(block)
        self.ops_def[key] = Op(name, args, d)

    def on_pre(self, tree):
        self.pre_txt = self.on_block(tree.children[0])

    def on_post(self, tree):
        self.post_txt = self.on_block(tree.children[0])

    def on_global(self, tree):
        self.globals.append(self.on_block(tree.children[0]))

def main():
    parser = Lark.open("vm.lark", rel_to=__file__)

    with open(sys.argv[1]) as f:
        ast = parser.parse(f.read())

    files = Emit().emit(ast)
    for file in files:
        os.makedirs(os.path.dirname(file), exist_ok=True)
        with open(file, 'w') as f:
            f.write(files[file])

if __name__ == '__main__':
    main()