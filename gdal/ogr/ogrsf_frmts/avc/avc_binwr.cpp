/* $Id$
 *
 * Name:     avc_binwr.c
 * Project:  Arc/Info vector coverage (AVC)  E00->BIN conversion library
 * Language: ANSI C
 * Purpose:  Binary files access functions (write mode).
 * Author:   Daniel Morissette, dmorissette@dmsolutions.ca
 *
 **********************************************************************
 * Copyright (c) 1999-2001, Daniel Morissette
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
 * $Log: avc_binwr.c,v $
 * Revision 1.18  2008/07/23 20:51:38  dmorissette
 * Fixed GCC 4.1.x compile warnings related to use of char vs unsigned char
 * (GDAL/OGR ticket http://trac.osgeo.org/gdal/ticket/2495)
 *
 * Revision 1.17  2006/06/14 16:31:28  daniel
 * Added support for AVCCoverPC2 type (bug 1491)
 *
 * Revision 1.16  2005/06/03 03:49:58  daniel
 * Update email address, website url, and copyright dates
 *
 * Revision 1.15  2001/07/06 05:09:33  daniel
 * Removed #ifdef around fseek to fix NT4 drive problem since ANSI-C requires
 * an fseek() between read and write operations so this applies to Unix too.
 *
 * Revision 1.14  2001/07/06 04:25:00  daniel
 * Fixed a problem writing arc.dir on NT4 networked drives in an empty dir.
 *
 * Revision 1.13  2000/10/16 16:13:29  daniel
 * Fixed sHeader.nPrecision when writing PC TXT files
 *
 * Revision 1.12  2000/09/26 21:40:18  daniel
 * Fixed writing of PC Coverage TXT... they're different from V7 TXT
 *
 * Revision 1.11  2000/09/26 20:21:04  daniel
 * Added AVCCoverPC write
 *
 * Revision 1.10  2000/09/22 19:45:20  daniel
 * Switch to MIT-style license
 *
 * Revision 1.9  2000/05/29 15:31:30  daniel
 * Added Japanese DBCS support
 *
 * Revision 1.8  2000/01/10 02:55:12  daniel
 * Added call to AVCAdjustCaseSensitiveFilename() when creating tables
 *
 * Revision 1.7  1999/12/24 07:18:34  daniel
 * Added PC Arc/Info coverages support
 *
 * Revision 1.6  1999/08/26 17:26:09  daniel
 * Removed printf() messages used in Windows tests
 *
 * Revision 1.5  1999/08/23 18:18:51  daniel
 * Fixed memory leak and some VC++ warnings
 *
 * Revision 1.4  1999/06/08 22:08:14  daniel
 * Modified CreateTable() to overwrite existing arc.dir entries if necessary
 *
 * Revision 1.3  1999/06/08 18:24:32  daniel
 * Fixed some problems with '/' vs '\\' on Windows
 *
 * Revision 1.2  1999/05/17 16:17:36  daniel
 * Added RXP + TXT/TX6/TX7 write support
 *
 * Revision 1.1  1999/05/11 02:34:46  daniel
 * Initial revision
 *
 **********************************************************************/

#include "cpl_port.h"
#include "cpl_vsi.h"
#include "avc.h"

CPL_INLINE static void CPL_IGNORE_RET_VAL_INT(CPL_UNUSED int unused) {}

#include <ctype.h>  /* tolower() */

/*=====================================================================
 * Stuff related to writing the binary coverage files
 *====================================================================*/

static void    _AVCBinWriteCloseTable(AVCBinFile *psFile);

static AVCBinFile *_AVCBinWriteCreateDBFTable(const char *pszPath,
                                              const char *pszCoverName,
                                              AVCTableDef *psSrcTableDef,
                                              AVCCoverType eCoverType,
                                              int nPrecision,
                                              AVCDBCSInfo *psDBCSInfo);

/**********************************************************************
 *                          AVCBinWriteCreate()
 *
 * Open a coverage file for writing, write a header if applicable, and
 * initialize the handle to be ready to write objects to the file.
 *
 * pszPath is the coverage (or info directory) path, terminated by
 *         a '/' or a '\\'
 * pszName is the name of the file to create relative to this directory.
 *
 * Note: For most file types except tables, passing pszPath="" and
 * including the coverage path as part of pszName instead would work.
 *
 * Returns a valid AVCBinFile handle, or nullptr if the file could
 * not be created.
 *
 * AVCBinClose() will eventually have to be called to release the
 * resources used by the AVCBinFile structure.
 **********************************************************************/
AVCBinFile *AVCBinWriteCreate(const char *pszPath, const char *pszName,
                              AVCCoverType eCoverType,
                              AVCFileType eType, int nPrecision,
                              AVCDBCSInfo *psDBCSInfo)
{
    AVCBinFile   *psFile;
    char         *pszFname = nullptr, *pszExt;
    GBool        bCreateIndex = FALSE;
    int          nLen;

    /*-----------------------------------------------------------------
     * Make sure precision value is valid (AVC_DEFAULT_PREC is NOT valid)
     *----------------------------------------------------------------*/
    if (nPrecision!=AVC_SINGLE_PREC && nPrecision!=AVC_DOUBLE_PREC)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "AVCBinWriteCreate(): Invalid precision parameter "
                 "(value must be AVC_SINGLE_PREC or AVC_DOUBLE_PREC)");
        return nullptr;
    }

    /*-----------------------------------------------------------------
     * The case of INFO tables is a bit different...
     * tables have to be created through a separate function.
     *----------------------------------------------------------------*/
    if (eType == AVCFileTABLE)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "AVCBinWriteCreate(): TABLEs must be created using "
                 "AVCBinWriteCreateTable()");
        return nullptr;
    }

    /*-----------------------------------------------------------------
     * Alloc and init the AVCBinFile struct.
     *----------------------------------------------------------------*/
    psFile = (AVCBinFile*)CPLCalloc(1, sizeof(AVCBinFile));

    psFile->eFileType = eType;
    psFile->nPrecision = nPrecision;

    psFile->pszFilename = (char*)CPLMalloc(strlen(pszPath)+strlen(pszName)+1);
    snprintf(psFile->pszFilename, strlen(pszPath)+strlen(pszName)+1, "%s%s", pszPath, pszName);

    psFile->eCoverType = eCoverType;

    /*-----------------------------------------------------------------
     * PRJ files are text files... we won't use the AVCRawBin*()
     * functions for them... the file will be created and closed
     * inside AVCBinWritePrj().
     *----------------------------------------------------------------*/
    if (eType == AVCFilePRJ)
    {
        return psFile;
    }

    /*-----------------------------------------------------------------
     * All other file types share a very similar creation method.
     *----------------------------------------------------------------*/
    psFile->psRawBinFile = AVCRawBinOpen(psFile->pszFilename, "w",
                                 AVC_COVER_BYTE_ORDER(psFile->eCoverType),
                                         psDBCSInfo);

    if (psFile->psRawBinFile == nullptr)
    {
        /* Failed to open file... just return nullptr since an error message
         * has already been issued by AVCRawBinOpen()
         */
        CPLFree(psFile->pszFilename);
        CPLFree(psFile);
        return nullptr;
    }

    /*-----------------------------------------------------------------
     * Create an Index file if applicable for current file type.
     * Yep, we'll have a problem if filenames come in as uppercase, but
     * this should not happen in a normal situation.
     * For each type there is 3 possibilities, e.g. "pal", "pal.adf", "ttt.pal"
     *----------------------------------------------------------------*/
    pszFname = CPLStrdup(psFile->pszFilename);
    nLen = (int)strlen(pszFname);
    if (eType == AVCFileARC &&
        ( (nLen>=3 && STARTS_WITH_CI((pszExt=pszFname+nLen-3), "arc")) ||
          (nLen>=7 && STARTS_WITH_CI((pszExt=pszFname+nLen-7), "arc.adf")) ) )
    {
        memcpy(pszExt, "arx", 3);
        bCreateIndex = TRUE;
    }
    else if ((eType == AVCFilePAL || eType == AVCFileRPL) &&
             ( (nLen>=3 && STARTS_WITH_CI((pszExt=pszFname+nLen-3), "pal")) ||
               (nLen>=7 && STARTS_WITH_CI((pszExt=pszFname+nLen-7), "pal.adf")) ) )
    {
        memcpy(pszExt, "pax", 3);
        bCreateIndex = TRUE;
    }
    else if (eType == AVCFileCNT &&
             ( (nLen>=3 && STARTS_WITH_CI((pszExt=pszFname+nLen-3), "cnt")) ||
               (nLen>=7 && STARTS_WITH_CI((pszExt=pszFname+nLen-7), "cnt.adf")) ) )
    {
        memcpy(pszExt, "cnx", 3);
        bCreateIndex = TRUE;
    }
    else if ((eType == AVCFileTXT || eType == AVCFileTX6) &&
             ( (nLen>=3 && STARTS_WITH_CI((pszExt=pszFname+nLen-3), "txt")) ||
               (nLen>=7 && STARTS_WITH_CI((pszExt=pszFname+nLen-7), "txt.adf")) ) )
    {
        memcpy(pszExt, "txx", 3);
        bCreateIndex = TRUE;
    }

    if (bCreateIndex)
    {
        psFile->psIndexFile = AVCRawBinOpen(pszFname, "w",
                                    AVC_COVER_BYTE_ORDER(psFile->eCoverType),
                                            psDBCSInfo);
    }

    CPLFree(pszFname);

    /*-----------------------------------------------------------------
     * Generate the appropriate headers for the main file and its index
     * if one was created.
     *----------------------------------------------------------------*/
    if (AVCBinWriteHeader(psFile) == -1)
    {
        /* Failed!  Return nullptr */
        AVCBinWriteClose(psFile);
        psFile = nullptr;
    }

    return psFile;
}


/**********************************************************************
 *                          _AVCBinWriteHeader()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinWriteHeader() instead)
 *
 * Generate a 100 bytes header using the info in psHeader.
 *
 * Note: PC Coverage files have an initial 256 bytes header followed by the
 * regular 100 bytes header.
 *
 * This function assumes that the file pointer is currently located at
 * the beginning of the file.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
static
int _AVCBinWriteHeader(AVCRawBinFile *psFile, AVCBinHeader *psHeader,
                       AVCCoverType eCoverType)
{
    int nStatus = 0;

    if (eCoverType == AVCCoverPC)
    {
        /* PC Coverage header starts with an initial 256 bytes header
         */
        AVCRawBinWriteInt16(psFile, 0x0400);  /* Signature??? */
        AVCRawBinWriteInt32(psFile, psHeader->nLength);

        AVCRawBinWriteZeros(psFile, 250);
    }

    AVCRawBinWriteInt32(psFile, psHeader->nSignature);
    AVCRawBinWriteInt32(psFile, psHeader->nPrecision);
    AVCRawBinWriteInt32(psFile, psHeader->nRecordSize);
    AVCRawBinWriteZeros(psFile, 12);
    AVCRawBinWriteInt32(psFile, psHeader->nLength);

    /* Pad the rest of the header with zeros
     */
    AVCRawBinWriteZeros(psFile, 72);

    if (CPLGetLastErrorNo() != 0)
        nStatus = -1;

    return nStatus;
}


/**********************************************************************
 *                          AVCBinWriteHeader()
 *
 * Write a header to the specified file using the values that apply to
 * this file's type.  The function simply does nothing if it is called
 * for a file type that requires no header.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
int AVCBinWriteHeader(AVCBinFile *psFile)
{
    AVCBinHeader sHeader;
    int          nStatus=0;
    GBool        bHeader = TRUE;

    memset(&sHeader, 0, sizeof(sHeader));

    /*-----------------------------------------------------------------
     * Set the appropriate header information for this file type.
     *----------------------------------------------------------------*/
    sHeader.nPrecision = 0;
    sHeader.nRecordSize = 0;
    sHeader.nLength = 0;
    sHeader.nSignature = 9994;
    switch(psFile->eFileType)
    {
      case AVCFileARC:
        sHeader.nPrecision = (psFile->nPrecision == AVC_DOUBLE_PREC)? -1 : 1;
        break;
      case AVCFilePAL:
      case AVCFileRPL:
        sHeader.nPrecision = (psFile->nPrecision == AVC_DOUBLE_PREC)? -11 : 11;
        break;
      case AVCFileLAB:
        sHeader.nSignature = 9993;
        sHeader.nPrecision = (psFile->nPrecision == AVC_DOUBLE_PREC)? -2 : 2;
        sHeader.nRecordSize = (psFile->nPrecision == AVC_DOUBLE_PREC)? 28 : 16;
        break;
      case AVCFileCNT:
        sHeader.nPrecision = (psFile->nPrecision == AVC_DOUBLE_PREC)? -14 : 14;
        break;
      case AVCFileTOL:
        /* single prec: tol.adf has no header
         * double prec: par.adf has a header
         */
        if (psFile->nPrecision == AVC_DOUBLE_PREC)
        {
            sHeader.nSignature = 9993;
            sHeader.nPrecision = 40;
            sHeader.nRecordSize = 8;
        }
        else
        {
            bHeader = FALSE;
        }
        break;
      case AVCFileTXT:
      case AVCFileTX6:
        if (psFile->eCoverType == AVCCoverPC)
            sHeader.nPrecision = 1;
        else
            sHeader.nPrecision = (psFile->nPrecision==AVC_DOUBLE_PREC)? -67:67;
        break;
      default:
        /* Other file types don't need a header */
        bHeader = FALSE;
    }

    /*-----------------------------------------------------------------
     * Write a header only if applicable.
     *----------------------------------------------------------------*/
    if (bHeader)
        nStatus = _AVCBinWriteHeader(psFile->psRawBinFile, &sHeader,
                                     psFile->eCoverType);

    /*-----------------------------------------------------------------
     * Write a header for the index file... it is identical to the main
     * file's header.
     *----------------------------------------------------------------*/
    if (nStatus == 0 && bHeader && psFile->psIndexFile)
        nStatus = _AVCBinWriteHeader(psFile->psIndexFile, &sHeader,
                                     psFile->eCoverType);

    return nStatus;
}

/**********************************************************************
 *                          AVCBinWriteClose()
 *
 * Close a coverage file opened for writing, and release all memory
 * (object strcut., buffers, etc.) associated with this file.
 **********************************************************************/

void    AVCBinWriteClose(AVCBinFile *psFile)
{
    if (psFile->eFileType == AVCFileTABLE)
    {
        _AVCBinWriteCloseTable(psFile);
        return;
    }

    /*-----------------------------------------------------------------
     * Write the file size (nbr of 2 byte words) in the header at
     * byte 24 in the 100 byte header (only if applicable)
     * (And write the same value at byte 2-5 in the first header of PC Cover)
     *----------------------------------------------------------------*/
    if (psFile->psRawBinFile &&
        (psFile->eFileType == AVCFileARC ||
         psFile->eFileType == AVCFilePAL ||
         psFile->eFileType == AVCFileRPL ||
         psFile->eFileType == AVCFileLAB ||
         psFile->eFileType == AVCFileCNT ||
         psFile->eFileType == AVCFileTXT ||
         psFile->eFileType == AVCFileTX6 ||
         (psFile->eFileType == AVCFileTOL &&
          psFile->nPrecision == AVC_DOUBLE_PREC) ) )
    {
        GInt32 n32Size;
        n32Size = psFile->psRawBinFile->nCurPos/2;

        if (psFile->eCoverType == AVCCoverPC)
        {
            /* PC Cover... Pad to multiple of 512 bytes and write 2 headers
             * extra bytes at EOF are not included in the size written in
             * header.
             * The first 256 bytes header is not counted in the file size
             * written in both headers
             */
            n32Size -= 128;  /* minus 256 bytes */

            if (psFile->psRawBinFile->nCurPos%512 != 0)
                AVCRawBinWriteZeros(psFile->psRawBinFile,
                                    512 - psFile->psRawBinFile->nCurPos%512);

            CPL_IGNORE_RET_VAL_INT(VSIFSeekL(psFile->psRawBinFile->fp, 2, SEEK_SET));
            AVCRawBinWriteInt32(psFile->psRawBinFile, n32Size);

            CPL_IGNORE_RET_VAL_INT(VSIFSeekL(psFile->psRawBinFile->fp, 256+24, SEEK_SET));
            AVCRawBinWriteInt32(psFile->psRawBinFile, n32Size);
        }
        else
        {
            /* V7 Cover ... only 1 header */
            CPL_IGNORE_RET_VAL_INT(VSIFSeekL(psFile->psRawBinFile->fp, 24, SEEK_SET));
            AVCRawBinWriteInt32(psFile->psRawBinFile, n32Size);
        }
    }

    AVCRawBinClose(psFile->psRawBinFile);
    psFile->psRawBinFile = nullptr;

    /*-----------------------------------------------------------------
     * Same for the index file if it exists.
     *----------------------------------------------------------------*/
    if (psFile->psIndexFile)
    {
        GInt32 n32Size;
        n32Size = psFile->psIndexFile->nCurPos/2;

        if (psFile->eCoverType == AVCCoverPC)
        {
            /* PC Cover... Pad to multiple of 512 bytes and write 2 headers
             * extra bytes at EOF are not included in the size written in
             * header
             */
            n32Size -= 128;  /* minus 256 bytes */

            if (psFile->psIndexFile->nCurPos%512 != 0)
                AVCRawBinWriteZeros(psFile->psIndexFile,
                                    512 - psFile->psIndexFile->nCurPos%512);

            CPL_IGNORE_RET_VAL_INT(VSIFSeekL(psFile->psIndexFile->fp, 2, SEEK_SET));
            AVCRawBinWriteInt32(psFile->psIndexFile, n32Size);

            CPL_IGNORE_RET_VAL_INT(VSIFSeekL(psFile->psIndexFile->fp, 256+24, SEEK_SET));
            AVCRawBinWriteInt32(psFile->psIndexFile, n32Size);
        }
        else
        {
            /* V7 Cover ... only 1 header */
            CPL_IGNORE_RET_VAL_INT(VSIFSeekL(psFile->psIndexFile->fp, 24, SEEK_SET));
            AVCRawBinWriteInt32(psFile->psIndexFile, n32Size);
        }

        AVCRawBinClose(psFile->psIndexFile);
        psFile->psIndexFile = nullptr;
    }

    CPLFree(psFile->pszFilename);

    CPLFree(psFile);
}



/**********************************************************************
 *                          _AVCBinWriteIndexEntry()
 *
 * (This function is for internal library use... the index entries
 * are automatically handled by the AVCBinWrite*() functions)
 *
 * Write an Index Entry at the current position in the file.
 *
 * Position is relative to the beginning of the file, including the header.
 * Both position and size are specified in number of 2 byte words.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
static
int _AVCBinWriteIndexEntry(AVCRawBinFile *psFile,
                           int nPosition, int nSize)
{
    AVCRawBinWriteInt32(psFile, nPosition);
    AVCRawBinWriteInt32(psFile, nSize);

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                          AVCBinWriteObject()
 *
 * Write a CNT (Polygon Centroid) structure to the fin object to a
 * coverage file.
 *
 * Simply redirects the call to the right function based on the value
 * of psFile->eFileType.
 *
 * Returns 0 on success or -1 on error.
 *
 * If a problem happens, then CPLError() will be called by the lower-level
 * functions and CPLGetLastErrorNo() can be used to find out what happened.
 **********************************************************************/
int AVCBinWriteObject(AVCBinFile *psFile, void *psObj)
{
    int nStatus = 0;
    switch(psFile->eFileType)
    {
      case AVCFileARC:
        nStatus = AVCBinWriteArc(psFile, (AVCArc *)psObj);
        break;
      case AVCFilePAL:
      case AVCFileRPL:
        nStatus = AVCBinWritePal(psFile, (AVCPal *)psObj);
        break;
      case AVCFileCNT:
        nStatus = AVCBinWriteCnt(psFile, (AVCCnt *)psObj);
        break;
      case AVCFileLAB:
        nStatus = AVCBinWriteLab(psFile, (AVCLab *)psObj);
        break;
      case AVCFileTOL:
        nStatus = AVCBinWriteTol(psFile, (AVCTol *)psObj);
        break;
      case AVCFilePRJ:
        nStatus = AVCBinWritePrj(psFile, (char **)psObj);
        break;
      case AVCFileTXT:
      case AVCFileTX6:
        nStatus = AVCBinWriteTxt(psFile, (AVCTxt *)psObj);
        break;
      case AVCFileRXP:
        nStatus = AVCBinWriteRxp(psFile, (AVCRxp *)psObj);
        break;
      case AVCFileTABLE:
        nStatus = AVCBinWriteTableRec(psFile, (AVCField *)psObj);
        break;
      default:
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "AVCBinWriteObject(): Unsupported file type!");
        nStatus = -1;
    }

    return nStatus;
}


/*=====================================================================
 *                              ARC
 *====================================================================*/

/**********************************************************************
 *                          _AVCBinWriteArc()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinWriteNextArc() instead)
 *
 * Write an Arc structure to the file.
 *
 * The contents of the psArc structure is assumed to be valid... this
 * function performs no validation on the consistency of what it is
 * given as input.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
static
int _AVCBinWriteArc(AVCRawBinFile *psFile, AVCArc *psArc,
                    int nPrecision, AVCRawBinFile *psIndexFile)
{
    int         i, nRecSize, nCurPos;

    nCurPos = psFile->nCurPos/2;  /* Value in 2 byte words */

    AVCRawBinWriteInt32(psFile, psArc->nArcId);
    if (CPLGetLastErrorNo() != 0)
        return -1;

    /*-----------------------------------------------------------------
     * Record size is expressed in 2 byte words, and does not count the
     * first 8 bytes of the ARC entry.
     *----------------------------------------------------------------*/
    nRecSize = (6 * 4 + psArc->numVertices*2 *
                ((nPrecision == AVC_SINGLE_PREC)? 4 : 8)) / 2;
    AVCRawBinWriteInt32(psFile, nRecSize);
    AVCRawBinWriteInt32(psFile, psArc->nUserId);
    AVCRawBinWriteInt32(psFile, psArc->nFNode);
    AVCRawBinWriteInt32(psFile, psArc->nTNode);
    AVCRawBinWriteInt32(psFile, psArc->nLPoly);
    AVCRawBinWriteInt32(psFile, psArc->nRPoly);
    AVCRawBinWriteInt32(psFile, psArc->numVertices);

    if (nPrecision == AVC_SINGLE_PREC)
    {
        for(i=0; i<psArc->numVertices; i++)
        {
            AVCRawBinWriteFloat(psFile, (float)psArc->pasVertices[i].x);
            AVCRawBinWriteFloat(psFile, (float)psArc->pasVertices[i].y);
        }
    }
    else
    {
        for(i=0; i<psArc->numVertices; i++)
        {
            AVCRawBinWriteDouble(psFile, psArc->pasVertices[i].x);
            AVCRawBinWriteDouble(psFile, psArc->pasVertices[i].y);
        }

    }

    /*-----------------------------------------------------------------
     * Write index entry (arx.adf)
     *----------------------------------------------------------------*/
    if (psIndexFile)
    {
        _AVCBinWriteIndexEntry(psIndexFile, nCurPos, nRecSize);
    }

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                          AVCBinWriteArc()
 *
 * Write the next Arc structure to the file.
 *
 * The contents of the psArc structure is assumed to be valid... this
 * function performs no validation on the consistency of what it is
 * given as input.
 *
 * Returns 0 on success or -1 on error.
 *
 * If a problem happens, then CPLError() will be called by the lower-level
 * functions and CPLGetLastErrorNo() can be used to find out what happened.
 **********************************************************************/
int AVCBinWriteArc(AVCBinFile *psFile, AVCArc *psArc)
{
    if (psFile->eFileType != AVCFileARC)
        return -1;

    return _AVCBinWriteArc(psFile->psRawBinFile, psArc,
                           psFile->nPrecision, psFile->psIndexFile);
}


/*=====================================================================
 *                              PAL
 *====================================================================*/

/**********************************************************************
 *                          _AVCBinWritePal()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinWritePal() instead)
 *
 * Write a PAL (Polygon Arc List) structure to the file.
 *
 * The contents of the psPal structure is assumed to be valid... this
 * function performs no validation on the consistency of what it is
 * given as input.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
static
int _AVCBinWritePal(AVCRawBinFile *psFile, AVCPal *psPal,
                    int nPrecision, AVCRawBinFile *psIndexFile)
{
    int i, nRecSize, nCurPos;

    nCurPos = psFile->nCurPos/2;  /* Value in 2 byte words */

    AVCRawBinWriteInt32(psFile, psPal->nPolyId);
    if (CPLGetLastErrorNo() != 0)
        return -1;

    /*-----------------------------------------------------------------
     * Record size is expressed in 2 byte words, and does not count the
     * first 8 bytes of the PAL entry.
     *----------------------------------------------------------------*/
    nRecSize = ( 4 + psPal->numArcs*3 * 4 +
                4 * ((nPrecision == AVC_SINGLE_PREC)? 4 : 8)) / 2;

    AVCRawBinWriteInt32(psFile, nRecSize);

    if (nPrecision == AVC_SINGLE_PREC)
    {
        AVCRawBinWriteFloat(psFile, (float)psPal->sMin.x);
        AVCRawBinWriteFloat(psFile, (float)psPal->sMin.y);
        AVCRawBinWriteFloat(psFile, (float)psPal->sMax.x);
        AVCRawBinWriteFloat(psFile, (float)psPal->sMax.y);
    }
    else
    {
        AVCRawBinWriteDouble(psFile, psPal->sMin.x);
        AVCRawBinWriteDouble(psFile, psPal->sMin.y);
        AVCRawBinWriteDouble(psFile, psPal->sMax.x);
        AVCRawBinWriteDouble(psFile, psPal->sMax.y);
    }

    AVCRawBinWriteInt32(psFile, psPal->numArcs);

    for(i=0; i<psPal->numArcs; i++)
    {
        AVCRawBinWriteInt32(psFile, psPal->pasArcs[i].nArcId);
        AVCRawBinWriteInt32(psFile, psPal->pasArcs[i].nFNode);
        AVCRawBinWriteInt32(psFile, psPal->pasArcs[i].nAdjPoly);
    }

    /*-----------------------------------------------------------------
     * Write index entry (pax.adf)
     *----------------------------------------------------------------*/
    if (psIndexFile)
    {
        _AVCBinWriteIndexEntry(psIndexFile, nCurPos, nRecSize);
    }

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                          AVCBinWritePal()
 *
 * Write a PAL (Polygon Arc List) structure to the file.
 *
 * The contents of the psPal structure is assumed to be valid... this
 * function performs no validation on the consistency of what it is
 * given as input.
 *
 * Returns 0 on success or -1 on error.
 *
 * If a problem happens, then CPLError() will be called by the lower-level
 * functions and CPLGetLastErrorNo() can be used to find out what happened.
 **********************************************************************/
int AVCBinWritePal(AVCBinFile *psFile, AVCPal *psPal)
{
    if (psFile->eFileType != AVCFilePAL && psFile->eFileType != AVCFileRPL)
        return -1;

    return _AVCBinWritePal(psFile->psRawBinFile, psPal,
                           psFile->nPrecision, psFile->psIndexFile);
}

/*=====================================================================
 *                              CNT
 *====================================================================*/

/**********************************************************************
 *                          _AVCBinWriteCnt()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinWriteCnt() instead)
 *
 * Write a CNT (Polygon Centroid) structure to the file.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
static
int _AVCBinWriteCnt(AVCRawBinFile *psFile, AVCCnt *psCnt,
                              int nPrecision, AVCRawBinFile *psIndexFile)
{
    int i, nRecSize, nCurPos;

    nCurPos = psFile->nCurPos/2;  /* Value in 2 byte words */

    AVCRawBinWriteInt32(psFile, psCnt->nPolyId);
    if (CPLGetLastErrorNo() != 0)
        return -1;

    /*-----------------------------------------------------------------
     * Record size is expressed in 2 byte words, and does not count the
     * first 8 bytes of the CNT entry.
     *----------------------------------------------------------------*/
    nRecSize = ( 4 + psCnt->numLabels * 4 +
                 2 * ((nPrecision == AVC_SINGLE_PREC)? 4 : 8)) / 2;

    AVCRawBinWriteInt32(psFile, nRecSize);

    if (nPrecision == AVC_SINGLE_PREC)
    {
        AVCRawBinWriteFloat(psFile, (float)psCnt->sCoord.x);
        AVCRawBinWriteFloat(psFile, (float)psCnt->sCoord.y);
    }
    else
    {
        AVCRawBinWriteDouble(psFile, psCnt->sCoord.x);
        AVCRawBinWriteDouble(psFile, psCnt->sCoord.y);
    }

    AVCRawBinWriteInt32(psFile, psCnt->numLabels);

    for(i=0; i<psCnt->numLabels; i++)
    {
        AVCRawBinWriteInt32(psFile, psCnt->panLabelIds[i]);
    }

    /*-----------------------------------------------------------------
     * Write index entry (cnx.adf)
     *----------------------------------------------------------------*/
    if (psIndexFile)
    {
        _AVCBinWriteIndexEntry(psIndexFile, nCurPos, nRecSize);
    }

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                          AVCBinWriteCnt()
 *
 * Write a CNT (Polygon Centroid) structure to the file.
 *
 * The contents of the psCnt structure is assumed to be valid... this
 * function performs no validation on the consistency of what it is
 * given as input.
 *
 * Returns 0 on success or -1 on error.
 *
 * If a problem happens, then CPLError() will be called by the lower-level
 * functions and CPLGetLastErrorNo() can be used to find out what happened.
 **********************************************************************/
int AVCBinWriteCnt(AVCBinFile *psFile, AVCCnt *psCnt)
{
    if (psFile->eFileType != AVCFileCNT)
        return -1;

    return _AVCBinWriteCnt(psFile->psRawBinFile, psCnt,
                           psFile->nPrecision, psFile->psIndexFile);
}

/*=====================================================================
 *                              LAB
 *====================================================================*/

/**********************************************************************
 *                          _AVCBinWriteLab()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinWriteLab() instead)
 *
 * Write a LAB (Centroid Label) structure to the file.
 *
 * The contents of the psLab structure is assumed to be valid... this
 * function performs no validation on the consistency of what it is
 * given as input.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
static
int _AVCBinWriteLab(AVCRawBinFile *psFile, AVCLab *psLab,
                    int nPrecision)
{

    AVCRawBinWriteInt32(psFile, psLab->nValue);
    if (CPLGetLastErrorNo() != 0)
        return -1;

    AVCRawBinWriteInt32(psFile, psLab->nPolyId);

    if (nPrecision == AVC_SINGLE_PREC)
    {
        AVCRawBinWriteFloat(psFile, (float)psLab->sCoord1.x);
        AVCRawBinWriteFloat(psFile, (float)psLab->sCoord1.y);
        AVCRawBinWriteFloat(psFile, (float)psLab->sCoord2.x);
        AVCRawBinWriteFloat(psFile, (float)psLab->sCoord2.y);
        AVCRawBinWriteFloat(psFile, (float)psLab->sCoord3.x);
        AVCRawBinWriteFloat(psFile, (float)psLab->sCoord3.y);
    }
    else
    {
        AVCRawBinWriteDouble(psFile, psLab->sCoord1.x);
        AVCRawBinWriteDouble(psFile, psLab->sCoord1.y);
        AVCRawBinWriteDouble(psFile, psLab->sCoord2.x);
        AVCRawBinWriteDouble(psFile, psLab->sCoord2.y);
        AVCRawBinWriteDouble(psFile, psLab->sCoord3.x);
        AVCRawBinWriteDouble(psFile, psLab->sCoord3.y);
    }

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}


/**********************************************************************
 *                          AVCBinWriteLab()
 *
 * Write a LAB (Centroid Label) structure to the file.
 *
 * The contents of the psLab structure is assumed to be valid... this
 * function performs no validation on the consistency of what it is
 * given as input.
 *
 * Returns 0 on success or -1 on error.
 *
 * If a problem happens, then CPLError() will be called by the lower-level
 * functions and CPLGetLastErrorNo() can be used to find out what happened.
 **********************************************************************/
int AVCBinWriteLab(AVCBinFile *psFile, AVCLab *psLab)
{
    if (psFile->eFileType != AVCFileLAB)
        return -1;

    return _AVCBinWriteLab(psFile->psRawBinFile, psLab,
                           psFile->nPrecision);
}

/*=====================================================================
 *                              TOL
 *====================================================================*/

/**********************************************************************
 *                          _AVCBinWriteTol()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinWriteTol() instead)
 *
 * Write a TOL (tolerance) structure to the file.
 *
 * The contents of the psTol structure is assumed to be valid... this
 * function performs no validation on the consistency of what it is
 * given as input.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
static
int _AVCBinWriteTol(AVCRawBinFile *psFile, AVCTol *psTol,
                    int nPrecision)
{

    AVCRawBinWriteInt32(psFile, psTol->nIndex);
    if (CPLGetLastErrorNo() != 0)
        return -1;

    AVCRawBinWriteInt32(psFile, psTol->nFlag);

    if (nPrecision == AVC_SINGLE_PREC)
    {
        AVCRawBinWriteFloat(psFile, (float)psTol->dValue);
    }
    else
    {
        AVCRawBinWriteDouble(psFile, psTol->dValue);
    }

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                          AVCBinWriteTol()
 *
 * Write a TOL (tolerance) structure to the file.
 *
 * The contents of the psTol structure is assumed to be valid... this
 * function performs no validation on the consistency of what it is
 * given as input.
 *
 * Returns 0 on success or -1 on error.
 *
 * If a problem happens, then CPLError() will be called by the lower-level
 * functions and CPLGetLastErrorNo() can be used to find out what happened.
 **********************************************************************/
int AVCBinWriteTol(AVCBinFile *psFile, AVCTol *psTol)
{
    if (psFile->eFileType != AVCFileTOL)
        return -1;

    return _AVCBinWriteTol(psFile->psRawBinFile, psTol,
                           psFile->nPrecision);
}

/*=====================================================================
 *                              PRJ
 *====================================================================*/

/**********************************************************************
 *                          AVCBinWritePrj()
 *
 * Write a PRJ (Projection info) to the file.
 *
 * Since a PRJ file is a simple text file and there is only ONE projection
 * info per prj.adf file, this function behaves differently from the
 * other ones... all the job is done here, including creating and closing
 * the output file.
 *
 * The contents of the papszPrj is assumed to be valid... this
 * function performs no validation on the consistency of what it is
 * given as input.
 *
 * Returns 0 on success or -1 on error.
 *
 * If a problem happens, then CPLError() will be called by the lower-level
 * functions and CPLGetLastErrorNo() can be used to find out what happened.
 **********************************************************************/
int AVCBinWritePrj(AVCBinFile *psFile, char **papszPrj)
{
    if (psFile->eFileType != AVCFilePRJ)
        return -1;

    CSLSave(papszPrj, psFile->pszFilename);

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}


/*=====================================================================
 *                              TXT/TX6/TX7
 *====================================================================*/

/**********************************************************************
 *                          _AVCBinWriteTxt()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinWriteTxt() instead)
 *
 * Write a TXT/TX6/TX7 (Annotation) structure to the file.
 *
 * The contents of the psTxt structure is assumed to be valid... this
 * function performs no validation on the consistency of what it is
 * given as input.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
static
int _AVCBinWriteTxt(AVCRawBinFile *psFile, AVCTxt *psTxt,
                              int nPrecision, AVCRawBinFile *psIndexFile)
{
    int i, nRecSize, nCurPos, nStrLen, numVertices;

    nCurPos = psFile->nCurPos/2;  /* Value in 2 byte words */

    AVCRawBinWriteInt32(psFile, psTxt->nTxtId);
    if (CPLGetLastErrorNo() != 0)
        return -1;

    /*-----------------------------------------------------------------
     * Record size is expressed in 2 byte words, and does not count the
     * first 8 bytes of the TXT entry.
     *----------------------------------------------------------------*/
    /* String uses a multiple of 4 bytes of storage */
    if (psTxt->pszText)
        nStrLen = (((int)strlen((char*)psTxt->pszText) + 3)/4)*4;
    else
        nStrLen = 0;

    numVertices = ABS(psTxt->numVerticesLine) + ABS(psTxt->numVerticesArrow);
    nRecSize = (112 + 8 + nStrLen +
                (numVertices*2+3)* ((nPrecision == AVC_SINGLE_PREC)?4:8)) / 2;

    AVCRawBinWriteInt32(psFile, nRecSize);

    AVCRawBinWriteInt32(psFile, psTxt->nUserId );
    AVCRawBinWriteInt32(psFile, psTxt->nLevel );
    AVCRawBinWriteFloat(psFile, psTxt->f_1e2 );
    AVCRawBinWriteInt32(psFile, psTxt->nSymbol );
    AVCRawBinWriteInt32(psFile, psTxt->numVerticesLine );
    AVCRawBinWriteInt32(psFile, psTxt->n28 );
    AVCRawBinWriteInt32(psFile, psTxt->numChars );
    AVCRawBinWriteInt32(psFile, psTxt->numVerticesArrow );

    for(i=0; i<20; i++)
    {
        AVCRawBinWriteInt16(psFile, psTxt->anJust1[i] );
    }
    for(i=0; i<20; i++)
    {
        AVCRawBinWriteInt16(psFile, psTxt->anJust2[i] );
    }

    if (nPrecision == AVC_SINGLE_PREC)
    {
        AVCRawBinWriteFloat(psFile, (float)psTxt->dHeight);
        AVCRawBinWriteFloat(psFile, (float)psTxt->dV2);
        AVCRawBinWriteFloat(psFile, (float)psTxt->dV3);
    }
    else
    {
        AVCRawBinWriteDouble(psFile, psTxt->dHeight);
        AVCRawBinWriteDouble(psFile, psTxt->dV2);
        AVCRawBinWriteDouble(psFile, psTxt->dV3);
    }

    if (nStrLen > 0)
        AVCRawBinWritePaddedString(psFile, nStrLen, psTxt->pszText);

    if (nPrecision == AVC_SINGLE_PREC)
    {
        for(i=0; i<numVertices; i++)
        {
            AVCRawBinWriteFloat(psFile, (float)psTxt->pasVertices[i].x);
            AVCRawBinWriteFloat(psFile, (float)psTxt->pasVertices[i].y);
        }
    }
    else
    {
        for(i=0; i<numVertices; i++)
        {
            AVCRawBinWriteDouble(psFile, psTxt->pasVertices[i].x);
            AVCRawBinWriteDouble(psFile, psTxt->pasVertices[i].y);
        }
    }

    AVCRawBinWriteZeros(psFile, 8);

    /*-----------------------------------------------------------------
     * Write index entry (cnx.adf)
     *----------------------------------------------------------------*/
    if (psIndexFile)
    {
        _AVCBinWriteIndexEntry(psIndexFile, nCurPos, nRecSize);
    }

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                          _AVCBinWritePCCoverageTxt()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinWriteTxt() instead)
 *
 * Write a TXT (Annotation) structure to a AVCCoverPC file.
 *
 * The contents of the psTxt structure is assumed to be valid... this
 * function performs no validation on the consistency of what it is
 * given as input.
 *
 * This function assumes that PC Coverages are always single precision.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
static
int _AVCBinWritePCCoverageTxt(AVCRawBinFile *psFile, AVCTxt *psTxt,
                              CPL_UNUSED int nPrecision,
                              AVCRawBinFile *psIndexFile)
{
    int i, nRecSize, nCurPos, nStrLen, numVertices;

    CPLAssert(nPrecision == AVC_SINGLE_PREC);

    nCurPos = psFile->nCurPos/2;  /* Value in 2 byte words */

    AVCRawBinWriteInt32(psFile, psTxt->nTxtId);
    if (CPLGetLastErrorNo() != 0)
        return -1;

    /*-----------------------------------------------------------------
     * Record size is expressed in 2 byte words, and does not count the
     * first 8 bytes of the TXT entry.
     *----------------------------------------------------------------*/
    /* String uses a multiple of 4 bytes of storage,
     * And if text is already a multiple of 4 bytes then we include 4 extra
     * spaces anyways (was probably a bug in the software!).
     */
    if (psTxt->pszText)
        nStrLen = (((int)strlen((char*)psTxt->pszText) + 4)/4)*4;
    else
        nStrLen = 4;

    nRecSize = (92 - 8 + nStrLen) / 2;

    AVCRawBinWriteInt32(psFile, nRecSize);
    AVCRawBinWriteInt32(psFile, psTxt->nLevel );

    /*-----------------------------------------------------------------
     * Number of vertices to write:
     * Note that because of the way V7 binary TXT files work, the rest of the
     * lib expects to receive duplicate coords for the first vertex, so
     * we will also receive an additional vertex for that but we won't write
     * it.  We also ignore the arrow vertices if there is any.
     *----------------------------------------------------------------*/
    numVertices = ABS(psTxt->numVerticesLine) -1;
    numVertices = MIN(4, numVertices);  /* Maximum of 4 points */

    AVCRawBinWriteInt32(psFile, numVertices );

    for(i=0; i<numVertices; i++)
    {
        AVCRawBinWriteFloat(psFile, (float)psTxt->pasVertices[i+1].x);
        AVCRawBinWriteFloat(psFile, (float)psTxt->pasVertices[i+1].y);
    }

    AVCRawBinWriteZeros(psFile, (4-numVertices)*4*2 + 28);

    AVCRawBinWriteFloat(psFile, (float)psTxt->dHeight);
    AVCRawBinWriteFloat(psFile, psTxt->f_1e2 );
    AVCRawBinWriteInt32(psFile, psTxt->nSymbol );
    AVCRawBinWriteInt32(psFile, psTxt->numChars );

    if (nStrLen > 0)
        AVCRawBinWritePaddedString(psFile, nStrLen, psTxt->pszText ? psTxt->pszText : (const GByte*)"    ");

    /*-----------------------------------------------------------------
     * Write index entry (cnx.adf)
     *----------------------------------------------------------------*/
    if (psIndexFile)
    {
        _AVCBinWriteIndexEntry(psIndexFile, nCurPos, nRecSize);
    }

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}


/**********************************************************************
 *                          AVCBinWriteTxt()
 *
 * Write a TXT/TX6/TX7 (Annotation) structure to the file.
 *
 * The contents of the psTxt structure is assumed to be valid... this
 * function performs no validation on the consistency of what it is
 * given as input.
 *
 * Returns 0 on success or -1 on error.
 *
 * If a problem happens, then CPLError() will be called by the lower-level
 * functions and CPLGetLastErrorNo() can be used to find out what happened.
 **********************************************************************/
int AVCBinWriteTxt(AVCBinFile *psFile, AVCTxt *psTxt)
{
    if (psFile->eFileType != AVCFileTXT && psFile->eFileType != AVCFileTX6)
        return -1;

    /* AVCCoverPC and AVCCoverWeird have a different TXT format than AVCCoverV7
     */
    if (psFile->eCoverType == AVCCoverPC ||
        psFile->eCoverType == AVCCoverWeird)
    {
        return _AVCBinWritePCCoverageTxt(psFile->psRawBinFile, psTxt,
                                         psFile->nPrecision,
                                         psFile->psIndexFile);
    }
    else
    {
        return _AVCBinWriteTxt(psFile->psRawBinFile, psTxt,
                               psFile->nPrecision, psFile->psIndexFile);
    }
}

/*=====================================================================
 *                              RXP
 *====================================================================*/

/**********************************************************************
 *                          _AVCBinWriteRxp()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinWriteRxp() instead)
 *
 * Write a RXP (Region something...) structure to the file.
 *
 * The contents of the psRxp structure is assumed to be valid... this
 * function performs no validation on the consistency of what it is
 * given as input.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
static
int _AVCBinWriteRxp(AVCRawBinFile *psFile,
                    AVCRxp *psRxp,
                    CPL_UNUSED int nPrecision)
{

    AVCRawBinWriteInt32(psFile, psRxp->n1);
    if (CPLGetLastErrorNo() != 0)
        return -1;

    AVCRawBinWriteInt32(psFile, psRxp->n2);

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                          AVCBinWriteRxp()
 *
 * Write a  RXP (Region something...) structure to the file.
 *
 * The contents of the psRxp structure is assumed to be valid... this
 * function performs no validation on the consistency of what it is
 * given as input.
 *
 * Returns 0 on success or -1 on error.
 *
 * If a problem happens, then CPLError() will be called by the lower-level
 * functions and CPLGetLastErrorNo() can be used to find out what happened.
 **********************************************************************/
int AVCBinWriteRxp(AVCBinFile *psFile, AVCRxp *psRxp)
{
    if (psFile->eFileType != AVCFileRXP)
        return -1;

    return _AVCBinWriteRxp(psFile->psRawBinFile, psRxp,
                           psFile->nPrecision);
}


/*=====================================================================
 *                              TABLES
 *====================================================================*/

/**********************************************************************
 *                          _AVCBinWriteArcDir()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinWriteCreateTable() instead)
 *
 * Write an ARC.DIR entry at the current position in file.
 *
 * The contents of the psTableDef structure is assumed to be valid... this
 * function performs no validation on the consistency of what it is
 * given as input.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
static
int _AVCBinWriteArcDir(AVCRawBinFile *psFile, AVCTableDef *psTableDef)
{
    /* STRING values MUST be padded with spaces.
     */
    AVCRawBinWritePaddedString(psFile, 32, (GByte*)psTableDef->szTableName);
    if (CPLGetLastErrorNo() != 0)
        return -1;

    AVCRawBinWritePaddedString(psFile, 8, (GByte*)psTableDef->szInfoFile);

    AVCRawBinWriteInt16(psFile, psTableDef->numFields);

    /* Record size must be a multiple of 2 bytes */
    AVCRawBinWriteInt16(psFile, (GInt16)(((psTableDef->nRecSize+1)/2)*2));

    /* ??? Unknown values ??? */
    AVCRawBinWritePaddedString(psFile, 16, (GByte*)"                    ");
    AVCRawBinWriteInt16(psFile, 132);
    AVCRawBinWriteInt16(psFile, 0);

    AVCRawBinWriteInt32(psFile, psTableDef->numRecords);

    AVCRawBinWriteZeros(psFile, 10);

    AVCRawBinWritePaddedString(psFile, 2, (GByte*)psTableDef->szExternal);

    AVCRawBinWriteZeros(psFile, 238);
    AVCRawBinWritePaddedString(psFile, 8, (GByte*)"                    ");
    AVCRawBinWriteZeros(psFile, 54);

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}


/**********************************************************************
 *                          _AVCBinWriteArcNit()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinWriteCreateTable() instead)
 *
 * Write an ARC####.NIT entry at the current position in file.
 *
 * The contents of the psTableDef structure is assumed to be valid... this
 * function performs no validation on the consistency of what it is
 * given as input.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
static
int _AVCBinWriteArcNit(AVCRawBinFile *psFile, AVCFieldInfo *psField)
{
    /* STRING values MUST be padded with spaces.
     */
    AVCRawBinWritePaddedString(psFile, 16, (GByte*)psField->szName);
    if (CPLGetLastErrorNo() != 0)
        return -1;

    AVCRawBinWriteInt16(psFile, psField->nSize);
    AVCRawBinWriteInt16(psFile, psField->v2);
    AVCRawBinWriteInt16(psFile, psField->nOffset);
    AVCRawBinWriteInt16(psFile, psField->v4);
    AVCRawBinWriteInt16(psFile, psField->v5);
    AVCRawBinWriteInt16(psFile, psField->nFmtWidth);
    AVCRawBinWriteInt16(psFile, psField->nFmtPrec);
    AVCRawBinWriteInt16(psFile, psField->nType1);
    AVCRawBinWriteInt16(psFile, psField->nType2);
    AVCRawBinWriteInt16(psFile, psField->v10);
    AVCRawBinWriteInt16(psFile, psField->v11);
    AVCRawBinWriteInt16(psFile, psField->v12);
    AVCRawBinWriteInt16(psFile, psField->v13);

    AVCRawBinWritePaddedString(psFile, 16, (GByte*)psField->szAltName);

    AVCRawBinWriteZeros(psFile, 56);

    AVCRawBinWriteInt16(psFile, psField->nIndex);

    AVCRawBinWriteZeros(psFile, 28);

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}


/**********************************************************************
 *                     _AVCBinWriteCreateArcDirEntry()
 *
 * Add an entry in the ARC.DIR for the table defined in psSrcTableDef.
 *
 * If an entry with the same table name already exists then this entry
 * will be reused and overwritten.
 *
 * Note: there could be a problem if 2 processes try to add an entry
 * at the exact same time... does Arc/Info do any locking on that file???
 *
 * Returns an integer value corresponding to the new table index (ARC####)
 * or -1 if something failed.
 **********************************************************************/

/* Prototype for _AVCBinReadNextArcDir() from avc_bin.c
 */
int _AVCBinReadNextArcDir(AVCRawBinFile *psFile, AVCTableDef *psArcDir);

static
int _AVCBinWriteCreateArcDirEntry(const char *pszArcDirFile,
                                  AVCTableDef *psTableDef,
                                  AVCDBCSInfo *psDBCSInfo)
{
    int          iEntry, numDirEntries=0, nTableIndex = 0;
    VSIStatBufL   sStatBuf;
    AVCRawBinFile *hRawBinFile;
    GBool        bFound;
    AVCTableDef  sEntry;

    /*-----------------------------------------------------------------
     * Open and Scan the ARC.DIR to establish the table index (ARC####)
     *----------------------------------------------------------------*/
#ifdef _WIN32
    /*-----------------------------------------------------------------
     * Note, DM, 20010507 - We used to use VSIFStat() to establish the
     * size of arc.dir, but when working on a WinNT4 networked drive, the
     * stat() information was not always right, and we sometimes ended
     * up overwriting arc.dir entries.  The solution: open and scan arc.dir
     * until EOF to establish its size.
     * That trick also seems to fix another network buffer problem: when
     * writing a coverage in a new empty dir (with no info dir yet), we
     * would get an error in fwrite() while writing the 3rd arc.dir
     * entry of the coverage.  That second problem could also have been
     * fixed by forcing a VSIFSeek() before the first fwrite()... we've
     * added it below.
     *----------------------------------------------------------------*/
    VSILFILE *fp;
    if ((fp = VSIFOpenL(pszArcDirFile, "r")) != nullptr)
    {
        char buf[380];
        while (!VSIFEofL(fp))
        {
            if (VSIFReadL(buf, 380, 1, fp) == 1)
                numDirEntries++;
        }
        VSIFCloseL(fp);
        hRawBinFile = AVCRawBinOpen(pszArcDirFile, "r+",
                                    AVC_COVER_BYTE_ORDER(AVCCoverV7),
                                    psDBCSInfo);
    }
    else
#endif
    /* On Unix we can still use fstat() */
    if ( VSIStatL(pszArcDirFile, &sStatBuf) != -1 )
    {
        numDirEntries = (int)(sStatBuf.st_size/380);
        hRawBinFile = AVCRawBinOpen(pszArcDirFile, "r+",
                                    AVC_COVER_BYTE_ORDER(AVCCoverV7),
                                    psDBCSInfo);
    }
    else
    {
        numDirEntries = 0;
        hRawBinFile = AVCRawBinOpen(pszArcDirFile, "w",
                                    AVC_COVER_BYTE_ORDER(AVCCoverV7),
                                    psDBCSInfo);
    }

    if (hRawBinFile == nullptr)
    {
        /* Failed to open file... just return -1 since an error message
         * has already been issued by AVCRawBinOpen()
         */
        return -1;
    }

    /* Init nTableIndex at -1 so that first table created should have
     * index 0
     */
    nTableIndex = -1;
    iEntry = 0;
    bFound = FALSE;
    while(!bFound && iEntry<numDirEntries &&
          _AVCBinReadNextArcDir(hRawBinFile, &sEntry) == 0)
    {
        nTableIndex = atoi(sEntry.szInfoFile+3);
        if (EQUALN(psTableDef->szTableName, sEntry.szTableName,
                   strlen(psTableDef->szTableName)))        {
            bFound = TRUE;
            break;
        }
        iEntry++;
    }

    /*-----------------------------------------------------------------
     * Reposition the file pointer and write the entry.
     *
     * We use VSIFSeek() directly since the AVCRawBin*() functions do
     * not support random access yet... it is OK to do so here since the
     * ARC.DIR does not have a header and we will close it right away.
     *----------------------------------------------------------------*/
    if (bFound)
        CPL_IGNORE_RET_VAL_INT(VSIFSeekL(hRawBinFile->fp, iEntry*380, SEEK_SET));
    else
    {
        /* Not found... Use the next logical table index */
        nTableIndex++;

        /* We are already at EOF so we should not need to fseek here, but
         * ANSI-C requires that a file positioning function be called
         * between read and writes... this had never been a problem before
         * on any system except with NT4 network drives.
         */
        CPL_IGNORE_RET_VAL_INT(VSIFSeekL(hRawBinFile->fp, numDirEntries*380, SEEK_SET));
    }

    snprintf(psTableDef->szInfoFile, sizeof(psTableDef->szInfoFile), "ARC%4.4d", nTableIndex);
    _AVCBinWriteArcDir(hRawBinFile, psTableDef);

    AVCRawBinClose(hRawBinFile);

    return nTableIndex;
}


/**********************************************************************
 *                          AVCBinWriteCreateTable()
 *
 * Open an INFO table for writing:
 *
 *  - Add an entry for the new table in the info/arc.dir
 *  - Write the attributes definitions to the info/arc####.nit
 *  - Create the data file, ready to write data records to it
 *  - If necessary, set the arc####.dat to point to the location of
 *    the data file.
 *
 * pszInfoPath is the info directory path, terminated by a '/' or a '\\'
 * It is assumed that this 'info' directory already exists and is writable.
 *
 * psTableDef should contain a valid table definition for this coverage.
 * This function will create and maintain its own copy of the structure.
 *
 * The name of the file to create and its location will be based on the
 * table name and the external ("XX") flag values in the psTableDef
 * structure, so you have to make sure that these values are valid.
 *
 * If a table with the same name is already present in the arc.dir, then
 * the same arc.dir entry will be used and overwritten.  This happens
 * when a coverage directory is deleted by hand.  The behavior implemented
 * here correspond to Arc/Info's behavior.
 *
 * For internal tables, the data file goes directly in the info directory, so
 * there is not much to worry about.
 *
 * For external tables, the table name is composed of 3 parts:
 *
 *         <COVERNAME>.<EXT><SUBCLASSNAME>
 *
 *  - <COVERNAME>:
 *    The first part of the table name (before the '.') is the
 *    name of the coverage to which the table belongs, and the data file
 *    will be created in this coverage's directory... so it is assumed that
 *    the directory "../<covername>" already exists and is writable.
 *  - <EXT>:
 *    The coverage name is followed by a 3 chars extension that will be
 *    used to build the name of the external table to create.
 *  - <SUBCLASSNAME>:
 *    For some table types, the extension is followed by a subclass name.
 *
 *  When <SUBCLASSNAME> is present, then the data file name will be:
 *            "../<covername>/<subclassname>.<ext>"
 *
 *    e.g. The table named "TEST.PATCOUNTY" would be stored in the file
 *         "../test/county.pat" (this path is relative to the info directory)
 *
 *  When the <SUBCLASSNAME> is not present, then the name of the data file
 *  will be the "../<covername>/<ext>.adf"
 *
 *    e.g. The table named "TEST.PAT" would be stored in the file
 *         "../test/pat.adf"
 *
 * Of course, it would be too easy if there were no exceptions to these
 * rules!  Single precision ".TIC" and ".BND" follow the above rules and
 * will be named "tic.adf" and "bnd.adf" but in double precision coverages,
 * they will be named "dbltic.adf" and "dblbnd.adf".
 *
 * Returns a valid AVCBinFile handle, or nullptr if the table could
 * not be created.
 *
 * AVCBinClose() will eventually have to be called to release the
 * resources used by the AVCBinFile structure.
 **********************************************************************/
AVCBinFile *AVCBinWriteCreateTable(const char *pszInfoPath,
                                   const char *pszCoverName,
                                   AVCTableDef *psSrcTableDef,
                                   AVCCoverType eCoverType,
                                   int nPrecision, AVCDBCSInfo *psDBCSInfo)
{
    AVCBinFile   *psFile;
    AVCRawBinFile *hRawBinFile;
    AVCTableDef  *psTableDef = nullptr;
    char         *pszFname = nullptr, szInfoFile[8]="";
    int          i, nTableIndex = 0;
    size_t       nFnameLen;

    if (eCoverType == AVCCoverPC || eCoverType == AVCCoverPC2)
        return _AVCBinWriteCreateDBFTable(pszInfoPath, pszCoverName,
                                          psSrcTableDef, eCoverType,
                                          nPrecision, psDBCSInfo);

    /*-----------------------------------------------------------------
     * Make sure precision value is valid (AVC_DEFAULT_PREC is NOT valid)
     *----------------------------------------------------------------*/
    if (nPrecision!=AVC_SINGLE_PREC && nPrecision!=AVC_DOUBLE_PREC)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "AVCBinWriteCreateTable(): Invalid precision parameter "
                 "(value must be AVC_SINGLE_PREC or AVC_DOUBLE_PREC)");
        return nullptr;
    }

    /* Alloc a buffer big enough for the longest possible filename...
     */
    nFnameLen = strlen(pszInfoPath)+81;
    pszFname = (char*)CPLMalloc(nFnameLen);


    /*-----------------------------------------------------------------
     * Alloc and init the AVCBinFile struct.
     *----------------------------------------------------------------*/
    psFile = (AVCBinFile*)CPLCalloc(1, sizeof(AVCBinFile));

    psFile->eFileType = AVCFileTABLE;
    /* Precision is not important for tables */
    psFile->nPrecision = nPrecision;
    psFile->eCoverType = eCoverType;

    psFile->hdr.psTableDef = psTableDef = _AVCDupTableDef(psSrcTableDef);

    /*-----------------------------------------------------------------
     * Add a record for this table in the "arc.dir"
     * Note: there could be a problem if 2 processes try to add an entry
     * at the exact same time... does Arc/Info do any locking on that file???
     *----------------------------------------------------------------*/
    snprintf(pszFname, nFnameLen, "%sarc.dir", pszInfoPath);

    nTableIndex = _AVCBinWriteCreateArcDirEntry(pszFname, psTableDef,
                                                psDBCSInfo);

    if (nTableIndex < 0)
    {
        /* Failed to add arc.dir entry... just return nullptr since an error
         * message has already been issued by _AVCBinWriteCreateArcDirEntry()
         */
        _AVCDestroyTableDef(psTableDef);
        CPLFree(psFile);
        CPLFree(pszFname);
        return nullptr;
    }

    snprintf(szInfoFile, sizeof(szInfoFile), "arc%4.4d", nTableIndex);

    /*-----------------------------------------------------------------
     * Create the "arc####.nit" with the attribute definitions.
     *----------------------------------------------------------------*/
    snprintf(pszFname, nFnameLen, "%s%s.nit", pszInfoPath, szInfoFile);

    hRawBinFile = AVCRawBinOpen(pszFname, "w",
                                AVC_COVER_BYTE_ORDER(AVCCoverV7),
                                psDBCSInfo);

    if (hRawBinFile == nullptr)
    {
        /* Failed to open file... just return nullptr since an error message
         * has already been issued by AVCRawBinOpen()
         */
        _AVCDestroyTableDef(psTableDef);
        CPLFree(psFile);
        CPLFree(pszFname);
        return nullptr;
    }

    for(i=0; i<psTableDef->numFields; i++)
    {
        _AVCBinWriteArcNit(hRawBinFile, &(psTableDef->pasFieldDef[i]));
    }

    AVCRawBinClose(hRawBinFile);
    hRawBinFile = nullptr;

    /*-----------------------------------------------------------------
     * The location of the data file depends on the external flag.
     *----------------------------------------------------------------*/
    if (EQUAL(psTableDef->szExternal, "  "))
    {
        /*-------------------------------------------------------------
         * Internal table: data goes directly in "arc####.dat"
         *------------------------------------------------------------*/
        psTableDef->szDataFile[0] = '\0';
        snprintf(pszFname, nFnameLen, "%s%s.dat", pszInfoPath, szInfoFile);
        psFile->pszFilename = CPLStrdup(pszFname);
    }
    else
    {
        /*-------------------------------------------------------------
         * External table: data stored in the coverage directory, and
         * the path to the data file is written to "arc####.dat"
         *... start by extracting the info to build the data file name...
         *------------------------------------------------------------*/
        char szCoverName[40]="", szExt[4]="", szSubclass[40]="", *pszPtr;
        int nLen;
        VSILFILE *fpOut;

        nLen = (int)strlen(psTableDef->szTableName);
        CPLAssert(nLen <= 32);
        if (nLen > 32) return nullptr;
        pszPtr = psTableDef->szTableName;

        for(i=0; *pszPtr!='\0' && *pszPtr!='.' && *pszPtr!=' ';  i++, pszPtr++)
        {
            szCoverName[i] = (char) tolower(*pszPtr);
        }
        szCoverName[i] = '\0';

        if (*pszPtr == '.')
            pszPtr++;

        for(i=0; i<3 && *pszPtr!='\0' && *pszPtr!=' ';  i++, pszPtr++)
        {
            szExt[i] = (char) tolower(*pszPtr);
        }
        szExt[i] = '\0';

        for(i=0; *pszPtr!='\0' && *pszPtr!=' ';  i++, pszPtr++)
        {
            szSubclass[i] = (char) tolower(*pszPtr);
        }
        szSubclass[i] = '\0';

        /*-------------------------------------------------------------
         * ... and build the data file name based on what we extracted
         *------------------------------------------------------------*/
        if (strlen(szSubclass) == 0)
        {
            if (nPrecision == AVC_DOUBLE_PREC &&
                (EQUAL(szExt, "TIC") || EQUAL(szExt, "BND")) )
            {
                /* "../<covername>/dbl<ext>.adf" */
                snprintf(psTableDef->szDataFile, sizeof(psTableDef->szDataFile),
                        "../%s/dbl%s.adf", szCoverName, szExt);
            }
            else
            {
                /* "../<covername>/<ext>.adf" */
                snprintf(psTableDef->szDataFile, sizeof(psTableDef->szDataFile),
                        "../%s/%s.adf", szCoverName, szExt);
            }
        }
        else
        {
            /* "../<covername>/<subclass>.<ext>" */
            snprintf(psTableDef->szDataFile, sizeof(psTableDef->szDataFile),
                    "../%s/%s.%s", szCoverName, szSubclass, szExt);
        }

        /*-------------------------------------------------------------
         * Write it to the arc####.dat
         * Note that the path that is written in the arc####.dat contains
         * '/' as a directory delimiter, even on Windows systems.
         *------------------------------------------------------------*/
        snprintf(pszFname, nFnameLen, "%s%s.dat", pszInfoPath, szInfoFile);
        fpOut = VSIFOpenL(pszFname, "wt");
        if (fpOut)
        {
            CPL_IGNORE_RET_VAL_INT(VSIFPrintfL(fpOut, "%-80.80s", psTableDef->szDataFile));
            VSIFCloseL(fpOut);
        }
        else
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Failed creating file %s.", pszFname);
            CPLFree(pszFname);
            _AVCDestroyTableDef(psTableDef);
            CPLFree(psFile);
            return nullptr;
        }

        snprintf(pszFname, nFnameLen, "%s%s",
                pszInfoPath, psTableDef->szDataFile);
        psFile->pszFilename = CPLStrdup(pszFname);

#ifdef WIN32
        /*-------------------------------------------------------------
         * On a Windows system, we have to change the '/' to '\\' in the
         * data file path.
         *------------------------------------------------------------*/
        for(i=0; psFile->pszFilename[i] != '\0'; i++)
            if (psFile->pszFilename[i] == '/')
                psFile->pszFilename[i] = '\\';
#endif /* WIN32 */

    }/* if "XX" */

    /*-----------------------------------------------------------------
     * OK, now we're ready to create the actual data file.
     *----------------------------------------------------------------*/
    AVCAdjustCaseSensitiveFilename(psFile->pszFilename);
    psFile->psRawBinFile = AVCRawBinOpen(psFile->pszFilename, "w",
                                         AVC_COVER_BYTE_ORDER(AVCCoverV7),
                                         psDBCSInfo);

    if (psFile->psRawBinFile == nullptr)
    {
        /* Failed to open file... just return nullptr since an error message
         * has already been issued by AVCRawBinOpen()
         */
        CPLFree(pszFname);
        CPLFree(psFile->pszFilename);
        _AVCDestroyTableDef(psTableDef);
        CPLFree(psFile);
        return nullptr;
    }

    CPLFree(pszFname);

    return psFile;
}


/**********************************************************************
 *                          _AVCBinWriteCreateDBFTable()
 *
 * Create a table (DBF file) in a PC Coverage and write the attribute defns to
 * the file.  The file will then be ready to write records to.
 *
 * In PC Coverages, only the following tables appear to be supported:
 *    - TEST.AAT -> AAT.DBF
 *    - TEST.PAT -> PAT.DBF
 *    - TEST.BND -> BND.DBF
 *    - TEST.TIC -> TIC.DBF
 *
 * However, this function will not fail if it is passed a table name not
 * supported by PC Arc/Info.
 * e.g. TEST.PATCOUNTY would be written as PATCOUNTY.DBF even if PC Arc/Info
 * would probably not recognize that name.
 *
 * Returns a valid AVCBinFile handle, or nullptr if the table could
 * not be created.
 *
 * AVCBinClose() will eventually have to be called to release the
 * resources used by the AVCBinFile structure.
 **********************************************************************/
AVCBinFile *_AVCBinWriteCreateDBFTable(const char *pszPath,
                                       const char *pszCoverName,
                                       AVCTableDef *psSrcTableDef,
                                       AVCCoverType eCoverType,
                                       int nPrecision,
                                       CPL_UNUSED AVCDBCSInfo *psDBCSInfo)
{
    AVCBinFile    *psFile;
    AVCTableDef   *psTableDef = nullptr;
    AVCFieldInfo  *pasDef;
    char          *pszDBFBasename, szFieldName[12];
    int           i, j, nType;

    /*-----------------------------------------------------------------
     * Alloc and init the AVCBinFile struct.
     *----------------------------------------------------------------*/
    psFile = (AVCBinFile*)CPLCalloc(1, sizeof(AVCBinFile));

    psFile->eFileType = AVCFileTABLE;
    /* Precision is not important for tables */
    psFile->nPrecision = nPrecision;
    psFile->eCoverType = eCoverType;

    psFile->hdr.psTableDef = psTableDef = _AVCDupTableDef(psSrcTableDef);

    /* nCurDBFRecord is used to keep track of the 0-based index of the
     * last record we read from the DBF file... this is to emulate
     * sequential access which is assumed by the rest of the lib.
     * Since the first record (record 0) has not been written yet, then
     * we init the index at -1.
     */
    psFile->nCurDBFRecord = -1;

    /*-----------------------------------------------------------------
     * Establish name of file to create.
     *----------------------------------------------------------------*/
    psFile->pszFilename = (char*)CPLCalloc(strlen(psSrcTableDef->szTableName)+
                                           strlen(pszPath)+10, sizeof(char));

    if (EQUALN(psSrcTableDef->szTableName, pszCoverName, strlen(pszCoverName))
        && psSrcTableDef->szTableName[strlen(pszCoverName)] == '.')
    {
        pszDBFBasename = psSrcTableDef->szTableName + strlen(pszCoverName)+1;
    }
    else
    {
        pszDBFBasename = psSrcTableDef->szTableName;
    }

    strcpy(psFile->pszFilename, pszPath);

    for(i=(int)strlen(psFile->pszFilename); *pszDBFBasename; i++, pszDBFBasename++)
    {
        psFile->pszFilename[i] = (char) tolower(*pszDBFBasename);
    }

    strcat(psFile->pszFilename, ".dbf");

    /*-----------------------------------------------------------------
     * OK, let's try to create the DBF file.
     *----------------------------------------------------------------*/
    AVCAdjustCaseSensitiveFilename(psFile->pszFilename);
    psFile->hDBFFile = DBFCreate(psFile->pszFilename);

    if (psFile->hDBFFile == nullptr)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Failed creating file %s.", psFile->pszFilename);
        CPLFree(psFile->pszFilename);
        _AVCDestroyTableDef(psTableDef);
        CPLFree(psFile);
        return nullptr;
    }

    /*-----------------------------------------------------------------
     * Create fields.
     *----------------------------------------------------------------*/
    pasDef = psTableDef->pasFieldDef;
    for(i=0; i<psTableDef->numFields; i++)
    {
        nType = pasDef[i].nType1*10;

        /*-------------------------------------------------------------
         * Special characters '#' and '-' in field names have to be replaced
         * with '_'.  PC Field names are limited to 10 chars.
         *------------------------------------------------------------*/
        strncpy(szFieldName, pasDef[i].szName, 10);
        szFieldName[10] = '\0';
        for(j=0; szFieldName[j]; j++)
        {
            if (szFieldName[j] == '#' || szFieldName[j] == '-')
                szFieldName[j] = '_';
        }

        if (nType ==  AVC_FT_DATE || nType == AVC_FT_CHAR)
        {
            /*---------------------------------------------------------
             * Values stored as strings
             *--------------------------------------------------------*/
            DBFAddField(psFile->hDBFFile, szFieldName, FTString,
                        pasDef[i].nSize, 0);
        }
        else if (nType == AVC_FT_FIXINT || nType == AVC_FT_FIXNUM)
        {
            /*---------------------------------------------------------
             * Numerics (internally stored as strings)
             *--------------------------------------------------------*/
            DBFAddField(psFile->hDBFFile, szFieldName, FTDouble,
                        pasDef[i].nSize, pasDef[i].nFmtPrec);
        }
        else if (nType == AVC_FT_BININT)
        {
            /*---------------------------------------------------------
             * Integers (16 and 32 bits)
             *--------------------------------------------------------*/
            DBFAddField(psFile->hDBFFile, szFieldName, FTInteger, 11, 0);
        }
        else if (nType == AVC_FT_BINFLOAT)
        {
            /*---------------------------------------------------------
             * Single + Double precision floats
             * Set them as width=13, prec=6 in the header like PC/Arc does
             *--------------------------------------------------------*/
            DBFAddField(psFile->hDBFFile, szFieldName, FTDouble,
                        13, 6);
        }
        else
        {
            /*---------------------------------------------------------
             * Hummm... unsupported field type...
             *--------------------------------------------------------*/
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported field type: (field=%s, type=%d, size=%d)",
                     szFieldName, nType, pasDef[i].nSize);
            _AVCBinWriteCloseTable(psFile);
            return nullptr;
        }
    }

    return psFile;
}


/**********************************************************************
 *                          _AVCBinWriteCloseTable()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinWriteClose() instead)
 *
 * Close an info table opened for writing, and release all memory
 * (object struct., buffers, etc.) associated with this file.
 **********************************************************************/
static void    _AVCBinWriteCloseTable(AVCBinFile *psFile)
{
    if (psFile->eFileType != AVCFileTABLE)
        return;

    /*-----------------------------------------------------------------
     * Close the data file
     *----------------------------------------------------------------*/
    if (psFile->hDBFFile)
    {
        /*-------------------------------------------------------------
         * The case of DBF files is simple!
         *------------------------------------------------------------*/
        DBFClose(psFile->hDBFFile);
        psFile->hDBFFile = nullptr;
    }
    else if (psFile->psRawBinFile)
    {
        /*-------------------------------------------------------------
         * __TODO__ make sure ARC.DIR entry contains accurate info about the
         * number of records written, etc.
         *------------------------------------------------------------*/
        AVCRawBinClose(psFile->psRawBinFile);
        psFile->psRawBinFile = nullptr;
    }

    /*-----------------------------------------------------------------
     * Release other memory
     *----------------------------------------------------------------*/
    _AVCDestroyTableDef(psFile->hdr.psTableDef);

    CPLFree(psFile->pszFilename);

    CPLFree(psFile);
}


/**********************************************************************
 *                          _AVCBinWriteTableRec()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinWriteTableRec() instead)
 *
 * Write a table data record at the current position in file.
 *
 * The contents of the pasDef and pasFields structures is assumed to
 * be valid... this function performs no validation on the consistency
 * of what it is given as input.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
static
int _AVCBinWriteTableRec(AVCRawBinFile *psFile, int nFields,
                         AVCFieldInfo *pasDef, AVCField *pasFields,
                         int nRecordSize, const char *pszFname)
{
    int i, nType, nBytesWritten=0;

    if (psFile == nullptr)
        return -1;

    for(i=0; i<nFields; i++)
    {
        if (CPLGetLastErrorNo() != 0)
            return -1;

        nType = pasDef[i].nType1*10;

        if (nType ==  AVC_FT_DATE || nType == AVC_FT_CHAR ||
            nType == AVC_FT_FIXINT || nType == AVC_FT_FIXNUM)
        {
            /*---------------------------------------------------------
             * Values stored as strings (MUST be padded with spaces)
             *--------------------------------------------------------*/
            AVCRawBinWritePaddedString(psFile, pasDef[i].nSize,
                                       pasFields[i].pszStr);
        }
        else if (nType == AVC_FT_BININT && pasDef[i].nSize == 4)
        {
            /*---------------------------------------------------------
             * 32 bit binary integers
             *--------------------------------------------------------*/
            AVCRawBinWriteInt32(psFile, pasFields[i].nInt32);
        }
        else if (nType == AVC_FT_BININT && pasDef[i].nSize == 2)
        {
            /*---------------------------------------------------------
             * 16 bit binary integers
             *--------------------------------------------------------*/
            AVCRawBinWriteInt16(psFile, pasFields[i].nInt16);
        }
        else if (nType == AVC_FT_BINFLOAT && pasDef[i].nSize == 4)
        {
            /*---------------------------------------------------------
             * Single precision floats
             *--------------------------------------------------------*/
            AVCRawBinWriteFloat(psFile, pasFields[i].fFloat);
        }
        else if (nType == AVC_FT_BINFLOAT && pasDef[i].nSize == 8)
        {
            /*---------------------------------------------------------
             * Double precision floats
             *--------------------------------------------------------*/
            AVCRawBinWriteDouble(psFile, pasFields[i].dDouble);
        }
        else
        {
            /*---------------------------------------------------------
             * Hummm... unsupported field type...
             *--------------------------------------------------------*/
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported field type in %s: (type=%d, size=%d)",
                     pszFname, nType, pasDef[i].nSize);
            return -1;
        }

        nBytesWritten += pasDef[i].nSize;
    }

    /*-----------------------------------------------------------------
     * Record size is rounded to a multiple of 2 bytes.
     * Check the number of bytes written, and pad with zeros if
     * necessary.
     *----------------------------------------------------------------*/
    nRecordSize = ((nRecordSize+1)/2)*2;
    if (nBytesWritten < nRecordSize)
        AVCRawBinWriteZeros(psFile, nRecordSize - nBytesWritten);

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                          _AVCBinWriteDBFTableRec()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinWriteTableRec() instead)
 *
 * Write a table data record at the current position in DBF file.
 *
 * The contents of the pasDef and pasFields structures is assumed to
 * be valid... this function performs no validation on the consistency
 * of what it is given as input.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
static
int _AVCBinWriteDBFTableRec(DBFHandle hDBFFile, int nFields,
                            AVCFieldInfo *pasDef, AVCField *pasFields,
                            int *nCurDBFRecord, const char *pszFname)
{
    int i, nType, nStatus = FALSE;

    if (hDBFFile == nullptr)
        return -1;

    (*nCurDBFRecord)++;

    for(i=0; i<nFields; i++)
    {
        if (CPLGetLastErrorNo() != 0)
            return -1;

        nType = pasDef[i].nType1*10;

        if (nType ==  AVC_FT_DATE || nType == AVC_FT_CHAR)
        {
            /*---------------------------------------------------------
             * Values stored as strings
             *--------------------------------------------------------*/
            nStatus = DBFWriteStringAttribute(hDBFFile, *nCurDBFRecord, i,
                                              (char *)pasFields[i].pszStr);
        }
        else if (nType == AVC_FT_FIXINT || nType == AVC_FT_FIXNUM)
        {
            /*---------------------------------------------------------
             * Numbers stored as strings
             *--------------------------------------------------------*/
            nStatus = DBFWriteAttributeDirectly(hDBFFile, *nCurDBFRecord, i,
                                                pasFields[i].pszStr);
        }
        else if (nType == AVC_FT_BININT && pasDef[i].nSize == 4)
        {
            /*---------------------------------------------------------
             * 32 bit binary integers
             *--------------------------------------------------------*/
            nStatus = DBFWriteIntegerAttribute(hDBFFile, *nCurDBFRecord, i,
                                               pasFields[i].nInt32);
        }
        else if (nType == AVC_FT_BININT && pasDef[i].nSize == 2)
        {
            /*---------------------------------------------------------
             * 16 bit binary integers
             *--------------------------------------------------------*/
            nStatus = DBFWriteIntegerAttribute(hDBFFile, *nCurDBFRecord, i,
                                               pasFields[i].nInt16);
        }
        else if (nType == AVC_FT_BINFLOAT)
        {
            /*---------------------------------------------------------
             * Single+double precision floats
             *--------------------------------------------------------*/
            char szBuf[32] = "";
            int nLen;

            if (pasDef[i].nSize == 4)
                nLen = AVCPrintRealValue(szBuf, sizeof(szBuf), AVC_FORMAT_DBF_FLOAT,
                                         AVCFileTABLE, pasFields[i].fFloat);
            else
                nLen = AVCPrintRealValue(szBuf, sizeof(szBuf), AVC_FORMAT_DBF_FLOAT,
                                         AVCFileTABLE, pasFields[i].dDouble);

            szBuf[nLen] = '\0';

            nStatus = DBFWriteAttributeDirectly(hDBFFile, *nCurDBFRecord, i,
                                                szBuf);
        }
        else
        {
            /*---------------------------------------------------------
             * Hummm... unsupported field type...
             *--------------------------------------------------------*/
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported field type in %s: (type=%d, size=%d)",
                     pszFname, nType, pasDef[i].nSize);
            return -1;
        }

        if (nStatus != TRUE)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed writing table field %d to record %d in %s",
                     i, *nCurDBFRecord, pszFname);
            return -1;
        }

    }

    return 0;
}

/**********************************************************************
 *                          AVCBinWriteTableRec()
 *
 * Write a table data record at the current position in file.
 *
 * The contents of the pasDef and pasFields structures is assumed to
 * be valid... this function performs no validation on the consistency
 * of what it is given as input.
 *
 * Returns 0 on success or -1 on error.
 *
 * If a problem happens, then CPLError() will be called by the lower-level
 * functions and CPLGetLastErrorNo() can be used to find out what happened.
 **********************************************************************/
int AVCBinWriteTableRec(AVCBinFile *psFile, AVCField *pasFields)
{
    if (psFile->eFileType != AVCFileTABLE||
        psFile->hdr.psTableDef->numRecords == 0)
        return -1;

    if (psFile->eCoverType == AVCCoverPC || psFile->eCoverType == AVCCoverPC2)
        return _AVCBinWriteDBFTableRec(psFile->hDBFFile,
                                       psFile->hdr.psTableDef->numFields,
                                       psFile->hdr.psTableDef->pasFieldDef,
                                       pasFields,
                                       &(psFile->nCurDBFRecord),
                                       psFile->pszFilename);
    else
        return _AVCBinWriteTableRec(psFile->psRawBinFile,
                                    psFile->hdr.psTableDef->numFields,
                                    psFile->hdr.psTableDef->pasFieldDef,
                                    pasFields,
                                    psFile->hdr.psTableDef->nRecSize,
                                    psFile->pszFilename);
}
