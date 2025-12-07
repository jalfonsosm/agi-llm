#include "nagi_llm_cloud.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

typedef struct {
    nagi_llm_cloud_config_t config;
    CURL *curl;
} cloud_backend_t;

typedef struct {
    char *data;
    size_t size;
} response_buffer_t;

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    response_buffer_t *mem = (response_buffer_t *)userp;
    
    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) return 0;
    
    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;
    
    return realsize;
}

static char *escape_json_string(const char *str) {
    static char buffer[8192];
    char *out = buffer;
    
    while (*str && (out - buffer) < sizeof(buffer) - 2) {
        if (*str == '"' || *str == '\\') *out++ = '\\';
        else if (*str == '\n') { *out++ = '\\'; *out++ = 'n'; str++; continue; }
        else if (*str == '\r') { *out++ = '\\'; *out++ = 'r'; str++; continue; }
        else if (*str == '\t') { *out++ = '\\'; *out++ = 't'; str++; continue; }
        *out++ = *str++;
    }
    *out = '\0';
    return buffer;
}

static int extract_content(const char *json, char *output, int output_size) {
    const char *content_start = strstr(json, "\"content\":\"");
    if (!content_start) return -1;
    
    content_start += 11;
    const char *content_end = content_start;
    while (*content_end && *content_end != '"') {
        if (*content_end == '\\' && *(content_end + 1)) content_end++;
        content_end++;
    }
    
    int len = content_end - content_start;
    if (len >= output_size) len = output_size - 1;
    
    char *out = output;
    for (int i = 0; i < len; i++) {
        if (content_start[i] == '\\' && i + 1 < len) {
            i++;
            if (content_start[i] == 'n') *out++ = '\n';
            else if (content_start[i] == 'r') *out++ = '\r';
            else if (content_start[i] == 't') *out++ = '\t';
            else *out++ = content_start[i];
        } else {
            *out++ = content_start[i];
        }
    }
    *out = '\0';
    return out - output;
}

int nagi_llm_cloud_init(nagi_llm_t *llm, const nagi_llm_cloud_config_t *config) {
    cloud_backend_t *backend = malloc(sizeof(cloud_backend_t));
    if (!backend) return -1;
    
    memcpy(&backend->config, config, sizeof(nagi_llm_cloud_config_t));
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    backend->curl = curl_easy_init();
    if (!backend->curl) {
        free(backend);
        return -1;
    }
    
    llm->backend_data = backend;
    printf("Cloud LLM initialized: %s (model: %s)\n", config->api_url, config->model);
    return 0;
}

int nagi_llm_cloud_generate(nagi_llm_t *llm, const char *prompt, char *output, int output_size) {
    cloud_backend_t *backend = (cloud_backend_t *)llm->backend_data;
    if (!backend || !backend->curl) return -1;
    
    char json_payload[16384];
    snprintf(json_payload, sizeof(json_payload),
        "{\"model\":\"%s\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],"
        "\"temperature\":%.2f,\"max_tokens\":%d}",
        backend->config.model, escape_json_string(prompt),
        backend->config.temperature, backend->config.max_tokens);
    
    response_buffer_t response = {NULL, 0};
    
    struct curl_slist *headers = NULL;
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", backend->config.api_key);
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    curl_easy_setopt(backend->curl, CURLOPT_URL, backend->config.api_url);
    curl_easy_setopt(backend->curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(backend->curl, CURLOPT_POSTFIELDS, json_payload);
    curl_easy_setopt(backend->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(backend->curl, CURLOPT_WRITEDATA, &response);
    
    CURLcode res = curl_easy_perform(backend->curl);
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        fprintf(stderr, "Cloud API error: %s\n", curl_easy_strerror(res));
        free(response.data);
        return -1;
    }
    
    int len = extract_content(response.data, output, output_size);
    free(response.data);
    
    return len;
}

void nagi_llm_cloud_cleanup(nagi_llm_t *llm) {
    cloud_backend_t *backend = (cloud_backend_t *)llm->backend_data;
    if (backend) {
        if (backend->curl) curl_easy_cleanup(backend->curl);
        free(backend);
        llm->backend_data = NULL;
    }
    curl_global_cleanup();
}
