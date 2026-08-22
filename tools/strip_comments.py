#!/usr/bin/env python3
"""Print a C source with every comment removed, and nothing else changed.

Why this exists: translating ~1200 lines of comment prose by hand is exactly
the kind of edit that moves a line of code by accident, and no test would
necessarily catch it -- a moved `ii++` inside a printer walk is §4.1 all over
again.  So the translation is checked by an INVARIANT: strip the comments from
the file before and after, and the two must be byte-identical.  A translation
that changes one character of code cannot pass.

String and character literals are respected, so a `"/*"` inside a printf is not
mistaken for a comment.
"""
import sys


def strip(src):
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c == '"' or c == "'":
            q = c
            out.append(c)
            i += 1
            while i < n:
                if src[i] == '\\':
                    out.append(src[i:i + 2])
                    i += 2
                    continue
                out.append(src[i])
                if src[i] == q:
                    i += 1
                    break
                i += 1
            continue
        if c == '/' and i + 1 < n and src[i + 1] == '*':
            i += 2
            while i + 1 < n and not (src[i] == '*' and src[i + 1] == '/'):
                if src[i] == '\n':
                    out.append('\n')      # keep line count stable
                i += 1
            i += 2
            continue
        if c == '/' and i + 1 < n and src[i + 1] == '/':
            while i < n and src[i] != '\n':
                i += 1
            continue
        out.append(c)
        i += 1
    #  Blank lines are dropped, not just trimmed: a translated comment may have
    #  a different number of lines than the original, and that must not count as
    #  a change to the code.
    return '\n'.join(l for l in (x.rstrip() for x in ''.join(out).split('\n')) if l)


if __name__ == "__main__":
    with open(sys.argv[1], encoding='utf-8') as fh:
        sys.stdout.write(strip(fh.read()))
