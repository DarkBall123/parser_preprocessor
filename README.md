# parser_preprocessor

`parser_preprocessor` is a modular C library that preprocesses source text provided as a null-terminated string. The project is designed for SQF/Arma-style directive workflows and is independent of the Arma runtime.

## Scope

The library currently provides:

- Directive processing:
  - `#define`
  - `#undef`
  - `#ifdef`
  - `#ifndef`
  - `#else`
  - `#endif`
- Conditional block evaluation with nesting control.
- Object-like macro replacement in active text regions.
- Deterministic diagnostics (`status`, `line`, `column`, `message`).
- Configurable limits for input size, output size, macro count, and conditional depth.

## Macro Expansion Rules

Macro expansion is applied only to active non-directive text and follows these rules:

- Replacement is token-based (identifier boundaries only).
- No replacement inside string literals (`"..."`).
- No replacement inside `// ...` comments.
- No replacement inside `/* ... */` comments when block comment support is enabled.

## Build Requirements

- C compiler with C99 support (Clang/GCC/MSVC-compatible toolchain)
- CMake 3.16 or newer

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

Test targets:

- `pp_unit_tests`
- `pp_smoke_tests`
- `pp_property_tests`

## Sanitizer Build

```bash
cmake -S . -B build-asan -DPP_ENABLE_SANITIZERS=ON
cmake --build build-asan
ctest --test-dir build-asan --output-on-failure
```

## Public API

Primary entry point:

```c
pp_status_code pp_preprocess(
    const char *source,
    const pp_config *config,
    pp_include_resolver_fn include_resolver,
    void *include_user_ctx,
    pp_result *out_result
);
```

Configuration defaults are initialized via:

```c
void pp_config_init(pp_config *config);
```

Allocated output and diagnostics must be released via:

```c
void pp_result_dispose(pp_result *result);
```

## Memory Ownership

- `source` and `config` are owned by the caller.
- On success, `pp_preprocess` allocates `out_result->out_text`.
- On failure, `pp_preprocess` may allocate `out_result->diagnostic.message`.
- Caller must release result-owned memory with `pp_result_dispose`.

## Project Layout

- `include/preprocessor/preprocessor.h` — public API
- `src/` — implementation modules (`api`, `core_state`, `macro_table`, `condition_stack`, `expander`, `buffer`, `diagnostics`)
- `tests/` — unit, smoke, and property tests
- `docs/` — API contract, limitations, and requirement coverage matrix

## Current Limitation

`#include` is intentionally not implemented yet. Active `#include` directives return a defined error status according to configuration.
