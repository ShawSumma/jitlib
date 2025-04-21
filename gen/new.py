
from lark import Lark
import gen.conv as conv
import sys

def main():
    parser = Lark.open("data/vmdef.lark", rel_to=__file__)

    with open(sys.argv[1]) as f:
        ast = parser.parse(f.read())

    conv.conv_start(ast)

if __name__ == '__main__':
    main()
