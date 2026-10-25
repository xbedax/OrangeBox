"""Export/validate the stable firmware log dictionary and current call sites.

Run: python scripts/export_log_messages.py [--check]
Message numbers come only from logger.h, never from call-site order.
"""
from pathlib import Path
import argparse
import csv
import io
import json
import re

ROOT = Path(__file__).resolve().parents[1]
TOKEN = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\[\s\S]|[^"\\])*"|\'(?:\\[\s\S]|[^\'\\])*\'')


def mask(text):
    return TOKEN.sub(lambda m: ''.join('\n' if c == '\n' else ' ' for c in m.group()), text)


def arguments(text, start):
    masked = mask(text)
    depth, begin, parts = 0, start + 1, []
    for i in range(start + 1, len(masked)):
        c = masked[i]
        if c in '([{':
            depth += 1
        elif c in ')]}':
            if depth == 0:
                tail = text[begin:i].strip()
                return parts + ([tail] if tail or parts else [])
            depth -= 1
        elif c == ',' and depth == 0:
            parts.append(text[begin:i].strip())
            begin = i + 1
    raise ValueError('Unterminated log call')


def collect():
    constants = {name: int(number) for name, number in re.findall(
        r'^#define\s+(A_\w+)\s+(\d+)\s*$', (ROOT / 'include/logger.h').read_text(), re.M)}
    assert len(set(constants.values())) == len(constants), 'Duplicate message ID'
    limit = int(re.search(r'^#define LOG_MAX_PARAMS (\d+)', (ROOT / 'include/config.h').read_text(), re.M)[1])
    definitions = {}
    pattern = re.compile(r'LOG_MESSAGE\((A_\w+), ("(?:\\.|[^"\\])*"), (nullptr|"(?:\\.|[^"\\])*")\)')
    for line in (ROOT / 'include/log_messages.def').read_text().splitlines():
        if not line.strip() or line.startswith('//'):
            continue
        m = pattern.fullmatch(line)
        assert m, f'Invalid dictionary entry: {line}'
        name, text, short = m.groups()
        assert name in constants and name not in definitions, name
        text = json.loads(text)
        short = None if short == 'nullptr' else json.loads(short)
        slots = {int(n) for n in re.findall(r'\{p(\d+)\}', text)}
        maximum = max(slots, default=0)
        assert maximum <= limit and slots == set(range(1, maximum + 1)), name
        assert set(re.findall(r'\{p\d+\}', short or '')) <= set(re.findall(r'\{p\d+\}', text)), name
        for template in (text, short or ''):
            assert not re.search(r'[{}]', re.sub(r'\{p\d+\}', '', template)), name
            assert '\n' not in template, name
        definitions[name] = dict(id=constants[name], text=text, shortText=short, count=maximum, calls=[])
    assert set(definitions) == set(constants), 'Constant and dictionary mismatch'
    assert list(d['id'] for d in definitions.values()) == sorted(constants.values()), 'Dictionary must be sorted by ID'
    for path in sorted((ROOT / 'src').glob('*.cpp')):
        if path.name == 'camera.cpp':
            continue
        text = path.read_text(encoding='utf-8-sig')
        masked = mask(text)
        for m in re.finditer(r'\blogger\.(logPrintFrom|logPrint)\s*\(', masked):
            args = arguments(text, masked.index('(', m.start()))
            if m[1] == 'logPrintFrom':
                args = args[1:]
            names = re.findall(r'\bA_\w+\b', args[0])
            assert names, f'Non-symbolic message ID in {path.name}'
            for name in names:
                assert name in definitions, name
                assert len(args) - 3 == definitions[name]['count'], (path.name, name, len(args) - 3)
                definitions[name]['calls'].append(dict(
                    file=path.relative_to(ROOT).as_posix(), line=text.count('\n', 0, m.start()) + 1,
                    area=args[1], severity=args[2]))
    return definitions


def render(definitions):
    ordered = sorted(definitions.items(), key=lambda item: item[1]['id'])
    registry = [{key: d[key] for key in ('id', 'text', 'shortText')} for name, d in ordered]
    csv_output = io.StringIO(newline='')
    writer = csv.writer(csv_output, delimiter=';')
    writer.writerow(('id', 'constant', 'text', 'short_text', 'param_count', 'call_sites'))
    md = ['# Katalog logovacích událostí firmware', '',
          'Číselník hlavního firmware bez kamery a lokálních SSH knihoven. '
          'ID `Axxx` je označení pro člověka; po síti se posílá číslo `xxx`. '
          'Čísla zůstávají stabilní a nepřidělují se podle pořadí volání.', '',
          'Zdrojem textů je [log_messages.def](../include/log_messages.def), '
          'číselné konstanty jsou v [logger.h](../include/logger.h). '
          'Export pro příjemce je [log_messages.json](log_messages.json); '
          '[CSV katalog](log_catalog.csv) umožňuje filtrování. '
          '[Mapování původních výpisů](log_migration.csv) zachovává vazbu na původní katalog.', '',
          'Aktualizace: `python scripts/export_log_messages.py`. '
          'Kontrola bez zápisu: `python scripts/export_log_messages.py --check`.', '',
          '`{p1}` odpovídá klíči `messageparams.p1`; pořadí atributů JSON nemá význam. '
          'Displej používá krátký text, pokud existuje. Serial přidává konec řádku. '
          'Limit počtu parametrů je `LOG_MAX_PARAMS` v `config.h` (12), '
          'limit celého bloku včetně typů a délek je `LOG_PARAM_DATA_MAX_LEN` (160 bajtů).', '',
          '| Závažnost | Hodnota |', '| --- | ---: |',
          '| DEBUG | 1 |', '| INFO | 2 |', '| WARNING | 3 |', '| ERROR | 4 |', '| CRITICAL | 5 |', '',
          '`LOGPRINT_MIN_SEVERITY` propouští tuto a vyšší závažnosti. '
          'Oblasti: COND 2, COMM 3, SYSTEM 4, ACCESS 5, OPERATION 6. Hodnota 1 je vyřazená.', '',
          'Parametry PIN/kódu nejsou samotné přístupové hodnoty; zaznamenává se výsledek nebo ID credentialu. '
          'Diagnostický snapshot A019 obsahuje 11 hodnot: epochový čas, volnou interní paměť, '
          'největší interní blok, minimum volné interní paměti, stejné tři metriky PSRAM, '
          'minimum volného zásobníku sledované úlohy, velikost flash, velikost firmware a prostor pro OTA.', '',
          f'Počet hlášek: **{len(ordered)}**.', '',
          '| ID | Konstanta | Text | Krátký text | Parametrů | Volání a klasifikace |',
          '| --- | --- | --- | --- | ---: | --- |']
    def cell(value):
        return '`' + value.replace('|', '&#124;') + '`' if value else '—'
    for name, d in ordered:
        sites = '; '.join(f"{c['file']}:{c['line']} {c['area']}/{c['severity']}" for c in d['calls'])
        writer.writerow((f"A{d['id']:03}", name, d['text'], d['shortText'], d['count'], sites))
        links = '; '.join(f"[{c['file']}:{c['line']}](../{c['file']}#L{c['line']}) "
                          f"{cell(c['area'])}/{cell(c['severity'])}" for c in d['calls'])
        if name in ('A_LOG_REJECTED', 'A_LOG_DROPPED'):
            links = 'Interní událost loggeru'
        md.append(f"| A{d['id']:03} | {cell(name)} | {cell(d['text'])} | {cell(d['shortText'])} | {d['count']} | {links or '—'} |")
    return {'Docs/log_messages.json': (json.dumps(registry, ensure_ascii=False, indent=2) + '\n').encode(),
            'Docs/log_catalog.csv': csv_output.getvalue().encode('utf-8-sig'),
            'Docs/log_catalog.md': ('\n'.join(md) + '\n').encode()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    definitions = collect()
    for rel, content in render(definitions).items():
        path = ROOT / rel
        if args.check:
            assert path.read_bytes() == content, f'Stale export: {rel}'
        else:
            path.write_bytes(content)
    print(f'{len(definitions)} stable message IDs and their call sites verified')


if __name__ == '__main__':
    main()
