# kbsh

*Kept-Bounded Shell*

A small, from-scratch POSIX shell.

The session loop is driven by a static state-transition map, and the memory
model uses a statically allocated arena in its core.

## Build

```sh
make
make test
make install PREFIX=$HOME/.local
```

## Usage

```sh
kbsh                      # interactive
kbsh script.sh            # run a script
kbsh -c 'echo hello'      # command string
```

## Status

Work in progress toward POSIX compliance.

Current `devel` branch target: `0.3.0-dev`

Current coverage includes:

- parameter expansion
- pipelines and exit-status propagation
- basic file redirection (`>`, `>>`, `<`, `2>`, `2>&1`)
- custom interactive line editing
- early command substitution (`$()`)

See `test/posix/cases.list` for current POSIX coverage.

## License

GPLv3. See `COPYING`.

## Links

- Source: <https://github.com/zstuartp/kbsh/>
- Bugs: <parsons.zackary@gmail.com>
