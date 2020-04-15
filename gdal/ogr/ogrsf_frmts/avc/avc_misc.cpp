/**********************************************************************
 * $Id$
 *
 * Name:     avc_misc.c
 * Project:  Arc/Info vector coverage (AVC)  BIN<->E00 conversion library
 * Language: ANSI C
 * Purpose:  Misc. functions used by several parts of the library
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
 * $Log: avc_misc.c,v $
 * Revision 1.9  2005/06/03 03:49:59  daniel
 * Update email address, website url, and copyright dates
 *
 * Revision 1.8  2004/08/31 21:00:20  warmerda
 * Applied Carl Anderson's patch to reduce the amount of stating while
 * trying to discover filename "case" on Unix in AVCAdjustCaseSensitiveFilename.
 * http://bugzilla.remotesensing.org/show_bug.cgi?id=314
 *
 * Revision 1.7  2001/11/25 21:38:01  daniel
 * Remap '\\' to '/' in AVCAdjustCaseSensitiveFilename() on Unix.
 *
 * Revision 1.6  2001/11/25 21:15:23  daniel
 * Added hack (AVC_MAP_TYPE40_TO_DOUBLE) to map type 40 fields bigger than 8
 * digits to double precision as we generate E00 output (bug599)
 *
 * Revision 1.5  2000/09/26 20:21:04  daniel
 * Added AVCCoverPC write
 *
 * Revision 1.4  2000/09/22 19:45:21  daniel
 * Switch to MIT-style license
 *
 * Revision 1.3  2000/01/10 02:53:21  daniel
 * Added AVCAdjustCaseSensitiveFilename() and AVCFileExists()
 *
 * Revision 1.2  1999/08/23 18:24:27  daniel
 * Fixed support for attribute fields of type 40
 *
 * Revision 1.1  1999/05/11 02:34:46  daniel
 * Initial revision
 *
 **********************************************************************/

#include "avc.h"


/**********************************************************************
 *                          AVCE00ComputeRecSize()
 *
 * Computes the number of chars required to generate a E00 attribute
 * table record.
 *
 * Returns -1 on error, i.e. if it encounters an unsupported field type.
 **********************************************************************/
int _AVCE00ComputeRecSize(int numFields, AVCFieldInfo *pasDef,
                          GBool bMapType40ToDouble)
{
    int i, nType, nBufSize=0;

    /*-------------------------------------------------------------
     * Add up the nbr of chars used by each field
     *------------------------------------------------------------*/
    for(i=0; i < numFields; i++)
    {
        nType = pasDef[i].nType1*10;
        if (nType ==  AVC_FT_DATE || nType == AVC_FT_CHAR ||
            nType == AVC_FT_FIXINT )
        {
            nBufSize += pasDef[i].nSize;
        }
        else if (nType == AVC_FT_BININT && pasDef[i].nSize == 4)
            nBufSize += 11;
        else if (nType == AVC_FT_BININT && pasDef[i].nSize == 2)
            nBufSize += 6;
        else if (bMapType40ToDouble &&
                 nType == AVC_FT_FIXNUM && pasDef[i].nSize > 8)
        {
            /* See explanation in AVCE00GenTableHdr() about this hack to remap
             * type 40 fields to double precision floats.
             */
            nBufSize += 24;  /* Remap to double float */
        }
        else if ((nType == AVC_FT_BINFLOAT && pasDef[i].nSize == 4) ||
                  nType == AVC_FT_FIXNUM )
            nBufSize += 14;
        else if (nType == AVC_FT_BINFLOAT && pasDef[i].nSize == 8)
            nBufSize += 24;
        else
        {
            /*-----------------------------------------------------
             * Hummm... unsupported field type...
             *----------------------------------------------------*/
            CPLError(CE_Failure, CPLE_NotSupported,
                     "_AVCE00ComputeRecSize(): Unsupported field type: "
                     "(type=%d, size=%d)",
                     nType, pasDef[i].nSize);
            return -1;
        }
    }

    return nBufSize;
}



/**********************************************************************
 *                          _AVCDestroyTableFields()
 *
 * Release all memory associated with an array of AVCField structures.
 **********************************************************************/
void _AVCDestroyTableFields(AVCTableDef *psTableDef, AVCField *pasFields)
{
    int     i, nFieldType;

    if (pasFields)
    {
        for(i=0; i<psTableDef->numFields; i++)
        {
            nFieldType = psTableDef->pasFieldDef[i].nType1*10;
            if (nFieldType == AVC_FT_DATE   ||
                nFieldType == AVC_FT_CHAR   ||
                nFieldType == AVC_FT_FIXINT ||
                nFieldType == AVC_FT_FIXNUM)
            {
                CPLFree(pasFields[i].pszStr);
            }
        }
        CPLFree(pasFields);
    }

}

/**********************************************************************
 *                          _AVCDestroyTableDef()
 *
 * Release all memory associated with a AVCTableDef structure.
 *
 **********************************************************************/
void _AVCDestroyTableDef(AVCTableDef *psTableDef)
{
    if (psTableDef)
    {
        CPLFree(psTableDef->pasFieldDef);
        CPLFree(psTableDef);
    }
}


/**********************************************************************
 *                          _AVCDupTableDef()
 *
 * Create a new copy of a AVCTableDef structure.
 **********************************************************************/
AVCTableDef *_AVCDupTableDef(AVCTableDef *psSrcDef)
{
    AVCTableDef *psNewDef;

    if (psSrcDef == nullptr)
        return nullptr;

    psNewDef = (AVCTableDef*)CPLMalloc(1*sizeof(AVCTableDef));

    memcpy(psNewDef, psSrcDef, sizeof(AVCTableDef));

    psNewDef->pasFieldDef = (AVCFieldInfo*)CPLMalloc(psSrcDef->numFields*
                                                     sizeof(AVCFieldInfo));

    memcpy(psNewDef->pasFieldDef, psSrcDef->pasFieldDef,
           psSrcDef->numFields*sizeof(AVCFieldInfo));

   return psNewDef;
}


/**********************************************************************
 *                          AVCFileExists()
 *
 * Returns TRUE if a file with the specified name exists in the
 * specified directory.
 *
 * For now I simply try to fopen() the file ... would it be more
 * efficient to use stat() ???
 **********************************************************************/
GBool AVCFileExists(const char *pszPath, const char *pszName)
{
    char        *pszBuf;
    GBool       bFileExists = FALSE;
    VSILFILE    *fp;

    pszBuf = (char*)CPLMalloc(strlen(pszPath)+strlen(pszName)+1);
    snprintf(pszBuf,
             strlen(pszPath)+strlen(pszName)+1, "%s%s", pszPath, pszName);

    AVCAdjustCaseSensitiveFilename(pszBuf);

    if ((fp = VSIFOpenL(pszBuf, "rb")) != nullptr)
    {
        bFileExists = TRUE;
        VSIFCloseL(fp);
    }

    CPLFree(pszBuf);

    return bFileExists;
}

/**********************************************************************
 *                     AVCAdjustCaseSensitiveFilename()
 *
 * Scan a filename and its path, adjust uppercase/lowercases if
 * necessary, and return a reference to that filename.
 *
 * This function works on the original buffer and returns a reference to it.
 *
 * NFW: It seems like this could be made somewhat more efficient by
 * getting a directory listing and doing a case insensitive search in
 * that list rather than all this stating that can be very expensive
 * in some circumstances.  However, at least with Carl's fix this is
 * somewhat faster.
 * see: http://bugzilla.remotesensing.org/show_bug.cgi?id=314
 **********************************************************************/
char *AVCAdjustCaseSensitiveFilename(char *pszFname)
{
    VSIStatBufL  sStatBuf;
    char        *pszTmpPath = nullptr;
    int         nTotalLen, iTmpPtr;
    GBool       bValidPath;

    /*-----------------------------------------------------------------
     * First check if the filename is OK as is.
     *----------------------------------------------------------------*/
    if (VSIStatL(pszFname, &sStatBuf) == 0)
    {
        return pszFname;
    }

    pszTmpPath = CPLStrdup(pszFname);
    nTotalLen = (int)strlen(pszTmpPath);

    /*-----------------------------------------------------------------
     * Remap '\\' to '/'
     *----------------------------------------------------------------*/
    for (iTmpPtr=0; iTmpPtr< nTotalLen; iTmpPtr++)
    {
        if (pszTmpPath[iTmpPtr] == '\\')
            pszTmpPath[iTmpPtr] = '/';
    }

    /*-----------------------------------------------------------------
     * Try all lower case, check if the filename is OK as that.
     *----------------------------------------------------------------*/
    for (iTmpPtr=0; iTmpPtr< nTotalLen; iTmpPtr++)
    {
        if ( pszTmpPath[iTmpPtr] >= 'A' && pszTmpPath[iTmpPtr] <= 'Z' )
            pszTmpPath[iTmpPtr] += 32;
    }

    if (VSIStatL(pszTmpPath, &sStatBuf) == 0)
    {
        strcpy(pszFname, pszTmpPath);
        CPLFree(pszTmpPath);
        return pszFname;
    }

    /*-----------------------------------------------------------------
     * Try all upper case, check if the filename is OK as that.
     *----------------------------------------------------------------*/
    for (iTmpPtr=0; iTmpPtr< nTotalLen; iTmpPtr++)
    {
        if ( pszTmpPath[iTmpPtr] >= 'a' && pszTmpPath[iTmpPtr] <= 'z' )
            pszTmpPath[iTmpPtr] -= 32;
    }

    if (VSIStatL(pszTmpPath, &sStatBuf) == 0)
    {
        strcpy(pszFname, pszTmpPath);
        CPLFree(pszTmpPath);
        return pszFname;
    }

    /*-----------------------------------------------------------------
     * OK, file either does not exist or has the wrong cases... we'll
     * go backwards until we find a portion of the path that is valid.
     *----------------------------------------------------------------*/
    strcpy(pszTmpPath, pszFname);

    /*-----------------------------------------------------------------
     * Remap '\\' to '/'
     *----------------------------------------------------------------*/
    for (iTmpPtr=0; iTmpPtr< nTotalLen; iTmpPtr++)
    {
        if (pszTmpPath[iTmpPtr] == '\\')
            pszTmpPath[iTmpPtr] = '/';
    }

    bValidPath = FALSE;
    while(iTmpPtr > 0 && !bValidPath)
    {
        /*-------------------------------------------------------------
         * Move back to the previous '/' separator
         *------------------------------------------------------------*/
        pszTmpPath[--iTmpPtr] = '\0';
        while( iTmpPtr > 0 && pszTmpPath[iTmpPtr-1] != '/' )
        {
            pszTmpPath[--iTmpPtr] = '\0';
        }

        if (iTmpPtr > 0 && VSIStatL(pszTmpPath, &sStatBuf) == 0)
            bValidPath = TRUE;
    }

    CPLAssert(iTmpPtr >= 0);

    /*-----------------------------------------------------------------
     * Assume that CWD is valid... so an empty path is a valid path
     *----------------------------------------------------------------*/
    if (iTmpPtr == 0)
        bValidPath = TRUE;

    /*-----------------------------------------------------------------
     * OK, now that we have a valid base, reconstruct the whole path
     * by scanning all the sub-directories.
     * If we get to a point where a path component does not exist then
     * we simply return the rest of the path as is.
     *----------------------------------------------------------------*/
    while(bValidPath && strlen(pszTmpPath) < (size_t)nTotalLen)
    {
        char    **papszDir=VSIReadDir(pszTmpPath);
        int     iEntry, iLastPartStart;

        iLastPartStart = iTmpPtr;

        /*-------------------------------------------------------------
         * Add one component to the current path
         *------------------------------------------------------------*/
        pszTmpPath[iTmpPtr] = pszFname[iTmpPtr];
        iTmpPtr++;
        for( ; pszFname[iTmpPtr] != '\0' && pszFname[iTmpPtr]!='/'; iTmpPtr++)
        {
            pszTmpPath[iTmpPtr] = pszFname[iTmpPtr];
        }

        while(iLastPartStart < iTmpPtr && pszTmpPath[iLastPartStart] == '/')
            iLastPartStart++;

        /*-------------------------------------------------------------
         * And do a case insensitive search in the current dir...
         *------------------------------------------------------------*/
        for(iEntry=0; papszDir && papszDir[iEntry]; iEntry++)
        {
            if (EQUAL(pszTmpPath+iLastPartStart, papszDir[iEntry]))
            {
                /* Fount it! */
#ifdef CSA_BUILD
                // Silence false positive warning about overlapping buffers
                memmove(pszTmpPath+iLastPartStart, papszDir[iEntry],
                        strlen(papszDir[iEntry]) + 1);
#else
                strcpy(pszTmpPath+iLastPartStart, papszDir[iEntry]);
#endif
                break;
            }
        }

        if (iTmpPtr > 0 && VSIStatL(pszTmpPath, &sStatBuf) != 0)
            bValidPath = FALSE;

        CSLDestroy(papszDir);
    }

    /*-----------------------------------------------------------------
     * We reached the last valid path component... just copy the rest
     * of the path as is.
     *----------------------------------------------------------------*/
    if (iTmpPtr < nTotalLen-1)
    {
        strncpy(pszTmpPath+iTmpPtr, pszFname+iTmpPtr, nTotalLen-iTmpPtr);
    }

    /*-----------------------------------------------------------------
     * Update the source buffer and return.
     *----------------------------------------------------------------*/
    strcpy(pszFname, pszTmpPath);
    CPLFree(pszTmpPath);

    return pszFname;
}



/**********************************************************************
 *                          AVCPrintRealValue()
 *
 * Format a floating point value according to the specified coverage
 * precision (AVC_SINGLE/DOUBLE_PREC),  and append the formatted value
 * to the end of the pszBuf buffer.
 *
 * The function returns the number of characters added to the buffer.
 **********************************************************************/
int  AVCPrintRealValue(char *pszBuf, size_t nBufLen, int nPrecision, AVCFileType eType,
                        double dValue)
{
    static int numExpDigits=-1;
    int        nLen = 0;

    /* WIN32 systems' printf() for floating point output generates 3
     * digits exponents (ex: 1.23E+012), but E00 files must have 2 digits
     * exponents (ex: 1.23E+12).
     * Run a test (only once per prg execution) to establish the number
     * of exponent digits on the current platform.
     */
    if (numExpDigits == -1)
    {
        char szBuf[50];
        int  i;

        CPLsnprintf(szBuf, sizeof(szBuf), "%10.7E", 123.45);
        numExpDigits = 0;
        for(i=(int)strlen(szBuf)-1; i>0; i--)
        {
            if (szBuf[i] == '+' || szBuf[i] == '-')
                break;
            numExpDigits++;
        }
    }

    /* We will append the value at the end of the current buffer contents.
     */
    nBufLen -= strlen(pszBuf);
    pszBuf = pszBuf+strlen(pszBuf);

    if (dValue < 0.0)
    {
        *pszBuf = '-';
        dValue = -1.0*dValue;
    }
    else
        *pszBuf = ' ';


    /* Just to make things more complicated, double values are
     * output in a different format in attribute tables than in
     * the other files!
     */
    if (nPrecision == AVC_FORMAT_DBF_FLOAT)
    {
        /* Float stored in DBF table in PC coverages */
        CPLsnprintf(pszBuf+1, nBufLen-1, "%9.6E", dValue);
        nLen = 13;
    }
    else if (nPrecision == AVC_DOUBLE_PREC && eType == AVCFileTABLE)
    {
        CPLsnprintf(pszBuf+1, nBufLen-1,"%20.17E", dValue);
        nLen = 24;
    }
    else if (nPrecision == AVC_DOUBLE_PREC)
    {
        CPLsnprintf(pszBuf+1, nBufLen-1,"%17.14E", dValue);
        nLen = 21;
    }
    else
    {
        CPLsnprintf(pszBuf+1, nBufLen-1,"%10.7E", dValue);
        nLen = 14;
    }

    /* Adjust number of exponent digits if necessary
     */
    if (numExpDigits > 2)
    {
        int n;
        n = (int)strlen(pszBuf);

        pszBuf[n - numExpDigits]    = pszBuf[n-2];
        pszBuf[n - numExpDigits +1] = pszBuf[n-1];
        pszBuf[n - numExpDigits +2] = '\0';
    }

    /* Just make sure that the actual output length is what we expected.
     */
    CPLAssert(strlen(pszBuf) == (size_t)nLen);

    return nLen;
}
