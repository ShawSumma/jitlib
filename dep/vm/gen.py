
from lark import Lark, Tree
from dataclasses import dataclass
import sys
import re

def indent(text, tab = ' ' * 4):
    return '\n'.join(tab + line for line in text.split('\n'))

def dedent(text):
    return '\n'.join(line.strip() for line in text.split('\n'))

@dataclass
class Arg:
    raw: str
    name: str
    type: str

@dataclass
class Op:
    key: tuple[str, ...]
    args: list[Arg]
    impl: str

class Emit:
    def opname(self, op):
        return f'{self.scope_name}_OP_{self.func(op)}'.upper()
    
    def func(self, op):
        if len(op.key) == 1:
            return op.key[0]
        else:
            args = ''.join(op.key[1:])
            return f'{op.key[0]}_{args}'

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
                    txt.append(indent(op.impl))
                    txt.append('\n} break;\n\n')
                return indent(''.join(txt).strip(), ind) + '\n'

            case ['globals']:
                return '\n'.join(self.globals)

            case ['includes']:
                txt = f'''
                    
                    #include <vmdef/{self.scope_name}.h>

                '''

                return indent(dedent(txt.strip()), ind)
                
            case _:
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
        self.scopes[f'src/vmdef/{name}.dasc'] = self.template('dep/vm/template.dasc')
        self.scopes[f'include/vmdef/{name}.h'] = self.template('dep/vm/template.h')

    def emit(self, tree):
        self.scopes = {}
        for scope in tree.children:
            self.on_scope(scope)
        return self.scopes
    
    def on_arg(self, arg):
        [raw, name, type] = arg.children
        match str(raw):
            case '$':
                raw = 'a'
            case '#':
                raw = 'n'
            case _:
                raw = str(raw)
        return Arg(raw, str(name), str(type))

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
        d = self.on_block(block)
        self.ops_def[key] = Op(key, args, d)

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
        with open(file, 'w') as f:
            f.write(files[file])

if __name__ == '__main__':
    main()