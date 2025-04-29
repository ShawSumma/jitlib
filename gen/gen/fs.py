
from typing import TextIO, Literal
from builtins import open as py_open
from enum import Enum
import os

class Base(Enum):
    NONE = '%s'
    INCLUDE = 'include/vmdef/%s'
    SRC = 'src/vmdef/%s'

class Data:
    @staticmethod
    def open(path: str, mode: str = 'r') -> TextIO:
        text = os.path.realpath(__file__)
        text = os.path.dirname(text)
        text = os.path.dirname(text)
        text = os.path.join(text, 'data', path)
        return py_open(text, mode, encoding = 'utf-8')

    @staticmethod
    def read(path: str) -> str:
        with Data.open(path, 'r') as f:
            return f.read()

    @staticmethod
    def write(*, file: str, data: str, to: Literal['src', 'include']) -> str:
        match to:
            case 'src':
                path = os.path.join('src/vmdef', file)
            case 'include':
                path = os.path.join('include/vmdef', file)
            case _:
                raise ValueError(f'{to=}')
        with py_open(path, 'w') as f:
            return f.write(data)
