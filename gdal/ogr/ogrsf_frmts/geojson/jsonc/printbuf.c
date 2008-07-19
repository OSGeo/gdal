/*
 * $Id: printbuf.c,v 1.5 2006/01/26 02:16:28 mclark Exp $
 *
 * Copyright (c) 2004, 2005 Metaparadigm Pte. Ltd.
 * Michael Clark <michael@metaparadigm.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See COPYING for details.
 *
 */

/* json-c private config. */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_STDARG_H
# include <stdarg.h>
#else /* !HAVE_STDARG_H */
# error Not enough var arg support!
#endif /* HAVE_STDARG_H */

#include "bits.h"
#include "debug.h"
#include "printbuf.h"

#include <cpl_port.h> /* MIN and MAX macros */
#include <cpl_string.h>

struct printbuf* printbuf_new()
{
  struct printbuf *p;

  if((p = calloc(1, sizeof(struct printbuf))) == NULL) return NULL;
  p->size = 32;
  p->bpos = 0;
  if((p->buf = malloc(p->size)) == NULL) {
    free(p);
    return NULL;
  }
  return p;
}


int printbuf_memappend(struct printbuf *p, char *buf, int size)
{
  char *t;
  if(p->size - p->bpos <= size) {
    int new_size = MAX(p->size * 2, p->bpos + size + 8);
#ifdef PRINTBUF_DEBUG
    mc_debug("printbuf_memappend: realloc "
	     "bpos=%d wrsize=%d old_size=%d new_size=%d\n",
	     p->bpos, size, p->size, new_size);
#endif /* PRINTBUF_DEBUG */
    if((t = realloc(p->buf, new_size)) == NULL) return -1;
    p->size = new_size;
    p->buf = t;
  }
  memcpy(p->buf + p->bpos, buf, size);
  p->bpos += size;
  p->buf[p->bpos]= '\0';
  return size;
}

int sprintbuf(struct printbuf *p, const char *msg, ...)
{
  va_list ap;
  char *t;
  int size, ret;

  va_start(ap, msg);
  if((size = CPLVASPrintf(&t, msg, ap)) == -1) return -1;
  va_end(ap);
  
  ret = printbuf_memappend(p, t, size);
  free(t);
  return ret;
}

void printbuf_reset(struct printbuf *p)
{
  p->buf[0] = '\0';
  p->bpos = 0;
}

void printbuf_free(struct printbuf *p)
{
  if(p) {
    free(p->buf);
    free(p);
  }
}
