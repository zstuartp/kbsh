#!/usr/bin/env kbsh
false
echo "$?"
echo hello | false
echo "$?"
result=$(false)
echo "$?"
