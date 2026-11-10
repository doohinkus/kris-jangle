#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Charles Vestal
"""tools/publish_site.sh: the site goes to the app's own repository.

An app can sit inside another repository (fm1-base holds several firmwares),
where "origin" is the wrong one: the site would be pushed to the platform's
repository instead of the app's. The script takes the repository from
fm1.toml's repo (the source's URL) and uses the remote that matches it.

This runs the script against stub tools and two local bare repositories: the
one fm1.toml names must get the gh-pages branch, the other must not.
"""
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
FAILED = []


def ok(cond, what):
    print(f"{what} {'ok' if cond else 'FAIL'}")
    if not cond:
        FAILED.append(what)


def run(cmd, cwd=None, env=None):
    return subprocess.run(cmd, cwd=cwd, env=env, capture_output=True, text=True)


def main():
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="publish-site-"))
    tools = tmp / "tools"
    tools.mkdir()
    shutil.copy(ROOT / "tools" / "publish_site.sh", tools / "publish_site.sh")
    (tools / "publish_site.sh").chmod(0o755)

    # the stubs: the script only needs the builds and the site to happen
    (tools / "build.py").write_text(
        "#!/usr/bin/env python3\n"
        "import pathlib, sys\n"
        "app = pathlib.Path(sys.argv[1])\n"
        "if '--release' in sys.argv and '--web' not in sys.argv:\n"
        "    name = (app / 'fm1.toml').read_text().split('name = \"')[1].split('\"')[0].lower()\n"
        "    ver = sys.argv[sys.argv.index('--release') + 1]\n"
        "    (app / 'build').mkdir(parents=True, exist_ok=True)\n"
        "    (app / 'build' / (name + '-' + ver + '.fwsc')).write_bytes(b'PKG')\n"
        "print('stub build')\n")
    (tools / "make_site.py").write_text(
        "#!/usr/bin/env python3\n"
        "import pathlib, sys\n"
        "out = pathlib.Path(sys.argv[4])\n"
        "out.mkdir(parents=True, exist_ok=True)\n"
        "(out / 'index.html').write_text('<html>the site</html>')\n"
        "print('stub site')\n")

    # the app: its own repository (named by fm1.toml's repo) and another one
    app = tmp / "app"
    app.mkdir()
    (app / "fm1.toml").write_text('name = "TESTFW"\nrepo = "https://github.com/someone/testfw"\n')
    mine = tmp / "the-app-repo"
    other = tmp / "the-platform-repo"
    for r in (mine, other):
        run(["git", "init", "-q", "--bare", str(r)])
    run(["git", "init", "-q", "-b", "main", str(app)])
    run(["git", "-C", str(app), "remote", "add", "origin", str(other)])       # the platform's
    run(["git", "-C", str(app), "remote", "add", "app", "https://github.com/someone/testfw.git"])
    (app / "readme").write_text("x\n")
    run(["git", "-C", str(app), "add", "-A"])
    run(["git", "-C", str(app), "-c", "user.email=t@example.com", "-c", "user.name=Test",
         "commit", "-q", "-m", "the app"])

    # git rewrites the app's URL to the local bare repository, so the push
    # stays local; a home of its own keeps the rewrite out of the user's config
    home = tmp / "home"
    home.mkdir()
    (home / ".gitconfig").write_text(f'[url "{mine}"]\n\tinsteadOf = https://github.com/someone/testfw\n')
    env = dict(os.environ)
    env.update(HOME=str(home), GIT_AUTHOR_NAME="Test", GIT_AUTHOR_EMAIL="t@example.com",
               GIT_COMMITTER_NAME="Test", GIT_COMMITTER_EMAIL="t@example.com",
               GH_HOST="127.0.0.1:1")     # the Pages request fails fast, and does nothing

    r = run(["sh", str(tools / "publish_site.sh"), str(app), "1.2"], env=env)
    print(r.stdout.strip()[-400:] if r.stdout else r.stderr.strip()[-400:])
    ok(r.returncode == 0, f"the script ran (exit {r.returncode})")

    branches = run(["git", "-C", str(mine), "branch", "--list", "gh-pages"]).stdout
    ok("gh-pages" in branches, "the app's own repository got the gh-pages branch")
    files = run(["git", "-C", str(mine), "ls-tree", "--name-only", "gh-pages"]).stdout
    ok("index.html" in files, "the site is on that branch")
    other_branches = run(["git", "-C", str(other), "branch", "--list"]).stdout
    ok("gh-pages" not in other_branches, "the other repository (origin) got nothing")

    # and with no repo in fm1.toml: the old behaviour, the origin
    (app / "fm1.toml").write_text('name = "TESTFW"\n')
    r = run(["sh", str(tools / "publish_site.sh"), str(app), "1.3"], env=env)
    ok(r.returncode == 0, f"the script ran without a repo (exit {r.returncode})")
    other_branches = run(["git", "-C", str(other), "branch", "--list"]).stdout
    ok("gh-pages" in other_branches, "without a repo in fm1.toml, the site goes to origin")

    shutil.rmtree(tmp, ignore_errors=True)
    print("publish_site tests failed" if FAILED else "publish_site tests passed")
    return 1 if FAILED else 0


if __name__ == "__main__":
    sys.exit(main())
