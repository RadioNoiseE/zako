#include "libzako.h"

void zako_init (struct zako *context, const char *file) {
  struct dictionary dictionary;
  dictionary_open (&dictionary, file);

  size_t offset, length, capacity;
  offset = length = capacity = 0;

  struct dictionary_entry *entry, **entries;
  entries = NULL;

  while ((entry = dictionary_parse (&dictionary, &offset))) {
    if (length == capacity) {
      capacity = capacity ? capacity * 2 : 2;
      entries  = realloc (entries, capacity * sizeof (*entries));
    }
    entries[length++] = entry;
  }

  context->trie = malloc (sizeof (*context->trie));
  trie_create (context->trie, entries, length);

  for (size_t i = 0; i < length; i++)
    free (entries[i]);
  free (entries);

  dictionary_close (&dictionary);

  context->state     = calloc (1, sizeof (*context->state));
  context->candidate = calloc (1, sizeof (*context->candidate));
}

void zako_dispose (struct zako *context) {
  trie_destroy (context->trie);
  free (context->trie);

  free (context->state->input);
  free (context->state);

  free (context->candidate->kanji);
  free (context->candidate);
}

bool zako_forward (struct zako *context, const char input) {
  if (!context->state->input) {
    // future-proof: support words
    context->state->input    = malloc (6 * sizeof (*context->state->input));
    context->state->input[0] = '\0';
  }

  size_t length = strlen (context->state->input);
  if (length == 5) {
    context->state->ready  = true;
    context->state->commit = context->candidate->kanji[0];
    length                 = 0;
  }

  context->state->input[length]     = input;
  context->state->input[length + 1] = '\0';
  // should we trust compiler optimizations?
  length = strlen (context->state->input);

  free (context->candidate->kanji);
  context->candidate->kanji = NULL;

  size_t offset = 0, check;
  for (size_t i = 0; i < length; i++) {
    check  = offset;
    offset = context->trie->base[offset] + context->state->input[i] - 'a';
    if (context->trie->check[offset] != check)
      return false;
  }

  struct trie_record *record = context->trie->records[offset];
  while (record) {
    if (!context->candidate->kanji) {
      context->candidate->count = 1;
      context->candidate->kanji = malloc (sizeof (*context->candidate->kanji));
    } else {
      context->candidate->count++;
      context->candidate->kanji = realloc (
        context->candidate->kanji,
        context->candidate->count * sizeof (*context->candidate->kanji));
    }

    context->candidate->kanji[context->candidate->count - 1] = record->kanji;
    record                                                   = record->record;
  }

  return true;
}

bool zako_backward (struct zako *context) {
  size_t length = strlen (context->state->input);

  if (length == 0)
    return false;

  context->state->input[length - 1] = '\0';
  // abuse zako_forward
  return zako_forward (context, '\0');
}

void zako_select_previous (struct zako *context) {
  if (context->state->preedit > 0)
    context->state->preedit--;
}

void zako_select_next (struct zako *context) {
  if (context->state->preedit < context->candidate->count - 1)
    context->state->preedit++;
}

char *zako_get_preedit (struct zako *context) {
  return context->candidate->kanji[context->state->preedit];
}

char *zako_get_commit (struct zako *context) {
  if (!context->state->ready) {
    context->state->input[0] = '\0';
    context->state->commit = context->candidate->kanji[context->state->preedit];
  }

  context->state->ready   = false;
  context->state->preedit = 0;

  return context->state->commit;
}
