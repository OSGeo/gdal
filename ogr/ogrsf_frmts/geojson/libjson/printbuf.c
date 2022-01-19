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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpl_string.h"

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#else /* !HAVE_STDARG_H */
#error Not enough var arg support!
#endif /* HAVE_STDARG_H */

#include "debug.h"
#include "printbuf.h"

static int printbuf_extend(struct printbuf *p, int min_size);

struct printbuf *printbuf_new(void)
{
	struct printbuf *p;

	p = (struct printbuf *)calloc(1, sizeof(struct printbuf));
	if (!p)
		return NULL;
	p->size = 32;
	p->bpos = 0;
	if (!(p->buf = (char *)malloc(p->size)))
	{
		free(p);
		return NULL;
	}
	p->buf[0] = '\0';
	return p;
}

/**
 * Extend the buffer p so it has a size of at least min_size.
 *
 * If the current size is large enough, nothing is changed.
 *
 * Note: this does not check the available space!  The caller
 *  is responsible for performing those calculations.
 */
static int printbuf_extend(struct printbuf *p, int min_size)
{
	char *t;
	int new_size;

	if (p->size >= min_size)
		return 0;
	/* Prevent signed integer overflows with large buffers. */
	if (min_size > INT_MAX - 8)
		return -1;
	if (p->size > INT_MAX / 2)
		new_size = min_size + 8;
	else {
		new_size = p->size * 2;
		if (new_size < min_size + 8)
			new_size = min_size + 8;
	}
#ifdef PRINTBUF_DEBUG
	MC_DEBUG("printbuf_memappend: realloc "
	         "bpos=%d min_size=%d old_size=%d new_size=%d\n",
	         p->bpos, min_size, p->size, new_size);
#endif /* PRINTBUF_DEBUG */
	if (!(t = (char *)realloc(p->buf, new_size)))
		return -1;
	p->size = new_size;
	p->buf = t;
	return 0;
}

int printbuf_memappend(struct printbuf *p, const char *buf, int size)
{
	/* Prevent signed integer overflows with large buffers. */
	if (size > INT_MAX - p->bpos - 1)
		return -1;
	if (p->size <= p->bpos + size + 1)
	{
		if (printbuf_extend(p, p->bpos + size + 1) < 0)
			return -1;
	}
	memcpy(p->buf + p->bpos, buf, size);
	p->bpos += size;
	p->buf[p->bpos] = '\0';
	return size;
}

int printbuf_memset(struct printbuf *pb, int offset, int charvalue, int len)
{
	int size_needed;

	if (offset == -1)
		offset = pb->bpos;
	/* Prevent signed integer overflows with large buffers. */
	if (len > INT_MAX - offset)
		return -1;
	size_needed = offset + len;
	if (pb->size < size_needed)
	{
		if (printbuf_extend(pb, size_needed) < 0)
			return -1;
	}

	memset(pb->buf + offset, charvalue, len);
	if (pb->bpos < size_needed)
		pb->bpos = size_needed;

	return 0;
}

/* Use CPLVASPrintf for portability issues */
int sprintbuf(struct printbuf *p, const char *msg, ...)
{
  va_list ap;
  char *t;
  int size, ret;

  /* user stack buffer first */
  va_start(ap, msg);
  size = CPLVASPrintf(&t, msg, ap);
  va_end(ap);
  if( size == -1 )
      return -1;

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
	if (p)
	{
		free(p->buf);
		free(p);
	}
}
