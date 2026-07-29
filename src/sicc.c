#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <math.h>
#include <regex.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// === Headers & Data Structures ===

typedef enum Tag Tag;
typedef struct Obj Obj;
typedef struct SrcFile SrcFile;
typedef struct Pos Pos;
typedef struct Parser Parser;
typedef struct List List;
typedef struct Atom Atom;
typedef struct Result Result;
typedef struct CCode CCode;

struct Pos {
  size_t row;
  size_t col;
};

struct SrcFile {
  char *name;
  FILE *fp;
  bool eof;
  Pos pos;
};

struct CCode {
  char **lines;
  size_t count;
  size_t buffer;
};

enum Tag {
  SEXP,
  ATOM,
};

struct List {
  Obj **buffer;
  size_t buffer_len;
  size_t len;
};

List *list_init();
void list_resize(List *, size_t);
void list_add(List *, Obj *);
void list_free(List *);
void list_print(List *, size_t indent);

struct Atom {
  char *buffer;
  size_t buffer_len;
  size_t len;
};

Atom *atom_init();
void atom_resize(Atom *, size_t);
void atom_add(Atom *, char);
void atom_free(Atom *);
void atom_print(Atom *);

struct Result {
  enum { OK, Err } tag;
};

struct Obj {
  Pos beg;
  Pos end;

  Tag tag;
  union {
    Atom *atom;
    List *sexp;
  };
};

Obj *obj_init(Tag tag);
void obj_free(Obj *obj);
void obj_print(Obj *obj);

struct Parser {
  List *list;
  SrcFile *srcfile;
};

typedef enum RuleContext {
  EXPRESSION = 1 << 0,
  STATEMENT = 1 << 1,
} RuleContext;

typedef struct TRule {
  char *match;
  void (*fn)(Obj *o, CCode *code);
  RuleContext ctx;
} TRule;

void transpile_obj(Obj *o, CCode *code, RuleContext ctx);
void transpile_return(Obj *o, CCode *code);
void transpile_include(Obj *o, CCode *code);
void transpile_fn(Obj *o, CCode *code);
void transpile_call(Obj *o, CCode *code);
void transpile_binary_op(Obj *o, CCode *code);
void transpile_deref(Obj *o, CCode *code);
void transpile_decl(Obj *o, CCode *code);
void transpile_set(Obj *o, CCode *code);
void transpile_while(Obj *o, CCode *code);
void transpile_cast(Obj *o, CCode *code);
void transpile_op_assign(Obj *o, CCode *code);
void transpile_statement(Obj *o, CCode *code);
void transpile_expression(Obj *o, CCode *code);
void transpile_for(Obj *o, CCode *code);
void transpile_if(Obj *o, CCode *code);
void transpile_do(Obj *o, CCode *code);
void transpile_ternary(Obj *o, CCode *code);
void transpile_incdec(Obj *o, CCode *code);

// === Implementations ===

// ==== Utilities ====
#define CHECK_ALLOC(ptr)                                                       \
  ({                                                                           \
    void *_tmp_ptr = (ptr);                                                    \
    if (_tmp_ptr == NULL) {                                                    \
      fprintf(stderr,                                                          \
              "error: " __FILE__ ":%d "                                        \
              "Couldn't allocate memory! Exiting.",                            \
              __LINE__);                                                       \
      exit(1);                                                                 \
    }                                                                          \
    _tmp_ptr;                                                                  \
  })

#define X(msg, ...)                                                            \
  do {                                                                         \
    time_t t = time(NULL);                                                     \
    struct tm *tm_info = localtime(&t);                                        \
    char time_buf[20];                                                         \
    strftime(time_buf, 20, "%Y-%m-%d %H:%M:%S", tm_info);                      \
    fprintf(stderr, "[%s] %s:%d: " msg "\n", time_buf, __FILE__, __LINE__,     \
            ##__VA_ARGS__);                                                    \
    fflush(stderr);                                                            \
  } while (0)

static const char *sic_srcname = "<input>";

_Noreturn static void fail_at(Pos pos, const char *fmt, ...) {
  fprintf(stderr, "%s:%zu:%zu: error: ", sic_srcname, pos.row + 1,
          pos.col + 1);
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");
  exit(EXIT_FAILURE);
}

// ==== Source files ====

SrcFile *srcfile_init(char *name) {
  FILE *fp = fopen(name, "r");
  if (fp == NULL) {
    return NULL;
  }

  SrcFile *srcfile = CHECK_ALLOC(calloc(1, sizeof(SrcFile)));

  size_t namelen = strnlen(name, FILENAME_MAX);
  srcfile->name = CHECK_ALLOC(malloc((namelen + 1) * sizeof(char)));

  memcpy(srcfile->name, name, namelen);
  srcfile->name[namelen] = '\0';

  srcfile->pos.row = 0;
  srcfile->pos.col = 0;

  srcfile->fp = fp;
  srcfile->eof = false;

  return srcfile;
}

void srcfile_free(SrcFile *srcfile) {
  free(srcfile->name);
  fclose(srcfile->fp);
  free(srcfile);
}

int srcfile_peek(SrcFile *srcfile) {
  int ch = fgetc(srcfile->fp);
  if (ch != EOF) {
    ungetc(ch, srcfile->fp);
  }
  return ch;
}

int srcfile_getc(SrcFile *srcfile) {
  int ch = fgetc(srcfile->fp);
  srcfile->pos.col++;
  if (ch == '\n') {
    srcfile->pos.row++;
    srcfile->pos.col = 0;
  } else if (ch == EOF) {
    srcfile->eof = true;
  }

  return ch;
}

bool srcfile_finished_p(SrcFile *srcfile) { return srcfile->eof; }

// ==== Objects ====

Obj *obj_init(Tag tag) {
  Obj *obj = CHECK_ALLOC(calloc(sizeof(Obj), 1));
  obj->tag = tag;
  switch (tag) {
  case SEXP:
    obj->sexp = list_init();
    break;
  case ATOM:
    obj->atom = atom_init();
    break;
  }
  return obj;
}

void obj_free(Obj *obj) {
  switch (obj->tag) {
  case SEXP:
    list_free(obj->sexp);
    break;
  case ATOM:
    atom_free(obj->atom);
    break;
  };

  free(obj);
}

void obj_print(Obj *obj) {
  switch (obj->tag) {
  case SEXP:
    printf("SEXP: (");
    for (size_t i = 0; i < obj->sexp->len; i++) {
      if (i > 0)
        printf(" ");
      switch (obj->sexp->buffer[i]->tag) {
      case ATOM:
        printf("%s", obj->sexp->buffer[i]->atom->buffer);
        break;
      case SEXP:
        printf("[%zu]", obj->sexp->buffer[i]->sexp->len);
        break;
      }
    }
    printf(") [%zu]\n", obj->sexp->len);
    break;
  case ATOM:
    printf("ATOM: %s\n", obj->atom->buffer);
    break;
  }
}

// ==== List handling ====

List *list_init() {
  List *list = CHECK_ALLOC(malloc(sizeof(List)));
  list->len = 0;
  list->buffer = NULL;
  list->buffer_len = 0;
  return list;
}

void list_resize(List *l, size_t buffer_len) {
  if (l->buffer != NULL) {
    l->buffer = CHECK_ALLOC(realloc(l->buffer, buffer_len * sizeof(Obj *)));
  } else {
    l->buffer = CHECK_ALLOC(malloc(buffer_len * sizeof(Obj *)));
  }
  l->buffer_len = buffer_len;
}

void list_add(List *list, Obj *obj) {
  if (list->len >= list->buffer_len) {
    size_t new_size = list->buffer_len == 0 ? 8 : list->buffer_len * 2;
    list_resize(list, new_size);
  }

  list->buffer[list->len++] = obj;
}

void list_free(List *l) {
  for (size_t i = 0; i < l->len; i++) {
    obj_free(l->buffer[i]);
  }
  free(l->buffer);
  free(l);
}

void list_print(List *l, size_t indent) {
  if (l->len == 0) {
    return;
  }

  size_t num_width = 1 + (size_t)log10(l->len);
  char formatstr[100];
  snprintf(formatstr, 100, "%%%zus%%%zuzu: %%s [%%zu, %%zu] -> [%%zu, %%zu]\n",
           indent, num_width);

  for (size_t i = 0; i < l->len; i++) {
    Obj *o = l->buffer[i];
    switch (o->tag) {
    case SEXP:
      printf(formatstr, "", i, "(", o->beg.row, o->beg.col, o->end.row,
             o->end.col);
      list_print(o->sexp, indent + 2);
      printf(formatstr, "", i, ")", o->beg.row, o->beg.col, o->end.row,
             o->end.col);
      break;
    case ATOM:
      printf(formatstr, "", i, o->atom->buffer, o->beg.row, o->beg.col,
             o->end.row, o->end.col);
      break;
    }
  }
}

// ==== Atom ====

Atom *atom_init() {
  Atom *atom = CHECK_ALLOC(malloc(sizeof(Atom)));
  atom->buffer = NULL;
  atom->buffer_len = 0;
  atom->len = 0;
  return atom;
}

void atom_resize(Atom *a, size_t buffer_len) {
  if (a->buffer != NULL) {
    a->buffer = CHECK_ALLOC(realloc(a->buffer, buffer_len * sizeof(char)));
  } else {
    a->buffer = CHECK_ALLOC(malloc(buffer_len * sizeof(char)));
  }
  a->buffer_len = buffer_len;
}

void atom_add(Atom *atom, char ch) {
  if (atom->len >= atom->buffer_len) {
    size_t new_size = atom->buffer_len == 0 ? 8 : atom->buffer_len * 2;
    atom_resize(atom, new_size);
  }
  atom->buffer[atom->len++] = ch;
}

void atom_free(Atom *a) {
  free(a->buffer);
  free(a);
}

void atom_print(Atom *a) {
  for (size_t i = 0; i < a->len; i++)
    printf("%c", a->buffer[i]);
}

// ==== Parser ====

void parser_next(Parser *parser, List *container, Atom *atom, Obj *parent) {
  int ch;

  while ((ch = srcfile_peek(parser->srcfile)) != EOF) {
    Obj *o = NULL;

#ifdef DEBUG
    printf("%c", ch);
#endif

    if (atom != NULL) {
      bool finished = false;
      if (atom->len > 0 &&
          (atom->buffer[0] == '"' || atom->buffer[0] == '\'')) {
        if (ch == atom->buffer[0]) {
          size_t escapes = 0;
          size_t idx = atom->len;
          while (idx > 1 && atom->buffer[idx - 1] == '\\') {
            escapes++;
            idx--;
          }
          if ((escapes & 1) == 0) {
            atom_add(atom, srcfile_getc(parser->srcfile));
            finished = true;
          }
        }
      } else if (isspace(ch) || ch == ')' || ch == ';') {
        finished = true;
      }

      if (finished) {
        atom_add(atom, '\0');
        return;
      }

      atom_add(atom, srcfile_getc(parser->srcfile));
    } else {
      if (isspace(ch)) {
        srcfile_getc(parser->srcfile);
        continue;
      }

      if (ch == ';') {
        while (ch != EOF && ch != '\n') {
          ch = srcfile_getc(parser->srcfile);
        }
        continue;
      }

      if (ch == ')') {
        if (parent == NULL) {
          fail_at(parser->srcfile->pos, "unmatched ')'");
        }
        srcfile_getc(parser->srcfile);
        return;
      }

      if (ch == '(') {
        o = obj_init(SEXP);
        o->beg = parser->srcfile->pos;
        list_add(container, o);
        srcfile_getc(parser->srcfile);
        parser_next(parser, o->sexp, NULL, o);
        o->end = parser->srcfile->pos;
        continue;
      }

      o = obj_init(ATOM);
      o->beg = parser->srcfile->pos;
      list_add(container, o);
      parser_next(parser, container, o->atom, parent);
      o->end = parser->srcfile->pos;
    }
  }

  if (atom != NULL) {
    if (atom->len > 0 && (atom->buffer[0] == '"' || atom->buffer[0] == '\'')) {
      fail_at(parser->srcfile->pos, "unterminated %s literal",
              atom->buffer[0] == '"' ? "string" : "character");
    }
    atom_add(atom, '\0');
  } else if (parent != NULL) {
    fail_at(parent->beg, "unclosed '('");
  }

  srcfile_getc(parser->srcfile);
}

void parser_parse(Parser *parser) {
  if (parser->srcfile == NULL) {
    fprintf(stderr, "error: Unable to access file.\n");
    exit(EXIT_FAILURE);
  }

  parser_next(parser, parser->list, NULL, NULL);
}

void parser_print(Parser *parser) {
  if (parser->list == NULL) {
    printf("(NULL)\n");
    return;
  }

  printf("=== %s ===\n", parser->srcfile->name);
  list_print(parser->list, 0);
}

Parser *parser_init(char *filename) {
  Parser *parser = CHECK_ALLOC(malloc(sizeof(Parser)));
  parser->srcfile = srcfile_init(filename);
  parser->list = list_init();
  return parser;
}

void parser_free(Parser *parser) {
  srcfile_free(parser->srcfile);
  list_free(parser->list);
  free(parser);
}

// === Output behavior ===

CCode *ccode_init() {
  CCode *code = CHECK_ALLOC(malloc(sizeof(CCode)));
  code->lines = NULL;
  code->buffer = 0;
  code->count = 0;
  return code;
}

void ccode_free(CCode *code) {
  for (size_t i = 0; i < code->count; i++) {
    free(code->lines[i]);
  }
  free(code->lines);
  free(code);
}

void ccode_resize(CCode *code, size_t size) {
  if (code->lines == NULL) {
    code->lines = CHECK_ALLOC(calloc(size, sizeof(char *)));
  } else {
    code->lines = CHECK_ALLOC(realloc(code->lines, sizeof(char *) * size));
  }

  code->buffer = size;
}

char *ccode_alloc_line(CCode *code, size_t size) {
  if (code->count >= code->buffer) {
    ccode_resize(code, code->buffer == 0 ? 8 : code->buffer * 2);
  }

  char *line = CHECK_ALLOC(malloc(sizeof(char) * size));
  code->lines[code->count++] = line;
  return line;
}

char *ccode_printf_line(CCode *code, const char *format, ...) {
  va_list args;
  va_start(args, format);
  size_t size = vsnprintf(NULL, 0, format, args);
  char *line = ccode_alloc_line(code, size + 1);
  vsnprintf(line, size + 1, format, args);
  va_end(args);

  return line;
}

void ccode_append(CCode *code, const char *format, ...) {
  if (code->count == 0) {
    ccode_printf_line(code, "");
  }

  va_list args;
  va_start(args, format);
  size_t size = vsnprintf(NULL, 0, format, args);
  va_end(args);

  char *last = code->lines[code->count - 1];
  size_t cur = strlen(last);
  last = CHECK_ALLOC(realloc(last, cur + size + 1));

  va_start(args, format);
  vsnprintf(last + cur, size + 1, format, args);
  va_end(args);

  code->lines[code->count - 1] = last;
}

void ccode_mark_line(CCode *code, Obj *o) {
#ifndef DISABLE_LINE
  ccode_printf_line(code, "#line %zu \"%s\"", o->beg.row + 1, sic_srcname);
#endif
}

void ccode_write(CCode *code, FILE *stream) {
  for (size_t i = 0; i < code->count; i++) {
    fprintf(stream, "%s\n", code->lines[i]);
  }
}

// === Transpiler ===
// Feeling this out, will need to be dramatically rewritten

static const TRule TRANSPILE_RULES[] = {
    {"^#include$", transpile_include, STATEMENT},
    {"^fn$", transpile_fn, STATEMENT},
    {"^return$", transpile_return, STATEMENT},
    {"^([-+*/%&|^]=|<<=|>>=)$", transpile_op_assign, STATEMENT},
    {"^(\\+\\+|--)$", transpile_incdec, EXPRESSION},
    {"^(\\+|-|\\*|/|%|<|>|<=|>=|==|!=|&&|\\|\\||&|\\||\\^|<<|>>|!|~)$",
     transpile_binary_op, EXPRESSION},
    {"^deref$", transpile_deref, EXPRESSION},
    {"^decl$", transpile_decl, STATEMENT},
    {"^set$", transpile_set, STATEMENT},
    {"^while$", transpile_while, STATEMENT},
    {"^for$", transpile_for, STATEMENT},
    {"^if$", transpile_if, STATEMENT},
    {"^do$", transpile_do, STATEMENT},
    {"^\\?:$", transpile_ternary, EXPRESSION},
    {"^:.*$", transpile_cast, EXPRESSION},
    {".*", transpile_call, EXPRESSION},
};
#define TRANSPILE_RULE_LEN (sizeof(TRANSPILE_RULES) / sizeof(TRule))

void transpile_binary_op(Obj *o, CCode *code) {
  char *op = o->sexp->buffer[0]->atom->buffer;
  bool prefix_ok = op[1] == '\0' && strchr("+-*&!~", op[0]) != NULL;
  bool prefix_only = op[1] == '\0' && (op[0] == '!' || op[0] == '~');

  if (o->sexp->len == 2 && prefix_ok) {
    ccode_append(code, "(%s", op);
    transpile_expression(o->sexp->buffer[1], code);
    ccode_append(code, ")");
    return;
  }

  if (prefix_only) {
    fail_at(o->beg, "operator '%s' takes exactly one operand", op);
  }
  if (o->sexp->len < 3) {
    fail_at(o->beg, "operator '%s' needs at least two operands", op);
  }

  ccode_append(code, "(");
  for (size_t i = 1; i < o->sexp->len; i++) {
    if (i != 1) {
      ccode_append(code, " %s ", op);
    }
    transpile_expression(o->sexp->buffer[i], code);
  }
  ccode_append(code, ")");
}

void transpile_incdec(Obj *o, CCode *code) {
  if (o->sexp->len != 2) {
    fail_at(o->beg, "'%s' takes exactly one operand",
            o->sexp->buffer[0]->atom->buffer);
  }

  ccode_append(code, "(%s", o->sexp->buffer[0]->atom->buffer);
  transpile_expression(o->sexp->buffer[1], code);
  ccode_append(code, ")");
}

void transpile_decl(Obj *o, CCode *code) {
  if (o->sexp->len < 3 || o->sexp->buffer[1]->tag != ATOM ||
      o->sexp->buffer[2]->tag != ATOM ||
      o->sexp->buffer[2]->atom->buffer[0] != ':') {
    fail_at(o->beg, "decl needs a name and a :type, e.g. (decl x :int)");
  }

  ccode_mark_line(code, o);

  char *typestr = o->sexp->buffer[2]->atom->buffer + 1;
  char *plaintype = NULL;
  char *arr_pt = strchr(typestr, '[');
  if (arr_pt) {
    size_t n = arr_pt - typestr;
    plaintype = malloc(arr_pt - typestr + 1);
    strncpy(plaintype, typestr, arr_pt - typestr);
    plaintype[n] = '\0';
    typestr = plaintype;
  } else {
    arr_pt = "";
  }

  if (o->sexp->len > 3) {
    ccode_printf_line(code, "%s %s%s = ", typestr,
                      o->sexp->buffer[1]->atom->buffer, arr_pt);
    transpile_expression(o->sexp->buffer[3], code);
    ccode_append(code, ";");
  } else {
    ccode_printf_line(code, "%s %s%s;", typestr,
                      o->sexp->buffer[1]->atom->buffer, arr_pt);
  }

  if (plaintype != NULL) {
    free(plaintype);
  }
}

void transpile_set(Obj *o, CCode *code) {
  if (o->sexp->len != 3 || o->sexp->buffer[1]->tag != ATOM) {
    fail_at(o->beg, "set needs a name and a value, e.g. (set x 1)");
  }

  ccode_mark_line(code, o);
  ccode_printf_line(code, "%s = ", o->sexp->buffer[1]->atom->buffer);
  transpile_expression(o->sexp->buffer[2], code);
  ccode_append(code, ";");
};

void transpile_while(Obj *o, CCode *code) {
  if (o->sexp->len < 2) {
    fail_at(o->beg, "while needs a condition");
  }

  ccode_mark_line(code, o);
  ccode_printf_line(code, "while (");
  transpile_expression(o->sexp->buffer[1], code);
  ccode_append(code, ") {");
  for (size_t i = 2; i < o->sexp->len; i++) {
    transpile_statement(o->sexp->buffer[i], code);
  }
  ccode_printf_line(code, "}");
};

void transpile_cast(Obj *o, CCode *code) {
  if (o->sexp->len != 2) {
    fail_at(o->beg, "a cast takes exactly one value, e.g. (:int x)");
  }

  ccode_append(code, "((%s)", o->sexp->buffer[0]->atom->buffer + 1);
  transpile_expression(o->sexp->buffer[1], code);
  ccode_append(code, ")");
};

void transpile_op_assign(Obj *o, CCode *code) {
  if (o->sexp->len != 3 || o->sexp->buffer[1]->tag != ATOM) {
    fail_at(o->beg, "'%s' needs a name and a value, e.g. (%s x 1)",
            o->sexp->buffer[0]->atom->buffer, o->sexp->buffer[0]->atom->buffer);
  }

  ccode_mark_line(code, o);
  ccode_printf_line(code, "%s %s ", o->sexp->buffer[1]->atom->buffer,
                    o->sexp->buffer[0]->atom->buffer);
  transpile_expression(o->sexp->buffer[2], code);
  ccode_append(code, ";");
};

void transpile_deref(Obj *o, CCode *code) {
  if (o->sexp->len != 2) {
    fail_at(o->beg, "deref takes exactly one value");
  }

  ccode_append(code, "*(");
  transpile_expression(o->sexp->buffer[1], code);
  ccode_append(code, ")");
}

void transpile_return(Obj *o, CCode *code) {
  if (o->sexp->len != 2) {
    fail_at(o->beg, "return takes exactly one value");
  }

  Obj *t = o->sexp->buffer[1];
  ccode_mark_line(code, t);
  ccode_printf_line(code, "return ");
  transpile_expression(t, code);
  ccode_append(code, ";");
}

void transpile_include(Obj *o, CCode *code) {
  if (o->sexp->len < 2) {
    fail_at(o->beg, "#include needs at least one header");
  }

  for (size_t i = 1; i < o->sexp->len; i++) {
    Obj *t = o->sexp->buffer[i];
    if (t->tag != ATOM) {
      fail_at(t->beg, "#include takes header names, not expressions");
    }

    ccode_mark_line(code, t);
    ccode_printf_line(code, "#include %s", t->atom->buffer);
  }
}

void transpile_call(Obj *o, CCode *code) {
  ccode_append(code, "%s(", o->sexp->buffer[0]->atom->buffer);

  for (size_t j = 1; j < o->sexp->len; j++) {
    if (j > 1) {
      ccode_append(code, ", ");
    }
    transpile_expression(o->sexp->buffer[j], code);
  }

  ccode_append(code, ")");
}

void transpile_fn(Obj *o, CCode *code) {
  if (o->sexp->len < 5 || o->sexp->buffer[1]->tag != ATOM ||
      o->sexp->buffer[2]->tag != ATOM ||
      o->sexp->buffer[2]->atom->buffer[0] != ':') {
    fail_at(o->beg,
            "fn needs a name, a :type, an argument list, and a body, "
            "e.g. (fn main :int (argc :int argv :char**) ...)");
  }

  Obj *name = o->sexp->buffer[1];
  Obj *type = o->sexp->buffer[2];

  ccode_mark_line(code, name);
  ccode_printf_line(code, "%s %s(", type->atom->buffer + 1,
                    name->atom->buffer);

  Obj *args = o->sexp->buffer[3];
  if (args->tag != SEXP || (args->sexp->len & 1) != 0) {
    fail_at(o->sexp->buffer[3]->beg,
            "fn arguments must be name :type pairs, e.g. (argc :int)");
  }
  for (size_t j = 0; j < args->sexp->len; j += 2) {
    Obj *arg_name = args->sexp->buffer[j];
    Obj *arg_type = args->sexp->buffer[j + 1];
    if (arg_name->tag != ATOM || arg_type->tag != ATOM ||
        arg_type->atom->buffer[0] != ':') {
      fail_at(arg_name->beg,
              "fn arguments must be name :type pairs, e.g. (argc :int)");
    }

    ccode_append(code, "%s%s %s", j == 0 ? "" : ", ",
                 arg_type->atom->buffer + 1, arg_name->atom->buffer);
  }

  ccode_append(code, ") {");

  for (size_t j = 4; j < o->sexp->len; j++) {
    transpile_statement(o->sexp->buffer[j], code);
  }

  ccode_printf_line(code, "}");
}

void transpile_for(Obj *o, CCode *code) {
  if (o->sexp->len < 4) {
    fail_at(o->beg, "for needs an init statement, a condition, and a step");
  }

  ccode_mark_line(code, o);
  ccode_printf_line(code, "for (");
  transpile_statement(o->sexp->buffer[1], code);
  ccode_append(code, " ");
  transpile_expression(o->sexp->buffer[2], code);
  ccode_append(code, "; ");
  transpile_expression(o->sexp->buffer[3], code);
  ccode_append(code, ") {");
  for (size_t i = 4; i < o->sexp->len; i++) {
    transpile_statement(o->sexp->buffer[i], code);
  }
  ccode_printf_line(code, "}");
}

void transpile_if(Obj *o, CCode *code) {
  if (o->sexp->len < 3 || o->sexp->len > 4) {
    fail_at(o->beg, "if needs a condition, a branch, and at most an else "
                    "branch; use (do ...) to group statements");
  }

  ccode_mark_line(code, o);
  ccode_printf_line(code, "if (");
  transpile_expression(o->sexp->buffer[1], code);
  ccode_append(code, ") {");
  transpile_statement(o->sexp->buffer[2], code);
  if (o->sexp->len == 4) {
    ccode_printf_line(code, "} else {");
    transpile_statement(o->sexp->buffer[3], code);
  }
  ccode_printf_line(code, "}");
}

void transpile_do(Obj *o, CCode *code) {
  ccode_mark_line(code, o);
  ccode_printf_line(code, "{");
  for (size_t i = 1; i < o->sexp->len; i++) {
    transpile_statement(o->sexp->buffer[i], code);
  }
  ccode_printf_line(code, "}");
}

void transpile_ternary(Obj *o, CCode *code) {
  if (o->sexp->len != 4) {
    fail_at(o->beg, "?: needs a condition and two values");
  }

  ccode_append(code, "(");
  transpile_expression(o->sexp->buffer[1], code);
  ccode_append(code, " ? ");
  transpile_expression(o->sexp->buffer[2], code);
  ccode_append(code, " : ");
  transpile_expression(o->sexp->buffer[3], code);
  ccode_append(code, ")");
}

void transpile_obj(Obj *o, CCode *code, RuleContext ctx) {
#ifdef DEBUG
  obj_print(o);
#endif

  static regex_t TRANSPILE_REGEXES[TRANSPILE_RULE_LEN];
  static bool initialized = false;
  if (!initialized) {
    for (size_t i = 0; i < TRANSPILE_RULE_LEN; i++) {
#ifdef DEBUG
      printf("REGEX: %s\n", TRANSPILE_RULES[i].match);
#endif
      if (regcomp(&TRANSPILE_REGEXES[i], TRANSPILE_RULES[i].match,
                  REG_EXTENDED | REG_NOSUB) != 0) {
        fprintf(stderr, "internal error: bad rule regex '%s'\n",
                TRANSPILE_RULES[i].match);
        exit(EXIT_FAILURE);
      }
    }
    initialized = true;
  }

  if (o->tag == SEXP) {
    if (o->sexp->len == 0) {
      fail_at(o->beg, "empty expression '()'");
    }
    if (o->sexp->buffer[0]->tag != ATOM) {
      fail_at(o->sexp->buffer[0]->beg,
              "operator position must hold a name, not an expression");
    }

    char *head = o->sexp->buffer[0]->atom->buffer;
    for (size_t i = 0; i < TRANSPILE_RULE_LEN; i++) {
      int result = regexec(&TRANSPILE_REGEXES[i], head, 0, NULL, 0);
      if (result == REG_NOMATCH) {
        continue;
      }

      if (ctx == EXPRESSION && (TRANSPILE_RULES[i].ctx & EXPRESSION) == 0) {
        fail_at(o->beg, "'%s' is a statement and has no value here", head);
      }

      bool as_statement =
          ctx == STATEMENT && (TRANSPILE_RULES[i].ctx & STATEMENT) == 0;
      if (as_statement) {
        ccode_mark_line(code, o);
        ccode_printf_line(code, "");
      }

      TRANSPILE_RULES[i].fn(o, code);

      if (as_statement) {
        ccode_append(code, ";");
      }
      return;
    }

    fail_at(o->beg, "no rule matches '%s' here", head);
  } else {
    if (ctx == STATEMENT) {
      ccode_mark_line(code, o);
      ccode_printf_line(code, "%s;", o->atom->buffer);
    } else {
      ccode_append(code, "%s", o->atom->buffer);
    }
  }
}

CCode *transpile(List *list) {
  CCode *code = ccode_init();

  for (size_t i = 0; i < list->len; i++) {
    transpile_statement(list->buffer[i], code);
  }

  return code;
}

void transpile_expression(Obj *o, CCode *code) {
  transpile_obj(o, code, EXPRESSION);
}

void transpile_statement(Obj *o, CCode *code) {
  transpile_obj(o, code, STATEMENT);
}

// === Main ===

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <file to transpile> <output file>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  sic_srcname = argv[1];
  Parser *parser = parser_init(argv[1]);
  parser_parse(parser);
  CCode *code = transpile(parser->list);
  parser_free(parser);

  FILE *fp = fopen(argv[2], "w");
  if (fp == NULL) {
    fprintf(stderr, "error: Unable to open %s for writing.\n", argv[2]);
    exit(EXIT_FAILURE);
  }
  ccode_write(code, fp);
  fclose(fp);
  ccode_free(code);
}
