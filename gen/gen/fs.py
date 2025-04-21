
from typing import Any
import os
from builtins import open as py_open

def open(path: str, mode: str = 'r') -> Any:
    text = os.path.realpath(__file__)
    text = os.path.dirname(text)
    text = os.path.dirname(text)
    text = os.path.join(text, 'data', path)
    return py_open(text, mode, encoding = 'utf-8')
