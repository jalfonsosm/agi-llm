/*
 * llm_context.h - LLM Context Management for NAGI
 *
 * This module manages the context that gets passed to the embedded LLM
 * for natural language understanding and generation. It tracks:
 * - Current game state (room, inventory, flags)
 * - Recent actions and their outcomes
 * - Dialogue history
 * - Scene descriptions
 *
 * The context is used by the LLM parser to:
 * - Understand player intent from natural language input
 * - Generate contextual responses
 * - Map free-form input to AGI verbs/nouns
 */

#ifndef NAGI_LLM_LIB_CONTEXT_H

/* Basic types (independent of nagi) */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define NAGI_LLM_LIB_CONTEXT_H



#ifdef __cplusplus
extern "C" {
#endif

/* Maximum sizes for context buffers */
#define LLM_MAX_CONTEXT_SIZE 8192
#define LLM_MAX_HISTORY_ENTRIES 50
#define LLM_MAX_ENTRY_SIZE 512
#define LLM_MAX_ROOM_DESC_SIZE 1024
#define LLM_MAX_OBJECTS_SIZE 512

/*
 * Context entry types
 */
typedef enum {
    CTX_PLAYER_INPUT,      /* Player typed something */
    CTX_GAME_OUTPUT,       /* Game displayed text */
    CTX_ROOM_CHANGE,       /* Player moved to new room */
    CTX_ACTION_SUCCESS,    /* An action succeeded */
    CTX_ACTION_FAIL,       /* An action failed */
    CTX_INVENTORY_CHANGE,  /* Inventory changed */
    CTX_FLAG_CHANGE,       /* A flag changed */
    CTX_SCENE_DESC,        /* Scene description */
    CTX_NPC_DIALOGUE,      /* NPC dialogue */
    CTX_SYSTEM_MSG         /* System message */
} llm_context_type_t;

/*
 * Single context entry
 */
typedef struct {
    llm_context_type_t type;
    u32 timestamp;             /* Game tick when this occurred */
    int room;                  /* Room where this occurred */
    char text[LLM_MAX_ENTRY_SIZE];
} llm_context_entry_t;

/*
 * Game object info for context
 */
typedef struct {
    int id;
    char name[64];
    int room;       /* 255 = in inventory */
    int visible;
} llm_object_info_t;

/*
 * Room info for context
 */
typedef struct {
    int room_num;
    char description[LLM_MAX_ROOM_DESC_SIZE];
    char exits[256];           /* Available exits */
    int objects[32];           /* Object IDs visible in room */
    int object_count;
} llm_room_info_t;

/*
 * Full LLM context structure
 */
typedef struct {
    /* Current game state */
    int current_room;
    int score;
    int max_score;

    /* Room information */
    llm_room_info_t room_info;

    /* Conversation/action history (circular buffer) */
    llm_context_entry_t history[LLM_MAX_HISTORY_ENTRIES];
    int history_head;
    int history_count;

    /* Inventory */
    int inventory[32];
    int inventory_count;

    /* Important flags (game-specific) */
    struct {
        int flag_num;
        const char *description;
        int value;
    } tracked_flags[64];
    int tracked_flags_count;

    /* Compiled context string (for LLM input) */
    char context_buffer[LLM_MAX_CONTEXT_SIZE];
    int context_dirty;  /* 1 if context needs rebuilding */
} llm_context_t;

/* Global context instance */
extern llm_context_t g_llm_context;

/*
 * Initialize the LLM context system
 */
void llm_context_init(void);

/*
 * Shutdown the LLM context system
 */
void llm_context_shutdown(void);

/*
 * Clear all context history
 */
void llm_context_clear(void);

/*
 * Add an entry to the context history
 *
 * @param type: Type of context entry
 * @param text: Text content of the entry
 */
void llm_context_add(llm_context_type_t type, const char *text);

/*
 * Add a formatted entry to the context history
 *
 * @param type: Type of context entry
 * @param fmt: Format string
 * @param ...: Format arguments
 */
void llm_context_addf(llm_context_type_t type, const char *fmt, ...);

/*
 * Update room information
 *
 * @param room_num: Room number
 * @param description: Room description text
 * @param exits: Available exits description
 */
void llm_context_set_room(int room_num, const char *description, const char *exits);

/*
 * Add an object to context
 *
 * @param obj_id: Object ID
 * @param name: Object name
 * @param room: Room where object is (255 = inventory)
 */
void llm_context_add_object(int obj_id, const char *name, int room);

/*
 * Update inventory in context
 */
void llm_context_update_inventory(void);

/*
 * Track a flag for context
 *
 * @param flag_num: Flag number
 * @param description: Human-readable description of what this flag means
 */
void llm_context_track_flag(int flag_num, const char *description);

/*
 * Update all tracked flags
 */
void llm_context_update_flags(void);

/*
 * Build the context string for LLM input
 * Returns a pointer to the internal context buffer.
 * The buffer is only rebuilt if context_dirty is set.
 *
 * @return: Pointer to context string
 */
const char *llm_context_build(void);

/*
 * Force rebuild of context string
 */
void llm_context_invalidate(void);

/*
 * Get recent history as a string
 *
 * @param buffer: Output buffer
 * @param buffer_size: Size of output buffer
 * @param max_entries: Maximum number of entries to include
 */
void llm_context_get_history(char *buffer, int buffer_size, int max_entries);

void llm_context_on_print(const char *text);
void llm_context_on_room_change(int old_room, int new_room);
void llm_context_on_flag_change(int flag_num, int new_value);
void llm_context_on_var_change(int var_num, int new_value);
void llm_context_on_player_input(const char *input);

/* Get the most recent raw player input text (internal buffer pointer, do not free) */
const char *llm_context_get_last_player_input(void);

/* Clear the last stored player input */
void llm_context_clear_last_player_input(void);

/*
 * Serialize context to JSON for external LLM APIs
 *
 * @param buffer: Output buffer
 * @param buffer_size: Size of output buffer
 * @return: Number of bytes written
 */
int llm_context_to_json(char *buffer, int buffer_size);

/*
 * Load room descriptions from a file
 * File format: room_num|description|exits
 *
 * @param filename: Path to room descriptions file
 * @return: Number of rooms loaded
 */
int llm_context_load_room_descs(const char *filename);

/*
 * Load object names from a file
 * File format: obj_id|name
 *
 * @param filename: Path to object names file
 * @return: Number of objects loaded
 */
int llm_context_load_object_names(const char *filename);

/*
 * Load flag descriptions from a file
 * File format: flag_num|description
 *
 * @param filename: Path to flag descriptions file
 * @return: Number of flags loaded
 */
int llm_context_load_flag_descs(const char *filename);

#ifdef __cplusplus
}
#endif

#endif /* NAGI_LLM_LIB_CONTEXT_H */
