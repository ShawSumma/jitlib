
import gen.fs as fs
from typing import Sequence
import re

def is_empty(text: str) -> bool:
    text = text.strip()
    return text == '' or text[0] == '#'

def expand_lines(ls: Sequence[str]) -> list[str]:
    check = re.compile('\\([^()]*\\)')
    
    done = []

    parts = [item.strip() for item in ls if not is_empty(item)]

    while len(parts) != 0:
        part = parts.pop()

        ls = check.findall(part)

        if len(ls) == 0:
            done.append(part.strip())
        
        for match in ls:
            for item in match[1:-1].split('|'):
                parts.append(part.replace(match, item))
    
    return set(done)

with fs.open('ctypes.txt') as file:
    C_TYPES = expand_lines(file.readlines())
