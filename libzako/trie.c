#include "trie.h"

static int trie_compare (const void *a, const void *b) {
  const struct dictionary_entry *entry_a = *(struct dictionary_entry **) a;
  const struct dictionary_entry *entry_b = *(struct dictionary_entry **) b;

  int result = strcmp (entry_a->input, entry_b->input);

  if (!result)
    return (entry_a->offset > entry_b->offset) -
           (entry_a->offset < entry_b->offset);

  return result;
}

void trie_create (struct trie *trie, struct dictionary_entry **entries,
                  size_t length) {
  qsort (entries, length, sizeof (*entries), &trie_compare);

  trie->size = (26 * 26 * 26 * 26 * 26 * 26 - 1) / 25 + 1;

  trie->base  = calloc (trie->size, sizeof (*trie->base));
  trie->check = calloc (trie->size, sizeof (*trie->check));
  trie->data  = calloc (trie->size, sizeof (*trie->data));

  bool   b;
  char   c, d, e[26];
  size_t h, i, j, k, l, m, n, o, p;

  h = 2;

  for (i = 0; i < 5; i++) {
    for (j = 0; j < length;) {
      for (k = j;
           k < length && strncmp (entries[j]->input, entries[k]->input, i) == 0;
           k++)
        ;

      m = c = 0;
      for (l = j; l < k; l++)
        if (strlen (entries[l]->input) > i && (d = entries[l]->input[i]) != c) {
          c      = d;
          e[m++] = c - 'a';
        }

      if (!m) {
        j = k;
        continue;
      }

      for (l = 2; l < h; l++) {
        b = false;

        for (n = 0; n < m; n++)
          if (trie->check[l + e[n]] != 0) {
            b = true;
            break;
          }

        if (!b)
          break;
      }

      if (h <= (n = l + e[m - 1]))
        h = n + 1;

      n = 1;
      for (o = 0; o < i; o++)
        n = trie->base[n] + entries[j]->input[o] - 'a';

      trie->base[n] = l;
      for (o = 0; o < m; o++)
        trie->check[l + e[o]] = n;

      for (o = j; o < k; o++)
        if (strlen (entries[o]->input) == i + 1) {
          p                      = l + entries[o]->input[i] - 'a';
          struct trie_data *data = trie->data[p];

          if (!data)
            data = trie->data[p] = calloc (1, sizeof (*data));
          else
            while (data->kanji) {
              if (!data->data)
                data->data = calloc (1, sizeof (*data));
              data = data->data;
            }

          data->kanji = strdup (entries[o]->kanji);
        }

      j = k;
    }
  }

  trie->size  = h;
  trie->base  = realloc (trie->base, trie->size * sizeof (*trie->base));
  trie->check = realloc (trie->check, trie->size * sizeof (*trie->check));
  trie->data  = realloc (trie->data, trie->size * sizeof (*trie->data));
}

void trie_destroy (struct trie *trie) {
  free (trie->base);
  free (trie->check);

  for (size_t i = 0; i < trie->size; i++) {
    struct trie_data *data = trie->data[i];

    while (data) {
      struct trie_data *next_data = data->data;
      free (data->kanji);
      free (data);

      data = next_data;
    }
  }

  free (trie->data);
}
