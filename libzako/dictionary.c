#include "dictionary.h"

void dictionary_open (struct dictionary *memory, const char *file) {
  int descriptor = open (file, O_RDONLY);
  if (descriptor == -1)
    goto fail;

  struct stat information;
  if (fstat (descriptor, &information) == -1)
    goto fail;

  void *mapping =
    mmap (NULL, information.st_size, PROT_READ, MAP_PRIVATE, descriptor, 0);
  if (mapping == MAP_FAILED)
    goto fail;

  if (close (descriptor) == -1)
    goto fail;

  memory->mapping = mapping;
  memory->length  = information.st_size;

fail:
  perror ("dictionary");
}

void dictionary_close (struct dictionary *memory) {
  if (munmap (memory->mapping, memory->length) == -1)
    perror ("dictionary");
}

struct dictionary_entry *dictionary_parse (struct dictionary *memory,
                                           size_t            *offset) {
  if (*offset >= memory->length)
    return NULL;

  struct dictionary_entry *entry = malloc (sizeof (*entry));

  char   *buffer = memory->mapping;
  uint8_t width;

  width = utf8_width ((uint8_t) buffer[*offset]);
  memcpy (entry->kanji, buffer + *offset, width);
  entry->kanji[width] = '\0';

  *offset += width + 1;

  for (width = 0;
       *offset + width < memory->length && buffer[*offset + width] != '\n';
       width++)
    entry->input[width] = buffer[*offset + width];
  entry->input[width] = '\0';

  *offset += width + 1;

  return entry;
}
