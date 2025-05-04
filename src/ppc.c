#include <stdio.h>
#include <stdlib.h>


void tokenize(f) {
  while (!feof(f)) {
    char ch = fgetc(f);

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
}


int main(int argc, char** argv) {
  if (argc < 1) {
    fprintf(stderr, "Usage: %s <file to compile>", argv[0]);
    exit(EXIT_FAILURE);
  }

  parse(argv[1]);
}
