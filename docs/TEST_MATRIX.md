# TEST MATRIX

Ниже покрытие ключевых требований ТЗ тестами из `tests/unit_tests.c` и `tests/smoke_tests.c`.

## Директивы

- `#define`: `define_and_ifdef_branch`, `macro_substitution_basic`, `macro_redefinition_works`, `empty_macro_replacement`
- `#undef`: `undef_removes_macro`
- `#ifdef/#ifndef/#else/#endif`: `ifndef_and_else_branch`, `else_branch_when_macro_not_defined`, `nested_conditionals`
- ошибки условных блоков: `else_without_if_reports_error`, `endif_without_if_reports_error`, `duplicate_else_reports_syntax_error`, `unclosed_conditional_reports_error`
- неизвестные директивы strict/non-strict: `unknown_directive_strict_mode_reports_error`, `unknown_directive_non_strict_is_kept`

## Подстановка макросов

- по границам идентификатора: `macro_substitution_identifier_boundaries`
- исключение строковых литералов: `macro_not_replaced_in_string_literal`
- исключение `//`: `macro_not_replaced_in_line_comment`
- исключение `/* ... */`: `macro_not_replaced_in_block_comment`, `macro_not_replaced_in_multiline_block_comment`
- директивы внутри block-comment: `directive_inside_block_comment_not_processed`

## Конфигурация и лимиты

- лимит входа/выхода: `input_limit_is_enforced`, `output_limit_is_enforced`
- лимит глубины/макросов: `depth_limit_is_enforced`, `macro_limit_is_enforced`
- режим block-comments: `block_comments_disabled_strict_mode_reports_error`, `block_comments_disabled_non_strict_keeps_plain_text_behavior`, `unclosed_block_comment_strict_mode_reports_error`, `unclosed_block_comment_non_strict_is_allowed`

## Совместимость ввода

- CRLF: `crlf_preserved_in_output`
- smoke по примеру из ТЗ: `tests/smoke_tests.c`

## Устойчивость

- property/determinism: `tests/property_tests.c` (CTest: `pp_property_tests`)
