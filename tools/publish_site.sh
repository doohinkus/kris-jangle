#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal (from fm1-fomni)
# Build a firmware's release VERSION, its emulator and its site (tools/make_site.py), and push the site to the
# gh-pages branch of the app's repository, replacing what was there (the branch holds only the generated site).
#   tools/publish_site.sh APP_DIR VERSION [PACKAGE]
# PACKAGE: a release package made elsewhere (the attested one from the release's workflow), on the site in place of
# a local release build.
# The repository the site goes to is the app's own: fm1.toml's repo (its source's URL). The app may sit in another
# repository (fm1-base holds several firmwares), where "origin" would be the wrong one: the remote whose URL is
# fm1.toml's repo is used, or the URL itself when the app has no such remote.
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
APP="$(cd "${1:?usage: tools/publish_site.sh APP_DIR VERSION}" && pwd)"
V="${2:?usage: tools/publish_site.sh APP_DIR VERSION}"
PY="${PYTHON:-python3}"
NAME="$(sed -n 's/^name *= *"\([^"]*\)".*/\1/p' "$APP/fm1.toml" | tr 'A-Z' 'a-z')"
# a git URL or a repository URL -> owner/repo, so the two can be compared
norm() {
    printf %s "$1" | sed -E 's#^(git@github\.com:|https?://[^/]+/)##; s#\.git$##; s#/+$##'
}
if [ -n "${3:-}" ]; then
    PKG="$(cd "$(dirname "$3")" && pwd)/$(basename "$3")"
    [ -f "$PKG" ] || { echo "publish_site.sh: no package $3"; exit 1; }
else
    "$PY" "$HERE/tools/build.py" "$APP" --release "$V"
    PKG="$APP/build/$NAME-$V.fwsc"
fi
"$PY" "$HERE/tools/build.py" "$APP" --web --release "$V"
"$PY" "$HERE/tools/make_site.py" "$APP" "$PKG" "$V" "$APP/build/site"
REPO_URL="$(sed -n 's/^repo *= *"\([^"]*\)".*/\1/p' "$APP/fm1.toml")"
REMOTE=""
if [ -n "$REPO_URL" ]; then
    WANT="$(norm "$REPO_URL")"
    for r in $(git -C "$APP" remote 2>/dev/null); do
        u="$(git -C "$APP" remote get-url "$r" 2>/dev/null || true)"
        [ -n "$u" ] || continue
        if [ "$(norm "$u")" = "$WANT" ]; then REMOTE="$u"; break; fi
    done
    [ -n "$REMOTE" ] || REMOTE="$REPO_URL"
fi
[ -n "$REMOTE" ] || REMOTE="$(git -C "$APP" remote get-url origin)"
SHA="$(git -C "$APP" rev-parse --short HEAD)"
cd "$APP/build/site"
rm -rf .git
git init -q -b gh-pages
git add -A
git commit -q -m "Site for $NAME $V (from $SHA)"
echo "pushing the site for $NAME $V to $REMOTE (gh-pages)"
git push -q -f "$REMOTE" gh-pages
rm -rf .git
# ask GitHub Pages to build it now: a push alone has left the first deploy stuck on "building"
REPO="$(printf %s "$REMOTE" | sed -E 's#(git@github.com:|https://github.com/)##; s#\.git$##')"
gh api -X POST "repos/$REPO/pages/builds" >/dev/null 2>&1 || echo "(could not ask GitHub Pages to build: check the repository's Pages settings)"
echo "pushed the site for $NAME $V to gh-pages"
