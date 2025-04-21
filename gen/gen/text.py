
TAB = ' ' * 4

def indent(text, tab = TAB):
    return '\n'.join(tab + line for line in text.split('\n'))

def dedent(text):
    return '\n'.join(line.strip() for line in text.split('\n'))
