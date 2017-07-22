/******************************************************************************
 *
 * Project:  JPEG-2000
 * Purpose:  Return a stream for a VSIL file
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************/

/* Following code is mostly derived from jas_stream.c, which is licensed */
/* under the below terms */

/*
 * Copyright (c) 1999-2000 Image Power, Inc. and the University of
 *   British Columbia.
 * Copyright (c) 2001-2003 Michael David Adams.
 * All rights reserved.
 * Copyright (c) 2009-2010, Even Rouault <even dot rouault at mines-paris dot org>
 */

/* __START_OF_JASPER_LICENSE__
 *
 * JasPer License Version 2.0
 *
 * Copyright (c) 2001-2006 Michael David Adams
 * Copyright (c) 1999-2000 Image Power, Inc.
 * Copyright (c) 1999-2000 The University of British Columbia
 *
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person (the
 * "User") obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * 1.  The above copyright notices and this permission notice (which
 * includes the disclaimer below) shall be included in all copies or
 * substantial portions of the Software.
 *
 * 2.  The name of a copyright holder shall not be used to endorse or
 * promote products derived from the Software without specific prior
 * written permission.
 *
 * THIS DISCLAIMER OF WARRANTY CONSTITUTES AN ESSENTIAL PART OF THIS
 * LICENSE.  NO USE OF THE SOFTWARE IS AUTHORIZED HEREUNDER EXCEPT UNDER
 * THIS DISCLAIMER.  THE SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS
 * "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL
 * INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  NO ASSURANCES ARE
 * PROVIDED BY THE COPYRIGHT HOLDERS THAT THE SOFTWARE DOES NOT INFRINGE
 * THE PATENT OR OTHER INTELLECTUAL PROPERTY RIGHTS OF ANY OTHER ENTITY.
 * EACH COPYRIGHT HOLDER DISCLAIMS ANY LIABILITY TO THE USER FOR CLAIMS
 * BROUGHT BY ANY OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL
 * PROPERTY RIGHTS OR OTHERWISE.  AS A CONDITION TO EXERCISING THE RIGHTS
 * GRANTED HEREUNDER, EACH USER HEREBY ASSUMES SOLE RESPONSIBILITY TO SECURE
 * ANY OTHER INTELLECTUAL PROPERTY RIGHTS NEEDED, IF ANY.  THE SOFTWARE
 * IS NOT FAULT-TOLERANT AND IS NOT INTENDED FOR USE IN MISSION-CRITICAL
 * SYSTEMS, SUCH AS THOSE USED IN THE OPERATION OF NUCLEAR FACILITIES,
 * AIRCRAFT NAVIGATION OR COMMUNICATION SYSTEMS, AIR TRAFFIC CONTROL
 * SYSTEMS, DIRECT LIFE SUPPORT MACHINES, OR WEAPONS SYSTEMS, IN WHICH
 * THE FAILURE OF THE SOFTWARE OR SYSTEM COULD LEAD DIRECTLY TO DEATH,
 * PERSONAL INJURY, OR SEVERE PHYSICAL OR ENVIRONMENTAL DAMAGE ("HIGH
 * RISK ACTIVITIES").  THE COPYRIGHT HOLDERS SPECIFICALLY DISCLAIM ANY
 * EXPRESS OR IMPLIED WARRANTY OF FITNESS FOR HIGH RISK ACTIVITIES.
 *
 * __END_OF_JASPER_LICENSE__
 */

#ifndef __STDC_LIMIT_MACROS
// Needed on RHEL 6 for SIZE_MAX availability, needed by Jasper
#define __STDC_LIMIT_MACROS 1
#endif

#include "jpeg2000_vsil_io.h"
#include "cpl_vsi.h"

CPL_CVSID("$Id$")

/*
 * File descriptor file object.
 */
typedef struct {
    VSILFILE* fp;
} jas_stream_VSIFL_t;

/******************************************************************************\
* File stream object.
\******************************************************************************/

static int JPEG2000_VSIL_read(jas_stream_obj_t *obj, char *buf, int cnt)
{
    jas_stream_VSIFL_t *fileobj = JAS_CAST(jas_stream_VSIFL_t *, obj);
    return static_cast<int>(VSIFReadL(buf, 1, cnt, fileobj->fp));
}

static int JPEG2000_VSIL_write(jas_stream_obj_t *obj, char *buf, int cnt)
{
    jas_stream_VSIFL_t *fileobj = JAS_CAST(jas_stream_VSIFL_t *, obj);
    return static_cast<int>(VSIFWriteL(buf, 1, cnt, fileobj->fp));
}

static long JPEG2000_VSIL_seek(jas_stream_obj_t *obj, long offset, int origin)
{
    jas_stream_VSIFL_t *fileobj = JAS_CAST(jas_stream_VSIFL_t *, obj);
    if (offset < 0 && origin == SEEK_CUR)
    {
        origin = SEEK_SET;
        offset += VSIFTellL(fileobj->fp);
    }
    else if (offset < 0 && origin == SEEK_END)
    {
        origin = SEEK_SET;
        VSIFSeekL(fileobj->fp, 0, SEEK_END);
        offset += VSIFTellL(fileobj->fp);
    }
    VSIFSeekL(fileobj->fp, offset, origin);
    return VSIFTellL(fileobj->fp);
}

static int JPEG2000_VSIL_close(jas_stream_obj_t *obj)
{
    jas_stream_VSIFL_t *fileobj = JAS_CAST(jas_stream_VSIFL_t *, obj);
    if( fileobj->fp != NULL )
    {
        VSIFCloseL(fileobj->fp);
        fileobj->fp = NULL;
    }
    jas_free(fileobj);
    return 0;
}

static const jas_stream_ops_t JPEG2000_VSIL_stream_fileops = {
    JPEG2000_VSIL_read,
    JPEG2000_VSIL_write,
    JPEG2000_VSIL_seek,
    JPEG2000_VSIL_close
};

/******************************************************************************\
* Code for opening and closing streams.
\******************************************************************************/

static jas_stream_t *JPEG2000_VSIL_jas_stream_create()
{
    jas_stream_t *stream;

    if (!(stream = (jas_stream_t*) jas_malloc(sizeof(jas_stream_t)))) {
        return NULL;
    }
    stream->openmode_ = 0;
    stream->bufmode_ = 0;
    stream->flags_ = 0;
    stream->bufbase_ = NULL;
    stream->bufstart_ = NULL;
    stream->bufsize_ = 0;
    stream->ptr_ = NULL;
    stream->cnt_ = 0;
    stream->ops_ = NULL;
    stream->obj_ = NULL;
    stream->rwcnt_ = 0;
    stream->rwlimit_ = -1;

    return stream;
}

static void JPEG2000_VSIL_jas_stream_destroy(jas_stream_t *stream)
{
    /* If the memory for the buffer was allocated with malloc, free
       this memory. */
    if ((stream->bufmode_ & JAS_STREAM_FREEBUF) && stream->bufbase_) {
        jas_free(stream->bufbase_);
        stream->bufbase_ = NULL;
    }
    jas_free(stream);
}

/******************************************************************************\
* Buffer initialization code.
\******************************************************************************/

static void JPEG2000_VSIL_jas_stream_initbuf(jas_stream_t *stream, int bufmode, char *buf,
  int bufsize)
{
    /* If this function is being called, the buffer should not have been
       initialized yet. */
    assert(!stream->bufbase_);

    if (bufmode != JAS_STREAM_UNBUF) {
        /* The full- or line-buffered mode is being employed. */
        if (!buf) {
            /* The caller has not specified a buffer to employ, so allocate
               one. */
            if ((stream->bufbase_ = (unsigned char*)jas_malloc(JAS_STREAM_BUFSIZE +
                                                               JAS_STREAM_MAXPUTBACK))) {
                stream->bufmode_ |= JAS_STREAM_FREEBUF;
                stream->bufsize_ = JAS_STREAM_BUFSIZE;
            } else {
                /* The buffer allocation has failed.  Resort to unbuffered
                   operation. */
                stream->bufbase_ = stream->tinybuf_;
                stream->bufsize_ = 1;
            }
        } else {
            /* The caller has specified a buffer to employ. */
            /* The buffer must be large enough to accommodate maximum
               putback. */
            assert(bufsize > JAS_STREAM_MAXPUTBACK);
            stream->bufbase_ = JAS_CAST(unsigned char *, buf);
            stream->bufsize_ = bufsize - JAS_STREAM_MAXPUTBACK;
        }
    } else {
        /* The unbuffered mode is being employed. */
        /* A buffer should not have been supplied by the caller. */
        assert(!buf);
        /* Use a trivial one-character buffer. */
        stream->bufbase_ = stream->tinybuf_;
        stream->bufsize_ = 1;
    }
    stream->bufstart_ = &stream->bufbase_[JAS_STREAM_MAXPUTBACK];
    stream->ptr_ = stream->bufstart_;
    stream->cnt_ = 0;
    stream->bufmode_ |= bufmode & JAS_STREAM_BUFMODEMASK;
}

static int JPEG2000_VSIL_jas_strtoopenmode(const char *s)
{
    int openmode = 0;
    while (*s != '\0') {
        switch (*s) {
          case 'r':
              openmode |= JAS_STREAM_READ;
              break;
          case 'w':
              openmode |= JAS_STREAM_WRITE | JAS_STREAM_CREATE;
              break;
          case 'b':
              openmode |= JAS_STREAM_BINARY;
              break;
          case 'a':
              openmode |= JAS_STREAM_APPEND;
              break;
          case '+':
              openmode |= JAS_STREAM_READ | JAS_STREAM_WRITE;
              break;
          default:
              break;
        }
        ++s;
    }
    return openmode;
}

jas_stream_t *JPEG2000_VSIL_fopen(const char *filename, const char *mode)
{
    jas_stream_t *stream;
    jas_stream_VSIFL_t *obj;

    /* Allocate a stream object. */
    if (!(stream = JPEG2000_VSIL_jas_stream_create())) {
        return NULL;
    }

    /* Parse the mode string. */
    stream->openmode_ = JPEG2000_VSIL_jas_strtoopenmode(mode);

    /* Allocate space for the underlying file stream object. */
    if (!(obj = (jas_stream_VSIFL_t*) jas_malloc(sizeof(jas_stream_VSIFL_t)))) {
        JPEG2000_VSIL_jas_stream_destroy(stream);
        return NULL;
    }
    obj->fp = NULL;
    stream->obj_ = (void *) obj;

    /* Select the operations for a file stream object. */
    stream->ops_ = const_cast<jas_stream_ops_t*> (&JPEG2000_VSIL_stream_fileops);

    /* Open the underlying file. */
    if ((obj->fp = VSIFOpenL(filename, mode)) == NULL) {
        jas_stream_close(stream);
        return NULL;
    }

    /* By default, use full buffering for this type of stream. */
    JPEG2000_VSIL_jas_stream_initbuf(stream, JAS_STREAM_FULLBUF, NULL, 0);

    return stream;
}
