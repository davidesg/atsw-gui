#!/usr/bin/env python3
"""Check the defect register: its shape, and that no defect number is loose.

WHY A CHECK AND NOT A CONVENTION.  drvec shares its engine and its cast with
fue, drvarma and drtran, so it shares ONE sequence of defect numbers.  Two
things go wrong with a hand-kept register of that kind, and both have happened
in this suite: a number gets reused in a second register and stops identifying
anything, and an entry gets written without saying what the defect COST, which
is the one part that cannot be reconstructed later.

So this checks three things:

  1. every entry in docs/BUGS.md has a number, a status and a cost;
  2. the numbers are unique and increasing, and none collides with the range
     another register owns (declared in FOREIGN below);
  3. every BUG-N mentioned anywhere in the repository is either registered here
     or declared foreign -- a reference to a number nobody registered is a
     reference to nothing.

Usage:  tools/check_bugs.py [register.md]
"""
import io
import os
import re
import sys

#  Ranges other registers of the suite own.  drvec's own start after them; see
#  docs/BUGS.md, "How this register works".
FOREIGN = {
    "drtran-python/docs/BUGS.md": range(1, 14),      # BUG-1 .. BUG-13
    "drtran-python/docs/BUGS.md (2026-09-23)": range(21, 23),   # BUG-21, BUG-22
}
#  fue numbers with four digits (BUG-0005 ...), a separate sequence that cannot
#  collide with this one.
FOUR_DIGIT = re.compile(r"BUG-\d{4}\b")

ENTRY = re.compile(r"^## BUG-(\d+)\s+—\s+(.+)$", re.M)
REF = re.compile(r"\bBUG-(\d{1,3})\b")
SKIP_DIRS = {".git", "build", "bin", "literature", "datasets", "benchmark"}
SKIP_EXT = {".o", ".inp", ".out", ".pre", ".png", ".pdf", ".xz", ".zip"}


def sections(text):
    """Split the register into (number, title, body) per entry."""
    hits = list(ENTRY.finditer(text))
    for i, m in enumerate(hits):
        end = hits[i + 1].start() if i + 1 < len(hits) else len(text)
        yield int(m.group(1)), m.group(2).strip(), text[m.end():end]


def check_register(path):
    problems = []
    text = io.open(path, encoding="utf-8").read()
    seen = []
    for num, title, body in sections(text):
        where = "%s: BUG-%d" % (path, num)
        if num in seen:
            problems.append("%s is registered twice" % where)
        seen.append(num)
        for owner, rng in FOREIGN.items():
            if num in rng:
                problems.append("%s collides with the range %s owns" % (where, owner))
        if not re.search(r"\*\*Status:\s*(OPEN|FIXED|WON'T FIX)", body):
            problems.append("%s has no **Status:** line (OPEN / FIXED / WON'T FIX)"
                            % where)
        if "What it cost" not in body:
            problems.append("%s does not say what it cost; that is the part that "
                            "cannot be reconstructed later" % where)
        if "How it was found" not in body:
            problems.append("%s does not say how it was found" % where)
    if seen != sorted(seen):
        problems.append("%s: the entries are not in increasing order: %s"
                        % (path, seen))
    return seen, problems


def repo_references(root):
    """Every BUG-N mentioned in the tree, with one file where it appears."""
    found = {}
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for name in filenames:
            if os.path.splitext(name)[1] in SKIP_EXT:
                continue
            full = os.path.join(dirpath, name)
            try:
                text = io.open(full, encoding="utf-8", errors="replace").read()
            except (IOError, OSError):
                continue
            text = FOUR_DIGIT.sub("", text)          # fue's own sequence
            for m in REF.finditer(text):
                found.setdefault(int(m.group(1)),
                                 os.path.relpath(full, root))
    return found


def main(argv):
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    path = argv[1] if len(argv) > 1 else os.path.join(root, "docs", "BUGS.md")
    registered, problems = check_register(path)

    known = set(registered)
    for rng in FOREIGN.values():
        known |= set(rng)

    for num, where in sorted(repo_references(root).items()):
        if num not in known:
            problems.append("BUG-%d is referenced in %s and registered nowhere"
                            % (num, where))

    for p in problems:
        print(p)
    if problems:
        print("%d problem(s) in the defect register; see docs/BUGS.md."
              % len(problems))
        return 1
    print("defect register: %d entries (%s), every reference accounted for"
          % (len(registered),
             ", ".join("BUG-%d" % n for n in registered) or "none"))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
