#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>


// === Headers & Data Structures ===

typedef enum State State;
typedef enum Tag Tag;
typedef struct Obj Obj;
typedef struct SrcFile SrcFile;
typedef struct Pos Pos;
typedef struct Parser Parser;
typedef struct List List;

enum State {
  BEGIN,
  IN_SEXP,
  IN_DQ_STR,
  IN_SQ_STR,
  IN_SYMBOL,
  IN_NUMBER,
  IN_WHITESPACE,
};

struct SrcFile {
  char* name;
  FILE* fp;
  size_t row;
  size_t col;
  bool eof;
};

struct CCode {
  char* lines;
};


enum Tag {
  SEXP,
  SYM,
  STR,
  NUM,
};

struct List {
  Obj **buffer;
  size_t buffer_len;
  size_t len;
};

struct Obj {
  size_t row;
  size_t col;
  size_t len;

  Tag tag;
  union {
    char *sym;
    char *str;
    char *num;
    List *sexp;
  };
};

struct Parser {
  State state;
  List *list;
  SrcFile *srcfile;
};

// === Implementations ===

// ==== Utilities ====
#define CHECK_ALLOC(ptr)					\
  ({								\
    void *_tmp_ptr = (ptr);					\
    if (_tmp_ptr == NULL) {					\
      fprintf(stderr, "error: " __FILE__ ":%d "			\
	      "Couldn't allocate memory! Exiting.", __LINE__);	\
      exit(1);							\
    }								\
    _tmp_ptr;							\
  })

// ==== Source files ====

SrcFile* srcfile_open(char *name) {
  FILE *fp = fopen(name, "r");
  if (fp == NULL) {
    return NULL;
  }

  SrcFile *srcfile = CHECK_ALLOC(calloc(1, sizeof(SrcFile)));

  size_t namelen = strnlen(name, FILENAME_MAX);
  srcfile->name = CHECK_ALLOC(malloc((namelen + 1) * sizeof(char)));

  memcpy(srcfile->name, name, namelen);
  srcfile->name[namelen] = '\0';

  srcfile->row = 0;
  srcfile->col = 0;

  srcfile->fp = fp;
  srcfile->eof = false;

  return srcfile;
}

void srcfile_close(SrcFile *srcfile) {
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
  srcfile->col++;
  if (ch == '\n') {
    srcfile->row++;
    srcfile->col = 0;
  } else if (ch == EOF) {
    srcfile->eof = true;
  }

  return ch;
}

bool srcfile_finished_p(SrcFile* srcfile) {
  return srcfile->eof;
}

// ==== List handling ====

List* list_init() {
  List *list = CHECK_ALLOC(malloc(sizeof(List)));
  list->len = 0;
  list->buffer = NULL;
  list->buffer_len = 0;
  return list;
}

void list_resize(List *l, size_t buffer_len) {
  if (l->buffer != NULL) {
    l->buffer = CHECK_ALLOC(realloc(l->buffer, buffer_len * sizeof(Obj*)));
  } else {
    l->buffer = CHECK_ALLOC(malloc(buffer_len * sizeof(Obj*)));
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
    free(l->buffer[i]);
  }
  free(l->buffer);
  free(l);
}


// ==== Objects ====

Obj* obj_init(Tag tag) {
  Obj *obj = CHECK_ALLOC(malloc(sizeof(Obj)));
  obj->row = 0;
  obj->col = 0;
  obj->len = 0;
  obj->tag = tag;
  return obj;
}

void obj_free(Obj *obj) {
  switch (obj->tag) {
  case SEXP:
    list_free(obj->sexp);
  case SYM:
    free(obj->sym);
  case STR:
    free(obj->str);
  case NUM:
    free(obj->num);
  };

  free(obj);
}


// ==== Parser ====

void consume_next(SrcFile *f, State state);

void consume_sexp(SrcFile *f) {
  // SExp s_exp;
  // s_exp.start_row = f->row;
  // s_exp.start_col = f->col;

  int start = srcfile_getc(f);
  assert(start == '(');

  consume_next(f, IN_SEXP);
}

void consume_symbol(SrcFile *f) {
}

void consume_dq_str() {}
void consume_sq_str() {}
void consume_number() {}

void consume_next(SrcFile *f, State state) {
  int ch;
  while ((ch = srcfile_peek(f)) != EOF) {
    switch (ch) {
    case ' ':
    case '\n':
      srcfile_getc(f);
      break;
    case '(':
      consume_sexp(f);
      break;
    case ')':
      if (state == IN_SEXP) return;
      // Maybe make this an error
    default:
      consume_symbol(f);
      printf("%c", srcfile_getc(f));
    }
  }

  srcfile_getc(f);
}


void parser_parse(Parser *parser) {
  if (parser->srcfile == NULL) {
    fprintf(stderr, "error: Unable to access file %s.", parser->srcfile->name);
    exit(EXIT_FAILURE);
  }

  while (!parser->srcfile->eof) {
    consume_next(parser->srcfile, BEGIN);
  }
}


Parser* parser_init(char *filename) {
  Parser *parser = CHECK_ALLOC(malloc(sizeof(Parser)));
  parser->srcfile = srcfile_open(filename);
  parser->list = list_init();
  parser->state = BEGIN;
  return parser;
}

void parser_free(Parser *parser) {
  list_free(parser->list);
  free(parser);
}


// === Transplite ===
void transpile(List *list) {
}


#ifndef TEST
// === Main ===

int main(int argc, char** argv) {
  if (argc < 1) {
    fprintf(stderr, "Usage: %s <file to transpile>", argv[0]);
    exit(EXIT_FAILURE);
  }

  Parser *parser = parser_init(argv[1]);
  parser_parse(parser);
  parser_free(parser);
}

#else
// === Tests ===
// Very coarse tests for now, will refactor later

void test_src_file_reads() {
  SrcFile *src = srcfile_open("hello.sic");
  while (true) {
    int ch = srcfile_getc(src);
    if (ch == EOF) {
      break;
    }

    printf("%c", ch);
  }
  srcfile_close(src);
}

void test_list_management() {
  List *list = list_init();

  Obj *o = CHECK_ALLOC(calloc(2, sizeof(Obj*)));
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

void test_parse() {
  Parser *parser = parser_init("hello.sic");
  parser_parse(parser);
  parser_free(parser);
}

int main(int argc, char** argv) {
  printf("=== Testing sicc! ===\n");
  // test_src_file_reads();
  // test_list_management();
  test_parse();
}


#endif
