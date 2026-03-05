#!/bin/sh
# NOTE: Keep this script POSIX /bin/sh compatible.
# Avoid bash/zsh-specific syntax and non-POSIX shell features.
set -eu

ROOT="${ROOT:-$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)}"
TARGET="${TARGET:-$ROOT/build/bin/kbsh}"
REF_SHELL="${REF_SHELL:-/bin/sh}"
CASES_FILE="${CASES_FILE:-$ROOT/test/posix/cases.list}"
STRICT_XPASS="${STRICT_XPASS:-0}"
WORKDIR=""
WORK_STAMP=""

if [ ! -x "$TARGET" ]; then
	echo "posix-test: target not executable: $TARGET" >&2
	echo "posix-test: run 'make' first" >&2
	exit 2
fi

if [ ! -x "$REF_SHELL" ]; then
	echo "posix-test: reference shell not executable: $REF_SHELL" >&2
	exit 2
fi

if [ ! -f "$CASES_FILE" ]; then
	echo "posix-test: missing cases file: $CASES_FILE" >&2
	exit 2
fi

WORKBASE="${TMPDIR:-/tmp}/kbsh-posix.$$"
WORKDIR="$WORKBASE"
work_try=0
while ! mkdir "$WORKDIR" 2>/dev/null; do
	work_try=$((work_try + 1))
	WORKDIR="$WORKBASE.$work_try"
	if [ "$work_try" -gt 50 ]; then
		echo "posix-test: unable to create work dir" >&2
		exit 2
	fi
done
WORK_STAMP="$WORKDIR/.posix-test-created"
touch "$WORK_STAMP"

cleanup() {
	dir="$WORKDIR"
	stamp="$WORK_STAMP"
	if [ -z "$dir" ]; then
		return
	fi
	case "$dir" in
		""|"/"|"~"|"."|".."|"../"*|"/Users"|"/home"|"/root")
			echo "posix-test: refusing unsafe cleanup dir '$dir'" >&2
			return
			;;
	esac
	if [ ! -f "$stamp" ]; then
		echo "posix-test: refusing cleanup; missing stamp '$stamp'" >&2
		return
	fi
	rm -rf -- "$dir"
}
trap cleanup EXIT HUP INT TERM

total=0
pass=0
fail=0
xfail=0
xpass=0

run_case() {
	case_expected="$1"
	case_relpath="$2"
	case_path="$ROOT/test/posix/$case_relpath"

	if [ ! -f "$case_path" ]; then
		echo "posix-test: missing case: $case_path" >&2
		fail=$((fail + 1))
		return
	fi

	total=$((total + 1))
	case_id="$WORKDIR/$total"
	ref_out="$case_id.ref.out"
	ref_err="$case_id.ref.err"
	kb_out="$case_id.kb.out"
	kb_err="$case_id.kb.err"

	set +e
	LC_ALL=C LANG=C "$REF_SHELL" "$case_path" >"$ref_out" 2>"$ref_err"
	ref_status=$?

	LC_ALL=C LANG=C "$TARGET" "$case_path" >"$kb_out" 2>"$kb_err"
	kb_status=$?
	set -e

	case_match=1
	if [ "$ref_status" -ne "$kb_status" ]; then
		case_match=0
	fi
	if ! cmp -s "$ref_out" "$kb_out"; then
		case_match=0
	fi
	if ! cmp -s "$ref_err" "$kb_err"; then
		case_match=0
	fi

	if [ "$case_expected" = "pass" ]; then
		if [ "$case_match" -eq 1 ]; then
			pass=$((pass + 1))
			printf "PASS  %s\n" "$case_relpath"
		else
			fail=$((fail + 1))
			printf "FAIL  %s\n" "$case_relpath"
			printf "  ref_status=%s kbsh_status=%s\n" "$ref_status" "$kb_status"
		fi
		return
	fi

	if [ "$case_expected" = "xfail" ]; then
		if [ "$case_match" -eq 1 ]; then
			xpass=$((xpass + 1))
			printf "XPASS %s\n" "$case_relpath"
		else
			xfail=$((xfail + 1))
			printf "XFAIL %s\n" "$case_relpath"
		fi
		return
	fi

	printf "posix-test: invalid expectation '%s' for %s\n" \
		"$case_expected" "$case_relpath" >&2
	fail=$((fail + 1))
}

while IFS= read -r case_line || [ -n "$case_line" ]; do
	case "$case_line" in
	''|'#'*) continue ;;
	esac
	set -- $case_line
	run_case "$1" "$2"
done <"$CASES_FILE"

printf "posix-test: total=%s pass=%s fail=%s xfail=%s xpass=%s\n" \
	"$total" "$pass" "$fail" "$xfail" "$xpass"

if [ "$fail" -ne 0 ]; then
	exit 1
fi

if [ "$STRICT_XPASS" = "1" ] && [ "$xpass" -ne 0 ]; then
	exit 1
fi
