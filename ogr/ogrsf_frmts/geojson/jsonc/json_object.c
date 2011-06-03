/*
 * $Id: json_object.c,v 1.17 2006/07/25 03:24:50 mclark Exp $
 *
 * Copyright (c) 2004, 2005 Metaparadigm Pte. Ltd.
 * Michael Clark <michael@metaparadigm.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See COPYING for details.
 *
 */

#include "cpl_conv.h"

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "debug.h"
#include "printbuf.h"
#include "linkhash.h"
#include "arraylist.h"
#include "json_object.h"
#include "json_object_private.h"

#if !HAVE_STRNDUP
  char* strndup(const char* str, size_t n);
#endif /* !HAVE_STRNDUP */

/* #define REFCOUNT_DEBUG 1 */

const char *json_number_chars = "0123456789.+-eE";
const char *json_hex_chars = "0123456789abcdef";

#ifdef REFCOUNT_DEBUG
static const char* json_type_name[] = {
  "null",
  "boolean",
  "double",
  "int",
  "object",
  "array",
  "string",
};
#endif /* REFCOUNT_DEBUG */

static void json_object_generic_delete(struct json_object* jso);
static struct json_object* json_object_new(enum json_type o_type);


/* ref count debugging */

#ifdef REFCOUNT_DEBUG

static struct lh_table *json_object_table;

static void json_object_init(void) __attribute__ ((constructor));
static void json_object_init(void) {
  MC_DEBUG("json_object_init: creating object table\n");
  json_object_table = lh_kptr_table_new(128, "json_object_table", NULL);
}

static void json_object_fini(void) __attribute__ ((destructor));
static void json_object_fini(void) {
  struct lh_entry *ent;
  if(MC_GET_DEBUG()) {
    if (json_object_table->count) {
      MC_DEBUG("json_object_fini: %d referenced objects at exit\n",
  	       json_object_table->count);
      lh_foreach(json_object_table, ent) {
        struct json_object* obj = (struct json_object*)ent->v;
        MC_DEBUG("\t%s:%p\n", json_type_name[obj->o_type], obj);
      }
    }
  }
  MC_DEBUG("json_object_fini: freeing object table\n");
  lh_table_free(json_object_table);
}
#endif /* REFCOUNT_DEBUG */


/* string escaping */

static int json_escape_str(struct printbuf *pb, char *str)
{
  int pos = 0, start_offset = 0;
  unsigned char c;
  do {
    c = str[pos];
    switch(c) {
    case '\0':
      break;
    case '\b':
    case '\n':
    case '\r':
    case '\t':
    case '"':
    case '\\':
    case '/':
      if(pos - start_offset > 0)
	printbuf_memappend(pb, str + start_offset, pos - start_offset);
      if(c == '\b') printbuf_memappend(pb, "\\b", 2);
      else if(c == '\n') printbuf_memappend(pb, "\\n", 2);
      else if(c == '\r') printbuf_memappend(pb, "\\r", 2);
      else if(c == '\t') printbuf_memappend(pb, "\\t", 2);
      else if(c == '"') printbuf_memappend(pb, "\\\"", 2);
      else if(c == '\\') printbuf_memappend(pb, "\\\\", 2);
      else if(c == '/') printbuf_memappend(pb, "\\/", 2);
      start_offset = ++pos;
      break;
    default:
      if(c < ' ') {
	if(pos - start_offset > 0)
	  printbuf_memappend(pb, str + start_offset, pos - start_offset);
	sprintbuf(pb, "\\u00%c%c",
		  json_hex_chars[c >> 4],
		  json_hex_chars[c & 0xf]);
	start_offset = ++pos;
      } else pos++;
    }
  } while(c);
  if(pos - start_offset > 0)
    printbuf_memappend(pb, str + start_offset, pos - start_offset);
  return 0;
}


/* reference counting */

extern struct json_object* json_object_get(struct json_object *jso)
{
  if(jso) {
    jso->_ref_count++;
  }
  return jso;
}

extern void json_object_put(struct json_object *jso)
{
  if(jso) {
    jso->_ref_count--;
    if(!jso->_ref_count) jso->_delete(jso);
  }
}


/* generic object construction and destruction parts */

static void json_object_generic_delete(struct json_object* jso)
{
#ifdef REFCOUNT_DEBUG
  MC_DEBUG("json_object_delete_%s: %p\n",
	   json_type_name[jso->o_type], jso);
  lh_table_delete(json_object_table, jso);
#endif /* REFCOUNT_DEBUG */
  printbuf_free(jso->_pb);
  free(jso);
}

static struct json_object* json_object_new(enum json_type o_type)
{
  struct json_object *jso;

  jso = (struct json_object*)calloc(sizeof(struct json_object), 1);
  if(!jso) return NULL;
  jso->o_type = o_type;
  jso->_ref_count = 1;
  jso->_delete = &json_object_generic_delete;
#ifdef REFCOUNT_DEBUG
  lh_table_insert(json_object_table, jso, jso);
  MC_DEBUG("json_object_new_%s: %p\n", json_type_name[jso->o_type], jso);
#endif /* REFCOUNT_DEBUG */
  return jso;
}


/* type checking functions */

int json_object_is_type(struct json_object *jso, enum json_type type)
{
  return (jso->o_type == type);
}

enum json_type json_object_get_type(struct json_object *jso)
{
  return jso->o_type;
}


/* json_object_to_json_string */

const char* json_object_to_json_string(struct json_object *jso)
{
  if(!jso) return "null";
  if(!jso->_pb) {
    if((jso->_pb = printbuf_new()) == NULL) return NULL;
  } else {
    printbuf_reset(jso->_pb);
  }
  if(jso->_to_json_string(jso, jso->_pb) < 0) return NULL;
  return jso->_pb->buf;
}


/* json_object_object */

static int json_object_object_to_json_string(struct json_object* jso,
					     struct printbuf *pb)
{
  int i=0;
  struct json_object_iter iter;
  iter.key = NULL;
  sprintbuf(pb, "{");

  /* CAW: scope operator to make ANSI correctness */
  /* CAW: switched to json_object_object_foreachC which uses an iterator struct */
	json_object_object_foreachC(jso, iter) {
			if(i) sprintbuf(pb, ",");
			sprintbuf(pb, " \"");
			json_escape_str(pb, iter.key);
			sprintbuf(pb, "\": ");
			if(iter.val == NULL) sprintbuf(pb, "null");
			else iter.val->_to_json_string(iter.val, pb);
			i++;
	}

  return sprintbuf(pb, " }");
}

static void json_object_lh_entry_free(struct lh_entry *ent)
{
  free(ent->k);
  json_object_put((struct json_object*)ent->v);
}

static void json_object_object_delete(struct json_object* jso)
{
  lh_table_free(jso->o.c_object);
  json_object_generic_delete(jso);
}

struct json_object* json_object_new_object(void)
{
  struct json_object *jso = json_object_new(json_type_object);
  if(!jso) return NULL;
  jso->_delete = &json_object_object_delete;
  jso->_to_json_string = &json_object_object_to_json_string;
  jso->o.c_object = lh_kchar_table_new(JSON_OBJECT_DEF_HASH_ENTRIES,
					NULL, &json_object_lh_entry_free);
  return jso;
}

struct lh_table* json_object_get_object(struct json_object *jso)
{
  if(!jso) return NULL;
  switch(jso->o_type) {
  case json_type_object:
    return jso->o.c_object;
  default:
    return NULL;
  }
}

void json_object_object_add(struct json_object* jso, const char *key,
			    struct json_object *val)
{
  lh_table_delete(jso->o.c_object, key);
  lh_table_insert(jso->o.c_object, strdup(key), val);
}

struct json_object* json_object_object_get(struct json_object* jso, const char *key)
{
  return (struct json_object*) lh_table_lookup(jso->o.c_object, key);
}

void json_object_object_del(struct json_object* jso, const char *key)
{
  lh_table_delete(jso->o.c_object, key);
}


/* json_object_boolean */

static int json_object_boolean_to_json_string(struct json_object* jso,
					      struct printbuf *pb)
{
  if(jso->o.c_boolean) return sprintbuf(pb, "true");
  else return sprintbuf(pb, "false");
}

struct json_object* json_object_new_boolean(boolean b)
{
  struct json_object *jso = json_object_new(json_type_boolean);
  if(!jso) return NULL;
  jso->_to_json_string = &json_object_boolean_to_json_string;
  jso->o.c_boolean = b;
  return jso;
}

boolean json_object_get_boolean(struct json_object *jso)
{
  if(!jso) return FALSE;
  switch(jso->o_type) {
  case json_type_boolean:
    return jso->o.c_boolean;
  case json_type_int:
    return (jso->o.c_int != 0);
  case json_type_double:
    return (jso->o.c_double != 0);
  case json_type_string:
    return (strlen(jso->o.c_string) != 0);
  default:
    return FALSE;
  }
}


/* json_object_int */

static int json_object_int_to_json_string(struct json_object* jso,
					  struct printbuf *pb)
{
  return sprintbuf(pb, "%d", jso->o.c_int);
}

struct json_object* json_object_new_int(int i)
{
  struct json_object *jso = json_object_new(json_type_int);
  if(!jso) return NULL;
  jso->_to_json_string = &json_object_int_to_json_string;
  jso->o.c_int = i;
  return jso;
}

int json_object_get_int(struct json_object *jso)
{
  int cint;

  if(!jso) return 0;
  switch(jso->o_type) {
  case json_type_int:
    return jso->o.c_int;
  case json_type_double:
    return (int)jso->o.c_double;
  case json_type_boolean:
    return jso->o.c_boolean;
  case json_type_string:
    if(sscanf(jso->o.c_string, "%d", &cint) == 1) return cint;
  default:
    return 0;
  }
}


/* json_object_double */

/* Begin: GDAL addition */
/************************************************************************/
/*                        json_OGRFormatDouble()                        */
/* Copied & slightly adapted from ogrutils.cpp                          */
/************************************************************************/

static int json_OGRFormatDouble( char *pszBuffer, int nBufferLen, double dfVal,
                                 char chDecimalSep, int nPrecision )
{
    int i;
    int bHasTruncated = FALSE;
    char szFormat[16];
    int ret;
    
    sprintf(szFormat, "%%.%df", nPrecision);

    ret = snprintf(pszBuffer, nBufferLen, szFormat, dfVal);
    /* Windows CRT doesn't conform with C99 and return -1 when buffer is truncated */
    if (ret >= nBufferLen || ret == -1)
        return -1;

    while(TRUE)
    {
        int nCountBeforeDot = 0;
        int iDotPos = -1;
        i = 0;
        while( pszBuffer[i] != '\0' )
        {
            if ((pszBuffer[i] == '.' || pszBuffer[i] == ',') && chDecimalSep != '\0')
            {
                iDotPos = i;
                pszBuffer[i] = chDecimalSep;
            }
            else if (iDotPos < 0 && pszBuffer[i] != '-')
                nCountBeforeDot ++;
            i++;
        }

    /* -------------------------------------------------------------------- */
    /*      Trim trailing 00000x's as they are likely roundoff error.       */
    /* -------------------------------------------------------------------- */
        if( i > 10 && iDotPos >=0 )
        {
            if (/* && pszBuffer[i-1] == '1' &&*/
                pszBuffer[i-2] == '0'
                && pszBuffer[i-3] == '0'
                && pszBuffer[i-4] == '0'
                && pszBuffer[i-5] == '0'
                && pszBuffer[i-6] == '0' )
            {
                pszBuffer[--i] = '\0';
            }
            else if( i - 8 > iDotPos && /* pszBuffer[i-1] == '1' */
                  /* && pszBuffer[i-2] == '0' && */
                    (nCountBeforeDot >= 4 || pszBuffer[i-3] == '0')
                    && (nCountBeforeDot >= 5 || pszBuffer[i-4] == '0')
                    && (nCountBeforeDot >= 6 || pszBuffer[i-5] == '0')
                    && (nCountBeforeDot >= 7 || pszBuffer[i-6] == '0')
                    && (nCountBeforeDot >= 8 || pszBuffer[i-7] == '0')
                    && pszBuffer[i-8] == '0'
                    && pszBuffer[i-9] == '0')
            {
                i -= 8;
                pszBuffer[i] = '\0';
            }
        }

    /* -------------------------------------------------------------------- */
    /*      Trim trailing zeros.                                            */
    /* -------------------------------------------------------------------- */
        while( i > 2 && pszBuffer[i-1] == '0' && pszBuffer[i-2] != '.' )
        {
            pszBuffer[--i] = '\0';
        }

    /* -------------------------------------------------------------------- */
    /*      Detect trailing 99999X's as they are likely roundoff error.     */
    /* -------------------------------------------------------------------- */
        if( !bHasTruncated &&
            i > 10 &&
            iDotPos >= 0 &&
            nPrecision >= 15)
        {
            if (/*pszBuffer[i-1] == '9' && */
                 pszBuffer[i-2] == '9'
                && pszBuffer[i-3] == '9'
                && pszBuffer[i-4] == '9'
                && pszBuffer[i-5] == '9'
                && pszBuffer[i-6] == '9' )
            {
                snprintf(pszBuffer, nBufferLen, "%.9f", dfVal);
                bHasTruncated = TRUE;
                continue;
            }
            else if (i - 9 > iDotPos && /*pszBuffer[i-1] == '9' && */
                     /*pszBuffer[i-2] == '9' && */
                    (nCountBeforeDot >= 4 || pszBuffer[i-3] == '9')
                    && (nCountBeforeDot >= 5 || pszBuffer[i-4] == '9')
                    && (nCountBeforeDot >= 6 || pszBuffer[i-5] == '9')
                    && (nCountBeforeDot >= 7 || pszBuffer[i-6] == '9')
                    && (nCountBeforeDot >= 8 || pszBuffer[i-7] == '9')
                    && pszBuffer[i-8] == '9'
                    && pszBuffer[i-9] == '9')
            {
                sprintf(szFormat, "%%.%df", MIN(5,12 - nCountBeforeDot));
                snprintf(pszBuffer, nBufferLen, szFormat, dfVal);
                bHasTruncated = TRUE;
                continue;
            }
        }

        break;
    }

    return strlen(pszBuffer);
}
/* End: GDAL addition */

static int json_object_double_to_json_string(struct json_object* jso,
					     struct printbuf *pb)
{
   /* GDAL modified */
  char szBuffer[75];
  int ret = json_OGRFormatDouble( szBuffer, sizeof(szBuffer), jso->o.c_double, '.',
                                  (jso->_precision < 0) ? 15 : jso->_precision );
  if (ret < 0)
    return ret;
  return printbuf_memappend(pb, szBuffer, ret);
}

struct json_object* json_object_new_double(double d)
{
  struct json_object *jso = json_object_new(json_type_double);
  if(!jso) return NULL;
  jso->_to_json_string = &json_object_double_to_json_string;
  jso->o.c_double = d;
  jso->_precision = -1; /* GDAL addition */
  return jso;
}

/* Begin: GDAL addition */
struct json_object* json_object_new_double_with_precision(double d, int nPrecision)
{
  struct json_object *jso = json_object_new(json_type_double);
  if(!jso) return NULL;
  jso->_to_json_string = &json_object_double_to_json_string;
  jso->o.c_double = d;
  jso->_precision = (nPrecision < 32) ? nPrecision : 32;
  return jso;
}
/* End: GDAL addition */

double json_object_get_double(struct json_object *jso)
{
  if(!jso) return 0.0;
  switch(jso->o_type) {
  case json_type_double:
    return jso->o.c_double;
  case json_type_int:
    return jso->o.c_int;
  case json_type_boolean:
    return jso->o.c_boolean;
  case json_type_string:
    return CPLAtof(jso->o.c_string);
  default:
    return 0.0;
  }
}


/* json_object_string */

static int json_object_string_to_json_string(struct json_object* jso,
					     struct printbuf *pb)
{
  sprintbuf(pb, "\"");
  json_escape_str(pb, jso->o.c_string);
  sprintbuf(pb, "\"");
  return 0;
}

static void json_object_string_delete(struct json_object* jso)
{
  free(jso->o.c_string);
  json_object_generic_delete(jso);
}

struct json_object* json_object_new_string(const char *s)
{
  struct json_object *jso = json_object_new(json_type_string);
  if(!jso) return NULL;
  jso->_delete = &json_object_string_delete;
  jso->_to_json_string = &json_object_string_to_json_string;
  jso->o.c_string = strdup(s);
  return jso;
}

struct json_object* json_object_new_string_len(const char *s, int len)
{
  struct json_object *jso = json_object_new(json_type_string);
  if(!jso) return NULL;
  jso->_delete = &json_object_string_delete;
  jso->_to_json_string = &json_object_string_to_json_string;
  jso->o.c_string = strndup(s, len);
  return jso;
}

const char* json_object_get_string(struct json_object *jso)
{
  if(!jso) return NULL;
  switch(jso->o_type) {
  case json_type_string:
    return jso->o.c_string;
  default:
    return json_object_to_json_string(jso);
  }
}


/* json_object_array */

static int json_object_array_to_json_string(struct json_object* jso,
					    struct printbuf *pb)
{
  int i;
  sprintbuf(pb, "[");
  for(i=0; i < json_object_array_length(jso); i++) {
	  struct json_object *val;
	  if(i) { sprintbuf(pb, ", "); }
	  else { sprintbuf(pb, " "); }

      val = json_object_array_get_idx(jso, i);
	  if(val == NULL) { sprintbuf(pb, "null"); }
	  else { val->_to_json_string(val, pb); }
  }
  return sprintbuf(pb, " ]");
}

static void json_object_array_entry_free(void *data)
{
  json_object_put((struct json_object*)data);
}

static void json_object_array_delete(struct json_object* jso)
{
  array_list_free(jso->o.c_array);
  json_object_generic_delete(jso);
}

struct json_object* json_object_new_array(void)
{
  struct json_object *jso = json_object_new(json_type_array);
  if(!jso) return NULL;
  jso->_delete = &json_object_array_delete;
  jso->_to_json_string = &json_object_array_to_json_string;
  jso->o.c_array = array_list_new(&json_object_array_entry_free);
  return jso;
}

struct array_list* json_object_get_array(struct json_object *jso)
{
  if(!jso) return NULL;
  switch(jso->o_type) {
  case json_type_array:
    return jso->o.c_array;
  default:
    return NULL;
  }
}

int json_object_array_length(struct json_object *jso)
{
  return array_list_length(jso->o.c_array);
}

int json_object_array_add(struct json_object *jso,struct json_object *val)
{
  return array_list_add(jso->o.c_array, val);
}

int json_object_array_put_idx(struct json_object *jso, int idx,
			      struct json_object *val)
{
  return array_list_put_idx(jso->o.c_array, idx, val);
}

struct json_object* json_object_array_get_idx(struct json_object *jso,
					      int idx)
{
  return (struct json_object*)array_list_get_idx(jso->o.c_array, idx);
}

