/*
 * $Id: printbuf.c,v 1.5 2006/01/26 02:16:28 mclark Exp $
 *
 * Copyright (c) 2004, 2005 Metaparadigm Pte. Ltd.
 * Michael Clark <michael@metaparadigm.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See COPYING for details.
 *
 *
 * Copyright (c) 2008-2009 Yahoo! Inc.  All rights reserved.
 * The copyrights to the contents of this file are licensed under the MIT License
 * (http://www.opensource.org/licenses/mit-license.php)
 */

#include "config.h"

#if defined(__GNUC__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1 /* seems to be required to bring in vasprintf */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpl_string.h" 

#if HAVE_STDARG_H
# include <stdarg.h>
#else /* !HAVE_STDARG_H */
# error Not enough var arg support!
#endif /* HAVE_STDARG_H */

#include "bits.h"
#include "debug.h"
#include "printbuf.h"

struct printbuf* printbuf_new(void)
{
  struct printbuf *p;

  p = (struct printbuf*)calloc(1, sizeof(struct printbuf));
  if(!p) return NULL;
  p->size = 32;
  p->bpos = 0;
  if((p->buf = (char*)malloc(p->size)) == NULL) {
    free(p);
    return NULL;
  }
  return p;
}


int printbuf_memappend(struct printbuf *p, const char *buf, int size)
{
  char *t;
  if(p->size - p->bpos <= size) {
    int new_size = json_max(p->size * 2, p->bpos + size + 8);
#ifdef PRINTBUF_DEBUG
    MC_DEBUG("printbuf_memappend: realloc "
	     "bpos=%d wrsize=%d old_size=%d new_size=%d\n",
	     p->bpos, size, p->size, new_size);
#endif /* PRINTBUF_DEBUG */
    if((t = (char*)realloc(p->buf, new_size)) == NULL) return -1;
    p->size = new_size;
    p->buf = t;
  }
  memcpy(p->buf + p->bpos, buf, size);
  p->bpos += size;
  p->buf[p->bpos]= '\0';
  return size;
}

/* Use CPLVASPrintf for portability issues */
int sprintbuf(struct printbuf *p, const char *msg, ...)
{
  va_list ap;
  char *t;
  int size, ret; 

  /* user stack buffer first */
  va_start(ap, msg);
  if((size = CPLVASPrintf(&t, msg, ap)) == -1) return -1; 
  va_end(ap);
  
  if (strcmp(msg, "%f") == 0)
  {
      char* pszComma = strchr(t, ',');
      if (pszComma)
          *pszComma = '.';
  }
  
  ret = printbuf_memappend(p, t, size);
  CPLFree(t);
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
