
from string import ascii_lowercase, digits

class Replace:
    sections: list[str]
    output: str

    def __init__(self, name: str, output: str) -> None:
        self.sections = name.split('.')
        self.output = output
        self.check()
    
    def __eq__(self, other: object) -> bool:
        return isinstance(other, Replace) and self.name == other.name
    
    def __hash__(self) -> int:
        return hash(self.name)

    def check(self) -> None:
        assert isinstance(self.output, str)
        assert isinstance(self.sections, list)
        assert all(isinstance(section, str) for section in self.sections)
        assert self.name != self.output

        for section in self.sections:
            for char in section:
                if char not in ascii_lowercase and char not in digits:
                    raise ValueError(f'invalid name: {self.name}')
            if len(section) == 0:
                raise ValueError(f'invalid name: {self.name}')

    @property
    def name(self) -> str:
        return '$' + '.'.join(self.sections)
    
    def has(self, text: str) -> bool:
        return self.name in text

    def exec(self, text: str) -> str:
        assert isinstance(text, str)
        return text.replace(self.name, self.output)

class Replaces:
    values: dict[str, Replace]

    def __init__(self) -> None:
        self.values = {}

    def replace(self, name: str, to: str) -> None:
        assert isinstance(name, str)
        assert isinstance(to, str)

        replace = Replace(name, to)

        self.values[replace.name] = replace

    def copy(self) -> 'Replaces':
        ret = Replaces()
        for key, value in self.values.items():
            ret.values[key] = value
        return ret

    def has(self, text: str) -> bool:
        for value in self.values.values():
            if value.has(text):
                return True
        return False

    def exec(self, text: str) -> str:
        for _ in range(8):
            if not self.has(text):
                return text
            for rep in sorted(self.values.values(), key = lambda x: -len(x.name)):
                text = rep.exec(text)
        raise Exception('recursive replace')
