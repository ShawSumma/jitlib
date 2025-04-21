
from lark import Tree as Tree
from typing import TypeAlias, Union
from re import compile

REGEX_UNDER_CAPS = compile('_[A-Z]')
REGEX_CNAME = compile('^[a-zA-Z_][a-zA-Z_0-9]*$')

Type: TypeAlias = Union['Const', 'Name', 'Ptr']

class Auto:
    def __hash__(self) -> int:
        return hash(str(self))

    def __eq__(self, other: object) -> bool:
        return isinstance(self, Auto) and str(self) == str(other)

class Const(Auto):
    base: Type

    def __init__(self, base: Type) -> None:
        self.base = base

    def __str__(self) -> str:
        return f'{self.base} const'
    
class Name(Auto):
    name: str

    def __init__(self, name: str) -> None:
        self.name = name

        if '__' in self.name:
            raise NameError(f'The name `{self.name}` is invalid in C.')
        found = REGEX_UNDER_CAPS.match(self.name)
        if found is not None:
            raise NameError(f'The name `{self.name}` is invalid in C.')
        if REGEX_CNAME.match(self.name) is None:
            raise NameError(f'The name `{self.name}` is invalid in C.')
        self.name = name

    def __str__(self) -> str:
        return self.name

class Ptr(Auto):
    base: Type

    def __init__(self, base: Type) -> None:
        self.base = base

    def __str__(self) -> str:
        return f'{self.base} *'

def _extend_type(base: Type, *exts: list[str]) -> Type:
    match exts:
        case []:
            return base
        case ['*', *rest]:
            return _extend_type(Ptr(base), *rest)
        case ['const', *rest]:
            return _extend_type(Const(base), *rest)
        case _:
            text = ' '.join(exts)
            raise Exception(f'weird type part: {text}')

def convert(tree: Tree) -> Type:
    assert isinstance(tree, Tree)
    assert tree.data == 'type'

    match all := [str(i) for i in tree.children]:
        case ['const', name, *exts] | [name, 'const', *exts]:
            return _extend_type(Const(Name(name)), *exts)
        case [name, *exts]:
            return _extend_type(Name(name), *exts)
        case _:
            text = ' '.join(all)
            raise Exception(f'weird type: {text}')
