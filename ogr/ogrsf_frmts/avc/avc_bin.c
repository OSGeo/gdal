/**********************************************************************
 * $Id: avc_bin.c,v 1.30 2008/07/23 20:51:38 dmorissette Exp $
 *
 * Name:     avc_bin.c
 * Project:  Arc/Info vector coverage (AVC)  BIN->E00 conversion library
 * Language: ANSI C
 * Purpose:  Binary files access functions.
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
 * $Log: avc_bin.c,v $
 * Revision 1.30  2008/07/23 20:51:38  dmorissette
 * Fixed GCC 4.1.x compile warnings related to use of char vs unsigned char
 * (GDAL/OGR ticket http://trac.osgeo.org/gdal/ticket/2495)
 *
 * Revision 1.29  2006/08/17 18:56:42  dmorissette
 * Support for reading standalone info tables (just tables, no coverage
 * data) by pointing AVCE00ReadOpen() to the info directory (bug 1549).
 *
 * Revision 1.28  2006/06/14 16:31:28  daniel
 * Added support for AVCCoverPC2 type (bug 1491)
 *
 * Revision 1.27  2005/06/03 03:49:58  daniel
 * Update email address, website url, and copyright dates
 *
 * Revision 1.26  2004/02/28 06:35:49  warmerda
 * Fixed AVCBinReadObject() index support to use 'x' or 'X' for index
 * depending on the case of the original name.
 * Fixed so that PC Arc/Info coverages with the extra 256 byte header work
 * properly when using indexes to read them.
 *   http://bugzilla.remotesensing.org/show_bug.cgi?id=493
 *
 * Revision 1.25  2004/02/11 05:49:44  daniel
 * Added support for deleted flag in arc.dir (bug 2332)
 *
 * Revision 1.24  2002/08/27 15:26:06  daniel
 * Removed C++ style comments for IRIX compiler (GDAL bug 192)
 *
 * Revision 1.23  2002/04/16 20:04:24  daniel
 * Use record size while reading ARC, PAL, CNT to skip junk bytes. (bug940)
 *
 * Revision 1.22  2002/03/18 19:03:37  daniel
 * Fixed AVCBinReadObject() for PAL objects (bug 848)
 *
 * Revision 1.21  2002/02/14 22:54:13  warmerda
 * added polygon and table support for random reading
 *
 * Revision 1.20  2002/02/13 20:35:24  warmerda
 * added AVCBinReadObject
 *
 * Revision 1.19  2001/11/25 22:01:23  daniel
 * Fixed order of args to AVCRawBinFSeek() in _AVCBinReadNextTableRec()
 *
 * Revision 1.18  2000/10/16 16:16:20  daniel
 * Accept TXT files in AVCCoverWeird that use both PC or V7 TXT structure
 *
 * Revision 1.17  2000/09/26 20:21:04  daniel
 * Added AVCCoverPC write
 *
 * Revision 1.16  2000/09/22 19:45:20  daniel
 * Switch to MIT-style license
 *
 * Revision 1.15  2000/09/20 15:09:34  daniel
 * Check for DAT/NIT fnames sometimes truncated to 8 chars in weird coverages
 *
 * Revision 1.14  2000/06/05 21:38:53  daniel
 * Handle precision field > 1000 in cover file header as meaning double prec.
 *
 * Revision 1.13  2000/05/29 15:31:30  daniel
 * Added Japanese DBCS support
 *
 * Revision 1.12  2000/02/14 17:22:36  daniel
 * Check file signature (9993 or 9994) when reading header.
 *
 * Revision 1.11  2000/02/02 04:24:52  daniel
 * Support double precision "weird" coverages
 *
 * Revision 1.10  2000/01/10 02:54:10  daniel
 * Added read support for "weird" coverages
 *
 * Revision 1.9  2000/01/07 07:11:51  daniel
 * Added support for reading PC Coverage TXT files
 *
 * Revision 1.8  1999/12/24 07:38:10  daniel
 * Added missing DBFClose()
 *
 * Revision 1.7  1999/12/24 07:18:34  daniel
 * Added PC Arc/Info coverages support
 *
 * Revision 1.6  1999/08/23 18:17:16  daniel
 * Modified AVCBinReadListTables() to return INFO fnames for DeleteCoverage()
 *
 * Revision 1.5  1999/05/11 01:49:08  daniel
 * Simple changes required by addition of coverage write support
 *
 * Revision 1.4  1999/03/03 18:42:53  daniel
 * Fixed problem with INFO table headers (arc.dir) that sometimes contain an
 * invalid number of records.
 *
 * Revision 1.3  1999/02/25 17:01:53  daniel
 * Added support for 16 bit integers in INFO tables (type=50, size=2)
 *
 * Revision 1.2  1999/02/25 03:41:28  daniel
 * Added TXT, TX6/TX7, RXP and RPL support
 *
 * Revision 1.1  1999/01/29 16:28:52  daniel
 * Initial revision
 *
 **********************************************************************/

#include "avc.h"

#include <ctype.h>      /* for isspace() */

/*=====================================================================
 * Prototypes for some static functions
 *====================================================================*/

static AVCBinFile *_AVCBinReadOpenTable(const char *pszInfoPath,
                                        const char *pszTableName,
                                        AVCCoverType eCoverType,
                                        AVCDBCSInfo *psDBCSInfo);
static AVCBinFile *_AVCBinReadOpenDBFTable(const char *pszInfoPath,
                                           const char *pszTableName);
static AVCBinFile *_AVCBinReadOpenPrj(const char *pszPath,const char *pszName);

static int _AVCBinReadNextTableRec(AVCRawBinFile *psFile, int nFields,
                                   AVCFieldInfo *pasDef, AVCField *pasFields,
                                   int nRecordSize);
static int _AVCBinReadNextDBFTableRec(DBFHandle hDBFFile, int *piRecordIndex, 
                                      int nFields, AVCFieldInfo *pasDef,
                                      AVCField *pasFields);

/*=====================================================================
 * Stuff related to reading the binary coverage files
 *====================================================================*/

/**********************************************************************
 *                          AVCBinReadOpen()
 *
 * Open a coverage file for reading, read the file header if applicable,
 * and initialize a temp. storage structure to be ready to read objects
 * from the file.
 *
 * pszPath is the coverage (or info directory) path, terminated by
 *         a '/' or a '\\'
 * pszName is the name of the file to open relative to this directory.
 *
 * Note: For most file types except tables, passing pszPath="" and 
 * including the coverage path as part of pszName instead would work.
 *
 * Returns a valid AVCBinFile handle, or NULL if the file could
 * not be opened.
 *
 * AVCBinClose() will eventually have to be called to release the 
 * resources used by the AVCBinFile structure.
 **********************************************************************/
AVCBinFile *AVCBinReadOpen(const char *pszPath, const char *pszName, 
                           AVCCoverType eCoverType, AVCFileType eFileType,
                           AVCDBCSInfo *psDBCSInfo)
{
    AVCBinFile   *psFile;

    /*-----------------------------------------------------------------
     * The case of INFO tables is a bit more complicated...
     * pass the control to a separate function.
     *----------------------------------------------------------------*/
    if (eFileType == AVCFileTABLE)
    {
        if (eCoverType == AVCCoverPC || eCoverType == AVCCoverPC2)
            return _AVCBinReadOpenDBFTable(pszPath, pszName);
        else
            return _AVCBinReadOpenTable(pszPath, pszName, 
                                        eCoverType, psDBCSInfo);
    }

    /*-----------------------------------------------------------------
     * PRJ files are text files... we won't use the AVCRawBin*()
     * functions for them...
     *----------------------------------------------------------------*/
    if (eFileType == AVCFilePRJ)
    {
        return _AVCBinReadOpenPrj(pszPath, pszName);
    }

    /*-----------------------------------------------------------------
     * All other file types share a very similar opening method.
     *----------------------------------------------------------------*/
    psFile = (AVCBinFile*)CPLCalloc(1, sizeof(AVCBinFile));

    psFile->eFileType = eFileType;
    psFile->eCoverType = eCoverType;

    psFile->pszFilename = (char*)CPLMalloc((strlen(pszPath)+strlen(pszName)+1)*
                                           sizeof(char));
    sprintf(psFile->pszFilename, "%s%s", pszPath, pszName);

    AVCAdjustCaseSensitiveFilename(psFile->pszFilename);

    psFile->psRawBinFile = AVCRawBinOpen(psFile->pszFilename, "r",
                                         AVC_COVER_BYTE_ORDER(eCoverType),
                                         psDBCSInfo);

    if (psFile->psRawBinFile == NULL)
    {
        /* Failed to open file... just return NULL since an error message
         * has already been issued by AVCRawBinOpen()
         */
        CPLFree(psFile->pszFilename);
        CPLFree(psFile);
        return NULL;
    }

    /*-----------------------------------------------------------------
     * Read the header, and set the precision field if applicable
     *----------------------------------------------------------------*/
    if (AVCBinReadRewind(psFile) != 0)
    {
        CPLFree(psFile->pszFilename);
        CPLFree(psFile);
        return NULL;
    }

    /*-----------------------------------------------------------------
     * Allocate a temp. structure to use to read objects from the file
     * (Using Calloc() will automatically initialize the struct contents
     *  to NULL... this is very important for ARCs and PALs)
     *----------------------------------------------------------------*/
    if (psFile->eFileType == AVCFileARC)
    {
        psFile->cur.psArc = (AVCArc*)CPLCalloc(1, sizeof(AVCArc));
    }
    else if (psFile->eFileType == AVCFilePAL ||
             psFile->eFileType == AVCFileRPL )
    {
        psFile->cur.psPal = (AVCPal*)CPLCalloc(1, sizeof(AVCPal));
    }
    else if (psFile->eFileType == AVCFileCNT)
    {
        psFile->cur.psCnt = (AVCCnt*)CPLCalloc(1, sizeof(AVCCnt));
    }
    else if (psFile->eFileType == AVCFileLAB)
    {
        psFile->cur.psLab = (AVCLab*)CPLCalloc(1, sizeof(AVCLab));
    }
    else if (psFile->eFileType == AVCFileTOL)
    {
        psFile->cur.psTol = (AVCTol*)CPLCalloc(1, sizeof(AVCTol));
    }
    else if (psFile->eFileType == AVCFileTXT ||
             psFile->eFileType == AVCFileTX6)
    {
        psFile->cur.psTxt = (AVCTxt*)CPLCalloc(1, sizeof(AVCTxt));
    }
    else if (psFile->eFileType == AVCFileRXP)
    {
        psFile->cur.psRxp = (AVCRxp*)CPLCalloc(1, sizeof(AVCRxp));
    }
    else
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "%s: Unsupported file type or corrupted file.",
                 psFile->pszFilename);
        CPLFree(psFile->pszFilename);
        CPLFree(psFile);
        psFile = NULL;
    }

    return psFile;
}

/**********************************************************************
 *                          AVCBinReadClose()
 *
 * Close a coverage file, and release all memory (object strcut., buffers,
 * etc.) associated with this file.
 **********************************************************************/
void    AVCBinReadClose(AVCBinFile *psFile)
{
    AVCRawBinClose(psFile->psRawBinFile);
    psFile->psRawBinFile = NULL;

    CPLFree(psFile->pszFilename);
    psFile->pszFilename = NULL;

    if (psFile->hDBFFile)
        DBFClose(psFile->hDBFFile);

    if( psFile->psIndexFile != NULL )
        AVCRawBinClose( psFile->psIndexFile );

    if (psFile->eFileType == AVCFileARC)
    {
        if (psFile->cur.psArc)
            CPLFree(psFile->cur.psArc->pasVertices);
        CPLFree(psFile->cur.psArc);
    }
    else if (psFile->eFileType == AVCFilePAL ||
             psFile->eFileType == AVCFileRPL )
    {
        if (psFile->cur.psPal)
            CPLFree(psFile->cur.psPal->pasArcs);
        CPLFree(psFile->cur.psPal);
    }
    else if (psFile->eFileType == AVCFileCNT)
    {
        if (psFile->cur.psCnt)
            CPLFree(psFile->cur.psCnt->panLabelIds);
        CPLFree(psFile->cur.psCnt);
    }
    else if (psFile->eFileType == AVCFileLAB)
    {
        CPLFree(psFile->cur.psLab);
    }
    else if (psFile->eFileType == AVCFileTOL)
    {
        CPLFree(psFile->cur.psTol);
    }
    else if (psFile->eFileType == AVCFilePRJ)
    {
        CSLDestroy(psFile->cur.papszPrj);
    }
    else if (psFile->eFileType == AVCFileTXT || 
             psFile->eFileType == AVCFileTX6)
    {
        if (psFile->cur.psTxt)
        {
            CPLFree(psFile->cur.psTxt->pasVertices);
            CPLFree(psFile->cur.psTxt->pszText);
        }
        CPLFree(psFile->cur.psTxt);
    }
    else if (psFile->eFileType == AVCFileRXP)
    {
        CPLFree(psFile->cur.psRxp);
    }
    else if (psFile->eFileType == AVCFileTABLE)
    {
        _AVCDestroyTableFields(psFile->hdr.psTableDef, psFile->cur.pasFields);
        _AVCDestroyTableDef(psFile->hdr.psTableDef);
    }
    else
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "Unsupported file type or invalid file handle!");
    }

    CPLFree(psFile);
}

/**********************************************************************
 *                          _AVCBinReadHeader()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinReadRewind() instead)
 *
 * Read the first 100 bytes header of the file and fill the AVCHeader
 * structure.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
int _AVCBinReadHeader(AVCRawBinFile *psFile, AVCBinHeader *psHeader,
                      AVCCoverType eCoverType)
{
    int nStatus = 0;

    /*-----------------------------------------------------------------
     * For AVCCoverPC coverages (files without hte .adf extension), 
     * there is a first 256 bytes header that we just skip and that 
     * precedes the 100 bytes header block.
     *
     * In AVCCoverV7, we only have the 100 bytes header.
     *----------------------------------------------------------------*/
    if (eCoverType == AVCCoverPC)
        AVCRawBinFSeek(psFile, 256, SEEK_SET);
    else
        AVCRawBinFSeek(psFile, 0, SEEK_SET);

    psHeader->nSignature = AVCRawBinReadInt32(psFile);

    if (AVCRawBinEOF(psFile))
        nStatus = -1;

    psHeader->nPrecision = AVCRawBinReadInt32(psFile);
    psHeader->nRecordSize= AVCRawBinReadInt32(psFile);

    /* Jump to 24th byte in header */
    AVCRawBinFSeek(psFile, 12, SEEK_CUR);
    psHeader->nLength    = AVCRawBinReadInt32(psFile);

    /*-----------------------------------------------------------------
     * File length, in words (16 bits)... pass the info to the RawBinFile
     * to prevent it from trying to read junk bytes at the end of files...
     * this problem happens specially with PC Arc/Info files.
     *----------------------------------------------------------------*/
    if (eCoverType == AVCCoverPC)
        AVCRawBinSetFileDataSize(psFile, psHeader->nLength*2 + 256);
    else
        AVCRawBinSetFileDataSize(psFile, psHeader->nLength*2 );

    /* Move the pointer at the end of the 100 bytes header
     */
    AVCRawBinFSeek(psFile, 72, SEEK_CUR);

    return nStatus;
}


/**********************************************************************
 *                          AVCBinReadRewind()
 *
 * Rewind the read pointer, and read/skip the header if necessary so
 * that we are ready to read the data objects from the file after
 * this call.
 *
 * Returns 0 on success, -1 on error, and -2 if file has an invalid
 * signature and is possibly corrupted.
 **********************************************************************/
int AVCBinReadRewind(AVCBinFile *psFile)
{
    AVCBinHeader sHeader;
    int          nStatus=0;

    /*-----------------------------------------------------------------
     * For AVCCoverPC coverages, there is a first 256 bytes header
     * that we just skip and that precedes the 100 bytes header block.
     *
     * In AVCCoverV7, AVCCoverPC2 and AVCCoverWeird, we only find the 
     * 100 bytes header.
     *
     * Note: it is the call to _AVCBinReadHeader() that takes care
     * of skipping the first 256 bytes header if necessary.
     *----------------------------------------------------------------*/

    AVCRawBinFSeek(psFile->psRawBinFile, 0, SEEK_SET);

    if ( psFile->eFileType == AVCFileARC ||
         psFile->eFileType == AVCFilePAL ||
         psFile->eFileType == AVCFileRPL ||
         psFile->eFileType == AVCFileCNT ||
         psFile->eFileType == AVCFileLAB ||
         psFile->eFileType == AVCFileTXT ||
         psFile->eFileType == AVCFileTX6  )
    {   
        nStatus = _AVCBinReadHeader(psFile->psRawBinFile, &sHeader, 
                                    psFile->eCoverType);

        /* Store the precision information inside the file handle.
         *
         * Of course, there had to be an exception...
         * At least PAL and TXT files in PC Arc/Info coverages sometimes 
         * have a negative precision flag even if they contain single 
         * precision data... why is that????  A PC Arc bug?
         *
         * 2000-06-05: Found a double-precision PAL file with a signature
         *             of 1011 (should have been -11).  So we'll assume
         *             that signature > 1000 also means double precision.
         */
        if ((sHeader.nPrecision < 0 || sHeader.nPrecision > 1000) && 
            psFile->eCoverType != AVCCoverPC)
            psFile->nPrecision = AVC_DOUBLE_PREC;
        else
            psFile->nPrecision = AVC_SINGLE_PREC;

        /* Validate the signature value... this will allow us to detect
         * corrupted files or files that do not belong in the coverage.
         */
        if (sHeader.nSignature != 9993 && sHeader.nSignature != 9994)
        {
            CPLError(CE_Warning, CPLE_AssertionFailed,
                     "%s appears to have an invalid file header.",
                     psFile->pszFilename);
            return -2;
        }

        /* In Weird coverages, TXT files can be stored in the PC or the V7
         * format.  Look at the 'precision' field in the header to tell which
         * type we have.
         *   Weird TXT in PC format: nPrecision = 16
         *   Weird TXT in V7 format: nPrecision = +/-67
         * Use AVCFileTXT for PC type, and AVCFileTX6 for V7 type.
         */
        if (psFile->eCoverType == AVCCoverWeird &&
            psFile->eFileType == AVCFileTXT && ABS(sHeader.nPrecision) == 67)
        {
            /* TXT file will be processed as V7 TXT/TX6/TX7 */
            psFile->eFileType = AVCFileTX6;
        }
    }
    else if (psFile->eFileType == AVCFileTOL)
    {
        /*-------------------------------------------------------------
         * For some reason, the tolerance files do not follow the 
         * general rules!
         * Single precision "tol.adf" have no header
         * Double precision "par.adf" have the usual 100 bytes header,
         *  but the 3rd field, which usually defines the precision has
         *  a positive value, even if the file is double precision!
         *
         * Also, we have a problem with PC Arc/Info TOL files since they
         * do not contain the first 256 bytes header either... so we will
         * just assume that double precision TOL files cannot exist in
         * PC Arc/Info coverages... this should be OK.
         *------------------------------------------------------------*/
        int nSignature = 0;
        nSignature = AVCRawBinReadInt32(psFile->psRawBinFile);

        if (nSignature == 9993)
        {
            /* We have a double precision par.adf... read the 100 bytes 
             * header and set the precision information inside the file 
             * handle.
             */
            nStatus = _AVCBinReadHeader(psFile->psRawBinFile, &sHeader, 
                                        psFile->eCoverType);

            psFile->nPrecision = AVC_DOUBLE_PREC;
        }
        else
        {
            /* It's a single precision tol.adf ... just set the 
             * precision field.
             */
            AVCRawBinFSeek(psFile->psRawBinFile, 0, SEEK_SET);
            psFile->nPrecision = AVC_SINGLE_PREC;
        }
    }

    return nStatus;
}

/**********************************************************************
 *                          AVCBinReadObject()
 *
 * Read the object with a particular index.  For fixed length record
 * files we seek directly to the object.  For variable files we try to
 * get the offset from the corresponding index file.  
 *
 * NOTE: Currently only implemented for ARC, PAL and TABLE files.
 *
 * Returns the read object on success or NULL on error.
 **********************************************************************/
void *AVCBinReadObject(AVCBinFile *psFile, int iObjIndex )
{
    int	 bIndexed = FALSE;
    int  nObjectOffset, nRecordSize=0, nRecordStart = 0, nLen;
    char *pszExt = NULL;

    if( iObjIndex < 0 )
        return NULL;

    /*-----------------------------------------------------------------
     * Determine some information from based on the coverage type.    
     *----------------------------------------------------------------*/
    nLen = strlen(psFile->pszFilename);
    if( psFile->eFileType == AVCFileARC &&
        ((nLen>=3 && EQUALN((pszExt=psFile->pszFilename+nLen-3), "arc", 3)) ||
         (nLen>=7 && EQUALN((pszExt=psFile->pszFilename+nLen-7),"arc.adf",7))))
    {
        bIndexed = TRUE;
    }
    else if( psFile->eFileType == AVCFilePAL &&
        ((nLen>=3 && EQUALN((pszExt=psFile->pszFilename+nLen-3), "pal", 3)) ||
         (nLen>=7 && EQUALN((pszExt=psFile->pszFilename+nLen-7),"pal.adf",7))))
    {
        bIndexed = TRUE;
    }
    else if( psFile->eFileType == AVCFileTABLE )
    {
        bIndexed = FALSE;
        nRecordSize = psFile->hdr.psTableDef->nRecSize;
        nRecordStart = 0;
    }
    else
        return NULL;

    /*-----------------------------------------------------------------
     * Ensure the index file is opened if an index file is required.
     *----------------------------------------------------------------*/

    if( bIndexed && psFile->psIndexFile == NULL )
    {
        char chOrig;

        if( pszExt == NULL )
            return NULL;

        chOrig = pszExt[2];
        if( chOrig > 'A' && chOrig < 'Z' )
            pszExt[2] = 'X';
        else
            pszExt[2] = 'x';

        psFile->psIndexFile = 
            AVCRawBinOpen( psFile->pszFilename, "rb", 
                           psFile->psRawBinFile->eByteOrder, 
                           psFile->psRawBinFile->psDBCSInfo);
        pszExt[2] = chOrig;

        if( psFile->psIndexFile == NULL )
            return NULL;
    }

    /*-----------------------------------------------------------------
     * Establish the offset to read the object from.
     *----------------------------------------------------------------*/
    if( bIndexed )
    {
        int nIndexOffset;

        if (psFile->eCoverType == AVCCoverPC)
            nIndexOffset = 356 + (iObjIndex-1)*8;
        else
            nIndexOffset = 100 + (iObjIndex-1)*8;

        AVCRawBinFSeek( psFile->psIndexFile, nIndexOffset, SEEK_SET );
        if( AVCRawBinEOF( psFile->psIndexFile ) )
            return NULL;

        nObjectOffset = AVCRawBinReadInt32( psFile->psIndexFile );
        nObjectOffset *= 2;

        if (psFile->eCoverType == AVCCoverPC)
            nObjectOffset += 256;
    }
    else
        nObjectOffset = nRecordStart + nRecordSize * (iObjIndex-1);

    /*-----------------------------------------------------------------
     * Seek to the start of the object in the data file.
     *----------------------------------------------------------------*/
    AVCRawBinFSeek( psFile->psRawBinFile, nObjectOffset, SEEK_SET );
    if( AVCRawBinEOF( psFile->psRawBinFile ) )
        return NULL;

    /*-----------------------------------------------------------------
     * Read and return the object.
     *----------------------------------------------------------------*/
    return AVCBinReadNextObject( psFile );
}


/**********************************************************************
 *                          AVCBinReadNextObject()
 *
 * Read the next structure from the file.  This function is just a generic
 * cover on top of the AVCBinReadNextArc/Lab/Pal/Cnt() functions.
 *
 * Returns a (void*) to a static structure with the contents of the object
 * that was read.  The contents of the structure will be valid only until
 * the next call.  
 * If you use the returned value, then make sure that you cast it to
 * the right type for the current file! (AVCArc, AVCPal, AVCCnt, ...)
 *
 * Returns NULL if an error happened or if EOF was reached.  
 **********************************************************************/
void *AVCBinReadNextObject(AVCBinFile *psFile)
{
    void *psObj = NULL;

    switch(psFile->eFileType)
    {
      case AVCFileARC:
        psObj = (void*)AVCBinReadNextArc(psFile);
        break;
      case AVCFilePAL:
      case AVCFileRPL:
        psObj = (void*)AVCBinReadNextPal(psFile);
        break;
      case AVCFileCNT:
        psObj = (void*)AVCBinReadNextCnt(psFile);
        break;
      case AVCFileLAB:
        psObj = (void*)AVCBinReadNextLab(psFile);
        break;
      case AVCFileTOL:
        psObj = (void*)AVCBinReadNextTol(psFile);
        break;
      case AVCFileTXT:
      case AVCFileTX6:
        psObj = (void*)AVCBinReadNextTxt(psFile);
        break;
      case AVCFileRXP:
        psObj = (void*)AVCBinReadNextRxp(psFile);
        break;
      case AVCFileTABLE:
        psObj = (void*)AVCBinReadNextTableRec(psFile);
        break;
      default:
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "AVCBinReadNextObject(): Unsupported file type!");
    }

    return psObj;
}



/**********************************************************************
 *                          AVCBinReadNextTableRec()
 *
 * Reads the next record from an attribute table.
 *
 * Returns a pointer to an array of static AVCField structure whose 
 * contents will be valid only until the next call,
 * or NULL if an error happened or if EOF was reached.  
 **********************************************************************/
AVCField *AVCBinReadNextTableRec(AVCBinFile *psFile)
{
    if (psFile->eCoverType != AVCCoverPC &&
        psFile->eCoverType != AVCCoverPC2 &&
        psFile->eFileType == AVCFileTABLE &&
        psFile->hdr.psTableDef->numRecords > 0 &&
        ! AVCRawBinEOF(psFile->psRawBinFile) &&
        _AVCBinReadNextTableRec(psFile->psRawBinFile, 
                                    psFile->hdr.psTableDef->numFields,
                                    psFile->hdr.psTableDef->pasFieldDef,
                                    psFile->cur.pasFields,
                                    psFile->hdr.psTableDef->nRecSize) == 0 )
    {
        return psFile->cur.pasFields;
    }
    else if ((psFile->eCoverType == AVCCoverPC ||
              psFile->eCoverType == AVCCoverPC2 ) &&
             psFile->eFileType == AVCFileTABLE &&
             psFile->hdr.psTableDef->numRecords > 0 &&
             _AVCBinReadNextDBFTableRec(psFile->hDBFFile, 
                                        &(psFile->nCurDBFRecord),
                                        psFile->hdr.psTableDef->numFields,
                                        psFile->hdr.psTableDef->pasFieldDef,
                                        psFile->cur.pasFields) == 0)
    {
        return psFile->cur.pasFields;
    }

    return NULL;
}




/*=====================================================================
 *                              ARC
 *====================================================================*/

/**********************************************************************
 *                          _AVCBinReadNextArc()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinReadNextArc() instead)
 *
 * Read the next Arc structure from the file.
 *
 * The contents of the psArc structure is assumed to be valid, and the
 * psArc->pasVertices buffer may be reallocated or free()'d if it is not
 * NULL.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
int _AVCBinReadNextArc(AVCRawBinFile *psFile, AVCArc *psArc,
                              int nPrecision)
{
    int i, numVertices;
    int nRecordSize, nStartPos, nBytesRead;

    psArc->nArcId  = AVCRawBinReadInt32(psFile);
    if (AVCRawBinEOF(psFile))
        return -1;

    nRecordSize    = AVCRawBinReadInt32(psFile) * 2;
    nStartPos      = psFile->nCurPos+psFile->nOffset;
    psArc->nUserId = AVCRawBinReadInt32(psFile);
    psArc->nFNode  = AVCRawBinReadInt32(psFile);
    psArc->nTNode  = AVCRawBinReadInt32(psFile);
    psArc->nLPoly  = AVCRawBinReadInt32(psFile);
    psArc->nRPoly  = AVCRawBinReadInt32(psFile);
    numVertices    = AVCRawBinReadInt32(psFile);

    /* Realloc the vertices array only if it needs to grow...
     * do not realloc to a smaller size.
     * Note that for simplicity reasons, we always store the vertices as
     * double values in memory, even for single precision coverages.
     */
    if (psArc->pasVertices == NULL || numVertices > psArc->numVertices)
        psArc->pasVertices = (AVCVertex*)CPLRealloc(psArc->pasVertices,
                                                numVertices*sizeof(AVCVertex));

    psArc->numVertices = numVertices;

    if (nPrecision == AVC_SINGLE_PREC)
    {
        for(i=0; i<numVertices; i++)
        {
            psArc->pasVertices[i].x = AVCRawBinReadFloat(psFile);
            psArc->pasVertices[i].y = AVCRawBinReadFloat(psFile);
        }
    }
    else
    {
        for(i=0; i<numVertices; i++)
        {
            psArc->pasVertices[i].x = AVCRawBinReadDouble(psFile);
            psArc->pasVertices[i].y = AVCRawBinReadDouble(psFile);
        }

    }

    /*-----------------------------------------------------------------
     * Record size may be larger than number of vertices.  Skip up to
     * start of next object.
     *----------------------------------------------------------------*/
    nBytesRead = (psFile->nCurPos + psFile->nOffset) - nStartPos;
    if ( nBytesRead < nRecordSize)
        AVCRawBinFSeek(psFile, nRecordSize - nBytesRead, SEEK_CUR);

    return 0;
}

/**********************************************************************
 *                          AVCBinReadNextArc()
 *
 * Read the next Arc structure from the file.
 *
 * Returns a pointer to a static AVCArc structure whose contents will be
 * valid only until the next call or NULL if an error happened or if EOF
 * was reached.  
 **********************************************************************/
AVCArc *AVCBinReadNextArc(AVCBinFile *psFile)
{
    if (psFile->eFileType != AVCFileARC ||
        AVCRawBinEOF(psFile->psRawBinFile) ||
        _AVCBinReadNextArc(psFile->psRawBinFile, psFile->cur.psArc,
                           psFile->nPrecision) !=0)
    {
        return NULL;
    }

    return psFile->cur.psArc;
}


/*=====================================================================
 *                              PAL
 *====================================================================*/

/**********************************************************************
 *                          _AVCBinReadNextPal()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinReadNextPal() instead)
 *
 * Read the next PAL (Polygon Arc List) structure from the file.
 *
 * The contents of the psPal structure is assumed to be valid, and the
 * psPal->paVertices buffer may be reallocated or free()'d if it is not
 * NULL.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
int _AVCBinReadNextPal(AVCRawBinFile *psFile, AVCPal *psPal, 
                              int nPrecision)
{
    int i, numArcs;
    int nRecordSize, nStartPos, nBytesRead;

    psPal->nPolyId = AVCRawBinReadInt32(psFile);
    nRecordSize    = AVCRawBinReadInt32(psFile) * 2;
    nStartPos      = psFile->nCurPos+psFile->nOffset;

    if (AVCRawBinEOF(psFile))
        return -1;

    if (nPrecision == AVC_SINGLE_PREC)
    {
        psPal->sMin.x  = AVCRawBinReadFloat(psFile);
        psPal->sMin.y  = AVCRawBinReadFloat(psFile);
        psPal->sMax.x  = AVCRawBinReadFloat(psFile);
        psPal->sMax.y  = AVCRawBinReadFloat(psFile);
    }
    else
    {
        psPal->sMin.x  = AVCRawBinReadDouble(psFile);
        psPal->sMin.y  = AVCRawBinReadDouble(psFile);
        psPal->sMax.x  = AVCRawBinReadDouble(psFile);
        psPal->sMax.y  = AVCRawBinReadDouble(psFile);
    }

    numArcs            = AVCRawBinReadInt32(psFile);

    /* Realloc the arc list array only if it needs to grow...
     * do not realloc to a smaller size.
     */
    if (psPal->pasArcs == NULL || numArcs > psPal->numArcs)
        psPal->pasArcs = (AVCPalArc*)CPLRealloc(psPal->pasArcs,
                                                 numArcs*sizeof(AVCPalArc));

    psPal->numArcs = numArcs;

    for(i=0; i<numArcs; i++)
    {
        psPal->pasArcs[i].nArcId = AVCRawBinReadInt32(psFile);
        psPal->pasArcs[i].nFNode = AVCRawBinReadInt32(psFile);
        psPal->pasArcs[i].nAdjPoly = AVCRawBinReadInt32(psFile);
    }

    /*-----------------------------------------------------------------
     * Record size may be larger than number of vertices.  Skip up to
     * start of next object.
     *----------------------------------------------------------------*/
    nBytesRead = (psFile->nCurPos + psFile->nOffset) - nStartPos;
    if ( nBytesRead < nRecordSize)
        AVCRawBinFSeek(psFile, nRecordSize - nBytesRead, SEEK_CUR);

    return 0;
}

/**********************************************************************
 *                          AVCBinReadNextPal()
 *
 * Read the next PAL structure from the file.
 *
 * Returns a pointer to a static AVCPal structure whose contents will be
 * valid only until the next call or NULL if an error happened or if EOF
 * was reached.  
 **********************************************************************/
AVCPal *AVCBinReadNextPal(AVCBinFile *psFile)
{
    if ((psFile->eFileType!=AVCFilePAL && psFile->eFileType!=AVCFileRPL) ||
        AVCRawBinEOF(psFile->psRawBinFile) ||
        _AVCBinReadNextPal(psFile->psRawBinFile, psFile->cur.psPal,
                           psFile->nPrecision) !=0)
    {
        return NULL;
    }

    return psFile->cur.psPal;
}


/*=====================================================================
 *                              CNT
 *====================================================================*/

/**********************************************************************
 *                          _AVCBinReadNextCnt()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinReadNextCnt() instead)
 *
 * Read the next CNT (Polygon Centroid) structure from the file.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
int _AVCBinReadNextCnt(AVCRawBinFile *psFile, AVCCnt *psCnt, 
                              int nPrecision)
{
    int i, numLabels;
    int nRecordSize, nStartPos, nBytesRead;

    psCnt->nPolyId = AVCRawBinReadInt32(psFile);
    nRecordSize    = AVCRawBinReadInt32(psFile) * 2;
    nStartPos      = psFile->nCurPos+psFile->nOffset;

    if (AVCRawBinEOF(psFile))
        return -1;

    if (nPrecision == AVC_SINGLE_PREC)
    {
        psCnt->sCoord.x  = AVCRawBinReadFloat(psFile);
        psCnt->sCoord.y  = AVCRawBinReadFloat(psFile);
    }
    else
    {
        psCnt->sCoord.x  = AVCRawBinReadDouble(psFile);
        psCnt->sCoord.y  = AVCRawBinReadDouble(psFile);
    }

    numLabels      = AVCRawBinReadInt32(psFile);

    /* Realloc the LabelIds array only if it needs to grow...
     * do not realloc to a smaller size.
     */
    if (psCnt->panLabelIds == NULL || numLabels > psCnt->numLabels)
        psCnt->panLabelIds = (GInt32 *)CPLRealloc(psCnt->panLabelIds,
                                                  numLabels*sizeof(GInt32));

    psCnt->numLabels = numLabels;

    for(i=0; i<numLabels; i++)
    {
        psCnt->panLabelIds[i] = AVCRawBinReadInt32(psFile);
    }

    /*-----------------------------------------------------------------
     * Record size may be larger than number of vertices.  Skip up to
     * start of next object.
     *----------------------------------------------------------------*/
    nBytesRead = (psFile->nCurPos + psFile->nOffset) - nStartPos;
    if ( nBytesRead < nRecordSize)
        AVCRawBinFSeek(psFile, nRecordSize - nBytesRead, SEEK_CUR);

    return 0;
}

/**********************************************************************
 *                          AVCBinReadNextCnt()
 *
 * Read the next CNT structure from the file.
 *
 * Returns a pointer to a static AVCCnt structure whose contents will be
 * valid only until the next call or NULL if an error happened or if EOF
 * was reached.  
 **********************************************************************/
AVCCnt *AVCBinReadNextCnt(AVCBinFile *psFile)
{
    if (psFile->eFileType != AVCFileCNT ||
        AVCRawBinEOF(psFile->psRawBinFile) ||
        _AVCBinReadNextCnt(psFile->psRawBinFile, psFile->cur.psCnt,
                           psFile->nPrecision) !=0)
    {
        return NULL;
    }

    return psFile->cur.psCnt;
}


/*=====================================================================
 *                              LAB
 *====================================================================*/

/**********************************************************************
 *                          _AVCBinReadNextLab()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinReadNextLab() instead)
 *
 * Read the next LAB (Centroid Label) structure from the file.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
int _AVCBinReadNextLab(AVCRawBinFile *psFile, AVCLab *psLab, 
                              int nPrecision)
{

    psLab->nValue  = AVCRawBinReadInt32(psFile);
    psLab->nPolyId = AVCRawBinReadInt32(psFile);

    if (AVCRawBinEOF(psFile))
        return -1;

    if (nPrecision == AVC_SINGLE_PREC)
    {
        psLab->sCoord1.x  = AVCRawBinReadFloat(psFile);
        psLab->sCoord1.y  = AVCRawBinReadFloat(psFile);
        psLab->sCoord2.x  = AVCRawBinReadFloat(psFile);
        psLab->sCoord2.y  = AVCRawBinReadFloat(psFile);
        psLab->sCoord3.x  = AVCRawBinReadFloat(psFile);
        psLab->sCoord3.y  = AVCRawBinReadFloat(psFile);
    }
    else
    {
        psLab->sCoord1.x  = AVCRawBinReadDouble(psFile);
        psLab->sCoord1.y  = AVCRawBinReadDouble(psFile);
        psLab->sCoord2.x  = AVCRawBinReadDouble(psFile);
        psLab->sCoord2.y  = AVCRawBinReadDouble(psFile);
        psLab->sCoord3.x  = AVCRawBinReadDouble(psFile);
        psLab->sCoord3.y  = AVCRawBinReadDouble(psFile);
    }

    return 0;
}

/**********************************************************************
 *                          AVCBinReadNextLab()
 *
 * Read the next LAB structure from the file.
 *
 * Returns a pointer to a static AVCLab structure whose contents will be
 * valid only until the next call or NULL if an error happened or if EOF
 * was reached.  
 **********************************************************************/
AVCLab *AVCBinReadNextLab(AVCBinFile *psFile)
{
    if (psFile->eFileType != AVCFileLAB ||
        AVCRawBinEOF(psFile->psRawBinFile) ||
        _AVCBinReadNextLab(psFile->psRawBinFile, psFile->cur.psLab,
                           psFile->nPrecision) !=0)
    {
        return NULL;
    }

    return psFile->cur.psLab;
}

/*=====================================================================
 *                              TOL
 *====================================================================*/

/**********************************************************************
 *                          _AVCBinReadNextTol()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinReadNextTol() instead)
 *
 * Read the next TOL (tolerance) structure from the file.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
int _AVCBinReadNextTol(AVCRawBinFile *psFile, AVCTol *psTol, 
                       int nPrecision)
{

    psTol->nIndex  = AVCRawBinReadInt32(psFile);
    psTol->nFlag   = AVCRawBinReadInt32(psFile);

    if (AVCRawBinEOF(psFile))
        return -1;

    if (nPrecision == AVC_SINGLE_PREC)
    {
        psTol->dValue  = AVCRawBinReadFloat(psFile);
    }
    else
    {
        psTol->dValue  = AVCRawBinReadDouble(psFile);
    }

    return 0;
}

/**********************************************************************
 *                          AVCBinReadNextTol()
 *
 * Read the next TOL structure from the file.
 *
 * Returns a pointer to a static AVCTol structure whose contents will be
 * valid only until the next call or NULL if an error happened or if EOF
 * was reached.  
 **********************************************************************/
AVCTol *AVCBinReadNextTol(AVCBinFile *psFile)
{
    if (psFile->eFileType != AVCFileTOL ||
        AVCRawBinEOF(psFile->psRawBinFile) ||
        _AVCBinReadNextTol(psFile->psRawBinFile, psFile->cur.psTol,
                           psFile->nPrecision) !=0)
    {
        return NULL;
    }

    return psFile->cur.psTol;
}

/*=====================================================================
 *                              PRJ
 *====================================================================*/

/**********************************************************************
 *                          _AVCBinReadOpenPrj()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinReadOpen() with type AVCFilePRJ instead)
 *
 * Open a PRJ file.  
 *
 * This call will actually read the whole PRJ file in memory since PRJ
 * files are small text files.
 **********************************************************************/
AVCBinFile *_AVCBinReadOpenPrj(const char *pszPath, const char *pszName)
{
    AVCBinFile   *psFile;
    char         *pszFname, **papszPrj;

    /*-----------------------------------------------------------------
     * Load the PRJ file contents into a stringlist.
     *----------------------------------------------------------------*/
    pszFname = (char*)CPLMalloc((strlen(pszPath)+strlen(pszName)+1)*
                                sizeof(char));
    sprintf(pszFname, "%s%s", pszPath, pszName);

    papszPrj = CSLLoad(pszFname);

    CPLFree(pszFname);

    if (papszPrj == NULL)
    {
        /* Failed to open file... just return NULL since an error message
         * has already been issued by CSLLoad()
         */
        return NULL;
    }

    /*-----------------------------------------------------------------
     * Alloc and init the AVCBinFile handle.
     *----------------------------------------------------------------*/
    psFile = (AVCBinFile*)CPLCalloc(1, sizeof(AVCBinFile));

    psFile->eFileType = AVCFilePRJ;
    psFile->psRawBinFile = NULL;
    psFile->cur.papszPrj = papszPrj;
    psFile->pszFilename = NULL;


    return psFile;
}

/**********************************************************************
 *                          AVCBinReadPrj()
 *
 * Return the contents of the previously opened PRJ (projection) file.
 *
 * PRJ files are simple text files with variable length lines, so we
 * don't use the AVCRawBin*() functions for this case.
 *
 * Returns a reference to a static stringlist with the whole file 
 * contents, or NULL in case of error.
 *
 * The returned stringlist should NOT be freed by the caller.
 **********************************************************************/
char **AVCBinReadNextPrj(AVCBinFile *psFile)
{
    /*-----------------------------------------------------------------
     * The file should have already been loaded by AVCBinFileOpen(),
     * so there is not much to do here!
     *----------------------------------------------------------------*/
    return psFile->cur.papszPrj;
}

/*=====================================================================
 *                              TXT/TX6/TX7
 *====================================================================*/

/**********************************************************************
 *                          _AVCBinReadNextTxt()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinReadNextTxt() instead)
 *
 * Read the next TXT/TX6/TX7 (Annotation) structure from the file.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
int _AVCBinReadNextTxt(AVCRawBinFile *psFile, AVCTxt *psTxt, 
                              int nPrecision)
{
    int i, numVerticesBefore, numVertices, numCharsToRead, nRecordSize;
    int numBytesRead;

    numVerticesBefore = ABS(psTxt->numVerticesLine) + 
                        ABS(psTxt->numVerticesArrow);

    psTxt->nTxtId  = AVCRawBinReadInt32(psFile);
    if (AVCRawBinEOF(psFile))
        return -1;

    nRecordSize    = 8 + 2*AVCRawBinReadInt32(psFile);

    psTxt->nUserId = AVCRawBinReadInt32(psFile);
    psTxt->nLevel  = AVCRawBinReadInt32(psFile);

    psTxt->f_1e2    = AVCRawBinReadFloat(psFile);
    psTxt->nSymbol  = AVCRawBinReadInt32(psFile);
    psTxt->numVerticesLine  = AVCRawBinReadInt32(psFile);
    psTxt->n28      = AVCRawBinReadInt32(psFile);
    psTxt->numChars = AVCRawBinReadInt32(psFile);
    psTxt->numVerticesArrow = AVCRawBinReadInt32(psFile);

    for(i=0; i<20; i++)
    {
        psTxt->anJust1[i] = AVCRawBinReadInt16(psFile);
    }
    for(i=0; i<20; i++)
    {
        psTxt->anJust2[i] = AVCRawBinReadInt16(psFile);
    }

    if (nPrecision == AVC_SINGLE_PREC)
    {
        psTxt->dHeight = AVCRawBinReadFloat(psFile);
        psTxt->dV2     = AVCRawBinReadFloat(psFile);
        psTxt->dV3     = AVCRawBinReadFloat(psFile);
    }
    else
    {
        psTxt->dHeight = AVCRawBinReadDouble(psFile);
        psTxt->dV2     = AVCRawBinReadDouble(psFile);
        psTxt->dV3     = AVCRawBinReadDouble(psFile);
    }

    numCharsToRead = ((int)(psTxt->numChars + 3)/4)*4;
    if (psTxt->pszText == NULL ||
        ((int)(strlen((char*)psTxt->pszText)+3)/4)*4 < numCharsToRead )
    {
        psTxt->pszText = (GByte*)CPLRealloc(psTxt->pszText,
                                            (numCharsToRead+1)*sizeof(char));
    }

    AVCRawBinReadString(psFile, numCharsToRead, psTxt->pszText);
    psTxt->pszText[psTxt->numChars] = '\0';

    /* Realloc the vertices array only if it needs to grow...
     * do not realloc to a smaller size.
     */
    numVertices = ABS(psTxt->numVerticesLine) + ABS(psTxt->numVerticesArrow);

    if (psTxt->pasVertices == NULL || numVertices > numVerticesBefore)
        psTxt->pasVertices = (AVCVertex*)CPLRealloc(psTxt->pasVertices,
                                              numVertices*sizeof(AVCVertex));

    if (nPrecision == AVC_SINGLE_PREC)
    {
        for(i=0; i<numVertices; i++)
        {
            psTxt->pasVertices[i].x = AVCRawBinReadFloat(psFile);
            psTxt->pasVertices[i].y = AVCRawBinReadFloat(psFile);
        }
    }
    else
    {
        for(i=0; i<numVertices; i++)
        {
            psTxt->pasVertices[i].x = AVCRawBinReadDouble(psFile);
            psTxt->pasVertices[i].y = AVCRawBinReadDouble(psFile);
        }
    }

    /* In V7 Coverages, we always have 8 bytes of junk at end of record.
     * In Weird coverages, these 8 bytes are sometimes present, and 
     * sometimes not!!! (Probably another AI "random feature"! ;-)
     * So we use the record size to establish if there is any junk to skip
     */
    if (nPrecision == AVC_SINGLE_PREC)
        numBytesRead = 132 + numCharsToRead + numVertices * 2 * 4;
    else
        numBytesRead = 144 + numCharsToRead + numVertices * 2 * 8;

    if (numBytesRead < nRecordSize)
        AVCRawBinFSeek(psFile, nRecordSize - numBytesRead, SEEK_CUR);

    return 0;
}

/**********************************************************************
 *                          _AVCBinReadNextPCCoverageTxt()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinReadNextTxt() instead)
 *
 * Read the next TXT (Annotation) structure from a PC Coverage file.
 * Note that it is assumed that PC Coverage files are always single 
 * precision.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
int _AVCBinReadNextPCCoverageTxt(AVCRawBinFile *psFile, AVCTxt *psTxt, 
                                 int nPrecision)
{
    int i, numVerticesBefore, numVertices, numCharsToRead, nRecordSize;

    numVerticesBefore = ABS(psTxt->numVerticesLine) + 
                        ABS(psTxt->numVerticesArrow);

    psTxt->nTxtId  = AVCRawBinReadInt32(psFile);
    if (AVCRawBinEOF(psFile))
        return -1;

    nRecordSize    = 8 + 2*AVCRawBinReadInt32(psFile);

    psTxt->nUserId = 0;
    psTxt->nLevel  = AVCRawBinReadInt32(psFile);

    psTxt->numVerticesLine  = AVCRawBinReadInt32(psFile);  
    /* We are not expecting more than 4 vertices */
    psTxt->numVerticesLine = MIN(psTxt->numVerticesLine, 4);

    psTxt->numVerticesArrow = 0;

    /* Realloc the vertices array only if it needs to grow...
     * do not realloc to a smaller size.
     *
     * Note that because of the way V7 binary TXT files work, the rest of the
     * lib expects to receive duplicate coords for the first vertex, so
     * we have to include an additional vertex for that.
     */
    psTxt->numVerticesLine += 1;
    numVertices = ABS(psTxt->numVerticesLine) + ABS(psTxt->numVerticesArrow);

    if (psTxt->pasVertices == NULL || numVertices > numVerticesBefore)
        psTxt->pasVertices = (AVCVertex*)CPLRealloc(psTxt->pasVertices,
                                              numVertices*sizeof(AVCVertex));

    for(i=1; i<numVertices; i++)
    {
        if (nPrecision == AVC_SINGLE_PREC)
        {
            psTxt->pasVertices[i].x = AVCRawBinReadFloat(psFile);
            psTxt->pasVertices[i].y = AVCRawBinReadFloat(psFile);
        }
        else
        {
            psTxt->pasVertices[i].x = AVCRawBinReadDouble(psFile);
            psTxt->pasVertices[i].y = AVCRawBinReadDouble(psFile);
        }
    }
    /* Duplicate the first vertex because that's the way the other binary TXT
     * files work and that's what the lib expects to generate the E00.
     */
    psTxt->pasVertices[0].x = psTxt->pasVertices[1].x;
    psTxt->pasVertices[0].y = psTxt->pasVertices[1].y;

    /* Skip the other floats (vertices) that are unused */
    if (nPrecision == AVC_SINGLE_PREC)
        AVCRawBinFSeek(psFile, 4*(15-2*(numVertices-1)) , SEEK_CUR);
    else
        AVCRawBinFSeek(psFile, 8*(15-2*(numVertices-1)) , SEEK_CUR);

    if (nPrecision == AVC_SINGLE_PREC)
    {
        psTxt->dHeight  = AVCRawBinReadFloat(psFile);
    }
    else
    {
        psTxt->dHeight  = AVCRawBinReadDouble(psFile);
    }
    psTxt->f_1e2    = AVCRawBinReadFloat(psFile);
    psTxt->nSymbol  = AVCRawBinReadInt32(psFile);
    psTxt->numChars = AVCRawBinReadInt32(psFile);

    /* In some cases, we may need to skip additional spaces after the
     * text string... more than should be required to simply align with
     * a 4 bytes boundary... include that in numCharsToRead
     */
    if (nPrecision == AVC_SINGLE_PREC)
    {
        numCharsToRead = nRecordSize - (28 + 16*4);
    }
    else
    {
        numCharsToRead = nRecordSize - (28 + 16*8);
    }

    /* Do a quick check in case file is corrupt! */
    psTxt->numChars = MIN(psTxt->numChars, numCharsToRead);

    if (psTxt->pszText == NULL ||
        ((int)(strlen((char*)psTxt->pszText)+3)/4)*4 < numCharsToRead )
    {
        psTxt->pszText = (GByte*)CPLRealloc(psTxt->pszText,
                                            (numCharsToRead+5)*sizeof(char));
    }


    AVCRawBinReadString(psFile, numCharsToRead, psTxt->pszText);
    psTxt->pszText[psTxt->numChars] = '\0';

    /* Set unused members to default values...
     */
    psTxt->dV2     = 0.0;
    psTxt->dV3     = 0.0;
    psTxt->n28      = 0;
    for(i=0; i<20; i++)
    {
        psTxt->anJust1[i] = 0;
        psTxt->anJust2[i] = 0;
    }

    return 0;
}

/**********************************************************************
 *                          AVCBinReadNextTxt()
 *
 * Read the next TXT/TX6/TX7 structure from the file.
 *
 * Returns a pointer to a static AVCTxt structure whose contents will be
 * valid only until the next call or NULL if an error happened or if EOF
 * was reached.  
 **********************************************************************/
AVCTxt *AVCBinReadNextTxt(AVCBinFile *psFile)
{
    int nStatus = 0;

    if ((psFile->eFileType != AVCFileTXT && psFile->eFileType != AVCFileTX6) ||
        AVCRawBinEOF(psFile->psRawBinFile) )
    {
        return NULL;
    }

    /* AVCCoverPC have a different TXT format than AVCCoverV7
     *
     * Note: Some Weird coverages use the PC TXT structure, and some use the
     *       V7 structure.  We distinguish them using the header's precision
     *       field in AVCBinReadRewind().
     */
    if (psFile->eFileType == AVCFileTXT &&
        (psFile->eCoverType == AVCCoverPC ||
         psFile->eCoverType == AVCCoverWeird) )
    {    
        /* TXT file in PC Coverages (and some Weird Coverages)
         */
        nStatus = _AVCBinReadNextPCCoverageTxt(psFile->psRawBinFile, 
                                               psFile->cur.psTxt,
                                               psFile->nPrecision);
    }
    else
    {   
        /* TXT in V7 Coverages (and some Weird Coverages), and TX6/TX7 in 
         * all coverage types
         */
        nStatus = _AVCBinReadNextTxt(psFile->psRawBinFile, psFile->cur.psTxt,
                                     psFile->nPrecision);
    }

    if (nStatus != 0)
    {
        return NULL;
    }

    return psFile->cur.psTxt;
}


/*=====================================================================
 *                              RXP
 *====================================================================*/

/**********************************************************************
 *                          _AVCBinReadNextRxp()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinReadNextRxp() instead)
 *
 * Read the next RXP (Region something...) structure from the file.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
int _AVCBinReadNextRxp(AVCRawBinFile *psFile, AVCRxp *psRxp, 
                       int nPrecision)
{

    psRxp->n1  = AVCRawBinReadInt32(psFile);
    if (AVCRawBinEOF(psFile))
        return -1;
    psRxp->n2  = AVCRawBinReadInt32(psFile);

    return 0;
}

/**********************************************************************
 *                          AVCBinReadNextRxp()
 *
 * Read the next RXP structure from the file.
 *
 * Returns a pointer to a static AVCRxp structure whose contents will be
 * valid only until the next call or NULL if an error happened or if EOF
 * was reached.  
 **********************************************************************/
AVCRxp *AVCBinReadNextRxp(AVCBinFile *psFile)
{
    if (psFile->eFileType != AVCFileRXP ||
        AVCRawBinEOF(psFile->psRawBinFile) ||
        _AVCBinReadNextRxp(psFile->psRawBinFile, psFile->cur.psRxp,
                           psFile->nPrecision) !=0)
    {
        return NULL;
    }

    return psFile->cur.psRxp;
}

/*=====================================================================
 *                         NATIVE (V7.x) TABLEs
 *
 * Note: Also applies to AVCCoverWeird
 *====================================================================*/

/**********************************************************************
 *                          _AVCBinReadNextArcDir()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinReadOpen() with type AVCFileTABLE instead)
 *
 * Read the next record from an arc.dir (or "arcdr9") file.
 *
 * Note that arc.dir files have no header... they start with the
 * first record immediately.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
int _AVCBinReadNextArcDir(AVCRawBinFile *psFile, AVCTableDef *psArcDir)
{
    int i;

    /* Arc/Info Table name 
     */
    AVCRawBinReadString(psFile, 32, (GByte *)psArcDir->szTableName);
    psArcDir->szTableName[32] = '\0';

    if (AVCRawBinEOF(psFile))
        return -1;

    /* "ARC####" basename for .DAT and .NIT files
     */
    AVCRawBinReadString(psFile, 8, (GByte *)psArcDir->szInfoFile);
    psArcDir->szInfoFile[7] = '\0';
    for (i=6; i>0 && psArcDir->szInfoFile[i]==' '; i--)
        psArcDir->szInfoFile[i] = '\0';

    psArcDir->numFields = AVCRawBinReadInt16(psFile);
    psArcDir->nRecSize  = AVCRawBinReadInt16(psFile);

    AVCRawBinFSeek(psFile, 18, SEEK_CUR);     /* Skip 18 bytes */
    
    psArcDir->bDeletedFlag = AVCRawBinReadInt16(psFile);
    psArcDir->numRecords = AVCRawBinReadInt32(psFile);

    AVCRawBinFSeek(psFile, 10, SEEK_CUR);     /* Skip 10 bytes */
    
    AVCRawBinReadBytes(psFile, 2, (GByte *)psArcDir->szExternal);
    psArcDir->szExternal[2] = '\0';

    AVCRawBinFSeek(psFile, 300, SEEK_CUR);  /* Skip the remaining 300 bytes */

    return 0;
}

/**********************************************************************
 *                          _AVCBinReadNextNit()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinReadOpen() with type AVCFileTABLE instead)
 *
 * Read the next record from an arc####.nit file.
 *
 * Note that arc####.nit files have no header... they start with the
 * first record immediately.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
int _AVCBinReadNextArcNit(AVCRawBinFile *psFile, AVCFieldInfo *psField)
{
    AVCRawBinReadString(psFile, 16, (GByte *)psField->szName);
    psField->szName[16] = '\0';

    if (AVCRawBinEOF(psFile))
        return -1;

    psField->nSize     = AVCRawBinReadInt16(psFile);
    psField->v2        = AVCRawBinReadInt16(psFile);  /* Always -1 ? */
    psField->nOffset   = AVCRawBinReadInt16(psFile);
    psField->v4        = AVCRawBinReadInt16(psFile);  /* Always 4 ?  */
    psField->v5        = AVCRawBinReadInt16(psFile);  /* Always -1 ? */
    psField->nFmtWidth = AVCRawBinReadInt16(psFile);
    psField->nFmtPrec  = AVCRawBinReadInt16(psFile);
    psField->nType1    = AVCRawBinReadInt16(psFile);
    psField->nType2    = AVCRawBinReadInt16(psFile);  /* Always 0 ? */
    psField->v10       = AVCRawBinReadInt16(psFile);  /* Always -1 ? */
    psField->v11       = AVCRawBinReadInt16(psFile);  /* Always -1 ? */
    psField->v12       = AVCRawBinReadInt16(psFile);  /* Always -1 ? */
    psField->v13       = AVCRawBinReadInt16(psFile);  /* Always -1 ? */

    AVCRawBinReadString(psFile, 16, (GByte *)psField->szAltName);   /* Always Blank ? */
    psField->szAltName[16] = '\0';

    AVCRawBinFSeek(psFile, 56, SEEK_CUR);             /* Skip 56 bytes */
    
    psField->nIndex    = AVCRawBinReadInt16(psFile);

    AVCRawBinFSeek(psFile, 28, SEEK_CUR);  /* Skip the remaining 28 bytes */

    return 0;
}

/**********************************************************************
 *                          _AVCBinReadGetInfoFilename()
 *
 * Look for the DAT or NIT files for a given table... returns TRUE if
 * they exist, or FALSE otherwise.
 *
 * If pszRetFnmae/pszRetNitFile != NULL then the filename with full path 
 * will be copied to the specified buffer.
 **********************************************************************/
GBool _AVCBinReadGetInfoFilename(const char *pszInfoPath, 
                                 const char *pszBasename,
                                 const char *pszDatOrNit,
                                 AVCCoverType eCoverType,
                                 char *pszRetFname)
{
    GBool       bFilesExist = FALSE;
    char        *pszBuf = NULL;
    VSIStatBuf  sStatBuf;

    if (pszRetFname)
        pszBuf = pszRetFname;
    else
        pszBuf = (char*)CPLMalloc((strlen(pszInfoPath)+strlen(pszBasename)+10)*
                                  sizeof(char));

    if (eCoverType == AVCCoverWeird)
    {
        sprintf(pszBuf, "%s%s%s", pszInfoPath, pszBasename, pszDatOrNit);
    }
    else
    {
        sprintf(pszBuf, "%s%s.%s", pszInfoPath, pszBasename, pszDatOrNit);
    }

    AVCAdjustCaseSensitiveFilename(pszBuf);

    if (VSIStat(pszBuf, &sStatBuf) == 0)
        bFilesExist = TRUE;

    if (eCoverType == AVCCoverWeird && !bFilesExist)
    {
        /* In some cases, the filename can be truncated to 8 chars
         * and we end up with "ARC000DA"... check that possibility.
         */
        pszBuf[strlen(pszBuf)-1] = '\0';

        AVCAdjustCaseSensitiveFilename(pszBuf);

        if (VSIStat(pszBuf, &sStatBuf) == 0)
            bFilesExist = TRUE;
    }

    if (pszRetFname == NULL)
        CPLFree(pszBuf);

    return bFilesExist;

}

/**********************************************************************
 *                          _AVCBinReadInfoFilesExist()
 *
 * Look for the DAT and NIT files for a given table... returns TRUE if
 * they exist, or FALSE otherwise.
 *
 * If pszRetDatFile/pszRetNitFile != NULL then the .DAT and .NIT filename
 * without the info path will be copied to the specified buffers.
 **********************************************************************/
GBool _AVCBinReadInfoFileExists(const char *pszInfoPath, 
                                const char *pszBasename,
                                AVCCoverType eCoverType)
{

    return (_AVCBinReadGetInfoFilename(pszInfoPath, pszBasename, 
                                       "dat", eCoverType, NULL) == TRUE &&
            _AVCBinReadGetInfoFilename(pszInfoPath, pszBasename, 
                                       "nit", eCoverType, NULL) == TRUE);

}

/**********************************************************************
 *                          AVCBinReadListTables()
 *
 * Scan the arc.dir file and return stringlist with one entry for the
 * Arc/Info name of each table that belongs to the specified coverage.
 * Pass pszCoverName = NULL to get the list of all tables.
 *
 * ppapszArcDatFiles if not NULL will be set to point to a stringlist
 * with the corresponding "ARC????" info file basenames corresponding
 * to each table found.
 *
 * Note that arc.dir files have no header... they start with the
 * first record immediately.
 *
 * In AVCCoverWeird, the file is called "arcdr9"
 *
 * Returns a stringlist that should be deallocated by the caller
 * with CSLDestroy(), or NULL on error.
 **********************************************************************/
char **AVCBinReadListTables(const char *pszInfoPath, const char *pszCoverName,
                            char ***ppapszArcDatFiles, AVCCoverType eCoverType,
                            AVCDBCSInfo *psDBCSInfo)
{
    char              **papszList = NULL;
    char               *pszFname;
    char                szNameToFind[33] = "";
    int                 nLen;
    AVCRawBinFile      *hFile;
    AVCTableDef         sEntry;

    if (ppapszArcDatFiles)
        *ppapszArcDatFiles = NULL;

    /*----------------------------------------------------------------- 
     * For AVCCoverV7Tables type we do not look for tables for a specific
     * coverage, we return all tables from the info dir.
     *----------------------------------------------------------------*/
    if (eCoverType == AVCCoverV7Tables)
        pszCoverName = NULL;

    /*----------------------------------------------------------------- 
     * All tables that belong to a given coverage have their name starting
     * with the coverage name (in uppercase letters), followed by a 3 
     * letters extension.
     *----------------------------------------------------------------*/
    if (pszCoverName != NULL)
        sprintf(szNameToFind, "%-.28s.", pszCoverName);
    nLen = strlen(szNameToFind);

    /*----------------------------------------------------------------- 
     * Open the arc.dir and add all entries that match the criteria
     * to our list.
     * In AVCCoverWeird, the file is called "arcdr9"
     *----------------------------------------------------------------*/
    pszFname = (char*)CPLMalloc((strlen(pszInfoPath)+9)*sizeof(char));
    if (eCoverType == AVCCoverWeird)
        sprintf(pszFname, "%sarcdr9", pszInfoPath);
    else
        sprintf(pszFname, "%sarc.dir", pszInfoPath);

    AVCAdjustCaseSensitiveFilename(pszFname);

    hFile = AVCRawBinOpen(pszFname, "r", AVC_COVER_BYTE_ORDER(eCoverType),
                          psDBCSInfo);

    if (hFile)
    {
        while (!AVCRawBinEOF(hFile) &&
               _AVCBinReadNextArcDir(hFile, &sEntry) == 0)
        {
            if (/* sEntry.numRecords > 0 && (DO NOT skip empty tables) */
                !sEntry.bDeletedFlag &&
                (pszCoverName == NULL ||
                 EQUALN(szNameToFind, sEntry.szTableName, nLen)) &&
                _AVCBinReadInfoFileExists(pszInfoPath, 
                                          sEntry.szInfoFile, 
                                          eCoverType) )
            {
                papszList = CSLAddString(papszList, sEntry.szTableName);

                if (ppapszArcDatFiles)
                    *ppapszArcDatFiles = CSLAddString(*ppapszArcDatFiles,
                                                      sEntry.szInfoFile);
            }
        }
        AVCRawBinClose(hFile);

    }

    CPLFree(pszFname);

    return papszList;
}

/**********************************************************************
 *                         _AVCBinReadOpenTable()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinReadOpen() with type AVCFileTABLE instead)
 *
 * Open a INFO table, read the header file (.NIT), and finally open
 * the associated data file to be ready to read records from it.
 *
 * Returns a valid AVCBinFile handle, or NULL if the file could
 * not be opened.
 *
 * _AVCBinReadCloseTable() will eventually have to be called to release the 
 * resources used by the AVCBinFile structure.
 **********************************************************************/
AVCBinFile *_AVCBinReadOpenTable(const char *pszInfoPath,
                                 const char *pszTableName,
                                 AVCCoverType eCoverType,
                                 AVCDBCSInfo *psDBCSInfo)
{
    AVCBinFile    *psFile;
    AVCRawBinFile *hFile;
    AVCTableDef    sTableDef;
    AVCFieldInfo  *pasFieldDef;
    char          *pszFname;
    GBool          bFound;
    int            i;

    /* Alloc a buffer big enough for the longest possible filename...
     */
    pszFname = (char*)CPLMalloc((strlen(pszInfoPath)+81)*sizeof(char));

    /*-----------------------------------------------------------------
     * Fetch info about this table from the "arc.dir"
     *----------------------------------------------------------------*/
    if (eCoverType == AVCCoverWeird)
        sprintf(pszFname, "%sarcdr9", pszInfoPath);
    else
        sprintf(pszFname, "%sarc.dir", pszInfoPath);

    AVCAdjustCaseSensitiveFilename(pszFname);

    hFile = AVCRawBinOpen(pszFname, "r", AVC_COVER_BYTE_ORDER(eCoverType),
                          psDBCSInfo);
    bFound = FALSE;

    if (hFile)
    {
        while(!bFound && _AVCBinReadNextArcDir(hFile, &sTableDef) == 0)
        {
            if (!sTableDef.bDeletedFlag &&
                EQUALN(sTableDef.szTableName, pszTableName, 
                       strlen(pszTableName)) &&
                _AVCBinReadInfoFileExists(pszInfoPath, 
                                          sTableDef.szInfoFile, 
                                          eCoverType))
            {
                bFound = TRUE;
            }
        }
        AVCRawBinClose(hFile);
    }

    /* Hummm... quite likely that this table does not exist!
     */
    if (!bFound)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Failed to open table %s", pszTableName);
        CPLFree(pszFname);
        return NULL;
    }

    /*-----------------------------------------------------------------
     * Establish the location of the data file... depends on the 
     * szExternal[] field.
     *----------------------------------------------------------------*/
    if (EQUAL(sTableDef.szExternal, "XX"))
    {
        /*-------------------------------------------------------------
         * The data file is located outside of the INFO directory.
         * Read the path to the data file from the arc####.dat file
         *------------------------------------------------------------*/
        _AVCBinReadGetInfoFilename(pszInfoPath, sTableDef.szInfoFile,
                                   "dat", eCoverType, pszFname);
        AVCAdjustCaseSensitiveFilename(pszFname);
    
        hFile = AVCRawBinOpen(pszFname, "r", AVC_COVER_BYTE_ORDER(eCoverType),
                              psDBCSInfo);

        if (hFile)
        {
            /* Read the relative file path, and remove trailing spaces.
             */
            AVCRawBinReadBytes(hFile, 80, (GByte *)sTableDef.szDataFile);
            sTableDef.szDataFile[80] = '\0';

            for(i = strlen(sTableDef.szDataFile)-1;
                isspace((unsigned char)sTableDef.szDataFile[i]);
                i--)
            {
                sTableDef.szDataFile[i] = '\0';
            }

            AVCRawBinClose(hFile);
        }
        else
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Failed to open file %s", pszFname);
            CPLFree(pszFname);
            return NULL;
        }
         
    }
    else
    {
        /*-------------------------------------------------------------
         * The data file IS the arc####.dat file
         * Note: sTableDef.szDataFile must be relative to info directory
         *------------------------------------------------------------*/
        _AVCBinReadGetInfoFilename(pszInfoPath, sTableDef.szInfoFile,
                                   "dat", eCoverType, pszFname);
        strcpy(sTableDef.szDataFile, pszFname+strlen(pszInfoPath));
   }

    /*-----------------------------------------------------------------
     * Read the table field definitions from the "arc####.nit" file.
     *----------------------------------------------------------------*/
    _AVCBinReadGetInfoFilename(pszInfoPath, sTableDef.szInfoFile,
                               "nit", eCoverType, pszFname);
    AVCAdjustCaseSensitiveFilename(pszFname);

    hFile = AVCRawBinOpen(pszFname, "r", AVC_COVER_BYTE_ORDER(eCoverType),
                          psDBCSInfo);

    if (hFile)
    {
        int iField;

        pasFieldDef = (AVCFieldInfo*)CPLCalloc(sTableDef.numFields,
                                               sizeof(AVCFieldInfo));

        /*-------------------------------------------------------------
         * There must be at least sTableDef.numFields valid entries
         * in the .NIT file...
         *
         * Note that we ignore any deleted field entries (entries with
         * index=-1)... I don't see any use for these deleted fields...
         * and I don't understand why Arc/Info includes them in their
         * E00 table headers...
         *------------------------------------------------------------*/
        for(i=0, iField=0; iField<sTableDef.numFields; i++)
        {
            if (_AVCBinReadNextArcNit(hFile, &(pasFieldDef[iField])) != 0)
            {
                /* Problems.... is the NIT file corrupt???
                 */
                AVCRawBinClose(hFile);
                CPLFree(pszFname);
                CPLFree(pasFieldDef);
                CPLError(CE_Failure, CPLE_FileIO,
                         "Failed reading table field info for table %s "
                         "File may be corrupt?",  pszTableName);
                return NULL;
            }

            /*---------------------------------------------------------
             * Check if the field has been deleted (nIndex == -1).
             * We just ignore deleted fields
             *--------------------------------------------------------*/
            if (pasFieldDef[iField].nIndex > 0)
                iField++;
        }

        AVCRawBinClose(hFile);
    }
    else
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Failed to open file %s", pszFname);
        CPLFree(pszFname);
        return NULL;
    }


    /*-----------------------------------------------------------------
     * Open the data file... ready to read records from it.
     * If the header says that table has 0 records, then we don't
     * try to open the file... but we don't consider that as an error.
     *----------------------------------------------------------------*/
    if (sTableDef.numRecords > 0 && 
        AVCFileExists(pszInfoPath, sTableDef.szDataFile))
    {
        VSIStatBuf      sStatBuf;

        sprintf(pszFname, "%s%s", pszInfoPath, sTableDef.szDataFile);
        AVCAdjustCaseSensitiveFilename(pszFname);

        hFile = AVCRawBinOpen(pszFname, "r", AVC_COVER_BYTE_ORDER(eCoverType),
                              psDBCSInfo);

        /* OOPS... data file does not exist!
         */
        if (hFile == NULL)
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Failed to open file %s", pszFname);
            CPLFree(pszFname);
            return NULL;
        }

        /*-------------------------------------------------------------
         * In some cases, the number of records field for a table in the 
         * arc.dir does not correspond to the real number of records
         * in the data file.  In this kind of situation, the number of
         * records returned by Arc/Info in an E00 file will be based
         * on the real data file size, and not on the value from the arc.dir.
         *
         * Fetch the data file size, and correct the number of record
         * field in the table header if necessary.
         *------------------------------------------------------------*/
        if ( VSIStat(pszFname, &sStatBuf) != -1 &&
             sTableDef.nRecSize > 0 &&
             sStatBuf.st_size/sTableDef.nRecSize != sTableDef.numRecords)
        {
            sTableDef.numRecords = sStatBuf.st_size/sTableDef.nRecSize;
        }

    }
    else
    {
        hFile = NULL;
        sTableDef.numRecords = 0;
    }

    /*-----------------------------------------------------------------
     * Alloc. and init. the AVCBinFile structure.
     *----------------------------------------------------------------*/
    psFile = (AVCBinFile*)CPLCalloc(1, sizeof(AVCBinFile));

    psFile->psRawBinFile = hFile;
    psFile->eCoverType = AVCCoverV7;
    psFile->eFileType = AVCFileTABLE;
    psFile->pszFilename = pszFname;

    psFile->hdr.psTableDef = (AVCTableDef*)CPLMalloc(sizeof(AVCTableDef));
    *(psFile->hdr.psTableDef) = sTableDef;

    psFile->hdr.psTableDef->pasFieldDef = pasFieldDef;

    /* We can't really tell the precision from a Table header...
     * just set an arbitrary value... it probably won't be used anyways!
     */
    psFile->nPrecision = AVC_SINGLE_PREC;

    /*-----------------------------------------------------------------
     * Allocate temp. structures to use to read records from the file
     * And allocate buffers for those fields that are stored as strings.
     *----------------------------------------------------------------*/
    psFile->cur.pasFields = (AVCField*)CPLCalloc(sTableDef.numFields,
                                                 sizeof(AVCField));

    for(i=0; i<sTableDef.numFields; i++)
    {
        if (pasFieldDef[i].nType1*10 == AVC_FT_DATE ||
            pasFieldDef[i].nType1*10 == AVC_FT_CHAR ||
            pasFieldDef[i].nType1*10 == AVC_FT_FIXINT ||
            pasFieldDef[i].nType1*10 == AVC_FT_FIXNUM )
        {
            psFile->cur.pasFields[i].pszStr = 
                (GByte*)CPLCalloc(pasFieldDef[i].nSize+1, sizeof(char));
        }
    }

    return psFile;
}


/**********************************************************************
 *                         _AVCBinReadNextTableRec()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinReadNextTableRec() instead)
 *
 * Reads the next record from an attribute table and fills the 
 * pasFields[] array.
 *
 * Note that it is assumed that the pasFields[] array has been properly
 * initialized, re the allocation of buffers for fields strored as
 * strings.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
int _AVCBinReadNextTableRec(AVCRawBinFile *psFile, int nFields,
                            AVCFieldInfo *pasDef, AVCField *pasFields,
                            int nRecordSize)
{
    int i, nType, nBytesRead=0;

    if (psFile == NULL)
        return -1;

    for(i=0; i<nFields; i++)
    {
        if (AVCRawBinEOF(psFile))
            return -1;

        nType = pasDef[i].nType1*10;

        if (nType ==  AVC_FT_DATE || nType == AVC_FT_CHAR ||
            nType == AVC_FT_FIXINT || nType == AVC_FT_FIXNUM)
        {
            /*---------------------------------------------------------
             * Values stored as strings
             *--------------------------------------------------------*/
            AVCRawBinReadString(psFile, pasDef[i].nSize, pasFields[i].pszStr);
            pasFields[i].pszStr[pasDef[i].nSize] = '\0';
        }
        else if (nType == AVC_FT_BININT && pasDef[i].nSize == 4)
        {
            /*---------------------------------------------------------
             * 32 bit binary integers
             *--------------------------------------------------------*/
            pasFields[i].nInt32 = AVCRawBinReadInt32(psFile);
        }
        else if (nType == AVC_FT_BININT && pasDef[i].nSize == 2)
        {
            /*---------------------------------------------------------
             * 16 bit binary integers
             *--------------------------------------------------------*/
            pasFields[i].nInt16 = AVCRawBinReadInt16(psFile);
        }
        else if (nType == AVC_FT_BINFLOAT && pasDef[i].nSize == 4)
        {
            /*---------------------------------------------------------
             * Single precision floats
             *--------------------------------------------------------*/
            pasFields[i].fFloat = AVCRawBinReadFloat(psFile);
        }
        else if (nType == AVC_FT_BINFLOAT && pasDef[i].nSize == 8)
        {
            /*---------------------------------------------------------
             * Double precision floats
             *--------------------------------------------------------*/
            pasFields[i].dDouble = AVCRawBinReadDouble(psFile);
        }
        else
        {
            /*---------------------------------------------------------
             * Hummm... unsupported field type...
             *--------------------------------------------------------*/
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported field type: (type=%d, size=%d)",
                     nType, pasDef[i].nSize);
            return -1;
        }

        nBytesRead += pasDef[i].nSize;
    }

    /*-----------------------------------------------------------------
     * Record size is rounded to a multiple of 2 bytes.
     * Check the number of bytes read, and move the read pointer if
     * necessary.
     *----------------------------------------------------------------*/
    if (nBytesRead < nRecordSize)
        AVCRawBinFSeek(psFile, nRecordSize - nBytesRead, SEEK_CUR);

    return 0;
}

/*=====================================================================
 *                         PC Arc/Info DBF TABLEs
 *====================================================================*/

void _AVCBinReadRepairDBFFieldName(char *pszFieldName);

/**********************************************************************
 *                         _AVCBinReadOpenDBFTable()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinReadOpen() with type AVCCoverPC/AVCFileTABLE instead)
 *
 * Open the DBF table, reads the header information and inits the
 * AVCBinFile handle to be ready to read records from it.
 *
 * Returns a valid AVCBinFile handle, or NULL if the file could
 * not be opened.
 *
 * _AVCBinReadCloseDBFTable() will eventually have to be called to release the 
 * resources used by the AVCBinFile structure.
 **********************************************************************/
AVCBinFile *_AVCBinReadOpenDBFTable(const char *pszDBFFilename,
                                    const char *pszArcInfoTableName)
{
    AVCBinFile    *psFile;
    DBFHandle     hDBFFile = NULL;
    int            iField;
    AVCTableDef   *psTableDef;
    AVCFieldInfo  *pasFieldDef;

    /*-----------------------------------------------------------------
     * Try to open the DBF file
     *----------------------------------------------------------------*/
    if ( (hDBFFile = DBFOpen(pszDBFFilename, "rb")) == NULL)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Failed to open table %s", pszDBFFilename);
        return NULL;
    }

    /*-----------------------------------------------------------------
     * Alloc. and init. the AVCBinFile structure.
     *----------------------------------------------------------------*/
    psFile = (AVCBinFile*)CPLCalloc(1, sizeof(AVCBinFile));

    psFile->hDBFFile = hDBFFile;

    psFile->eCoverType = AVCCoverPC;
    psFile->eFileType = AVCFileTABLE;
    psFile->pszFilename = CPLStrdup(pszDBFFilename);

    psFile->hdr.psTableDef = NULL;

    /* nCurDBFRecord is used to keep track of the 0-based index of the
     * last record we read from the DBF file... this is to emulate 
     * sequential access which is assumed by the rest of the lib.
     * Since the first record (record 0) has not been read yet, then
     * we init the index at -1.
     */
    psFile->nCurDBFRecord = -1;

    /* We can't really tell the precision from a Table header...
     * just set an arbitrary value... it probably won't be used anyways!
     */
    psFile->nPrecision = AVC_SINGLE_PREC;

    /*-----------------------------------------------------------------
     * Build TableDef from the info in the DBF header
     *----------------------------------------------------------------*/
    /* Use calloc() to init some unused struct members */
    psTableDef = (AVCTableDef*)CPLCalloc(1, sizeof(AVCTableDef));
    psFile->hdr.psTableDef = psTableDef;

    sprintf(psTableDef->szTableName, "%-32.32s", pszArcInfoTableName);

    psTableDef->numFields = DBFGetFieldCount(hDBFFile);

    /* We'll compute nRecSize value when we read fields info later */
    psTableDef->nRecSize = 0;  

    psTableDef->numRecords = DBFGetRecordCount(hDBFFile);

    /* All DBF tables are considered External */
    strcpy(psTableDef->szExternal, "XX");

    /*-----------------------------------------------------------------
     * Build Field definitions
     *----------------------------------------------------------------*/
    pasFieldDef = (AVCFieldInfo*)CPLCalloc(psTableDef->numFields,
                                           sizeof(AVCFieldInfo));

    psTableDef->pasFieldDef = pasFieldDef;

    for(iField=0; iField< psTableDef->numFields; iField++)
    {
        int nWidth, nDecimals;
        DBFFieldType eDBFType;
        char         cNativeType;

        /*-------------------------------------------------------------
         * Fetch DBF Field info and convert to Arc/Info type... 
         * Note that since DBF fields names are limited to 10 chars, 
         * we do not have to worry about field name length in the process.
         *------------------------------------------------------------*/
        eDBFType = DBFGetFieldInfo(hDBFFile, iField, 
                                   pasFieldDef[iField].szName,
                                   &nWidth, &nDecimals);
        cNativeType = DBFGetNativeFieldType(hDBFFile, iField);

        pasFieldDef[iField].nFmtWidth = (GInt16)nWidth;
        pasFieldDef[iField].nFmtPrec = (GInt16)nDecimals;

        /* nIndex is the 1-based field index that we see in the E00 header */
        pasFieldDef[iField].nIndex = iField+1;

        if (cNativeType == 'F' || (cNativeType == 'N' && nDecimals > 0) )
        {
            /*---------------------------------------------------------
             * BINARY FLOAT
             *--------------------------------------------------------*/
            pasFieldDef[iField].nType1 = AVC_FT_BINFLOAT/10;
            pasFieldDef[iField].nSize = 4;
            pasFieldDef[iField].nFmtWidth = 12; /* PC Arc/Info ignores the */
            pasFieldDef[iField].nFmtPrec = 3;   /* DBF width/precision     */
        }
        else if (cNativeType == 'N')
        {
            /*---------------------------------------------------------
             * BINARY INTEGER
             *--------------------------------------------------------*/
            pasFieldDef[iField].nType1 = AVC_FT_BININT/10;
            pasFieldDef[iField].nSize = 4;
            pasFieldDef[iField].nFmtWidth = 5;  /* PC Arc/Info ignores the */
            pasFieldDef[iField].nFmtPrec = -1;  /* DBF width/precision     */
        
            /*---------------------------------------------------------
             * Some special integer fields need to have their names 
             * repaired because DBF does not support special characters.
             *--------------------------------------------------------*/
            _AVCBinReadRepairDBFFieldName(pasFieldDef[iField].szName);
        }
        else if (cNativeType == 'D')
        {
            /*---------------------------------------------------------
             * DATE - Actually handled as a string internally
             *--------------------------------------------------------*/
            pasFieldDef[iField].nType1 = AVC_FT_DATE/10;
            pasFieldDef[iField].nSize = nWidth;
            pasFieldDef[iField].nFmtPrec = -1;

        }
        else /* (cNativeType == 'C' || cNativeType == 'L') */
        {
            /*---------------------------------------------------------
             * CHAR STRINGS ... and all unknown types also handled as strings
             *--------------------------------------------------------*/
            pasFieldDef[iField].nType1 = AVC_FT_CHAR/10;
            pasFieldDef[iField].nSize = nWidth;
            pasFieldDef[iField].nFmtPrec = -1;

        }

        /*---------------------------------------------------------
         * Keep track of position of field in record... first one always
         * starts at offset=1
         *--------------------------------------------------------*/
        if (iField == 0)
            pasFieldDef[iField].nOffset = 1;
        else
            pasFieldDef[iField].nOffset = (pasFieldDef[iField-1].nOffset +
                                            pasFieldDef[iField-1].nSize );

        /*---------------------------------------------------------
         * Set default values for all other unused members in the struct
         *--------------------------------------------------------*/
        pasFieldDef[iField].v2     = -1;  /* Always -1 ? */
        pasFieldDef[iField].v4     = 4;   /* Always 4 ?  */
        pasFieldDef[iField].v5     = -1;  /* Always -1 ? */
        pasFieldDef[iField].nType2 = 0;   /* Always 0 ?  */
        pasFieldDef[iField].v10    = -1;  /* Always -1 ? */
        pasFieldDef[iField].v11    = -1;  /* Always -1 ? */
        pasFieldDef[iField].v12    = -1;  /* Always -1 ? */
        pasFieldDef[iField].v13    = -1;  /* Always -1 ? */

    }

    /*-----------------------------------------------------------------
     * Compute record size...
     * Record size has to be rounded to a multiple of 2 bytes.
     *----------------------------------------------------------------*/
    if (psTableDef->numFields > 0)
    {
        psTableDef->nRecSize = (pasFieldDef[psTableDef->numFields-1].nOffset-1+
                                pasFieldDef[psTableDef->numFields-1].nSize);
        psTableDef->nRecSize = ((psTableDef->nRecSize+1)/2)*2;
    }
    else
        psTableDef->nRecSize = 0;

    /*-----------------------------------------------------------------
     * Allocate temp. structures to use to read records from the file
     * And allocate buffers for those fields that are stored as strings.
     *----------------------------------------------------------------*/
    psFile->cur.pasFields = (AVCField*)CPLCalloc(psTableDef->numFields,
                                                 sizeof(AVCField));

    for(iField=0; iField<psTableDef->numFields; iField++)
    {
        if (pasFieldDef[iField].nType1*10 == AVC_FT_DATE ||
            pasFieldDef[iField].nType1*10 == AVC_FT_CHAR ||
            pasFieldDef[iField].nType1*10 == AVC_FT_FIXINT ||
            pasFieldDef[iField].nType1*10 == AVC_FT_FIXNUM )
        {
            psFile->cur.pasFields[iField].pszStr = 
                (GByte*)CPLCalloc(pasFieldDef[iField].nSize+1, sizeof(GByte));
        }
    }

    return psFile;
}


/**********************************************************************
 *                         _AVCBinReadNextDBFTableRec()
 *
 * (This function is for internal library use... external calls should
 * go to AVCBinReadNextTableRec() instead)
 *
 * Reads the next record from a AVCCoverPC DBF attribute table and fills the 
 * pasFields[] array.
 *
 * Note that it is assumed that the pasFields[] array has been properly
 * initialized, re the allocation of buffers for fields stored as
 * strings.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
int _AVCBinReadNextDBFTableRec(DBFHandle hDBFFile, int *piRecordIndex, 
                                      int nFields, AVCFieldInfo *pasDef,
                                      AVCField *pasFields)
{
    int         i, nType;

    /*-----------------------------------------------------------------
     * Increment current record index.
     * We use nCurDBFRecord to keep track of the 0-based index of the
     * last record we read from the DBF file... this is to emulate 
     * sequential access which is assumed by the rest of the lib.
     *----------------------------------------------------------------*/
    if (hDBFFile == NULL || piRecordIndex == NULL || 
        pasDef == NULL || pasFields == NULL)
        return -1;

    (*piRecordIndex)++;

    if (*piRecordIndex >= DBFGetRecordCount(hDBFFile))
        return -1;  /* Reached EOF */

    /*-----------------------------------------------------------------
     * Read/convert each field based on type
     *----------------------------------------------------------------*/
    for(i=0; i<nFields; i++)
    {
        nType = pasDef[i].nType1*10;

        if (nType ==  AVC_FT_DATE || nType == AVC_FT_CHAR ||
            nType == AVC_FT_FIXINT || nType == AVC_FT_FIXNUM)
        {
            /*---------------------------------------------------------
             * Values stored as strings
             *--------------------------------------------------------*/
            const char *pszValue;
            pszValue = DBFReadStringAttribute(hDBFFile, 
                                              *piRecordIndex, i);
            strncpy((char*)pasFields[i].pszStr, pszValue, pasDef[i].nSize);
            pasFields[i].pszStr[pasDef[i].nSize] = '\0';
        }
        else if (nType == AVC_FT_BININT && pasDef[i].nSize == 4)
        {
            /*---------------------------------------------------------
             * 32 bit binary integers
             *--------------------------------------------------------*/
            pasFields[i].nInt32 = DBFReadIntegerAttribute(hDBFFile, 
                                                          *piRecordIndex, i);
        }
        else if (nType == AVC_FT_BININT && pasDef[i].nSize == 2)
        {
            /*---------------------------------------------------------
             * 16 bit binary integers
             *--------------------------------------------------------*/
            pasFields[i].nInt16 = (GInt16)DBFReadIntegerAttribute(hDBFFile, 
                                                               *piRecordIndex,
                                                                  i);
        }
        else if (nType == AVC_FT_BINFLOAT && pasDef[i].nSize == 4)
        {
            /*---------------------------------------------------------
             * Single precision floats
             *--------------------------------------------------------*/
            pasFields[i].fFloat = (float)DBFReadDoubleAttribute(hDBFFile, 
                                                                *piRecordIndex,
                                                                i);
        }
        else if (nType == AVC_FT_BINFLOAT && pasDef[i].nSize == 8)
        {
            /*---------------------------------------------------------
             * Double precision floats
             *--------------------------------------------------------*/
            pasFields[i].dDouble = DBFReadDoubleAttribute(hDBFFile, 
                                                          *piRecordIndex,
                                                          i);
        }
        else
        {
            /*---------------------------------------------------------
             * Hummm... unsupported field type...
             *--------------------------------------------------------*/
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unsupported field type: (type=%d, size=%d)",
                     nType, pasDef[i].nSize);
            return -1;
        }

    }

    return 0;
}


/**********************************************************************
 *                         _AVCBinReadRepairDBFFieldName()
 *
 * Attempt to repair some special integer field names that usually
 * carry special chars such as '#' or '-' but that are lost because of
 * DBF limitations and are replaced by '_'.
 *
 **********************************************************************/
void _AVCBinReadRepairDBFFieldName(char *pszFieldName)
{
    char *pszTmp;

    if ((pszTmp = strrchr(pszFieldName, '_')) == NULL)
        return;  /* No special char to process */

    /*-----------------------------------------------------------------
     * Replace '_' at end of field name by a '#', as in:
     *   COVER# , FNODE#, TNODE#, LPOLY#, RPOLY#
     *
     * and replace names that end with "_ID" with "-ID" as in COVER-ID
     *----------------------------------------------------------------*/
    if (EQUAL(pszTmp, "_"))
        *pszTmp = '#';
    else if (EQUAL(pszTmp, "_ID"))
        *pszTmp = '-';

}



