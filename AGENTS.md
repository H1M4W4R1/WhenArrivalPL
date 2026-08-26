# AGENTS.md — C / C++ Project Rules

Author: H1M4W4R1

This file defines the coding and project-structure rules for C and C++ firmware projects. The rules are inspired by NASA/JPL’s “Power of 10” safety-oriented C rules, but adapted for practical embedded development.

The project assumes compilation with all common warnings enabled, but not necessarily with strict pedantic mode. Function pointers are allowed when they improve architecture, dispatch, driver abstraction, or callback handling.

## Core priorities

Code must be simple, deterministic, reviewable, and suitable for embedded systems.

Prefer boring, explicit code over clever abstractions. Avoid hidden behavior, uncontrolled dynamic allocation, unbounded execution, and ambiguous ownership. The code should be easy to inspect by a human and easy to analyze by tools.

## Language standard

Use C or C++ according to the project configuration.

For C projects, prefer modern C such as C99 or C11 unless the target toolchain requires otherwise.

For C++ projects, prefer a restricted embedded-friendly subset. Avoid exceptions, RTTI, hidden heap allocation, and complex template metaprogramming unless the project explicitly allows them.

Headers must be compatible with C++ inclusion when practical:

```c
#ifdef __cplusplus
extern "C" {
#endif

/* declarations */

#ifdef __cplusplus
}
#endif
```

## Compiler warnings

Compile with all common warnings enabled.

Recommended baseline for GCC/Clang-style toolchains:

```txt
-Wall -Wextra -Wconversion -Wshadow -Wundef -Wdouble-promotion -Wformat=2
```

Pedantic warnings are not required by this project.

Warnings must not be ignored. Fix warnings in project code. If a warning must be suppressed, the reason must be local, documented, and narrow in scope.

For external libraries (e.g. Arduino.h) wrapping is required to prevent acquisition of external issues:
```cpp
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wundef"
#include <Arduino.h>
// Other libraries
#pragma GCC diagnostic pop
```

## Power-of-10-inspired rules

### 1. Keep control flow simple

Avoid complex control flow.

Do not use `goto`.

Avoid recursion.

Avoid deeply nested logic. Prefer early returns for error handling where it makes the code clearer.

Keep loops simple and bounded wherever possible.

### 2. Use bounded loops

Every loop should have a clear upper bound.

Avoid infinite loops except in the top-level firmware scheduler, main loop, RTOS task body, or hardware wait loop. When an infinite loop is intentional, document the reason.

Prefer this:

```c
for (size_t i = 0; i < item_count; ++i)
{
    process_item(&items[i]);
}
```

Avoid unbounded polling without timeout:

```c
while (!device_ready())
{
    /* bad unless there is a timeout or this is a deliberate blocking wait */
}
```

### 3. Avoid dynamic memory allocation after initialization

Avoid `malloc`, `calloc`, `realloc`, `free`, `new`, and `delete` after system initialization.

Prefer static allocation, stack allocation, fixed-size pools, or caller-provided buffers.

If dynamic allocation is used during initialization, ownership must be explicit and failure must be handled.

### 4. Keep functions small and focused

Functions should do one thing.

Prefer short functions that are easy to review. As a guideline, keep functions under roughly 60 lines where practical.

Split large functions into smaller internal helpers.

### 5. Use strong typing and explicit sizes

Use fixed-width integer types from `<stdint.h>` / `<cstdint>` for protocol, storage, hardware register, and binary-layout code.

Use `size_t` for sizes and indexes where appropriate.

Avoid plain `int` for values whose size matters.

Prefer explicit enums for states, modes, commands, and error classes.

### 6. Check return values

Check return values from functions that can fail.

Do not discard error codes silently.

If a return value is intentionally ignored, make that explicit and document why.

```c
(void)driver_flush(&driver); /* best-effort cleanup during shutdown */
```

### 7. Limit preprocessor use

Use the preprocessor only for include guards, feature flags, constants required by the compiler, and conditional platform support.

Avoid complex macros.

Prefer `static inline` functions, enums, and typed constants over function-like macros.

### 8. Restrict pointer complexity

Avoid pointer tricks.

Avoid more than two levels of indirection unless required by an API.

Pointers must have clear ownership and lifetime.

Validate pointers at module boundaries unless the function contract explicitly forbids null.

### 9. Avoid hidden global state

Global mutable state should be avoided.

When persistent state is needed, place it in an explicit context structure and pass it to functions.

Module-private state is allowed when required for hardware, interrupts, callbacks, or platform APIs, but it must be clearly contained inside the module.

### 10. Make code reviewable by tools and humans

Keep code simple enough for static analysis.

Avoid clever expressions with side effects.

Do not write multiple operations with hidden sequencing dependencies in one expression.

Prefer clarity over compactness.

## Project-specific additional rules

### 11. File naming

Use `sys_xxx` for system or peripheral-related files.

Examples:

```txt
sys_ble.c
sys_ble.h
sys_rf.c
sys_rf.h
sys_storage.c
sys_storage.h
```

Use `fw_xxx` for firmware-only logic that is not directly tied to a peripheral.

Examples:

```txt
fw_session.c
fw_session.h
fw_auth.c
fw_auth.h
fw_timer.c
fw_timer.h
```

Use lowercase snake_case for file names.

### 12. Author metadata

Project files, generated headers, and templates should identify the author as:

```txt
H1M4W4R1
```

Use the project’s existing license and copyright format when present.

### 13. Folder structure

Use the same logical folders under both `include/` and `src/`.

```txt
include/
  drivers/
  systems/
  operation/
  ui/

src/
  drivers/
  systems/
  operation/
  ui/
```

Folder meanings:

```txt
drivers/    Peripheral drivers and hardware-specific device access.
systems/    Low-level system modules and platform services.
operation/  User-level firmware behavior, workflows, sessions, and application logic.
ui/         User interface code. HMI code belongs here, not in operation/.
```

Examples:

```txt
include/drivers/driver_display.h
include/systems/sys_ble.h
include/operation/fw_session.h
include/ui/ui_status_screen.h

src/drivers/driver_display.c
src/systems/sys_ble.c
src/operation/fw_session.c
src/ui/ui_status_screen.c
```

### 14. Header placement

Do not place headers in `src/`.

All public and internal project headers must be placed under `include/`.

A source file in `src/` may include a matching header from `include/`.

Correct:

```txt
include/systems/sys_ble.h
src/systems/sys_ble.c
```

Incorrect:

```txt
src/systems/sys_ble.h
src/systems/sys_ble.c
```

## Naming rules

### Types

All project-defined types must use lowercase snake_case and end with `_t`.

Examples:

```c
typedef struct
{
    uint32_t id;
    uint8_t flags;
} device_status_t;

typedef enum
{
    session_state_idle = 0,
    session_state_active,
    session_state_error
} session_state_t;

typedef void (*ble_write_callback_t)(const char *value);
```

Do not use Hungarian notation.

Avoid prefixes such as:

```txt
g_
s_
m_
p_
str_
u32_
b_
```

The only allowed prefix is a single leading underscore for private data or private implementation details.

Examples:

```c
typedef struct
{
    uint32_t _last_tick_ms;
    bool _is_connected;
} sys_ble_t;
```

Use the underscore prefix sparingly and only for private structure members or private internal symbols where it improves clarity.

Do not use reserved C/C++ identifiers. Avoid names beginning with double underscores or an underscore followed by an uppercase letter.

### Functions

Use lowercase snake_case for functions.

Function names should start with the module name.

Examples:

```c
void sys_ble_init(sys_ble_t *ble);
void sys_ble_start_advertising(sys_ble_t *ble);
void fw_session_start(fw_session_t *session);
void ui_status_screen_render(ui_status_screen_t *screen);
```

### Variables

Use lowercase snake_case for variables.

Do not use Hungarian notation.

Avoid global-style prefixes such as `g_` and static prefixes such as `s_`.

Prefer meaningful names:

```c
uint32_t elapsed_ms;
bool is_connected;
size_t payload_length;
```

### Constants

Use lowercase snake_case for typed constants where possible.

```c
static const uint32_t ble_advertising_timeout_ms = 30000u;
```

Preprocessor constants may use uppercase only when required by platform convention or build configuration.

```c
#define PROJECT_USE_BLE 1
```

### Enums

Enum type names must end with `_t`.

Enum values should be prefixed with the enum’s logical name, not with Hungarian notation.

```c
typedef enum
{
    ble_state_idle = 0,
    ble_state_advertising,
    ble_state_connected,
    ble_state_error
} ble_state_t;
```

## Header rules

Every header must have an include guard.

Use a guard based on the path.

```c
#ifndef SYSTEMS_SYS_BLE_H
#define SYSTEMS_SYS_BLE_H

/* declarations */

#endif /* SYSTEMS_SYS_BLE_H */
```

Headers must include what they use.

Do not rely on include order.

Headers should expose only the minimum required public API.

Private helper declarations should stay in the `.c` / `.cpp` file unless they are needed by other modules.

## Module structure

Each module should normally have one header and one source file.

Example:

```txt
include/systems/sys_ble.h
src/systems/sys_ble.c
```

The header contains public types and function declarations.

The source file contains implementation details, private helpers, and private module state if unavoidable.

Preferred layout for source files:

```c
#include "systems/sys_ble.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* private constants */
/* private types */
/* private function declarations */
/* private data */
/* public functions */
/* private functions */
```

## Error handling

Use explicit error codes for recoverable failures.

Prefer project-specific result enums:

```c
typedef enum
{
    fw_result_ok = 0,
    fw_result_error,
    fw_result_timeout,
    fw_result_invalid_argument,
    fw_result_busy
} fw_result_t;
```

Do not use ambiguous boolean returns for operations that can fail in multiple ways.

Boolean returns are acceptable for simple predicates:

```c
bool sys_ble_is_connected(const sys_ble_t *ble);
```

## Memory and buffer handling

All buffer-writing functions must receive the buffer pointer and buffer size.

The function must not write past the provided size.

Prefer functions that return the number of bytes written or a result code.

```c
fw_result_t fw_session_format_time(
    const fw_session_t *session,
    char *buffer,
    size_t buffer_size);
```

Avoid unsafe string functions such as `strcpy`, `strcat`, and `sprintf`.

Prefer bounded alternatives such as `snprintf`, with return-value checks.

## Function pointers

Function pointers are allowed.

Use them for callbacks, event dispatch, driver abstraction, protocol handlers, and platform adaptation.

Function pointer types must be named with `_t`.

```c
typedef void (*sys_ble_disconnect_callback_t)(void *user_context);

typedef struct
{
    sys_ble_disconnect_callback_t on_disconnect;
    void *user_context;
} sys_ble_callbacks_t;
```

Function pointers must be checked before use unless guaranteed by initialization.

```c
if (ble->_callbacks.on_disconnect != NULL)
{
    ble->_callbacks.on_disconnect(ble->_callbacks.user_context);
}
```

Avoid large, opaque callback webs. Keep callback ownership and call context documented.

## C++ restrictions

For C++ firmware code:

Avoid exceptions unless the project explicitly enables them.

Avoid RTTI unless required.

Avoid heap allocation after initialization.

Avoid complex inheritance trees.

Prefer composition over inheritance.

Prefer `enum class` for strongly typed enums when the codebase is C++-only.

Use RAII only when lifetime is clear and deterministic.

Do not hide hardware side effects in constructors, destructors, overloaded operators, or implicit conversions.

Avoid templates unless they provide clear compile-time value without harming readability.

## Concurrency and interrupts

Interrupt handlers must be short and deterministic.

Do not allocate memory in interrupts.

Do not block in interrupts.

Do not call non-ISR-safe APIs from interrupts.

Shared data between interrupts and main code must be marked and protected appropriately.

Use `volatile` only for memory-mapped registers or variables shared with interrupts where required. Do not use `volatile` as a general synchronization mechanism.

## Hardware access

Hardware register access must be isolated to drivers or low-level systems.

Application logic must not directly manipulate hardware registers.

Peripheral code belongs in `drivers/` or `systems/`.

User-level behavior belongs in `operation/`.

UI and HMI behavior belongs in `ui/`.

## Comments and documentation

Comment why something is done, not what the code obviously does.

Document assumptions, units, ranges, timing constraints, and hardware side effects.

Public functions should have short comments when their behavior, ownership, timing, or failure modes are not obvious.

Example:

```c
/* Starts BLE advertising. Safe to call again after disconnect. */
fw_result_t sys_ble_start_advertising(sys_ble_t *ble);
```

## Forbidden or discouraged patterns

Do not use:

```txt
goto
recursion
unbounded loops without a documented reason
hidden dynamic allocation
ignored error codes
large macros
headers inside src/
Hungarian notation
g_ / s_ / m_ / p_ variable prefixes
```

Avoid:

```txt
global mutable state
deep nesting
large functions
implicit ownership
side effects hidden in expressions
complex C++ templates
exceptions in firmware
RTTI in firmware
```

## Agent instructions

When modifying this repository, follow these rules.

Before editing code:

1. Inspect the relevant existing module structure.
2. Preserve the folder layout.
3. Place headers under `include/`.
4. Place implementation files under `src/`.
5. Use `sys_xxx` for peripheral/system modules.
6. Use `fw_xxx` for firmware behavior modules.
7. Follow `_t` type naming.
8. Do not introduce Hungarian notation.
9. Do not introduce `g_` or `s_` prefixes.

When adding C++ code:

1. Keep it compatible with embedded constraints.
2. Do not introduce exceptions, RTTI, or hidden heap allocation unless explicitly approved.
3. Prefer simple classes with explicit ownership and deterministic lifetimes.

When in doubt, choose the simpler implementation.
