#pragma once

#include <stdlib.h>
#include <string.h>

#include "dictionary.h"

struct trie_record {
  char               *kanji;
  struct trie_record *record;
};

struct trie {
  size_t              *base;
  size_t              *check;
  struct trie_record **records;
  size_t               length;
};

void trie_create (struct trie *trie, struct dictionary_entry **entries,
                  size_t length);
void trie_destroy (struct trie *trie);
