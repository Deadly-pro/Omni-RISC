# firmware/lib — where the firmware track's own sources live

Every `*.c` in this directory is compiled and linked into the **non-RTOS** apps
(see `LIB_SRCS` / `LIB_OBJ` in `../Makefile`), with `-fno-builtin`. This is
where the firmware-track phases put their work:

| Phase | File | Interface | Gate |
|---|---|---|---|
| F1 | `omni_string.c` | `include/omni/string.h` | `tests/test_libc.sh`, `tests/test_convert.sh` |
| F2 | `omni_printf.c` | `include/omni/stdio.h` | `tests/test_printf.sh` |
| F5 | `omni_alloc.c` | `include/omni/alloc.h` (to come) | `tests/test_alloc.sh` |

New interface headers go in `../include/omni/` so apps can `#include <omni/...>`.

The RTOS apps (`rtos`, `rtos_q`, `rtos_sem`, `rtos_metrics`, `shell`) do **not**
link this directory — they use `../rtos/omni_libc.c` instead, so a `memcpy` here
can never collide with the one there. To put your library on the RTOS path too,
add it to the RTOS link explicitly and drop the duplicate from `omni_libc.c`.

Two things that bite in a `-nostdlib` build with no libgcc:

- **No 64-bit helpers.** `__udivdi3` / `__muldi3` / friends do not exist, so a
  64-bit divide or multiply becomes an undefined reference at link time. Keep
  the arithmetic 32-bit — the F1 conversion accumulator saturates as soon as it
  would pass `ULONG_MAX` for exactly this reason, and still consumes the
  remaining digits so `endptr` lands correctly.
- **gcc rewrites loops into library calls.** `memset`-shaped and `strlen`-shaped
  loops get pattern-matched into the very functions you are implementing. That is
  why this directory (and the gate apps) build with `-fno-builtin`: the gate must
  exercise *your* code, not gcc's.
