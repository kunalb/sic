#include <stdio.h>
#include <stdlib.h>

enum State {
  BEGIN,
  IN_SEXP,
  IN_DQ_STR,
  IN_SQ_STR,
  IN_SYMBOL,
  IN_NUMBER,
  IN_WHITESPACE,
};

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

int main(int argc, char** argv) {
  if (argc < 1) {
    fprintf(stderr, "Usage: %s <file to transpile>", argv[0]);
    exit(EXIT_FAILURE);
  }

  parse(argv[1]);
}
