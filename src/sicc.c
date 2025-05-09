#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
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

struct Pos {
  size_t row;
  size_t col;
};

struct SrcFile {
  char* name;
  FILE* fp;
  bool eof;
  Pos pos;
};

struct CCode {
  char* lines;
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

List* list_init();
void list_resize(List*, size_t);
void list_add(List*, Obj*);
void list_free(List*);
void list_print(List*, size_t indent);

struct Atom {
  char *buffer;
  size_t buffer_len;
  size_t len;
};

Atom *atom_init();
void atom_resize(Atom*, size_t);
void atom_add(Atom*, char);
void atom_free(Atom*);
void atom_print(Atom*);


struct Result {
  enum {
    OK,
    Err
  } tag;

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

Obj* obj_init(Tag tag);
void obj_free(Obj* obj);

struct Parser {
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

#define X(msg, ...) do { \
    time_t t = time(NULL); \
    struct tm *tm_info = localtime(&t); \
    char time_buf[20]; \
    strftime(time_buf, 20, "%Y-%m-%d %H:%M:%S", tm_info); \
    fprintf(stderr, "[%s] %s:%d: " msg "\n", time_buf, __FILE__, __LINE__, ##__VA_ARGS__); \
    fflush(stderr); \
} while(0)

// ==== Source files ====

SrcFile* srcfile_init(char *name) {
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

bool srcfile_finished_p(SrcFile* srcfile) {
  return srcfile->eof;
}

// ==== Objects ====

Obj* obj_init(Tag tag) {
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
    obj_free(l->buffer[i]);
  }
  free(l->buffer);
  free(l);
}

void list_print(List *l, size_t indent) {
  size_t num_width = 1 + (size_t) log10(l->len);
  char formatstr[100];
  snprintf(formatstr, 100, "%%%zus%%%zuzu: %%s\n", indent, num_width);

  for (size_t i = 0; i < l->len; i++) {
    switch (l->buffer[i]->tag) {
    case SEXP:
      printf(formatstr, "", i, "(");
      list_print(l->buffer[i]->sexp, indent + 2);
      printf(formatstr, "", i, ")");
      break;
    case ATOM:
      printf(formatstr, "", i, l->buffer[i]->atom->buffer);
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

    printf("%c", ch);

    if (atom != NULL) {
      if (isspace(ch) || ch == ')') {
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
    fprintf(stderr, "error: Unable to access file %s.", parser->srcfile->name);
    exit(EXIT_FAILURE);
  }

  parser_next(parser, parser->list, NULL);

  printf("\n\n");
}

void parser_print(Parser *parser) {
  if (parser->list == NULL) {
    printf("(NULL)\n");
    return;
  }

  printf("=== %s ===\n", parser->srcfile->name);
  list_print(parser->list, 0);
}


Parser* parser_init(char *filename) {
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
  SrcFile *src = srcfile_init("hello.sic");
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
  parser_print(parser);
  parser_free(parser);
}

int main(int argc, char** argv) {
  printf("=== Testing sicc! ===\n");
  // test_src_file_reads();
  // test_list_management();
  test_parse();
}


#endif
