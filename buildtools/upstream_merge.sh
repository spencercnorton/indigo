#!/bin/sh
# Move Indigo to another upstream Swiss commit: merge upstream's changes since
# the commit UPSTREAM names into the working tree, and point UPSTREAM at the
# new one.
#
# Indigo's text files end lines with LF and upstream's often with CRLF, so both
# upstream trees are merged with their carriage returns stripped. A conflict is
# left in its file with the usual markers and listed at the end. Then: resolve
# them, refresh the audit fixtures that copy upstream code (AGENTS.md), run
# the checks, and commit.
#
# usage: buildtools/upstream_merge.sh <40-character upstream commit> [upstream repository]
#   (a tag's commit: git ls-remote https://github.com/emukidid/swiss-gc.git 'v0.6r2119^{}')
set -eu
new=${1:-}
printf '%s\n' "$new" | grep -Eqx '[0-9a-f]{40}' ||
	{ echo "usage: buildtools/upstream_merge.sh <40-character upstream commit> [upstream repository]" >&2; exit 2; }
cd "$(git rev-parse --show-toplevel)"
[ -z "$(git status --porcelain --untracked-files=no)" ] ||
	{ echo "upstream_merge: commit or stash your changes first" >&2; exit 1; }
src=${2:-$(sed -n 's/^upstream[[:space:]]\{1,\}//p' UPSTREAM)}
old=$(sed -n 's/^commit[[:space:]]\{1,\}\([0-9a-f]\{40\}\).*/\1/p' UPSTREAM)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
git init -q --bare "$tmp/up.git"
git -C "$tmp/up.git" fetch -q --depth=1 --no-tags "$src" "$old" "$new"

# An upstream commit's tree with carriage returns stripped, as a commit here
# (an object only: no branch or reflog points at it).
gitdir=$(git rev-parse --absolute-git-dir)
lf() {
	rm -rf "$tmp/tree" && mkdir "$tmp/tree"
	git -C "$tmp/up.git" archive "$1" | tar -x -C "$tmp/tree"
	grep -rlI "$(printf '\r')" "$tmp/tree" | while IFS= read -r file; do
		perl -pi -e 's/\r\n/\n/g' "$file"
	done
	tree=$(cd "$tmp/tree" && GIT_INDEX_FILE="$tmp/index" git --git-dir="$gitdir" --work-tree=. add -A . &&
		GIT_INDEX_FILE="$tmp/index" git --git-dir="$gitdir" write-tree)
	rm -f "$tmp/index"
	commit "$tree" "${2:-}"
}
commit() { # a commit object for tree $1, with parent $2 when there is one
	tree=$1
	if [ -n "$2" ]; then set -- -p "$2"; else set --; fi
	GIT_AUTHOR_NAME=upstream GIT_AUTHOR_EMAIL=upstream@example.invalid \
	GIT_COMMITTER_NAME=upstream GIT_COMMITTER_EMAIL=upstream@example.invalid \
		git commit-tree "$@" -m "upstream_merge" "$tree"
}
base=$(lf "$old")
theirs=$(lf "$new" "$base")
# HEAD's tree over the old upstream, so git finds that as the merge base (no
# --merge-base, which needs git 2.40).
ours=$(commit "$(git rev-parse 'HEAD^{tree}')" "$base")
git merge-tree --write-tree --name-only "$ours" "$theirs" > "$tmp/merge" || true
git read-tree -u --reset "$(head -n 1 "$tmp/merge")"
sed "s/^commit [0-9a-f]\{40\}.*/commit $new/" UPSTREAM > "$tmp/UPSTREAM" && cat "$tmp/UPSTREAM" > UPSTREAM

# A submodule is vendored here, so a new commit of one needs copying by hand.
git -C "$tmp/up.git" ls-tree -r "$old" | grep '^160000' > "$tmp/links.old" || true
git -C "$tmp/up.git" ls-tree -r "$new" | grep '^160000' > "$tmp/links.new" || true
cmp -s "$tmp/links.old" "$tmp/links.new" ||
	{ echo "upstream moved a submodule; update its vendored copy:"; diff "$tmp/links.old" "$tmp/links.new" || true; }
conflicts=$(awk 'NR == 1 { next } /^$/ { exit } { print }' "$tmp/merge")
if [ -n "$conflicts" ]; then
	echo "merged upstream $old..$new; resolve the markers in:"
	printf '%s\n' "$conflicts" | sed 's/^/  /'
	echo "then refresh the audit fixtures that copy upstream code (AGENTS.md)"
	exit 1
fi
echo "merged upstream $old..$new cleanly; refresh the audit fixtures that copy upstream code (AGENTS.md)"
