#pragma once

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "unicode.h"

struct dictionary {
  void  *mapping;
  size_t length;
};

struct dictionary_entry {
  char kanji[5];
  char input[6];
};

void dictionary_open (struct dictionary *memory, const char *file);
void dictionary_close (struct dictionary *memory);

struct dictionary_entry *dictionary_parse (struct dictionary *memory,
                                           size_t            *offset);
