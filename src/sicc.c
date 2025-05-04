#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>


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

struct SrcFile {
  char* name;
  FILE* fp;
  size_t line;
  bool eof;
};
typedef struct SrcFile SrcFile;


struct CCode {
  char* lines;
};

struct SExp {

};

struct Transpiler {
};

// === Implementations ===

// ==== Utilities ====
void* checked_malloc(size_t sz) {
  void *ptr = malloc(sz);
  if (ptr == NULL) {
    fprintf(stderr, "Could not allocate %lu bytes of memory! Exiting.", sz);
    exit(1);
  }

  return ptr;
}


// ==== Source files ====

SrcFile* srcfile_open(char *name) {
  FILE *fp = fopen(name, "r");
  if (fp == NULL) {
    return NULL;
  }

  SrcFile *srcfile = calloc(1, sizeof(SrcFile));

  size_t namelen = strnlen(name, FILENAME_MAX);
  srcfile->name = checked_malloc((namelen + 1) * sizeof(char));
  memcpy(srcfile->name, name, namelen);
  srcfile->name[namelen] = '\0';

  srcfile->line = 0;
  srcfile->fp = fp;
  srcfile->eof = false;

  return srcfile;
}

void srcfile_close(SrcFile *srcfile) {
  free(srcfile->name);
  fclose(srcfile->fp);
  free(srcfile);
}

int srcfile_getc(SrcFile *srcfile) {
  int ch = fgetc(srcfile->fp);
  if (ch == '\n') {
    srcfile->line++;
  } else if (ch == EOF) {
    srcfile->eof = true;
  }

  return ch;
}

bool srcfile_finished_p(SrcFile* srcfile) {
  return srcfile->eof;
}


// ==== Output Code ====

void begin_transpiling() {}
void consume_sexp() {}
void consume_dq_str() {}
void consume_sq_str() {}
void consume_number() {}
void consume_whitespace() {}

void tokenize(FILE* f) {
  int ch;
  enum State state = BEGIN;
  while ((ch = fgetc(f)) != EOF) {
    printf("%c", ch);
    switch (state) {

    }
  }
}

void parse(char* filename) {
  FILE* f = fopen(filename, "r");
  if (f == NULL) {
    fprintf(stderr, "error: Unable to access file %s.", filename);
    exit(EXIT_FAILURE);
  }

  tokenize(f);
  fclose(f);
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

int main(int argc, char** argv) {
  printf("=== Testing sicc! ===\n");
  test_src_file_reads();
}


#endif
