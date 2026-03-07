#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dictionary.h"
#include "trie.h"

struct zako_state {
  bool   ready;
  char  *input;
  char  *commit;
  size_t preedit;
};

struct zako_candidate {
  char **kanji;
  size_t count;
};

struct zako {
  struct trie           *trie;
  struct zako_state     *state;
  struct zako_candidate *candidate;
};
