# kcalc

A calculator that runs directly inside the Linux kernel. Expressions are
sent to a character device; the kernel tokenizes, parses (shunting-yard),
and evaluates them, and the result is read back from the same device.

Originally a from-scratch userspace C port of a
[shunting-yard implementation](https://github.com/xDDoubleTea/shuntingyard),
ported into a loadable kernel module.

## Demo

![Demo](assets/demo.gif)

## Features

- Standard arithmetic: `+ - * / % ^`, parentheses, unary `+`/`-`.
- Operator precedence and right-associativity (`^`) handled via
  shunting-yard, same as the userspace reference implementation.
- Overflow-checked arithmetic (`check_add_overflow` et al.) — returns
  `-EOVERFLOW` rather than silently wrapping.
- Character device interface (`/dev/kcalc_chardev`): `write()` an
  expression, `read()` the result.

## Requirements

- Linux kernel headers matching your running kernel
  (`linux-headers-$(uname -r)` or equivalent).
- `gcc`, `make`.
- `python3` + `pytest` for the test suite.

## Build

```sh
make
```

Produces `kcalc.ko`, composed from `kcalc_main.c`, `kcalc_tokenize.c`,
`kcalc_shunting_yard.c`, `kcalc_eval.c`, and `kcalc_chardev.c` (see
[Architecture](#architecture)).

## Load / unload

```sh
sudo insmod kcalc.ko
sudo dmesg -L        # check for load confirmation, major number
sudo rmmod kcalc
```

For verbose per-stage debug output (tokenize/shunting-yard/eval
intermediate state), enable dynamic debug at load time:

```sh
sudo insmod kcalc.ko dyndbg=+p
```

## Usage

Once loaded, `/dev/kcalc_chardev` accepts one expression per write and
returns the decimal result on the next read.

```sh
printf '%s' "12 + 300 - 5" | sudo tee /dev/kcalc_chardev > /dev/null
sudo cat /dev/kcalc_chardev
# 307
```

> Notes:
>
> - Use `printf '%s'`, not plain `echo`, to avoid sending a trailing
>   newline — the tokenizer rejects unrecognized characters, including
>   `\n`.
> - `tee` echoes its stdin to your terminal in addition to writing the
>   device; redirect to `/dev/null` if you only want the device write.
> - The device node is root-only by default (`crw-------`); commands
>   above assume `sudo`.
> - On failure, `write()` returns a negative errno and no result is
>   stored (e.g. malformed expression → `-EINVAL`, "Invalid argument";
>   arithmetic overflow → `-EOVERFLOW`, "Value too large for defined
>   data type").

You can also compile and execute the provided `demo.c`

```sh
gcc -Wall -Wextra -o demo.out demo.c
sudo ./demo.out
```

Then start typing expressions, send EOF to exit.

## Architecture

| File                    | Responsibility                                             |
| ----------------------- | ---------------------------------------------------------- |
| `kcalc.h`               | Shared `Token`/`token_t` types, public prototypes          |
| `kcalc_stack.h`         | Generic stack macro (`DEFINE_STACK`)                       |
| `kcalc_tokenize.c`      | Lexes an expression string into a `Token` array            |
| `kcalc_shunting_yard.c` | Infix → postfix via the shunting-yard algorithm            |
| `kcalc_eval.c`          | Evaluates a postfix `Token` array, with overflow checks    |
| `kcalc_chardev.c`       | `/dev/kcalc_chardev` — wires the above into `read`/`write` |
| `kcalc_main.c`          | Module init/exit, wiring `kcalc_chardev_init/exit`         |

Error handling convention throughout: functions return an `int` status
code (`0` on success, negative errno otherwise); results are written
through an out-parameter rather than as the return value. This mirrors
the kernel's own `read`/`write` conventions and keeps error paths
uniform across the pipeline.

## Testing

```sh
pytest test_kcalc.py
```

The suite builds the module, loads it once for the whole session, then
exercises `/dev/kcalc_chardev` per test case via `tee`/`cat`
(stdin-based, not shell redirection, so expressions containing shell
metacharacters are never misinterpreted). Requires root or passwordless
`sudo` for `insmod`/`rmmod`/`tee`/`cat`, and cannot run in a container
without kernel module privileges.

## License

GPL-2.0, see `LICENSE`.

## Notes on development process

This project's kernel-programming conventions (module structure,
`copy_to_user`/`copy_from_user` usage, error-handling patterns, char
device lifecycle) were learned primarily from LKMPG
(sysprog21/lkmpg) and the kernel source tree. AI assistance (Claude)
was used throughout development for code review, debugging build/runtime
errors, explaining kernel APIs and conventions, and as a learning aid —
all algorithm design, kernel module architecture decisions, and testing
strategy are my own.

## References

[The Linux Kernel Module Programming Guide](https://sysprog21.github.io/lkmpg/)

[sysprog21/lkmpg: The Linux Kernel Module Programming Guide (updated for 5.0+ kernels)](https://github.com/sysprog21/lkmpg)
