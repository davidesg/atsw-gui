#!/bin/sh
#  check_version.sh — the version number says the same thing everywhere.
#
#  WHY.  The number has ONE definition, the #define in src/drvec.c, and five
#  places that have to agree with it by hand: CITATION.cff, CHANGELOG.md, the
#  record in docs/VERSIONS.md, the git tag, and whatever the binary prints.
#  Hand-copied numbers rot -- a release whose CITATION.cff still carries the
#  previous version is a release nobody can cite correctly -- so they are
#  checked instead of trusted.  See docs/VERSIONS.md 2.
#
#  Exit 0 if they agree, 1 if they do not.  Prints what it found either way.
#
#  The git tag is checked only if one exists for the current version: tagging
#  happens after the commit, so between the two this must not fail.

set -u
root=$(dirname "$0")/..
cd "$root" || exit 1
fail=0

say()  { printf '  %-22s %s\n' "$1" "$2"; }
bad()  { printf '  %-22s %s   <-- MISMATCH\n' "$1" "$2"; fail=1; }

ver=$(sed -n 's/^#define DRVEC_VERSION "\(.*\)"/\1/p' src/drvec.c | head -1)
if [ -z "$ver" ]; then
    echo "check_version: no DRVEC_VERSION in src/drvec.c"
    exit 1
fi
echo "drvec version, as defined in src/drvec.c: $ver"

cff=$(sed -n 's/^version: *"\{0,1\}\([^"]*\)"\{0,1\}/\1/p' CITATION.cff | head -1)
[ "$cff" = "$ver" ] && say "CITATION.cff" "$cff" || bad "CITATION.cff" "${cff:-<none>}"

chg=$(sed -n 's/^## \([0-9][0-9.]*\) .*/\1/p' CHANGELOG.md | head -1)
[ "$chg" = "$ver" ] && say "CHANGELOG.md" "$chg" || bad "CHANGELOG.md" "${chg:-<none>}"

vdoc=$(sed -n 's/^### \([0-9][0-9.]*\) .*/\1/p' docs/VERSIONS.md | head -1)
[ "$vdoc" = "$ver" ] && say "docs/VERSIONS.md" "$vdoc" || bad "docs/VERSIONS.md" "${vdoc:-<none>}"

if [ -x bin/drvec ]; then
    bin=$(bin/drvec --version 2>/dev/null | head -1 | awk '{print $2}')
    [ "$bin" = "$ver" ] && say "bin/drvec --version" "$bin" || bad "bin/drvec --version" "${bin:-<none>}"
else
    say "bin/drvec" "not built, not checked"
fi

if command -v git >/dev/null 2>&1 && [ -d .git ]; then
    if git rev-parse "v$ver" >/dev/null 2>&1; then
        say "git tag v$ver" "present"
    else
        say "git tag v$ver" "not created yet (tag after the commit)"
    fi
fi

if [ "$fail" -eq 0 ]; then
    echo "check_version: every copy agrees on $ver"
else
    echo "check_version: the version number disagrees with itself; see docs/VERSIONS.md 2"
fi
exit "$fail"
