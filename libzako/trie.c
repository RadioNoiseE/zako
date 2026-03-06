#include "trie.h"

static int trie_compare (const void *a, const void *b) {
  const struct dictionary_entry *entry_a = *(struct dictionary_entry **) a;
  const struct dictionary_entry *entry_b = *(struct dictionary_entry **) b;

  return strcmp (entry_a->input, entry_b->input);
}

void trie_create (struct trie *trie, struct dictionary_entry **entries,
                  size_t length) {
  qsort (entries, length, sizeof (*entries), &trie_compare);

  trie->length = 26 * 26 * 26 * 26 * 26 + 1;

  trie->base    = calloc (trie->length, sizeof (*trie->base));
  trie->check   = calloc (trie->length, sizeof (*trie->check));
  trie->records = calloc (trie->length, sizeof (*trie->records));

  trie->base[0]  = 1;
  trie->check[1] = 0;

  size_t boundary = 26;

  for (size_t i = 0; i < 5; i++) {
    char *previous = NULL;

    for (size_t j = 0; j < length; j++) {
      if (i >= strlen (entries[j]->input))
        continue;

      if (previous && strncmp (previous, entries[j]->input, i + 1) == 0) {
        if (entries[j]->input[i + 1] != '\0')
          continue;

        size_t k = 0;
        for (size_t l = 0; l <= i; l++)
          k = trie->base[k] + entries[j]->input[l] - 'a';

        struct trie_record *record = trie->records[k];
        if (!record)
          record = trie->records[k] = calloc (1, sizeof (*record));
        else
          while (record->kanji) {
            if (!record->record)
              record->record = calloc (1, sizeof (*record));
            record = record->record;
          }

        record->kanji = strdup (entries[j]->kanji);
        continue;
      }

      previous = entries[j]->input;

      size_t k, l;
      k = 0;

      for (size_t m = 0; m <= i; m++) {
        l = k;
        k = trie->base[l] + entries[j]->input[m] - 'a';
      }

      trie->base[k]  = boundary;
      trie->check[k] = l;

      if (i != 4)
        boundary += 26;

      if (entries[j]->input[i + 1] == '\0') {
        struct trie_record *record = trie->records[k];
        if (!record)
          record = trie->records[k] = calloc (1, sizeof (*record));

        record->kanji = strdup (entries[j]->kanji);
      }
    }
  }

  trie->length = boundary;

  trie->base    = realloc (trie->base, boundary * sizeof (*trie->base));
  trie->check   = realloc (trie->check, boundary * sizeof (*trie->check));
  trie->records = realloc (trie->records, boundary * sizeof (*trie->records));
}

void trie_destroy (struct trie *trie) {
  free (trie->base);
  free (trie->check);

  for (size_t i = 0; i < trie->length; i++) {
    struct trie_record *record = trie->records[i];

    while (record) {
      struct trie_record *next_record = record->record;
      free (record->kanji);
      free (record);

      record = next_record;
    }
  }

  free (trie->records);
}
