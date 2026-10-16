/**
 * Tree-sitter grammar for Azure API Management policy documents.
 *
 * APIM policies are XML, but the gateway pre-processes policy expressions
 * before XML parsing: `@(...)` and `@{...}` are matched by balancing brackets,
 * so their C# bodies may contain raw `"`, `<` and `&` even inside a
 * double-quoted attribute. A stock XML grammar cannot parse that, so the C#
 * bodies are lexed by the external scanner (src/scanner.c) and exposed as
 * `csharp_code` nodes for editors to inject a C# grammar into.
 */

module.exports = grammar({
  name: 'azure_apim',

  externals: $ => [
    // Body of `@( ... )`: everything up to the matching `)`.
    $._paren_code,
    // Body of `@{ ... }`: everything up to the matching `}`.
    $._brace_code,
    // `<!-- ... -->`, lenient about `--` inside the comment as APIM is.
    $.comment,
    // Never produced; marks that tree-sitter is in error recovery.
    $._error_sentinel,
  ],

  extras: $ => [/\s/],

  rules: {
    document: $ => repeat($._node),

    _node: $ => choice(
      $.element,
      $.comment,
      $.cdata,
      $.processing_instruction,
      $.expression,
      $.named_value,
      $.entity,
      $.text,
    ),

    element: $ => choice(
      seq($.start_tag, repeat($._node), $.end_tag),
      $.self_closing_tag,
    ),

    start_tag: $ => seq('<', $.tag_name, repeat($.attribute), '>'),
    end_tag: $ => seq('</', $.tag_name, '>'),
    self_closing_tag: $ => seq('<', $.tag_name, repeat($.attribute), '/>'),

    tag_name: _ => /[A-Za-z_][\w.:-]*/,
    attribute_name: _ => /[A-Za-z_][\w.:-]*/,

    attribute: $ => seq($.attribute_name, '=', $.attribute_value),

    // A policy expression must be the whole attribute value; anything else is
    // literal text that may embed `{{named-value}}` references.
    attribute_value: $ => choice(
      seq('"', optional(choice($.expression, repeat1($._dq_part))), '"'),
      seq('\'', optional(choice($.expression, repeat1($._sq_part))), '\''),
    ),

    _dq_part: $ => choice(alias(/[^"&{@\s][^"&{@]*/, $.text), $._lone_brace, $._lone_at, $.named_value, $.entity),
    _sq_part: $ => choice(alias(/[^'&{@\s][^'&{@]*/, $.text), $._lone_brace, $._lone_at, $.named_value, $.entity),

    expression: $ => choice(
      seq('@(', optional(alias($._paren_code, $.csharp_code)), ')'),
      seq('@{', optional(alias($._brace_code, $.csharp_code)), '}'),
    ),

    // `{{name}}` — a named value reference, or a Liquid output tag inside
    // `<set-body template="liquid">`.
    named_value: _ => token(seq('{{', /[^{}]*/, '}}')),

    entity: _ => token(choice(
      /&[A-Za-z][A-Za-z0-9]*;/,
      /&#[0-9]+;/,
      /&#x[0-9A-Fa-f]+;/,
    )),

    text: $ => prec.right(repeat1(choice(/[^<&{@\s][^<&{@]*/, $._lone_brace, $._lone_at))),

    // A `{` or `@` that doesn't start a named value or expression is plain text
    // (e.g. JSON bodies, e-mail addresses).
    _lone_brace: _ => token(prec(-1, '{')),
    _lone_at: _ => token(prec(-1, '@')),

    cdata: $ => seq('<![CDATA[', optional(alias(/([^\]]|\][^\]]|\]\][^>])+/, $.cdata_content)), ']]>'),

    processing_instruction: _ => token(seq('<?', /([^?]|\?[^>])*/, '?>')),
  },
});
