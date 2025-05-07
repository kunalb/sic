#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>


// === Headers & Data Structures ===

enum State {
  BEGIN,
  IN_SEXP,
  IN_DQ_STR,
  IN_SQ_STR,
  IN_SYMBOL,
  IN_NUMBER,
  IN_WHITESPACE,
};
typedef enum State State;

struct SrcFile {
  char* name;
  FILE* fp;
  size_t row;
  size_t col;
  bool eof;
};
typedef struct SrcFile SrcFile;

struct CCode {
  char* lines;
};

struct SExp {
  size_t start_row;
  size_t start_col;
};
typedef struct SExp SExp;

struct Transpiler {
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


// ==== Output Code ====

void consume_next(SrcFile *f, State state);

void consume_sexp(SrcFile *f) {
  SExp s_exp;
  s_exp.start_row = f->row;
  s_exp.start_col = f->col;

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


void parse(char* filename) {
  SrcFile *f = srcfile_open(filename);
  if (f == NULL) {
    fprintf(stderr, "error: Unable to access file %s.", filename);
    exit(EXIT_FAILURE);
  }

  SExp parsed[128];

  while (!f->eof) {
    consume_next(f, BEGIN);
  }
  srcfile_close(f);
}


// === Transplite ===
void transpile(char *infile, char* outfile) {
}


#ifndef TEST
// === Main ===

int main(int argc, char** argv) {
  if (argc < 1) {
    fprintf(stderr, "Usage: %s <file to transpile>", argv[0]);
    exit(EXIT_FAILURE);
  }

  parse(argv[1]);
}

#else
// === Tests ===

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


void test_parse() {
  parse("hello.sic");
}

int main(int argc, char** argv) {
  printf("=== Testing sicc! ===\n");
  // test_src_file_reads();
  test_parse();
}


#endif
