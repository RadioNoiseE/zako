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

void zako_init (struct zako *context, const char *file);
void zako_dispose (struct zako *context);

bool zako_forward (struct zako *context, const char input);
bool zako_backward (struct zako *context);

void zako_select_previous (struct zako *context);
void zako_select_next (struct zako *context);

char *zako_get_preedit (struct zako *context);
char *zako_get_commit (struct zako *context);

void zako_reset (struct zako *context);
