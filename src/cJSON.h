/*
 *  SPDX-License-Identifier: MIT
 *
 *  Minimal cJSON stand-in.
 *
 *  The X6100 app dialogs build a JSON object with cJSON and hand it to the live
 *  remote server. That server is not ported yet, so the calls have nothing to
 *  publish: these are no-ops with the same shape, which keeps the ported dialogs
 *  readable instead of stripping their bodies. Replace this file with the real
 *  cJSON (cJSON.c/h) when the remote server lands.
 */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cJSON {
    struct cJSON *next, *prev, *child;
    int           type;
    int           valueint;
    double        valuedouble;
    char         *valuestring;
    char         *string;
} cJSON;

enum { cJSON_Invalid, cJSON_False, cJSON_True, cJSON_NULL, cJSON_Number, cJSON_String, cJSON_Array, cJSON_Object, cJSON_Raw };

#define cJSON_IsNumber(j)  ((j) != NULL && (j)->type == cJSON_Number)
#define cJSON_IsBool(j)    ((j) != NULL && ((j)->type == cJSON_True || (j)->type == cJSON_False))
#define cJSON_IsTrue(j)    ((j) != NULL && (j)->type == cJSON_True)
#define cJSON_IsArray(j)   ((j) != NULL && (j)->type == cJSON_Array)
#define cJSON_IsString(j)  ((j) != NULL && (j)->type == cJSON_String)
#define cJSON_ArrayForEach(el, arr) for ((el) = NULL; (el) != NULL; (el) = (el)->next)

static inline cJSON *cJSON_CreateObject(void) { return NULL; }
static inline cJSON *cJSON_CreateArray(void) { return NULL; }
static inline cJSON *cJSON_Parse(const char *s) { (void) s; return NULL; }
static inline void   cJSON_Delete(cJSON *j) { (void) j; }
static inline cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON *j, const char *k) { (void) j; (void) k; return NULL; }
static inline cJSON *cJSON_GetArrayItem(const cJSON *j, int i) { (void) j; (void) i; return NULL; }
static inline int    cJSON_GetArraySize(const cJSON *j) { (void) j; return 0; }
static inline char  *cJSON_GetStringValue(const cJSON *j) { (void) j; return NULL; }
static inline cJSON *cJSON_AddStringToObject(cJSON *j, const char *k, const char *v) { (void) j; (void) k; (void) v; return NULL; }
static inline cJSON *cJSON_AddNumberToObject(cJSON *j, const char *k, double v) { (void) j; (void) k; (void) v; return NULL; }
static inline cJSON *cJSON_AddBoolToObject(cJSON *j, const char *k, int v) { (void) j; (void) k; (void) v; return NULL; }
static inline cJSON *cJSON_AddArrayToObject(cJSON *j, const char *k) { (void) j; (void) k; return NULL; }
static inline void   cJSON_AddItemToArray(cJSON *a, cJSON *i) { (void) a; (void) i; }
static inline cJSON *cJSON_CreateString(const char *s) { (void) s; return NULL; }
static inline cJSON *cJSON_CreateNumber(double v) { (void) v; return NULL; }

#ifdef __cplusplus
}
#endif
