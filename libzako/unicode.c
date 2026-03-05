#include "unicode.h"

uint8_t utf8_width (const uint8_t leading) {
  if (leading <= 0x7f)
    return 1;
  if (leading <= 0xdf)
    return 2;
  if (leading <= 0xef)
    return 3;
  if (leading <= 0xf4)
    return 4;

  return 0;
}
