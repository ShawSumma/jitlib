
from dataclasses import dataclass
from pathlib import Path

@dataclass(frozen = True)
class OutFile:
    path: Path
    text: str

@dataclass
class OutFiles:
    files: set[OutFile]

    def add(self, file: OutFile):
        self.files.add(file)
