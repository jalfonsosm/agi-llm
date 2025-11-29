#include "llm.h"

#include "../ui/msg.h"

u8 *cmd_update_context(u8 *c)
{
    	u8 message_index = *(c++);
    	const char *message_text = logic_msg(message_index);
    
    	#if LOG_DEBUG
    	printf("update.context: index=%d, text='%s'\n", message_index, message_text);
    	#endif

    	if (message_text != NULL) {
                process_context_update(message_text);
        }
    
    	return c;
}

void process_context_update(const char *message)
{
        printf("Processing context update with message: %s\n", message);
}