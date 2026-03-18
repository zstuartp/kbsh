#!/usr/bin/env kbsh
/bin/sh -c 'printf "stderr\n" >&2' 2>/tmp/kbsh_smoke_stderr.txt
cat /tmp/kbsh_smoke_stderr.txt
/bin/sh -c 'printf "merged\n" >&2' >/tmp/kbsh_smoke_merged.txt 2>&1
cat /tmp/kbsh_smoke_merged.txt
rm -f /tmp/kbsh_smoke_stderr.txt /tmp/kbsh_smoke_merged.txt
