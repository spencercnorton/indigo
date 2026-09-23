#!/bin/sh
# Repository-aware whitespace/EOL gate.
#
# Swiss intentionally retains CRLF in a small number of legacy translation
# units. Git's default whitespace policy mistakes the carriage return for
# trailing whitespace in a fresh clone, while a developer-local config can
# hide mixed-EOL edits. This wrapper makes both policies explicit and portable.
#
# usage: buildtools/check_whitespace.sh [base-ref]

set -eu

BASE="${1:-origin/master}"
ROOT=$(git rev-parse --show-toplevel)
cd "$ROOT"

git -c core.whitespace=blank-at-eol,blank-at-eof,space-before-tab,cr-at-eol \
	diff --check "$BASE"...HEAD --

git diff --name-only "$BASE"...HEAD -- | while IFS= read -r file; do
	case "$file" in
		*.c|*.h|*.s|*.S|*.sh|*.py|*.md|*.txt|*.yml|*.yaml)
			;;
		*)
			continue
			;;
	esac
	[ -f "$file" ] || continue
	if ! perl -0777 -e '
		my $text = <>;
		exit(($text =~ /\r\n/ && $text =~ /(?<!\r)\n/) ? 1 : 0);
	' "$file"; then
		echo "WHITESPACE GATE FAILED — mixed CRLF/LF endings: $file" >&2
		exit 1
	fi
done

echo "whitespace gate OK (portable CRLF policy; no mixed-EOL text files)"
