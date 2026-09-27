#!/bin/sh
# The checks that read the source without building it: shell and Python syntax,
# the CI tools' own tests, and the workflow policy. CI runs this in Source
# checks; run it from the repository root before pushing a CI change.
set -eu
cd "$(git rev-parse --show-toplevel)"

echo "== shell scripts (sh -n, bash -n, shellcheck errors) =="
scripts=$(git ls-files 'buildtools/*.sh' 'buildtools/**/*.sh')
for script in $scripts; do
	case $(head -n 1 "$script") in
		*bash*) bash -n "$script" ;;
		*) sh -n "$script" ;;
	esac
done
if command -v shellcheck >/dev/null; then
	# shellcheck disable=SC2086 # one argument per tracked script
	shellcheck --severity=error $scripts
else
	echo "shellcheck not installed; syntax checked only"
fi

echo "== Python syntax =="
git ls-files -z 'buildtools/*.py' 'buildtools/**/*.py' |
	xargs -0 python3 -c 'import ast, sys
for path in sys.argv[1:]:
    ast.parse(open(path, encoding="utf-8").read(), path)
print(len(sys.argv) - 1, "files parse")'

echo "== CI tools and the runner supervisor =="
python3 -m unittest discover -s buildtools/ci -p 'test_*.py'
python3 -m unittest discover -s buildtools/ci/runner -p 'test_*.py'

echo "== workflows =="
python3 buildtools/ci/check_workflows.py .
