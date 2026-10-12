#ifndef STRELA_STATE_JSON_H
#define STRELA_STATE_JSON_H

#include "strela/system.h"
#include <stddef.h>

/* Returns bytes written, or -1 if the buffer is too small or input is invalid.
   A zone value of zero omits tape-zone inspection. */
int strela_state_json(const strela_machine *machine, unsigned ram,
                      unsigned zone, char *output, size_t capacity);

/* A bounded page of the actual punched deck, up to 128 words. */
int strela_punched_json(const strela_machine *machine, size_t start,
                        unsigned limit, char *output, size_t capacity);

#endif
