# API Contract (Iteration 4)

## Core Entry Point

```c
pp_status_code pp_preprocess(
    const char *source,
    const pp_config *config,
    pp_include_resolver_fn include_resolver,
    void *include_user_ctx,
    pp_result *out_result
);
```

## Ownership and Lifetime

- `source` и `config` остаются во владении вызывающего кода.
- На успехе `pp_preprocess` выделяет `out_result->out_text`.
- На ошибке может выделяться `out_result->diagnostic.message`.
- Освобождение всех выделенных полей результата: `pp_result_dispose(&result)`.

## Config Defaults

`pp_config_init(&cfg)` выставляет:
- `max_input_len = 0` (без лимита)
- `max_output_len = 0` (без лимита)
- `max_macro_count = 4096`
- `max_conditional_depth = 256`
- `max_include_depth = 64`
- `enable_include = 0`
- `strict_mode = 1`
- `support_block_comments = 1`

## Result Rules

- На `PP_STATUS_OK`: `out_text != NULL`, `out_len` валиден.
- На ошибке: выходной текст не использовать.
- `diagnostic.line/column` заполняются минимум с `1:1` для ошибок уровня входа.

## Directive Support

Поддержаны:
- `#define`
- `#undef`
- `#ifdef`
- `#ifndef`
- `#else`
- `#endif`

Поведение:
- директивы не попадают в выходной текст;
- неизвестные директивы:
  - `strict_mode = 1` -> ошибка `PP_STATUS_UNKNOWN_DIRECTIVE`
  - `strict_mode = 0` -> строка сохраняется как обычный текст в активной области

## Macro Expansion Support

Поддержано:
- подстановка только в активных текстовых строках (не в директивах);
- подстановка по границам идентификатора;
- без подстановки внутри строковых литералов (`"..."`);
- без подстановки внутри `//` комментариев;
- без подстановки внутри `/* ... */` комментариев.
- директивы внутри открытого многострочного комментария обрабатываются как обычный текст.

Параметр `support_block_comments`:
- `1` -> `/* ... */` обрабатываются как комментарии, включая многострочные;
- `1` и `strict_mode = 1` -> незакрытый block-comment к концу ввода дает `PP_STATUS_SYNTAX_ERROR`;
- `0` и `strict_mode = 1` -> `PP_STATUS_SYNTAX_ERROR` при встрече `/*`;
- `0` и `strict_mode = 0` -> блок-комментарии не распознаются как комментарии.

## Determinism

- одинаковый `source + config` всегда дает одинаковый `status/out_text/diagnostic`.
- это дополнительно проверяется property-тестом.

## Include Resolver

`#include` в этой итерации еще не реализован. Если директива активна:
- при `enable_include = 0` -> `PP_STATUS_INCLUDE_DISABLED`
- при `enable_include = 1` -> `PP_STATUS_INTERNAL_ERROR`
