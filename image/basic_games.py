"""Prepare the pinned 101 BASIC listings and the CENTOS-compatible ports."""
from pathlib import Path
import re

UPSTREAM = Path(__file__).resolve().parents[1] / 'swext/101-basic-games'
ARCHIVE = 'BAS101'

# Work around differences in the original dialects and a few listing typos.
# Line numbers refer to the pinned upstream listing.
PORTS = {
    '23mtch': {},
    'chemst': {},
    'chief': {},
    'dice': {},
    'hi-lo': {180: 'Y=INT(100*RND)+1', 280: 'IF B<6 THEN 200'},
    'hurkle': {
        240: 'PRINT "PAIR OF WHOLE NUMBERS ENTERED SEPARATELY. TRY TO"',
        310: 'K=1',
        330: 'INPUT X:INPUT Y',
        380: 'K=K+1:IF K<=N THEN 320',
        530: 'PRINT CHR$(7)',
    },
    'letter': {430: 'INPUT A$:IF LEN(A$)=0 THEN 410',
               435: 'A=ASC(A$):IF A>=97 AND A<=122 THEN A=A-32',
               436: 'PRINT', 510: 'PRINT CHR$(7);'},
    'nicoma': {90: 'REM'},
    'rocksp': {
        8: 'IF Q>=1 THEN 11',
        40: 'IF X=1 THEN 50',
        41: 'IF X=2 THEN 60',
        42: 'GOTO 70',
        105: 'IF K=3 THEN 115',
        106: 'GOTO 95',
        140: 'IF X<>3 THEN 115',
        141: 'GOTO 95',
        195: 'PRINT "AND";Q-(C+H);"GAME(S) ENDED IN A TIE."',
    },
    'stars': {330: 'K=1', 530: 'K=K+1:IF K<=M THEN 340'},
    'train': {41: 'IF A<=0 THEN 35', 50: 'E=INT(ABS((V-A)*100)/A+.5)'},
}

QUOTED = re.compile(r'("[^"]*")')


def centos_name(stem):
    name = stem.upper().replace('-', '')
    if name[0].isdigit():
        name = 'G' + name
    if not re.fullmatch(r'[A-Z][A-Z0-9]{0,9}', name):
        raise ValueError(f'Cannot give {stem} a native CENTOS filename')
    return name


def print_separators(statement):
    """Make separators explicit for a DEC PRINT with adjacent strings."""
    fields = QUOTED.split(statement[5:].lstrip())
    for index in range(0, len(fields), 2):
        field = fields[index].strip()
        if field.strip(' ;,') and '$' not in field:
            # DEC's numeric output included padding. Preserve word boundaries
            # when Stefan BASIC renders a number between literal strings.
            if index and field[0] != ',' and fields[index - 1][-2:-1].isalnum():
                fields[index - 1] = fields[index - 1][:-1] + ' "'
            if (index + 1 < len(fields) and field[-1] != ','
                    and fields[index + 1][1:2].isalnum()):
                fields[index + 1] = '" ' + fields[index + 1][1:]
        if index and re.match(r'[A-Z0-9.(]', field, re.I):
            field = ';' + field
        if index + 1 < len(fields) and re.search(r'[A-Z0-9)$]$', field, re.I):
            field += ';'
        fields[index] = field
    return 'PRINT ' + ''.join(fields)


def normalize(body):
    if re.match(r'^REM\b', body):
        return body
    # Preserve quoted prompts exactly, including backslashes and punctuation.
    fragments = QUOTED.split(body)
    for index in range(0, len(fragments), 2):
        code = fragments[index]
        code = re.sub(r'\s+//.*$', '', code)
        code = re.sub(r'\bGO\s+TO\b', 'GOTO', code)
        code = re.sub(r'\bRND(?:\(0\)|(?!\s*\())', 'RND(1)', code)
        fragments[index] = code.replace('\\', ':')
    statements = []
    # Split on statement separators outside strings.
    statement = ''
    for index, fragment in enumerate(fragments):
        if index % 2:
            statement += fragment
        else:
            parts = fragment.split(':')
            if len(parts) > 1:
                statements.extend([statement + parts[0], *parts[1:-1]])
                statement = parts[-1]
            else:
                statement += fragment
    statements.append(statement)
    result = []
    for statement in statements:
        statement = statement.strip()
        if statement in ('RANDOM', 'RANDOMIZE'):
            # The target uses the BASIC session's existing random state.
            statement = 'REM'
        if match := re.fullmatch(r'INPUT\s+("[^"]*")\s*;\s*([A-Z]\$?)', statement):
            statement = f'PRINT {match[1]};:INPUT {match[2]}'
        if match := re.fullmatch(r'PRINT TAB\((\w+)\)\s*("[^"]*")', statement):
            # Start these unadorned lines at the original print column on VT100.
            statement = f'FOR T=1 TO {match[1]}:PRINT " ";:NEXT T:PRINT {match[2]}'
        elif statement == 'PRINT' or statement.startswith('PRINT '):
            statement = print_separators(statement).rstrip()
        result.append(statement)
    return ':'.join(result)


def port(source, edits):
    lines = {}
    for line in source.splitlines():
        if not line.strip():
            continue
        number, body = line.split(maxsplit=1)
        number = int(number)
        if number in lines:
            raise ValueError(f'Duplicate BASIC line {number}')
        lines[number] = body
    lines.update(edits)
    return ''.join(f'{number} {normalize(lines[number])}\n' for number in sorted(lines))


def prepare_games(work):
    sources = sorted(UPSTREAM.glob('*.bas'))
    if not sources:
        raise ValueError('Initialize the 101 BASIC Computer Games submodule first')
    names = [centos_name(source.stem) for source in sources]
    if len(set(names)) != len(names):
        raise ValueError('Duplicate native filename in the 101 BASIC catalog')
    missing = set(PORTS) - {source.stem for source in sources}
    if missing:
        raise ValueError('Missing BASIC source for ' + ', '.join(sorted(missing)))
    originals, playable = {}, {}
    for name, source in zip(names, sources):
        listing = source.read_text(encoding='ascii')
        originals[name] = listing
        if source.stem in PORTS:
            path = work / f'{name}.bas'
            path.write_text(port(listing, PORTS[source.stem]), encoding='ascii')
            playable[name] = path
    return originals, playable
