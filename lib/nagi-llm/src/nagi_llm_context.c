/*
 * llm_context.c - LLM Context Management for NAGI
 *
 * Implementation of the context management system for LLM integration.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#include "../include/nagi_llm_context.h"

/* MSVC uses strtok_s instead of strtok_r */
#ifdef _MSC_VER
#define strtok_r(str, delim, saveptr) strtok_s(str, delim, saveptr)
#endif

/* Global context instance */
llm_context_t g_llm_context = {0};

/* Internal room descriptions storage */
static struct {
    int room_num;
    char description[LLM_MAX_ROOM_DESC_SIZE];
    char exits[256];
} room_descs[256];
static int room_descs_count = 0;

/* Internal object names storage */
static struct {
    int obj_id;
    char name[64];
} object_names[256];
static int object_names_count = 0;

/*
 * Initialize the LLM context system
 */
void llm_context_init(void)
{
    memset(&g_llm_context, 0, sizeof(g_llm_context));
    g_llm_context.context_dirty = 1;
    printf("LLM Context: Initialized\n");
}

/*
 * Shutdown the LLM context system
 */
void llm_context_shutdown(void)
{
    memset(&g_llm_context, 0, sizeof(g_llm_context));
    printf("LLM Context: Shutdown\n");
}

/*
 * Clear all context history
 */
void llm_context_clear(void)
{
    g_llm_context.history_head = 0;
    g_llm_context.history_count = 0;
    g_llm_context.context_dirty = 1;
}

/*
 * Add an entry to the context history
 */
void llm_context_add(llm_context_type_t type, const char *text)
{
    llm_context_entry_t *entry;
    int idx;

    /* Calculate insertion index (circular buffer) */
    idx = (g_llm_context.history_head + g_llm_context.history_count) % LLM_MAX_HISTORY_ENTRIES;

    if (g_llm_context.history_count >= LLM_MAX_HISTORY_ENTRIES) {
        /* Buffer full, overwrite oldest entry */
        g_llm_context.history_head = (g_llm_context.history_head + 1) % LLM_MAX_HISTORY_ENTRIES;
    } else {
        g_llm_context.history_count++;
    }

    entry = &g_llm_context.history[idx];
    entry->type = type;
    entry->timestamp = 0;  /* Game engine should set this via llm_context_set_room() if needed */
    entry->room = g_llm_context.current_room;

    strncpy(entry->text, text, LLM_MAX_ENTRY_SIZE - 1);
    entry->text[LLM_MAX_ENTRY_SIZE - 1] = '\0';

    g_llm_context.context_dirty = 1;
}

/*
 * Add a formatted entry to the context history
 */
void llm_context_addf(llm_context_type_t type, const char *fmt, ...)
{
    char buffer[LLM_MAX_ENTRY_SIZE];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    llm_context_add(type, buffer);
}

/*
 * Update room information
 */
void llm_context_set_room(int room_num, const char *description, const char *exits)
{
    g_llm_context.room_info.room_num = room_num;

    if (description) {
        strncpy(g_llm_context.room_info.description, description,
                LLM_MAX_ROOM_DESC_SIZE - 1);
        g_llm_context.room_info.description[LLM_MAX_ROOM_DESC_SIZE - 1] = '\0';
    }

    if (exits) {
        strncpy(g_llm_context.room_info.exits, exits, sizeof(g_llm_context.room_info.exits) - 1);
        g_llm_context.room_info.exits[sizeof(g_llm_context.room_info.exits) - 1] = '\0';
    }

    g_llm_context.current_room = room_num;
    g_llm_context.context_dirty = 1;
}

/*
 * Add an object to context
 */
void llm_context_add_object(int obj_id, const char *name, int room)
{
    /* Store in room info if in current room */
    if (room == g_llm_context.current_room) {
        if (g_llm_context.room_info.object_count < 32) {
            g_llm_context.room_info.objects[g_llm_context.room_info.object_count++] = obj_id;
        }
    }

    /* Store in inventory if carried */
    if (room == 255) {
        if (g_llm_context.inventory_count < 32) {
            g_llm_context.inventory[g_llm_context.inventory_count++] = obj_id;
        }
    }

    g_llm_context.context_dirty = 1;
}

/*
 * Update inventory in context
 */
void llm_context_update_inventory(void)
{
    /* TODO: Iterate through actual game objects and update inventory list */
    g_llm_context.context_dirty = 1;
}

/*
 * Track a flag for context
 */
void llm_context_track_flag(int flag_num, const char *description)
{
    int idx;

    if (g_llm_context.tracked_flags_count >= 64) return;

    idx = g_llm_context.tracked_flags_count++;
    g_llm_context.tracked_flags[idx].flag_num = flag_num;
    g_llm_context.tracked_flags[idx].description = description;
    g_llm_context.tracked_flags[idx].value = 0;
}

/*
 * Get context entry type as string
 */
static const char *context_type_str(llm_context_type_t type)
{
    switch (type) {
        case CTX_PLAYER_INPUT:     return "PLAYER";
        case CTX_GAME_OUTPUT:      return "GAME";
        case CTX_ROOM_CHANGE:      return "ROOM";
        case CTX_ACTION_SUCCESS:   return "SUCCESS";
        case CTX_ACTION_FAIL:      return "FAIL";
        case CTX_INVENTORY_CHANGE: return "INVENTORY";
        case CTX_FLAG_CHANGE:      return "FLAG";
        case CTX_SCENE_DESC:       return "SCENE";
        case CTX_NPC_DIALOGUE:     return "NPC";
        case CTX_SYSTEM_MSG:       return "SYSTEM";
        default:                   return "UNKNOWN";
    }
}

/*
 * Build the context string for LLM input
 */
const char *llm_context_build(void)
{
    char *buf = g_llm_context.context_buffer;
    int remaining = LLM_MAX_CONTEXT_SIZE;
    int written;

    if (!g_llm_context.context_dirty) {
        return buf;
    }

    buf[0] = '\0';

    /* Header with current state */
    written = snprintf(buf, remaining,
        "=== GAME STATE ===\n"
        "Room: %d\n"
        "Score: %d/%d\n\n",
        g_llm_context.current_room,
        g_llm_context.score,
        g_llm_context.max_score);

    if (written > 0 && written < remaining) {
        buf += written;
        remaining -= written;
    }

    /* Room description if available */
    if (g_llm_context.room_info.description[0]) {
        written = snprintf(buf, remaining,
            "=== CURRENT LOCATION ===\n%s\n",
            g_llm_context.room_info.description);

        if (written > 0 && written < remaining) {
            buf += written;
            remaining -= written;
        }

        if (g_llm_context.room_info.exits[0]) {
            written = snprintf(buf, remaining, "Exits: %s\n",
                g_llm_context.room_info.exits);
            if (written > 0 && written < remaining) {
                buf += written;
                remaining -= written;
            }
        }
        written = snprintf(buf, remaining, "\n");
        if (written > 0 && written < remaining) {
            buf += written;
            remaining -= written;
        }
    }

    /* Inventory */
    if (g_llm_context.inventory_count > 0) {
        written = snprintf(buf, remaining, "=== INVENTORY ===\n");
        if (written > 0 && written < remaining) {
            buf += written;
            remaining -= written;
        }

        for (int i = 0; i < g_llm_context.inventory_count; i++) {
            int obj_id = g_llm_context.inventory[i];
            const char *name = "unknown object";

            /* Look up object name */
            for (int j = 0; j < object_names_count; j++) {
                if (object_names[j].obj_id == obj_id) {
                    name = object_names[j].name;
                    break;
                }
            }

            written = snprintf(buf, remaining, "- %s\n", name);
            if (written > 0 && written < remaining) {
                buf += written;
                remaining -= written;
            }
        }
        written = snprintf(buf, remaining, "\n");
        if (written > 0 && written < remaining) {
            buf += written;
            remaining -= written;
        }
    }

    /* Tracked flags (game state) */
    if (g_llm_context.tracked_flags_count > 0) {
        written = snprintf(buf, remaining, "=== GAME FLAGS ===\n");
        if (written > 0 && written < remaining) {
            buf += written;
            remaining -= written;
        }

        for (int i = 0; i < g_llm_context.tracked_flags_count; i++) {
            if (g_llm_context.tracked_flags[i].value) {
                written = snprintf(buf, remaining, "- %s\n",
                    g_llm_context.tracked_flags[i].description);
                if (written > 0 && written < remaining) {
                    buf += written;
                    remaining -= written;
                }
            }
        }
        written = snprintf(buf, remaining, "\n");
        if (written > 0 && written < remaining) {
            buf += written;
            remaining -= written;
        }
    }

    /* Recent history */
    written = snprintf(buf, remaining, "=== RECENT EVENTS ===\n");
    if (written > 0 && written < remaining) {
        buf += written;
        remaining -= written;
    }

    /* Show last N entries from history */
    int start_idx = 0;
    int show_count = g_llm_context.history_count;
    if (show_count > 20) {
        start_idx = show_count - 20;
        show_count = 20;
    }

    for (int i = 0; i < show_count; i++) {
        int idx = (g_llm_context.history_head + start_idx + i) % LLM_MAX_HISTORY_ENTRIES;
        llm_context_entry_t *entry = &g_llm_context.history[idx];

        written = snprintf(buf, remaining, "[%s] %s\n",
            context_type_str(entry->type), entry->text);
        if (written > 0 && written < remaining) {
            buf += written;
            remaining -= written;
        }
    }

    g_llm_context.context_dirty = 0;
    return g_llm_context.context_buffer;
}

/*
 * Force rebuild of context string
 */
void llm_context_invalidate(void)
{
    g_llm_context.context_dirty = 1;
}

/*
 * Get recent history as a string
 */
void llm_context_get_history(char *buffer, int buffer_size, int max_entries)
{
    char *buf = buffer;
    int remaining = buffer_size;
    int written;

    buf[0] = '\0';

    int start_idx = 0;
    int show_count = g_llm_context.history_count;
    if (show_count > max_entries) {
        start_idx = show_count - max_entries;
        show_count = max_entries;
    }

    for (int i = 0; i < show_count; i++) {
        int idx = (g_llm_context.history_head + start_idx + i) % LLM_MAX_HISTORY_ENTRIES;
        llm_context_entry_t *entry = &g_llm_context.history[idx];

        written = snprintf(buf, remaining, "[%s] %s\n",
            context_type_str(entry->type), entry->text);
        if (written > 0 && written < remaining) {
            buf += written;
            remaining -= written;
        }
    }
}

/*
 * Callback: Game printed text
 */
void llm_context_on_print(const char *text)
{
    llm_context_add(CTX_GAME_OUTPUT, text);
}

/*
 * Callback: Room changed
 */
void llm_context_on_room_change(int old_room, int new_room)
{
    llm_context_addf(CTX_ROOM_CHANGE, "Moved from room %d to room %d", old_room, new_room);

    /* Look up room description */
    for (int i = 0; i < room_descs_count; i++) {
        if (room_descs[i].room_num == new_room) {
            llm_context_set_room(new_room,
                room_descs[i].description,
                room_descs[i].exits);
            break;
        }
    }

    g_llm_context.current_room = new_room;
}

/*
 * Callback: Flag changed
 */
void llm_context_on_flag_change(int flag_num, int new_value)
{
    /* Check if this is a tracked flag */
    for (int i = 0; i < g_llm_context.tracked_flags_count; i++) {
        if (g_llm_context.tracked_flags[i].flag_num == flag_num) {
            g_llm_context.tracked_flags[i].value = new_value;
            llm_context_addf(CTX_FLAG_CHANGE, "%s: %s",
                g_llm_context.tracked_flags[i].description,
                new_value ? "true" : "false");
            break;
        }
    }
}

/*
 * Callback: Variable changed
 */
void llm_context_on_var_change(int var_num, int new_value)
{
    /* Track important variables */
    /* Variable 3 is the score in AGI standard */
    if (var_num == 3) {
        g_llm_context.score = new_value;
        llm_context_addf(CTX_SYSTEM_MSG, "Score changed to %d", new_value);
    }
}

/*
 * Callback: Player input
 */
void llm_context_on_player_input(const char *input)
{
    llm_context_add(CTX_PLAYER_INPUT, input);
}

/*
 * Serialize context to JSON
 */
int llm_context_to_json(char *buffer, int buffer_size)
{
    int written;

    /* Build a simple JSON representation */
    written = snprintf(buffer, buffer_size,
        "{\n"
        "  \"room\": %d,\n"
        "  \"score\": %d,\n"
        "  \"maxScore\": %d,\n"
        "  \"roomDescription\": \"%s\",\n"
        "  \"exits\": \"%s\",\n"
        "  \"inventoryCount\": %d,\n"
        "  \"historyCount\": %d\n"
        "}\n",
        g_llm_context.current_room,
        g_llm_context.score,
        g_llm_context.max_score,
        g_llm_context.room_info.description,
        g_llm_context.room_info.exits,
        g_llm_context.inventory_count,
        g_llm_context.history_count);

    return written;
}

/*
 * Load room descriptions from a file
 */
int llm_context_load_room_descs(const char *filename)
{
    FILE *f;
    char line[2048];
    int count = 0;

    f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "LLM Context: Could not open room descriptions file: %s\n", filename);
        return 0;
    }

    while (fgets(line, sizeof(line), f) && room_descs_count < 256) {
        char *room_str, *desc_str, *exits_str;
        char *saveptr;

        /* Remove newline */
        line[strcspn(line, "\r\n")] = '\0';

        /* Parse: room_num|description|exits */
        room_str = strtok_r(line, "|", &saveptr);
        desc_str = strtok_r(NULL, "|", &saveptr);
        exits_str = strtok_r(NULL, "|", &saveptr);

        if (room_str && desc_str) {
            room_descs[room_descs_count].room_num = atoi(room_str);
            strncpy(room_descs[room_descs_count].description, desc_str,
                    LLM_MAX_ROOM_DESC_SIZE - 1);
            room_descs[room_descs_count].description[LLM_MAX_ROOM_DESC_SIZE - 1] = '\0';

            if (exits_str) {
                strncpy(room_descs[room_descs_count].exits, exits_str,
                        sizeof(room_descs[room_descs_count].exits) - 1);
                room_descs[room_descs_count].exits[sizeof(room_descs[room_descs_count].exits) - 1] = '\0';
            } else {
                room_descs[room_descs_count].exits[0] = '\0';
            }

            room_descs_count++;
            count++;
        }
    }

    fclose(f);
    printf("LLM Context: Loaded %d room descriptions\n", count);
    return count;
}

/*
 * Load object names from a file
 */
int llm_context_load_object_names(const char *filename)
{
    FILE *f;
    char line[256];
    int count = 0;

    f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "LLM Context: Could not open object names file: %s\n", filename);
        return 0;
    }

    while (fgets(line, sizeof(line), f) && object_names_count < 256) {
        char *id_str, *name_str;
        char *saveptr;

        /* Remove newline */
        line[strcspn(line, "\r\n")] = '\0';

        /* Parse: obj_id|name */
        id_str = strtok_r(line, "|", &saveptr);
        name_str = strtok_r(NULL, "|", &saveptr);

        if (id_str && name_str) {
            object_names[object_names_count].obj_id = atoi(id_str);
            strncpy(object_names[object_names_count].name, name_str,
                    sizeof(object_names[object_names_count].name) - 1);
            object_names[object_names_count].name[sizeof(object_names[object_names_count].name) - 1] = '\0';

            object_names_count++;
            count++;
        }
    }

    fclose(f);
    printf("LLM Context: Loaded %d object names\n", count);
    return count;
}

/*
 * Load flag descriptions from a file
 */
int llm_context_load_flag_descs(const char *filename)
{
    FILE *f;
    char line[256];
    int count = 0;
    static char flag_desc_storage[64][128];  /* Static storage for descriptions */

    f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "LLM Context: Could not open flag descriptions file: %s\n", filename);
        return 0;
    }

    while (fgets(line, sizeof(line), f) && g_llm_context.tracked_flags_count < 64) {
        char *flag_str, *desc_str;
        char *saveptr;

        /* Remove newline */
        line[strcspn(line, "\r\n")] = '\0';

        /* Parse: flag_num|description */
        flag_str = strtok_r(line, "|", &saveptr);
        desc_str = strtok_r(NULL, "|", &saveptr);

        if (flag_str && desc_str) {
            int flag_num = atoi(flag_str);
            strncpy(flag_desc_storage[g_llm_context.tracked_flags_count], desc_str,
                    sizeof(flag_desc_storage[0]) - 1);
            flag_desc_storage[g_llm_context.tracked_flags_count][sizeof(flag_desc_storage[0]) - 1] = '\0';

            llm_context_track_flag(flag_num, flag_desc_storage[g_llm_context.tracked_flags_count - 1]);
            count++;
        }
    }

    fclose(f);
    printf("LLM Context: Loaded %d flag descriptions\n", count);
    return count;
}

/* Return last raw player input stored in history (or NULL) */
const char *llm_context_get_last_player_input(void)
{
    if (g_llm_context.history_count == 0) return NULL;

    for (int i = g_llm_context.history_count - 1; i >= 0; --i) {
        int idx = (g_llm_context.history_head + i) % LLM_MAX_HISTORY_ENTRIES;
        if (g_llm_context.history[idx].type == CTX_PLAYER_INPUT) {
            return g_llm_context.history[idx].text;
        }
    }
    return NULL;
}

/* Clear the last stored player input */
void llm_context_clear_last_player_input(void)
{
    if (g_llm_context.history_count == 0) return;

    for (int i = g_llm_context.history_count - 1; i >= 0; --i) {
        int idx = (g_llm_context.history_head + i) % LLM_MAX_HISTORY_ENTRIES;
        if (g_llm_context.history[idx].type == CTX_PLAYER_INPUT) {
            g_llm_context.history[idx].text[0] = '\0';
            return;
        }
    }
}
