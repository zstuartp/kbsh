# POSIX testcase: keep /bin/sh syntax only.
/bin/sh -c 'printf "stderr\n" >&2' 2>/tmp/kbsh_posix_stderr.txt
cat /tmp/kbsh_posix_stderr.txt
/bin/sh -c 'printf "merged\n" >&2' >/tmp/kbsh_posix_merged.txt 2>&1
cat /tmp/kbsh_posix_merged.txt
rm -f /tmp/kbsh_posix_stderr.txt /tmp/kbsh_posix_merged.txt
