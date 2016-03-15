/**********************************************************************
 * $Id: avc_mbyte.h,v 1.4 2008/07/23 20:51:38 dmorissette Exp $
 *
 * Name:     avc.h
 * Project:  Arc/Info Vector coverage (AVC) BIN<->E00 conversion library
 * Language: ANSI C
 * Purpose:  Header file containing all definitions for the library.
 * Author:   Daniel Morissette, dmorissette@dmsolutions.ca
 *
 **********************************************************************
 * Copyright (c) 1999-2005, Daniel Morissette
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************
 *
 * $Log: avc_mbyte.h,v $
 * Revision 1.4  2008/07/23 20:51:38  dmorissette
 * Fixed GCC 4.1.x compile warnings related to use of char vs unsigned char
 * (GDAL/OGR ticket http://trac.osgeo.org/gdal/ticket/2495)
 *
 * Revision 1.3  2005/06/03 03:49:59  daniel
 * Update email address, website url, and copyright dates
 *
 * Revision 1.2  2000/09/22 19:45:21  daniel
 * Switch to MIT-style license
 *
 * Revision 1.1  2000/05/29 15:31:03  daniel
 * Initial revision - Japanese support
 *
 **********************************************************************/

#ifndef AVC_MBYTE_H_INCLUDED_
#define AVC_MBYTE_H_INCLUDED_

CPL_C_START

/*---------------------------------------------------------------------
 * Supported multibyte codepage numbers
 *--------------------------------------------------------------------*/
#define AVC_DBCS_JAPANESE       932

#define AVC_CODE_UNKNOWN        0

/*---------------------------------------------------------------------
 * Definitions for Japanese encodings  (AVC_DBCS_JAPANESE)
 *--------------------------------------------------------------------*/
#define AVC_CODE_JAP_UNKNOWN    0
#define AVC_CODE_JAP_SHIFTJIS   1
#define AVC_CODE_JAP_EUC        2


/*---------------------------------------------------------------------
 * We use the following structure to keep track of DBCS info.
 *--------------------------------------------------------------------*/
typedef struct AVCDBCSInfo_t
{
    int         nDBCSCodePage;
    int         nDBCSEncoding;
    unsigned char *pszDBCSBuf;
    int         nDBCSBufSize;
} AVCDBCSInfo;

/*---------------------------------------------------------------------
 * Functions prototypes
 *--------------------------------------------------------------------*/

AVCDBCSInfo *AVCAllocDBCSInfo(void);
void AVCFreeDBCSInfo(AVCDBCSInfo *psInfo);
int AVCGetDBCSCodePage(void);
GBool AVCE00DetectEncoding(AVCDBCSInfo *psDBCSInfo, const GByte *pszLine);
const GByte *AVCE00Convert2ArcDBCS(AVCDBCSInfo *psDBCSInfo,
                                   const GByte *pszLine,
                                   int nMaxOutputLen);
const GByte *AVCE00ConvertFromArcDBCS(AVCDBCSInfo *psDBCSInfo,
                                      const GByte *pszLine,
                                      int nMaxOutputLen);

CPL_C_END

#endif /* AVC_MBYTE_H_INCLUDED_ */
