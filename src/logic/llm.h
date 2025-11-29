#ifndef LLM_H
#define LLM_H

#include "../agi.h"

extern u8 *cmd_update_context(u8 *c);

void process_context_update(const char *message);

#endif // LLM_H