#!/usr/bin/env python3
"""Fail if a C source carries Spanish in its comments or in its user-facing
strings.

WHY THIS IS A TEST AND NOT A STYLE NOTE.  drvec.c was written in a mix of
Spanish and English: the comments carried the reasoning, the documentation was
in English, and docs/README.md quoted output the program did not produce ("1 df"
where the program wrote "1 g.l.").  A reader who cannot read the comments cannot
audit the reasoning, and a document that quotes output the program does not emit
is a document that has stopped being checkable.  Both were fixed on 2026-08-22
(P7); this keeps them fixed, because a mixed-language file does not become mixed
again in one big commit -- it does so one line at a time.

The rule is deliberately crude and it is meant to be: a fragment is Spanish when
it carries more Spanish function words than English ones.  It cannot be fooled by
a Spanish identifier or a proper name, and it does not care about accents (the
sources are ASCII except for the mathematical symbols).

Usage:  tools/check_language.py src/drvec.c [more.c ...]
"""
import io
import re
import sys

ES = (r"que|para|con|del|los|las|una|por|esta|este|esto|aqui|porque|asi|solo|"
      r"tambien|hay|son|pero|desde|donde|cuando|entre|sobre|cada|todo|toda|"
      r"nada|otro|otra|mismo|misma|hasta|antes|despues|puede|pueden|tiene|"
      r"tienen|dice|luego|siempre|nunca|entonces|mientras|queda|deja|sale|"
      r"lleva|pone|existe|sin|semilla|escalera|peldano|prevision|verosimilitud|"
      r"arranque|ajuste|fichero|ficheros|niveles|nivel|rango|restringido|libre|"
      r"grados|libertad|numero|datos|modelo|salida|entrada|"
      r"parametro|parametros|muestra|regresion|devuelve|escribe|el|la|de|se|"
      r"un|su|lo|al|ya|si|es|mas")
EN = (r"the|of|and|is|to|that|it|in|for|with|this|which|not|are|be|has|have|"
      r"from|at|one|only|what|when|where|but|by|as|its|there|they|was|were|"
      r"been|would|could|should|does|do|did|than|then|because|while|each|every|"
      r"any|all|into|out|over|under|so|if|on|or|an|a|no|two|three|first|second|"
      r"last|same|other|another|here|now|before|after|between|about|through|"
      r"without|within|both|more|most|less|much|many|such|own|up|down|why|how")

_es = re.compile(r"(?<![A-Za-zÀ-ſ])(%s)(?![A-Za-zÀ-ſ])" % ES, re.I)
_en = re.compile(r"(?<![A-Za-z])(%s)(?![A-Za-z])" % EN, re.I)
_word = re.compile(r"[A-Za-zÀ-ſ]{2,}")


def looks_spanish(text, min_words=2):
    if len(_word.findall(text)) < min_words:
        return False
    return len(_es.findall(text)) > len(_en.findall(text))


def split(src):
    """Return (comments, strings): lists of (line, text)."""
    comments, strings = [], []
    i, n, line = 0, len(src), 1
    while i < n:
        c = src[i]
        if c == '\n':
            line += 1; i += 1; continue
        if c in '"\'':
            q, s0, l0 = c, i, line
            i += 1
            while i < n:
                if src[i] == '\\':
                    i += 2; continue
                if src[i] == '\n':
                    line += 1
                if src[i] == q:
                    i += 1; break
                i += 1
            if q == '"':
                strings.append((l0, src[s0 + 1:i - 1]))
            continue
        if c == '/' and i + 1 < n and src[i + 1] == '*':
            s0, l0 = i, line
            i += 2
            while i + 1 < n and not (src[i] == '*' and src[i + 1] == '/'):
                if src[i] == '\n':
                    line += 1
                i += 1
            i += 2
            comments.append((l0, src[s0:i]))
            continue
        if c == '/' and i + 1 < n and src[i + 1] == '/':
            s0, l0 = i, line
            while i < n and src[i] != '\n':
                i += 1
            comments.append((l0, src[s0:i]))
            continue
        i += 1
    return comments, strings


def check(path):
    src = io.open(path, encoding='utf-8', errors='replace').read()
    comments, strings = split(src)
    bad = []
    for line, text in comments:
        if looks_spanish(text):
            bad.append((line, 'comment', text.strip().split('\n')[0][:72]))
    #  Strings are joined per line so that a message split across several
    #  literals is judged whole; a two-word fragment is not evidence.
    per_line = {}
    for line, text in strings:
        per_line.setdefault(line, []).append(text)
    for line in sorted(per_line):
        joined = ' '.join(per_line[line])
        if looks_spanish(joined, min_words=3):
            bad.append((line, 'string', joined.strip()[:72]))
    return bad


def main(argv):
    total = 0
    for path in argv[1:]:
        for line, kind, text in sorted(check(path)):
            print("%s:%d: Spanish in %s: %s" % (path, line, kind, text))
            total += 1
    if total:
        print("%d Spanish fragment(s); the sources are English (P7)." % total)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
