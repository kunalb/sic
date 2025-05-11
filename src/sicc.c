#define _POSIX_C_SOURCE 200809L

#include <assert.h>
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

struct Parser {
  List *list;
  SrcFile *srcfile;
};

typedef struct TRule {
  char *match;
  void (*fn)(Obj *o, CCode *code);
} TRule;

void t2_return(Obj *o, CCode *code);
void t2_include(Obj *o, CCode *code);
void t2_fn(Obj *o, CCode *code);
void t2_call(Obj *o, CCode *code);
void t2_obj(Obj *o, CCode *code);

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
  for (int i = 0; i < l->len; i++) {
    obj_free(l->buffer[i]);
  }
  free(l->buffer);
  free(l);
}

void list_print(List *l, size_t indent) {
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

void parser_next(Parser *parser, List *container, Atom *atom) {
  int ch;

  while ((ch = srcfile_peek(parser->srcfile)) != EOF) {
    Obj *o = NULL;

#ifdef DEBUG
    printf("%c", ch);
#endif

    if (atom != NULL) {
      bool finished = false;
      if (atom->len > 0 && atom->buffer[0] == '"') {
        if (ch == '"') {
          size_t escapes = 0;
          size_t idx = atom->len - 1;
          while (atom->buffer[idx--] == '\\' && idx >= 0)
            escapes++;
          if ((escapes & 1) == 0)
            finished = true;
        }
      } else if (isspace(ch) || ch == ')') {
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

      if (ch == ')') {
        assert(container != NULL);
        srcfile_getc(parser->srcfile);
        return;
      }

      if (ch == '(') {
        o = obj_init(SEXP);
        o->beg = parser->srcfile->pos;
        list_add(container, o);
        srcfile_getc(parser->srcfile);
        parser_next(parser, o->sexp, NULL);
        o->end = parser->srcfile->pos;
        continue;
      }

      o = obj_init(ATOM);
      o->beg = parser->srcfile->pos;
      list_add(container, o);
      parser_next(parser, container, o->atom);
      o->end = parser->srcfile->pos;
    }
  }

  assert(srcfile_getc(parser->srcfile) == EOF);
}

void parser_parse(Parser *parser) {
  if (parser->srcfile == NULL) {
    fprintf(stderr, "error: Unable to access file.\n");
    exit(EXIT_FAILURE);
  }

  parser_next(parser, parser->list, NULL);
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

void ccode_write(CCode *code, FILE *stream) {
  for (size_t i = 0; i < code->count; i++) {
    fprintf(stream, "%s\n", code->lines[i]);
  }
}

// === Transpiler ===
// Feeling this out, will need to be dramatically rewritten

static const TRule TRANSPILE_RULES[] = {
    {"#include", t2_include},
    {"fn", t2_fn},
    {"return", t2_return},
    {".*", t2_call},
};
#define TRANSPILE_RULE_LEN (sizeof(TRANSPILE_RULES) / sizeof(TRule))

void t2_return(Obj *o, CCode *code) {
  assert(o->tag == SEXP);
  assert(o->sexp->len == 2);

  Obj *t = o->sexp->buffer[1];
  assert(t->tag == ATOM);
  ccode_printf_line(code, "#line %zu", t->beg.row + 1);
  ccode_printf_line(code, "return %s;", t->atom->buffer);
}

void t2_include(Obj *o, CCode *code) {
  assert(o->tag == SEXP);
  assert(o->sexp->len > 1);

  for (size_t i = 1; i < o->sexp->len; i++) {
    Obj *t = o->sexp->buffer[i];
    // Will need to be extended to evaluate statically
    assert(t->tag == ATOM);

    ccode_printf_line(code, "#line %zu", t->beg.row + 1);
    ccode_printf_line(code, "#include %s;", t->atom->buffer);
  }
}

void t2_call(Obj *o, CCode *code) {
  assert(o->tag == SEXP);
  assert(o->sexp->len > 0);

  ccode_printf_line(code, "#line %d", o->beg.row + 1);
  ccode_printf_line(code, "%s(", o->sexp->buffer[0]->atom->buffer);

  for (size_t j = 1; j < o->sexp->len; j++) {
    ccode_printf_line(code, "#line %d", o->sexp->buffer[j]->beg.row + 1);
    ccode_printf_line(code, "%s,", o->sexp->buffer[j]->atom->buffer);
  }

  ccode_printf_line(code, ");");
}

void t2_fn(Obj *o, CCode *code) {
  assert(o->tag == SEXP);
  assert(o->sexp->len >= 5);

  Obj *name = o->sexp->buffer[1];
  Obj *type = o->sexp->buffer[2];
  assert(type->atom->buffer[0] == ':');

  ccode_printf_line(code, "#line %d", name->beg.row + 1);
  ccode_printf_line(code, "%s %s (", type->atom->buffer + 1,
                    name->atom->buffer);

  Obj *args = o->sexp->buffer[3];
  assert(args->tag == SEXP);
  for (size_t j = 0; j < args->sexp->len; j += 2) {
    Obj *arg_name = args->sexp->buffer[j];
    Obj *arg_type = args->sexp->buffer[j + 1];
    assert(arg_type->atom->buffer[0] == ':');

    ccode_printf_line(code, "#line %d", arg_name->beg.row + 1);
    ccode_printf_line(code, "  %s %s,", arg_type->atom->buffer + 1,
                      arg_name->atom->buffer);
  }

  ccode_printf_line(code, ") {");

  t2_obj(o->sexp->buffer[4], code);

  ccode_printf_line(code, "}");
}

void t2_obj(Obj *o, CCode *code) {
  static regex_t TRANSPILE_REGEXES[TRANSPILE_RULE_LEN];
  static bool initialized = false;
  if (!initialized) {
    for (size_t i = 0; i < TRANSPILE_RULE_LEN; i++) {
      assert(regcomp(&TRANSPILE_REGEXES[i], TRANSPILE_RULES[i].match,
                     REG_EXTENDED | REG_NOSUB) == 0);
    }
    initialized = true;
  }

  if (o->tag == SEXP) {
    assert(o->sexp->len > 0);
    for (size_t i = 0; i < TRANSPILE_RULE_LEN; i++) {
      int result = regexec(&TRANSPILE_REGEXES[i],
                           o->sexp->buffer[0]->atom->buffer, i, NULL, 0);
      if (result != REG_NOMATCH) {
        TRANSPILE_RULES[i].fn(o, code);
        break;
      }
    }
  } else {
    assert(o->tag == ATOM);
  }
}

CCode *t2(List *list) {
  CCode *code = ccode_init();

  for (size_t i = 0; i < list->len; i++) {
    t2_obj(list->buffer[i], code);
  }

  return code;
}

void transpile_include(Obj *o, CCode *code) {
  assert(o->sexp->len == 2);
  assert(o->sexp->buffer[1]->tag == ATOM);
  Atom *atom = o->sexp->buffer[1]->atom;
  size_t maxlen = 8 + atom->len + 1;
  char *line = ccode_alloc_line(code, maxlen);
  snprintf(line, maxlen, "#include %s", atom->buffer);
}

// (something args) => something(args);
void transpile_call(Obj *o, CCode *code) {
  size_t len = 1;
  for (size_t i = 0; i < o->sexp->len; i++) {
    len += o->sexp->buffer[i]->atom->len + 2;
  }

  char *line = ccode_alloc_line(code, len);
  size_t w = 0;
  for (size_t i = 0; i < o->sexp->len; i++) {
    size_t l = o->sexp->buffer[i]->atom->len;
    strncpy(&line[w], o->sexp->buffer[i]->atom->buffer, l);
    w += l - 1;
    if (i == 0) {
      line[w] = '(';
    } else {
      line[w] = ',';
      line[w] = ' ';
    }
    w++;
  }
  w -= 1;
  line[w++] = ')';
  line[w++] = ';';
  line[w++] = '\0';
}

// (return X) -> return X;
void transpile_return(Obj *o, CCode *code) { Obj **b = o->sexp->buffer; }

// (fn name :type (arg1 :type1 arg2 :type2 ...) ...)
void transpile_fn(Obj *o, CCode *code) {
  Obj **b = o->sexp->buffer;

  assert(o->sexp->len >= 5);
  assert(b[1]->tag == ATOM);
  assert(b[2]->tag == ATOM);
  assert(b[3]->tag == SEXP);

  size_t len = 2;
  for (size_t i = 1; i <= 2; i++)
    len += b[i]->atom->len + 1;

  for (size_t i = 0; i < b[3]->sexp->len; i++) {
    len += b[3]->sexp->buffer[i]->atom->len + 1;
  }

  char *line = ccode_alloc_line(code, len);

  size_t w = 0;
  for (size_t i = 1; i < b[2]->atom->len - 1; i++) {
    line[w++] = b[2]->atom->buffer[i]; // Skip the :
  }

  line[w++] = ' ';

  for (size_t i = 0; i < b[1]->atom->len - 1; i++) {
    line[w++] = b[1]->atom->buffer[i];
  }

  line[w++] = '(';
  for (size_t j = 0; j < b[3]->sexp->len; j += 2) {
    for (size_t k = 1; k < b[3]->sexp->buffer[j + 1]->atom->len - 1; k++) {
      line[w++] = b[3]->sexp->buffer[j + 1]->atom->buffer[k];
    }
    line[w++] = ' ';
    for (size_t k = 0; k < b[3]->sexp->buffer[j]->atom->len - 1; k++) {
      line[w++] = b[3]->sexp->buffer[j]->atom->buffer[k];
    }
    line[w++] = ',';
    line[w++] = ' ';
  }
  w -= 2;
  line[w++] = ')';
  line[w++] = ' ';
  line[w++] = '{';
  line[w++] = '\0';

  for (size_t s = 4; s < o->sexp->len; s++) {
    if (o->sexp->buffer[s]->tag == ATOM) {
      size_t l = o->sexp->buffer[s]->atom->len;
      char *atom_line = ccode_alloc_line(code, l);
      strncpy(atom_line, o->sexp->buffer[s]->atom->buffer, l);
    } else {
      transpile_call(o->sexp->buffer[s], code);
    }
  }

  char *endline = ccode_alloc_line(code, 2);
  endline[0] = '}';
  endline[1] = '\0';
}

CCode *transpile(List *list) {
  CCode *code = ccode_init();
  for (size_t i = 0; i < list->len; i++) {
    Obj *o = list->buffer[i];

    if (o->tag == SEXP) {
      if (o->sexp->len == 0)
        continue;

      Obj *elem = o->sexp->buffer[0];
      if (elem->tag == ATOM) {
        if (strncmp("#include", elem->atom->buffer, 8) == 0) {
          transpile_include(o, code);
        } else if (strncmp("fn", elem->atom->buffer, 2) == 0) {
          transpile_fn(o, code);
        } else {
          transpile_call(o, code);
        }
      } else {
        X("Unhandled nested sexp");
      }
    } else {
      assert(false);
    }
  }

  return code;
}

#ifndef TEST
// === Main ===

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <file to transpile> <output file>", argv[0]);
    exit(EXIT_FAILURE);
  }

  Parser *parser = parser_init(argv[1]);
  parser_parse(parser);
  // CCode *code = transpile(parser->list);
  CCode *code = t2(parser->list);
  parser_free(parser);

  FILE *fp = fopen(argv[2], "w");
  ccode_write(code, fp);
  fclose(fp);
  ccode_free(code);
}

#else
// === Tests ===
// Very coarse tests for now, will refactor later

void test_src_file_reads() {
  SrcFile *src = srcfile_init("examples/hello.sic");
  while (true) {
    int ch = srcfile_getc(src);
    if (ch == EOF) {
      break;
    }

    printf("%c", ch);
  }
  srcfile_free(src);
}

void test_list_management() {
  List *list = list_init();

  Obj *o = CHECK_ALLOC(calloc(2, sizeof(Obj *)));
  for (int i = 0; i < list->len; i++) {
    list_add(list, &o[i]);
  }

  for (int i = 0; i < list->len; i++) {
    assert(list->buffer[i] == &o[i]);
  }

  list_resize(list, 10);
  for (int i = 0; i < list->len; i++) {
    assert(list->buffer[i] == &o[i]);
  }

  list_free(list);
  free(o);
}

void test_transpile() {
  Parser *parser = parser_init("examples/hello.sic");
  parser_parse(parser);
  parser_print(parser);
  CCode *code = transpile(parser->list);
  CCode *code2 = t2(parser->list);
  parser_free(parser);

  printf("\n\n");
  ccode_write(code, stdout);
  ccode_free(code);

  printf("\n\n");
  ccode_write(code2, stdout);
  ccode_free(code2);
}

int main(int argc, char **argv) {
  printf("=== Testing sicc! ===\n");
  // test_src_file_reads();
  // test_list_management();
  test_transpile();
}

#endif
