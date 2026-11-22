"""Prepare the tested CENTOS ports from the pinned 101 BASIC listings."""
from pathlib import Path
import re

UPSTREAM = Path(__file__).resolve().parents[1] / 'swext/101-basic-games'
BLANKS = '"' + ' ' * 80 + '"'

# Work around differences in the original dialects and a few listing typos.
# Line numbers refer to the pinned upstream listing.
PORTS = {
    '23mtch': {},
    '3dplot': {
        5: 'REM',
        130: 'W=5*INT(SQR(900-X*X)/5)',
        140: 'FOR Y=W TO -W STEP -5',
        150: 'P=SQR(X*X+Y*Y):Z=INT(25+30*EXP(-P*P/100)-.7*Y)',
        170: f'PRINT LEFT$({BLANKS},Z-L-1);"*";',
        180: 'L=Z',
    },
    'aceydu': {},
    'change': {
        20: 'C=P-A:M=C:IF C<>0 THEN 90',
        21: 'PRINT "CORRECT AMOUNT, THANK YOU"',
        90: 'IF C>0 THEN 120',
        91: 'PRINT "SORRY, YOU HAVE SHORT CHANGED ME $";A-P',
    },
    'chemst': {},
    'chief': {},
    'chomp': {
        90: 'DEFINT A,I,J,N,T',
        95: 'DIM A(99)',
        350: 'REM',
        375: 'A((I-1)*10+J-1)=0',
        400: 'INPUT P:IF P<1 OR P<>INT(P) THEN 390',
        440: 'IF R>=1 AND R<=9 AND R=INT(R) THEN 470',
        490: 'IF C>=1 AND C<=9 AND C=INT(C) THEN 530',
        560: 'A((I-1)*10+J-1)=1',
        590: 'A(0)=-1',
        620: 'PRINT "       1 2 3 4 5 6 7 8 9"',
        640: 'PRINT I;"      ";',
        660: 'IF A((I-1)*10+J-1)=-1 THEN 700',
        670: 'IF A((I-1)*10+J-1)=0 THEN 710',
        830: 'PRINT "COORDINATES OF CHOMP (ROW THEN COLUMN)";',
        840: 'INPUT R1:INPUT C1',
        845: 'IF R1<>INT(R1) OR C1<>INT(C1) THEN 920',
        890: 'IF A((R1-1)*10+C1-1)=0 THEN 920',
        900: 'IF A((R1-1)*10+C1-1)=-1 THEN 1010',
        960: 'A((I-1)*10+J-1)=0',
    },
    'diamnd': {
        1: 'DEFINT A,C,L,M,N,Q,X,Y,Z',
        5: 'INPUT R:PRINT:IF R<5 OR R>31 OR R MOD 2<>1 THEN 3',
        25: f'PRINT LEFT$({BLANKS},(R-N)/2);',
        53: 'IF M=Q THEN 56',
        55: f'PRINT LEFT$({BLANKS},R-N);',
    },
    'dice': {},
    'even': {20: 'REM'},
    'hi-lo': {180: 'Y=INT(100*RND)+1', 280: 'IF B<6 THEN 200'},
    'hmrabi': {
        228: 'P=INT(P/2)',
        235: 'PRINT "YOU HARVESTED";Y;"BUSHELS PER ACRE."',
        270: 'IF Z=11 THEN 860',
        450: 'IF INT(D/2)<=S THEN 459',
        451: 'REM',
        459: 'IF D<=10*P THEN 510',
        990: 'PRINT:PRINT CHR$(7);',
    },
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
    'kinema': {110: 'V=5+INT(35*RND(1))', 508: 'PRINT "CORRECT! VERY GOOD!"'},
    'litqz': {},
    'mathd': {},
    'nicoma': {90: 'REM'},
    'number': {
        15: 'R=INT(5*RND(1))+1', 16: 'S=INT(5*RND(1))+1',
        17: 'T=INT(5*RND(1))+1', 18: 'U=INT(5*RND(1))+1',
        19: 'V=INT(5*RND(1))+1',
    },
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
    'rusrou': {40: 'IF RND(1)>.83333 THEN 70'},
    'stars': {330: 'K=1', 530: 'K=K+1:IF K<=M THEN 340'},
    'train': {41: 'IF A<=0 THEN 35', 50: 'E=INT(ABS((V-A)*100)/A+.5)'},
    'trap': {105: 'PRINT "ENTER THE TWO NUMBERS AT SEPARATE PROMPTS."',
             190: 'Q=1', 220: 'INPUT A:INPUT B', 330: 'Q=Q+1:IF Q<=G THEN 200'},
}

QUOTED = re.compile(r'("[^"]*")')
VARIABLES = {
    'chomp': {'I1': 'T', 'P1': 'N', 'R1': 'X', 'C1': 'Y'},
    'even': {'A1': 'A', 'M1': 'B', 'Y1': 'D'},
    'hmrabi': {'D1': 'F', 'P1': 'G'},
    'mathd': {'T1': 'B', 'T2': 'C'},
}
NUMBERED_VARIABLE = re.compile(r'(?<![.A-Z0-9])[A-Z][0-9]+\b')


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


def normalize(body, aliases=None):
    if re.match(r'^REM\b', body):
        return body
    # Preserve quoted prompts exactly, including backslashes and punctuation.
    fragments = QUOTED.split(body)
    for index in range(0, len(fragments), 2):
        code = fragments[index]
        if aliases:
            code = NUMBERED_VARIABLE.sub(lambda match: aliases.get(match[0], match[0]), code)
        if index + 1 < len(fragments):
            code = re.sub(r'\bPRINT\s*$', 'PRINT ', code)
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


def port(source, edits, aliases=None):
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
    return ''.join(f'{number} {normalize(lines[number], aliases)}\n' for number in sorted(lines))


def prepare_games(work):
    if not UPSTREAM.is_dir():
        raise ValueError('Initialize the 101 BASIC Computer Games submodule first')
    if len({centos_name(stem) for stem in PORTS}) != len(PORTS):
        raise ValueError('Duplicate native filename in the 101 BASIC catalog')
    playable = {}
    for stem, edits in sorted(PORTS.items()):
        source = UPSTREAM / f'{stem}.bas'
        name = centos_name(stem)
        listing = source.read_text(encoding='ascii')
        path = work / f'{name}.bas'
        path.write_text(port(listing, edits, VARIABLES.get(stem)), encoding='ascii')
        playable[name] = path
    return playable
