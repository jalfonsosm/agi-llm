/*
 * llm_config_parser.h - Unified LLM Configuration Parser
 *
 * Parses llm_config.ini file and loads configuration for all backends.
 * Each backend reads only the sections/fields it needs and ignores the rest.
 */

#ifndef LLM_CONFIG_PARSER_H
#define LLM_CONFIG_PARSER_H

#include "nagi_llm.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Load unified configuration from llm_config.ini
 *
 * Reads common settings and backend-specific settings.
 *
 * @param config: Configuration structure to populate
 * @param backend: Backend type (to know which section to read)
 * @param config_file: Path to config file (default: "llm_config.ini")
 * @return: 1 on success, 0 on failure
 */
int nagi_llm_load_config(nagi_llm_config_t *config,
                         nagi_llm_backend_t backend,
                         const char *config_file);

/*
 * Helper: Trim whitespace from string
 */
void trim_whitespace(char *str);

/*
 * Helper: Parse key=value line
 */
int parse_config_line(const char *line, char *key, char *value, int max_len);

#ifdef __cplusplus
}
#endif

#endif /* LLM_CONFIG_PARSER_H */
