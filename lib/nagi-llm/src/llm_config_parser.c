/*
 * llm_config_parser.c - Unified LLM Configuration Parser Implementation
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "../include/nagi_llm.h"

/*
 * Trim leading and trailing whitespace from a string
 */
void trim_whitespace(char *str)
{
    if (!str) return;

    /* Trim leading whitespace */
    char *start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    /* Move trimmed string to beginning */
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }

    /* Trim trailing whitespace */
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
}

/*
 * Parse a key=value line from config file
 * Returns 1 on success, 0 if not a valid key=value line
 */
int parse_config_line(const char *line, char *key, char *value, int max_len)
{
    if (!line || !key || !value) return 0;

    /* Skip comments and empty lines */
    char temp[1024];
    strncpy(temp, line, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    trim_whitespace(temp);

    if (temp[0] == '\0' || temp[0] == '#' || temp[0] == ';') {
        return 0;
    }

    /* Check for section header [section] */
    if (temp[0] == '[') {
        return 0;
    }

    /* Find = sign */
    char *eq = strchr(temp, '=');
    if (!eq) return 0;

    /* Extract key */
    size_t key_len = eq - temp;
    if (key_len >= (size_t)max_len) key_len = max_len - 1;
    strncpy(key, temp, key_len);
    key[key_len] = '\0';
    trim_whitespace(key);

    /* Extract value */
    strncpy(value, eq + 1, max_len - 1);
    value[max_len - 1] = '\0';
    trim_whitespace(value);

    return (key[0] != '\0');
}

/*
 * Check if line is a section header and extract section name
 * Returns 1 if section header, 0 otherwise
 */
static int is_section_header(const char *line, char *section_name, int max_len)
{
    char temp[256];
    strncpy(temp, line, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    trim_whitespace(temp);

    if (temp[0] != '[') return 0;

    char *end = strchr(temp, ']');
    if (!end) return 0;

    /* Extract section name */
    size_t len = end - temp - 1;
    if (len >= (size_t)max_len) len = max_len - 1;
    strncpy(section_name, temp + 1, len);
    section_name[len] = '\0';
    trim_whitespace(section_name);

    return 1;
}

/*
 * Load unified configuration from llm_config.ini
 */
int nagi_llm_load_config(nagi_llm_config_t *config,
                         nagi_llm_backend_t backend,
                         const char *config_file)
{
    const char *filename;
    FILE *f;
    char line[1024];
    char current_section[64] = "";
    char key[128], value[512];
    const char *backend_section;
    
    if (!config) return 0;

    /* Initialize config with defaults */
    memset(config, 0, sizeof(nagi_llm_config_t));
    config->temperature = 0.0f;
    config->temperature_creative_base = 0.3f;
    config->temperature_creative_offset = 0.2f;
    config->max_tokens = 512;
    config->verbose = 0;
    config->context_size = 4096;
    config->batch_size = 1024;
    config->u_batch_size = 512;
    config->n_threads = 4;
    config->top_p = 0.9f;
    config->top_k = 40;
    config->use_gpu = 1;
    config->flash_attn = 0;
    config->n_seq_max = 1;

    /* Use default filename if not specified */
    filename = config_file ? config_file : "llm_config.ini";

    f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "LLM Config: Could not open %s\n", filename);
        return 0;
    }

    /* Determine which sections to read based on backend */
    backend_section = NULL;
    switch (backend) {
        case NAGI_LLM_BACKEND_LLAMACPP:
            backend_section = "llamacpp";
            break;
        case NAGI_LLM_BACKEND_BITNET:
            backend_section = "bitnet";
            break;
        case NAGI_LLM_BACKEND_CLOUD:
            backend_section = "cloud";
            break;
        default:
            backend_section = "";
            break;
    }

    /* Parse file line by line */
    while (fgets(line, sizeof(line), f)) {
        /* Check for section header */
        char section_name[64];
        if (is_section_header(line, section_name, sizeof(section_name))) {
            strncpy(current_section, section_name, sizeof(current_section) - 1);
            current_section[sizeof(current_section) - 1] = '\0';
            continue;
        }

        /* Parse key=value */
        if (!parse_config_line(line, key, value, sizeof(key))) {
            continue;
        }

        /* Parse common settings */
        if (strcmp(current_section, "common") == 0) {
            if (strcmp(key, "temperature_extraction") == 0) {
                config->temperature = atof(value);
            } else if (strcmp(key, "temperature_creative_base") == 0) {
                config->temperature_creative_base = atof(value);
            } else if (strcmp(key, "temperature_creative_offset") == 0) {
                config->temperature_creative_offset = atof(value);
            } else if (strcmp(key, "max_tokens") == 0) {
                config->max_tokens = atoi(value);
            } else if (strcmp(key, "verbose") == 0) {
                config->verbose = atoi(value);
            }
        }
        /* Parse backend-specific settings */
        else if (backend_section && strcmp(current_section, backend_section) == 0) {
            /* LlamaCPP/BitNet settings */
            if (backend == NAGI_LLM_BACKEND_LLAMACPP || backend == NAGI_LLM_BACKEND_BITNET) {
                if (strcmp(key, "context_size") == 0) {
                    config->context_size = atoi(value);
                } else if (strcmp(key, "batch_size") == 0) {
                    config->batch_size = atoi(value);
                } else if (strcmp(key, "u_batch_size") == 0) {
                    config->u_batch_size = atoi(value);
                } else if (strcmp(key, "n_threads") == 0) {
                    config->n_threads = atoi(value);
                } else if (strcmp(key, "top_p") == 0) {
                    config->top_p = atof(value);
                } else if (strcmp(key, "top_k") == 0) {
                    config->top_k = atoi(value);
                } else if (strcmp(key, "use_gpu") == 0) {
                    config->use_gpu = atoi(value);
                } else if (strcmp(key, "flash_attn") == 0) {
                    config->flash_attn = atoi(value);
                } else if (strcmp(key, "n_seq_max") == 0) {
                    config->n_seq_max = atoi(value);
                }
            }
            /* Cloud-specific settings */
            else if (backend == NAGI_LLM_BACKEND_CLOUD) {
                if (strcmp(key, "api_url") == 0) {
                    strncpy(config->api_endpoint, value, sizeof(config->api_endpoint) - 1);
                    config->api_endpoint[sizeof(config->api_endpoint) - 1] = '\0';
                } else if (strcmp(key, "api_key") == 0) {
                    strncpy(config->api_key, value, sizeof(config->api_key) - 1);
                    config->api_key[sizeof(config->api_key) - 1] = '\0';
                } else if (strcmp(key, "model") == 0) {
                    strncpy(config->model_path, value, sizeof(config->model_path) - 1);
                    config->model_path[sizeof(config->model_path) - 1] = '\0';
                }
                /* For cloud backend, temperature is used from common section's temperature_creative_base */
            }
        }
    }

    fclose(f);
    return 1;
}
