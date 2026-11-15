#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Normalize visual indentation of #pragma region blocks (C++/GLSL).

    py -3 tools/indent_regions.py path/to/file.cpp
    py -3 tools/indent_regions.py --check path/to/file.cpp

Unlike a blind '+4 spaces per containing region' pass, this script compares
existing indentation with the C++/GLSL brace-scoped baseline. Thus a function
already indented inside a namespace isn't indented a second time by the first
region. Inner regions still add one visual indentation level. Repeated runs are
idempotent for ordinary Allman-style C++/GLSL code.

This is a best-effort structural formatter, not a C++ parser. Review diffs on
macros, unusual nested initializers and constructs disabled from formatting.
"""
from __future__ import annotations

import argparse
import difflib
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

START = re.compile(r'^(?P<ws>[ \t]*)(?P<text>#\s*pragma\s+region\b[^\r\n]*|//\s*#region\b[^\r\n]*|#region\b[^\r\n]*)$')
END = re.compile(r'^(?P<ws>[ \t]*)(?P<text>#\s*pragma\s+endregion\b[^\r\n]*|//\s*#endregion\b[^\r\n]*|#endregion\b[^\r\n]*)$')
FORMAT_OFF = re.compile(r'//\s*clang-format\s+off\b')
FORMAT_ON = re.compile(r'//\s*clang-format\s+on\b')
RAW_OPEN = re.compile(r'(?<![\w])(?:u8|u|U|L)?R"([^\s()\\]{0,16})\(')
MACRO_OPEN = re.compile(r'^\s*#\s*define\b')


def split_nl(line: str) -> tuple[str, str]:
    if line.endswith('\r\n'):
        return line[:-2], '\r\n'
    if line.endswith(('\n', '\r')):
        return line[:-1], line[-1]
    return line, ''


def prefix_width(s: str, width: int) -> int:
    col = 0
    for c in s:
        if c == ' ':
            col += 1
        elif c == '\t':
            col += width - col % width
        else:
            break
    return col


@dataclass
class Region:
    start: int
    end: int = -1
    parent: int | None = None
    children: list[int] = field(default_factory=list)
    start_indent: int = 0
    body_floor: int = 0
    extra: int = 0
    old_extra: int = 0
    family: str = ""


def clean_braces(line: str, block_comment: bool) -> tuple[str, bool]:
    """Replace strings/comments with spaces before counting braces."""
    out = []
    i = 0
    n = len(line)
    while i < n:
        if block_comment:
            j = line.find('*/', i)
            if j < 0:
                out.extend(' ' * (n - i))
                return ''.join(out), True
            out.extend(' ' * (j + 2 - i))
            i = j + 2
            block_comment = False
        elif line.startswith('//', i):
            out.extend(' ' * (n - i))
            break
        elif line.startswith('/*', i):
            block_comment = True
            out.extend('  ')
            i += 2
        elif line[i] in ('"', "'"):
            q = line[i]
            j = i + 1
            while j < n:
                if line[j] == '\\':
                    j += 2
                elif line[j] == q:
                    j += 1
                    break
                else:
                    j += 1
            out.extend(' ' * (min(j, n) - i))
            i = min(j, n)
        else:
            out.append(line[i])
            i += 1
    return ''.join(out), block_comment


def format_regions(source: str, width: int = 4) -> str:
    rows = [split_nl(s) for s in source.splitlines(keepends=True)]
    n = len(rows)
    regions: list[Region] = []
    region_line = [None] * n
    marker = [None] * n
    protected = [False] * n
    scope = [0] * n
    stack: list[int] = []
    raw_end = None
    macro_cont = False
    format_off = False
    block_comment = False
    braces = 0

    for i, (body, _) in enumerate(rows):
        if raw_end is not None:
            protected[i] = True
            if raw_end in body:
                raw_end = None
            continue
        if macro_cont:
            protected[i] = True
            macro_cont = body.rstrip().endswith('\\')
            continue
        if FORMAT_OFF.search(body):
            format_off = True
        if format_off:
            protected[i] = True
            if FORMAT_ON.search(body):
                format_off = False
            continue
        if MACRO_OPEN.match(body) and body.rstrip().endswith('\\'):
            protected[i] = True
            macro_cont = True
            continue
        raw = RAW_OPEN.search(body)
        if raw is not None and (')' + raw.group(1) + '"') not in body[raw.end():]:
            raw_end = ')' + raw.group(1) + '"'
        # Record syntax nesting BEFORE this line; a leading close-brace is outdented.
        cleaned, block_comment = clean_braces(body, block_comment)
        begin_close = len(re.match(r'^\s*}*', cleaned).group(0).lstrip())
        scope[i] = max(0, braces - begin_close) * width
        braces = max(0, braces + cleaned.count('{') - cleaned.count('}'))
        m_start = START.match(body)
        m_end = END.match(body)
        if m_start:
            parent = stack[-1] if stack else None
            family = "pragma" if m_start.group("text").startswith("#pragma") else ("comment" if m_start.group("text").startswith("//") else "hash")
            r = Region(start=i, parent=parent, family=family)
            idx = len(regions)
            regions.append(r)
            if parent is not None:
                regions[parent].children.append(idx)
            marker[i] = ('start', idx, m_start.group('text'))
            stack.append(idx)
        elif m_end:
            if not stack:
                raise ValueError(f'line {i + 1}: endregion without region')
            idx = stack.pop()
            r = regions[idx]
            family = "pragma" if m_end.group("text").startswith("#pragma") else ("comment" if m_end.group("text").startswith("//") else "hash")
            if family != r.family:
                raise ValueError(f"line {i + 1}: {family} endregion closes {r.family} region")
            r.end = i
            marker[i] = ('end', idx, m_end.group('text'))
        else:
            region_line[i] = stack[-1] if stack else None
    if stack:
        raise ValueError('unclosed region at line(s) ' + ', '.join(str(regions[j].start + 1) for j in stack))
    if raw_end:
        raise ValueError('unterminated raw string')

    # Parent before child. Marker indentation is derived from the parent body.
    for idx, r in enumerate(regions):
        body, _ = rows[r.start]
        if r.parent is None:
            r.start_indent = prefix_width(body, width)
        else:
            r.start_indent = regions[r.parent].body_floor
        r.body_floor = max(scope[r.start], r.start_indent + width)
        r.extra = max(0, r.body_floor - scope[r.start])

        # Find the actual visual offset already present after clang-format or
        # a previous invocation. Anchor on non-comment, non-preprocessor code.
        offsets = []
        for i in range(r.start + 1, r.end):
            if region_line[i] != idx or protected[i]:
                continue
            code = rows[i][0]
            stripped = code.strip()
            if (not stripped or stripped.startswith(('#', '//', '/*', '*', '}', '{'))
                    or stripped.startswith(('public:', 'private:', 'protected:'))):
                continue
            delta = prefix_width(code, width) - scope[i]
            if delta >= 0 and delta % width == 0:
                offsets.append(delta)
        r.old_extra = min(offsets) if offsets else r.extra

    result = []
    for i, (body, nl) in enumerate(rows):
        if protected[i] or not body.strip():
            result.append(body + nl)
            continue
        if marker[i] is not None:
            kind, idx, text = marker[i]
            result.append(' ' * regions[idx].start_indent + text + nl)
            continue
        idx = region_line[i]
        if idx is None:
            result.append(body + nl)
            continue
        r = regions[idx]
        stripped = body.lstrip(' \t')
        # A one-line region description directly beneath the opener has a
        # stable, predictable indentation regardless of its previous spacing.
        is_description = i == r.start + 1 and stripped.startswith('//')
        if is_description:
            new_indent = r.body_floor
        else:
            old_indent = prefix_width(body, width)
            new_indent = max(r.body_floor, scope[i] + r.extra,
                             old_indent - r.old_extra + r.extra)
        result.append(' ' * new_indent + stripped + nl)
    return ''.join(result)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('file', type=Path)
    ap.add_argument('--width', type=int, default=4)
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--diff', action='store_true')
    args = ap.parse_args()
    if not 1 <= args.width <= 16:
        ap.error('--width must be 1..16')
    try:
        raw = args.file.read_bytes()
        bom = raw.startswith(b'\xef\xbb\xbf')
        text = raw[3:] if bom else raw
        source = text.decode('utf-8')
        revised = format_regions(source, args.width)
        result = (b'\xef\xbb\xbf' if bom else b'') + revised.encode('utf-8')
        if args.diff and source != revised:
            sys.stdout.writelines(difflib.unified_diff(source.splitlines(True), revised.splitlines(True),
                                 fromfile='before', tofile='after'))
        if args.check:
            return 0 if raw == result else 1
        if result != raw:
            args.file.write_bytes(result)
            print(f'Updated {args.file}')
        else:
            print(f'No changes {args.file}')
        return 0
    except (OSError, UnicodeError, ValueError) as exc:
        print(f'Region error: {exc}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())
