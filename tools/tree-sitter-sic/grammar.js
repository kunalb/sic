// Tree-sitter grammar for (sic). Atoms are whitespace/paren-delimited,
// so lexing is one regex per atom category; structure is just lists.
module.exports = grammar({
  name: 'sic',

  extras: $ => [/\s/, $.comment],

  rules: {
    source_file: $ => repeat($._form),

    _form: $ => choice($.list, $._atom),

    list: $ => seq('(', repeat($._form), ')'),

    _atom: $ => choice(
      $.number, $.string, $.character, $.type, $.preproc, $.symbol),

    comment: _ => token(/;[^\n]*/),
    string: _ => token(/"(\\.|[^"\\])*"/),
    character: _ => token(/'([^'\\]|\\[^']+)'/),
    // An optional sign then a digit starts a number; the rest of the
    // atom rides along so 1.0f, 0xFF, and 1e-9 are single tokens.
    number: _ => token(prec(1, /[+-]?[0-9][^\s()"';]*/)),
    type: _ => token(/:[^\s()"';]+/),
    preproc: _ => token(/#[^\s()"';]+/),
    symbol: _ => token(/[^\s()"';:#0-9][^\s()"';]*/),
  },
});
