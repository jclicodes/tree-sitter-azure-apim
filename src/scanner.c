// External scanner for APIM policy expression bodies.
//
// Consumes the C# inside `@( ... )` or `@{ ... }` up to (not including) the
// bracket that closes the expression. Brackets inside string literals, char
// literals, comments and interpolated-string text are ignored, mirroring how
// the APIM gateway finds the end of an expression.

#include "tree_sitter/parser.h"

#include <stdbool.h>
#include <stdint.h>

enum TokenType {
  PAREN_CODE,
  BRACE_CODE,
  COMMENT,
  ERROR_SENTINEL,
};

static inline void advance(TSLexer *lexer) { lexer->advance(lexer, false); }

static bool scan_code(TSLexer *lexer, int32_t open, int32_t close);

// Lexer is just past the opening quote of a string literal. Consumes through
// the closing quote. Regular strings stop at a newline so an unterminated
// literal can't swallow the rest of the document.
static void scan_string(TSLexer *lexer, bool verbatim, bool interpolated) {
  while (!lexer->eof(lexer)) {
    int32_t c = lexer->lookahead;
    if (c == '"') {
      advance(lexer);
      if (verbatim && lexer->lookahead == '"') {
        advance(lexer);
        continue;
      }
      return;
    }
    if (!verbatim && (c == '\n' || c == '\r')) return;
    if (!verbatim && c == '\\') {
      advance(lexer);
      if (!lexer->eof(lexer)) advance(lexer);
      continue;
    }
    if (interpolated && c == '{') {
      advance(lexer);
      if (lexer->lookahead == '{') {
        advance(lexer);
        continue;
      }
      scan_code(lexer, '{', '}');
      if (lexer->lookahead == '}') advance(lexer);
      continue;
    }
    advance(lexer);
  }
}

// Lexer is just past the opening `'` of a char literal.
static void scan_char(TSLexer *lexer) {
  while (!lexer->eof(lexer)) {
    int32_t c = lexer->lookahead;
    if (c == '\n' || c == '\r') return;
    advance(lexer);
    if (c == '\'') return;
    if (c == '\\' && !lexer->eof(lexer)) advance(lexer);
  }
}

// Consumes C# code until an unmatched `close` (left unconsumed) or EOF.
// Returns whether anything was consumed.
static bool scan_code(TSLexer *lexer, int32_t open, int32_t close) {
  bool consumed = false;
  uint32_t depth = 0;

  while (!lexer->eof(lexer)) {
    int32_t c = lexer->lookahead;

    if (c == close) {
      if (depth == 0) return consumed;
      depth--;
      advance(lexer);
    } else if (c == open) {
      depth++;
      advance(lexer);
    } else if (c == '"') {
      advance(lexer);
      scan_string(lexer, false, false);
    } else if (c == '\'') {
      advance(lexer);
      scan_char(lexer);
    } else if (c == '@' || c == '$') {
      // String prefixes: @"..", $"..", $@"..", @$"..
      bool verbatim = c == '@';
      bool interpolated = c == '$';
      advance(lexer);
      if (lexer->lookahead == '@' || lexer->lookahead == '$') {
        verbatim = verbatim || lexer->lookahead == '@';
        interpolated = interpolated || lexer->lookahead == '$';
        advance(lexer);
      }
      if (lexer->lookahead == '"') {
        advance(lexer);
        scan_string(lexer, verbatim, interpolated);
      }
    } else if (c == '/') {
      advance(lexer);
      if (lexer->lookahead == '/') {
        while (!lexer->eof(lexer) && lexer->lookahead != '\n') advance(lexer);
      } else if (lexer->lookahead == '*') {
        advance(lexer);
        while (!lexer->eof(lexer)) {
          int32_t d = lexer->lookahead;
          advance(lexer);
          if (d == '*' && lexer->lookahead == '/') {
            advance(lexer);
            break;
          }
        }
      }
    } else {
      advance(lexer);
    }
    consumed = true;
  }
  return consumed;
}

void *tree_sitter_azure_apim_external_scanner_create(void) { return NULL; }

void tree_sitter_azure_apim_external_scanner_destroy(void *payload) { (void)payload; }

unsigned tree_sitter_azure_apim_external_scanner_serialize(void *payload, char *buffer) {
  (void)payload;
  (void)buffer;
  return 0;
}

void tree_sitter_azure_apim_external_scanner_deserialize(void *payload, const char *buffer,
                                                         unsigned length) {
  (void)payload;
  (void)buffer;
  (void)length;
}

bool tree_sitter_azure_apim_external_scanner_scan(void *payload, TSLexer *lexer,
                                                  const bool *valid_symbols) {
  (void)payload;
  if (valid_symbols[ERROR_SENTINEL]) return false;

  if (valid_symbols[PAREN_CODE] || valid_symbols[BRACE_CODE]) {
    bool paren = valid_symbols[PAREN_CODE];
    if (!scan_code(lexer, paren ? '(' : '{', paren ? ')' : '}')) return false;
    lexer->mark_end(lexer);
    lexer->result_symbol = paren ? PAREN_CODE : BRACE_CODE;
    return true;
  }

  if (valid_symbols[COMMENT]) {
    while (lexer->lookahead == ' ' || lexer->lookahead == '\t' || lexer->lookahead == '\n' ||
           lexer->lookahead == '\r') {
      lexer->advance(lexer, true);
    }
    if (lexer->lookahead != '<') return false;
    advance(lexer);
    if (lexer->lookahead != '!') return false;
    advance(lexer);
    for (int i = 0; i < 2; i++) {
      if (lexer->lookahead != '-') return false;
      advance(lexer);
    }
    unsigned dashes = 0;
    while (!lexer->eof(lexer)) {
      int32_t c = lexer->lookahead;
      advance(lexer);
      if (c == '>' && dashes >= 2) {
        lexer->mark_end(lexer);
        lexer->result_symbol = COMMENT;
        return true;
      }
      dashes = c == '-' ? dashes + 1 : 0;
    }
    return false;
  }
  return false;
}
