/*
 * $Id: json_tokener.c,v 1.20 2006/07/25 03:24:50 mclark Exp $
 *
 * Copyright (c) 2004, 2005 Metaparadigm Pte. Ltd.
 * Michael Clark <michael@metaparadigm.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See COPYING for details.
 *
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bits.h"
#include "debug.h"
#include "printbuf.h"
#include "arraylist.h"
#include "json_object.h"
#include "json_tokener.h"

#include <cpl_port.h> /* MIN and MAX macros */

#if !HAVE_STRNCASECMP && defined(_MSC_VER)
  /* MSC has the version as _strnicmp */
# define strncasecmp _strnicmp
#endif /* HAVE_STRNCASECMP */


static const char* json_null_str = "null";
static const char* json_true_str = "true";
static const char* json_false_str = "false";

const char* json_tokener_errors[] = {
  "success",
  "continue",
  "nesting to deep",
  "unexpected end of data",
  "unexpected character",
  "null expected",
  "boolean expected",
  "number expected",
  "array value separator ',' expected",
  "quoted object property name expected",
  "object property name separator ':' expected",
  "object value separator ',' expected",
  "invalid string sequence",
  "expected comment",
};


struct json_tokener* json_tokener_new()
{
  struct json_tokener *tok = calloc(1, sizeof(struct json_tokener));
  tok->pb = printbuf_new();
  json_tokener_reset(tok);
  return tok;
}

void json_tokener_free(struct json_tokener *tok)
{
  json_tokener_reset(tok);
  if(tok) printbuf_free(tok->pb);
  free(tok);
}

static void json_tokener_reset_level(struct json_tokener *tok, int depth)
{
  tok->stack[depth].state = json_tokener_state_eatws;
  tok->stack[depth].saved_state = json_tokener_state_start;
  json_object_put(tok->stack[depth].current);
  tok->stack[depth].current = NULL;
  free(tok->stack[depth].obj_field_name);
  tok->stack[depth].obj_field_name = NULL;
}

void json_tokener_reset(struct json_tokener *tok)
{
  int i;
  for(i = tok->depth; i >= 0; i--)
    json_tokener_reset_level(tok, i);
  tok->depth = 0;
  tok->err = json_tokener_success;
}

struct json_object* json_tokener_parse(const char *str)
{
  struct json_tokener* tok;
  struct json_object* obj;

  tok = json_tokener_new();
  obj = json_tokener_parse_ex(tok, str, -1);
  if(tok->err != json_tokener_success)
    obj = error_ptr(-tok->err);
  json_tokener_free(tok);
  return obj;
}


#if !HAVE_STRNDUP
/* CAW: compliant version of strndup() */
char* strndup(const char* str, size_t n)
{
  if(str) {
    size_t len = strlen(str);
    size_t nn = MIN(len,n);
    char* s = (char*)malloc(sizeof(char) * (nn + 1));

    if(s) {
      memcpy(s, str, nn);
      s[nn] = '\0';
    }

    return s;
  }

  return NULL;
}
#endif


#define state  tok->stack[tok->depth].state
#define saved_state  tok->stack[tok->depth].saved_state
#define current tok->stack[tok->depth].current
#define obj_field_name tok->stack[tok->depth].obj_field_name

struct json_object* json_tokener_parse_ex(struct json_tokener *tok,
					  const char *str, int len)
{
  struct json_object *obj = NULL;
  char c;

  tok->char_offset = 0;
  tok->err = json_tokener_success;

  do {
    if(tok->char_offset == len) {
      if(tok->depth == 0 && state == json_tokener_state_eatws &&
	 saved_state == json_tokener_state_finish)
	tok->err = json_tokener_success;
      else
	tok->err = json_tokener_continue;
      goto out;
    }

    c = *str;
  redo_char:
    switch(state) {

    case json_tokener_state_eatws:
      if(isspace(c)) {
	/* okay */
      } else if(c == '/') {
	printbuf_reset(tok->pb);
	printbuf_memappend(tok->pb, &c, 1);
	state = json_tokener_state_comment_start;
      } else {
	state = saved_state;
	goto redo_char;
      }
      break;

    case json_tokener_state_start:
      switch(c) {
      case '{':
	state = json_tokener_state_eatws;
	saved_state = json_tokener_state_object_field_start;
	current = json_object_new_object();
	break;
      case '[':
	state = json_tokener_state_eatws;
	saved_state = json_tokener_state_array;
	current = json_object_new_array();
	break;
      case 'N':
      case 'n':
	state = json_tokener_state_null;
	printbuf_reset(tok->pb);
	tok->st_pos = 0;
	goto redo_char;
      case '"':
      case '\'':
	state = json_tokener_state_string;
	printbuf_reset(tok->pb);
	tok->quote_char = c;
	break;
      case 'T':
      case 't':
      case 'F':
      case 'f':
	state = json_tokener_state_boolean;
	printbuf_reset(tok->pb);
	tok->st_pos = 0;
	goto redo_char;
#if defined(__GNUC__)
	  case '0' ... '9':
#else
	  case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
#endif
      case '-':
	state = json_tokener_state_number;
	printbuf_reset(tok->pb);
	tok->is_double = 0;
	goto redo_char;
      default:
	tok->err = json_tokener_error_parse_unexpected;
	goto out;
      }
      break;

    case json_tokener_state_finish:
      if(tok->depth == 0) goto out;
      obj = json_object_get(current);
      json_tokener_reset_level(tok, tok->depth);
      tok->depth--;
      goto redo_char;

    case json_tokener_state_null:
      printbuf_memappend(tok->pb, &c, 1);
      if(strncasecmp(json_null_str, tok->pb->buf,
		     MIN(tok->st_pos+1, (int)strlen(json_null_str))) == 0) {
	if(tok->st_pos == (int)strlen(json_null_str)) {
	  current = NULL;
	  saved_state = json_tokener_state_finish;
	  state = json_tokener_state_eatws;
	  goto redo_char;
	}
      } else {
	tok->err = json_tokener_error_parse_null;
	goto out;
      }
      tok->st_pos++;
      break;

    case json_tokener_state_comment_start:
      if(c == '*') {
	state = json_tokener_state_comment;
      } else if(c == '/') {
	state = json_tokener_state_comment_eol;
      } else {
	tok->err = json_tokener_error_parse_comment;
	goto out;
      }
      printbuf_memappend(tok->pb, &c, 1);
      break;

    case json_tokener_state_comment:
      if(c == '*') state = json_tokener_state_comment_end;
      printbuf_memappend(tok->pb, &c, 1);
      break;

    case json_tokener_state_comment_eol:
      if(c == '\n') {
	mc_debug("json_tokener_comment: %s\n", tok->pb->buf);
	state = json_tokener_state_eatws;
      } else {
	printbuf_memappend(tok->pb, &c, 1);
      }
      break;

    case json_tokener_state_comment_end:
      printbuf_memappend(tok->pb, &c, 1);
      if(c == '/') {
	mc_debug("json_tokener_comment: %s\n", tok->pb->buf);
	state = json_tokener_state_eatws;
      } else {
	state = json_tokener_state_comment;
      }
      break;

    case json_tokener_state_string:
      if(c == tok->quote_char) {
	current = json_object_new_string(tok->pb->buf);
	saved_state = json_tokener_state_finish;
	state = json_tokener_state_eatws;
      } else if(c == '\\') {
	saved_state = json_tokener_state_string;
	state = json_tokener_state_string_escape;
      } else {
	printbuf_memappend(tok->pb, &c, 1);
      }
      break;

    case json_tokener_state_string_escape:
      switch(c) {
      case '"':
      case '\\':
      case '/':
	printbuf_memappend(tok->pb, &c, 1);
	state = saved_state;
	break;
      case 'b':
      case 'n':
      case 'r':
      case 't':
	if(c == 'b') printbuf_memappend(tok->pb, "\b", 1);
	else if(c == 'n') printbuf_memappend(tok->pb, "\n", 1);
	else if(c == 'r') printbuf_memappend(tok->pb, "\r", 1);
	else if(c == 't') printbuf_memappend(tok->pb, "\t", 1);
	state = saved_state;
	break;
      case 'u':
	tok->ucs_char = 0;
	tok->st_pos = 0;
	state = json_tokener_state_escape_unicode;
	break;
      default:
	tok->err = json_tokener_error_parse_string;
	goto out;
      }
      break;

    case json_tokener_state_escape_unicode:
      if(strchr(json_hex_chars, c)) {
	tok->ucs_char += ((unsigned int)hexdigit(c) << ((3-tok->st_pos++)*4));
	if(tok->st_pos == 4) {
	  unsigned char utf_out[3];
	  if (tok->ucs_char < 0x80) {
	    utf_out[0] = (unsigned char)tok->ucs_char;
	    printbuf_memappend(tok->pb, (char*)utf_out, 1);
	  } else if (tok->ucs_char < 0x800) {
	    utf_out[0] = (unsigned char)(0xc0 | (tok->ucs_char >> 6));
	    utf_out[1] = (unsigned char)(0x80 | (tok->ucs_char & 0x3f));
	    printbuf_memappend(tok->pb, (char*)utf_out, 2);
	  } else {
	    utf_out[0] = (unsigned char)(0xe0 | (tok->ucs_char >> 12));
	    utf_out[1] = (unsigned char)(0x80 | ((tok->ucs_char >> 6) & 0x3f));
	    utf_out[2] = (unsigned char)(0x80 | (tok->ucs_char & 0x3f));
	    printbuf_memappend(tok->pb, (char*)utf_out, 3);
	  }
	  state = saved_state;
	}
      } else {
	tok->err = json_tokener_error_parse_string;
	goto out;
      }
      break;

    case json_tokener_state_boolean:
      printbuf_memappend(tok->pb, &c, 1);
      if(strncasecmp(json_true_str, tok->pb->buf,
		     MIN(tok->st_pos+1, (int)strlen(json_true_str))) == 0) {
	if(tok->st_pos == (int)strlen(json_true_str)) {
	  current = json_object_new_boolean(1);
	  saved_state = json_tokener_state_finish;
	  state = json_tokener_state_eatws;
	  goto redo_char;
	}
      } else if(strncasecmp(json_false_str, tok->pb->buf,
			    MIN(tok->st_pos+1, (int)strlen(json_false_str))) == 0) {
	if(tok->st_pos == (int)strlen(json_false_str)) {
	  current = json_object_new_boolean(0);
	  saved_state = json_tokener_state_finish;
	  state = json_tokener_state_eatws;
	  goto redo_char;
	}
      } else {
	tok->err = json_tokener_error_parse_boolean;
	goto out;
      }
      tok->st_pos++;
      break;

    case json_tokener_state_number:
      if(c && strchr(json_number_chars, c)) {
	printbuf_memappend(tok->pb, &c, 1);	
	if(c == '.' || c == 'e' || c == 'E')
        tok->is_double = 1;
      } else {
	int numi;
	double numd;
	if(!tok->is_double && sscanf(tok->pb->buf, "%d", &numi) == 1) {
	  current = json_object_new_int(numi);
	} else if(tok->is_double && sscanf(tok->pb->buf, "%lf", &numd) == 1) {
	  current = json_object_new_double(numd);
	} else {
	  tok->err = json_tokener_error_parse_number;
	  goto out;
	}
	saved_state = json_tokener_state_finish;
	state = json_tokener_state_eatws;
	goto redo_char;
      }
      break;

    case json_tokener_state_array:
      if(c == ']') {
	saved_state = json_tokener_state_finish;
	state = json_tokener_state_eatws;
      } else {
	if(tok->depth >= JSON_TOKENER_MAX_DEPTH-1) {
	  tok->err = json_tokener_error_depth;
	  goto out;
	}
	state = json_tokener_state_array_add;
	tok->depth++;
	json_tokener_reset_level(tok, tok->depth);
	goto redo_char;
      }
      break;

    case json_tokener_state_array_add:
      json_object_array_add(current, obj);
      saved_state = json_tokener_state_array_sep;
      state = json_tokener_state_eatws;
      goto redo_char;

    case json_tokener_state_array_sep:
      if(c == ']') {
	saved_state = json_tokener_state_finish;
	state = json_tokener_state_eatws;
      } else if(c == ',') {
	saved_state = json_tokener_state_array;
	state = json_tokener_state_eatws;
      } else {
	tok->err = json_tokener_error_parse_array;
	goto out;
      }
      break;

    case json_tokener_state_object_field_start:
      if(c == '}') {
	saved_state = json_tokener_state_finish;
	state = json_tokener_state_eatws;
      } else if (c == '"' || c == '\'') {
	tok->quote_char = c;
	printbuf_reset(tok->pb);
	state = json_tokener_state_object_field;
      } else {
	tok->err = json_tokener_error_parse_object_key_name;
	goto out;
      }
      break;

    case json_tokener_state_object_field:
      if(c == tok->quote_char) {
	obj_field_name = strdup(tok->pb->buf);
	saved_state = json_tokener_state_object_field_end;
	state = json_tokener_state_eatws;
      } else if(c == '\\') {
	saved_state = json_tokener_state_object_field;
	state = json_tokener_state_string_escape;
      } else {
	printbuf_memappend(tok->pb, &c, 1);
      }
      break;

    case json_tokener_state_object_field_end:
      if(c == ':') {
	saved_state = json_tokener_state_object_value;
	state = json_tokener_state_eatws;
      } else {
	tok->err = json_tokener_error_parse_object_key_sep;
	goto out;
      }
      break;

    case json_tokener_state_object_value:
      if(tok->depth >= JSON_TOKENER_MAX_DEPTH-1) {
	tok->err = json_tokener_error_depth;
	goto out;
      }
      state = json_tokener_state_object_value_add;
      tok->depth++;
      json_tokener_reset_level(tok, tok->depth);
      goto redo_char;

    case json_tokener_state_object_value_add:
      json_object_object_add(current, obj_field_name, obj);
      free(obj_field_name);
      obj_field_name = NULL;
      saved_state = json_tokener_state_object_sep;
      state = json_tokener_state_eatws;
      goto redo_char;

    case json_tokener_state_object_sep:
      if(c == '}') {
	saved_state = json_tokener_state_finish;
	state = json_tokener_state_eatws;
      } else if(c == ',') {
	saved_state = json_tokener_state_object_field_start;
	state = json_tokener_state_eatws;
      } else {
	tok->err = json_tokener_error_parse_object_value_sep;
	goto out;
      }
      break;

    }
    str++;
    tok->char_offset++;
  } while(c);

  if(state != json_tokener_state_finish &&
     saved_state != json_tokener_state_finish)
    tok->err = json_tokener_error_parse_eof;

 out:
  if(tok->err == json_tokener_success) return json_object_get(current);
  mc_debug("json_tokener_parse_ex: error %s at offset %d\n",
	   json_tokener_errors[tok->err], tok->char_offset);
  return NULL;
}
