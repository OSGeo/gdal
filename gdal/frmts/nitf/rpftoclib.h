/******************************************************************************
 * $Id$
 *
 * Project:  RPF A.TOC read Library
 * Purpose:  Main GDAL independent include file for RPF TOC support.
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 **********************************************************************
 * Copyright (c) 2007-2010, Even Rouault <even dot rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef RPFTOCLIB_H_INCLUDED
#define RPFTOCLIB_H_INCLUDED

#include "cpl_error.h"
#include "cpl_port.h"

#include "nitflib.h"

CPL_C_START

typedef struct
{
  int              exists;
  int              fileExists;
  unsigned short   frameRow;
  unsigned short   frameCol;
  char            *directory;
  char             filename[12+1];
  char             georef[6+1];
  char            *fullFilePath;
} RPFTocFrameEntry;

typedef struct
{
    char          type[5+1];
    char          compression[5+1];
    char          scale[12+1];
    char          zone[1+1];
    char          producer[5+1];
    double        nwLat;
    double        nwLong;
    double        swLat;
    double        swLong;
    double        neLat;
    double        neLong;
    double        seLat;
    double        seLong;
    double        vertResolution;
    double        horizResolution;
    double        vertInterval;
    double        horizInterval;
    unsigned int  nVertFrames;
    unsigned int  nHorizFrames;

    int           boundaryId;
    int           isOverviewOrLegend;

    const char*   seriesAbbreviation;  /* (may be NULL) eg "GNC" */
    const char*   seriesName;          /* (may be NULL) eg "Global Navigation Chart" */

    RPFTocFrameEntry* frameEntries;
} RPFTocEntry;

typedef struct
{
    int            nEntries;
    RPFTocEntry  *entries;
} RPFToc;

/* -------------------------------------------------------------------- */
/*      TOC file API                                                    */
/* -------------------------------------------------------------------- */

/** Get the TOC information from a NITF TOC file */
RPFToc     CPL_DLL *RPFTOCRead(const char* pszFilename, NITFFile* psFile);

/** Get the TOC information from a NITF TOC file or a non NITF TOC file */
RPFToc     CPL_DLL *RPFTOCReadFromBuffer(const char* pszFilename, VSILFILE* fp, const char* tocHeader);

void       CPL_DLL  RPFTOCFree(RPFToc*  nitfToc);

CPL_C_END

#endif /* ndef RPFTOCLIB_H_INCLUDED */
