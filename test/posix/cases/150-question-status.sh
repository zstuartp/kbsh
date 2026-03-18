# POSIX testcase: keep /bin/sh syntax only.
false
echo "$?"
echo hello | false
echo "$?"
result=$(false)
echo "$?"
