# POSIX Test Harness

`test/posix/run.sh` compares kbsh behavior against a reference POSIX shell
(default: `/bin/sh`) for each case listed in `cases.list`.

## Case expectations

- `pass`: kbsh must match reference stdout/stderr/exit-status.
- `xfail`: known gap; mismatch is expected for now.

## Usage

```sh
make test-posix
```

Optional environment variables:

- `REF_SHELL=/path/to/sh`
- `STRICT_XPASS=1` (treat unexpected passes as failure)
- `CASES_FILE=/path/to/cases.list`
