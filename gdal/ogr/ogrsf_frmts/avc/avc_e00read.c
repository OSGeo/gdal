/**********************************************************************
 * $Id: avc_e00read.c,v 1.28 2008/07/30 19:22:18 dmorissette Exp $
 *
 * Name:     avc_e00read.c
 * Project:  Arc/Info vector coverage (AVC)  BIN->E00 conversion library
 * Language: ANSI C
 * Purpose:  Functions to open a binary coverage and read it as if it
 *           was an ASCII E00 file.  This file is the main entry point
 *           for the library.
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
 * $Log: avc_e00read.c,v $
 * Revision 1.28  2008/07/30 19:22:18  dmorissette
 * Move detection of EXP header directly in AVCE00ReadOpenE00() and use
 * VSIFGets() instead of CPLReadLine() to avoid problem with huge one line
 * files (GDAL/OGR ticket #1989)
 *
 * Revision 1.27  2008/07/30 18:35:53  dmorissette
 * Avoid scanning the whole E00 input file in AVCE00ReadOpenE00() if the
 * file does not start with an EXP line (GDAL/OGR ticket 1989)
 *
 * Revision 1.26  2008/07/30 16:17:46  dmorissette
 * Detect compressed E00 input files and refuse to open them instead of
 * crashing (bug 1928, GDAL/OGR ticket 2513)
 *
 * Revision 1.25  2008/07/24 20:34:12  dmorissette
 * Fixed VC++ WIN32 build problems in GDAL/OGR environment
 * (GDAL/OGR ticket http://trac.osgeo.org/gdal/ticket/2500)
 *
 * Revision 1.24  2008/07/24 13:49:20  dmorissette
 * Fixed GCC compiler warning (GDAL ticket #2495)
 *
 * Revision 1.23  2006/08/17 19:51:01  dmorissette
 * #include <unistd.h> to solve warning on 64 bit platforms (bug 1461)
 *
 * Revision 1.22  2006/08/17 18:56:42  dmorissette
 * Support for reading standalone info tables (just tables, no coverage
 * data) by pointing AVCE00ReadOpen() to the info directory (bug 1549).
 *
 * Revision 1.21  2006/06/27 18:38:43  dmorissette
 * Cleaned up E00 reading (bug 1497, patch from James F.)
 *
 * Revision 1.20  2006/06/27 18:06:34  dmorissette
 * Applied patch for EOP processing from James F. (bug 1497)
 *
 * Revision 1.19  2006/06/16 11:48:11  daniel
 * New functions to read E00 files directly as opposed to translating to
 * binary coverage. Used in the implementation of E00 read support in OGR.
 * Contributed by James E. Flemer. (bug 1497)
 *
 * Revision 1.18  2006/06/14 16:31:28  daniel
 * Added support for AVCCoverPC2 type (bug 1491)
 *
 * Revision 1.17  2005/06/03 03:49:58  daniel
 * Update email address, website url, and copyright dates
 *
 * Revision 1.16  2004/07/14 18:49:50  daniel
 * Fixed leak when trying to open something that's not a coverage (bug513)
 *
 * Revision 1.15  2002/08/27 15:46:15  daniel
 * Applied fix made in GDAL/OGR by 'aubin' (moved include ctype.h after avc.h)
 *
 * Revision 1.14  2000/09/22 19:45:21  daniel
 * Switch to MIT-style license
 *
 * Revision 1.13  2000/05/29 15:31:31  daniel
 * Added Japanese DBCS support
 *
 * Revision 1.12  2000/02/14 17:21:01  daniel
 * Made more robust for corrupted or invalid files in cover directory
 *
 * Revision 1.11  2000/02/02 04:26:04  daniel
 * Support reading TX6/TX7/RXP/RPL files in weird coverages
 *
 * Revision 1.10  2000/01/10 02:56:30  daniel
 * Added read support for "weird" coverages
 *
 * Revision 1.9  2000/01/07 07:12:49  daniel
 * Added support for reading PC Coverage TXT files
 *
 * Revision 1.8  1999/12/24 07:41:08  daniel
 * Check fname length before testing for extension in AVCE00ReadFindCoverType()
 *
 * Revision 1.7  1999/12/24 07:18:34  daniel
 * Added PC Arc/Info coverages support
 *
 * Revision 1.6  1999/08/26 17:22:18  daniel
 * Use VSIFopen() instead of fopen() directly
 *
 * Revision 1.5  1999/08/23 18:21:41  daniel
 * New syntax for AVCBinReadListTables()
 *
 * Revision 1.4  1999/05/11 02:10:01  daniel
 * Free psInfo struct inside AVCE00ReadClose()
 *
 * Revision 1.3  1999/04/06 19:43:26  daniel
 * Added E00 coverage path in EXP 0  header line
 *
 * Revision 1.2  1999/02/25 04:19:01  daniel
 * Added TXT, TX6/TX7, RXP and RPL support + other minor changes
 *
 * Revision 1.1  1999/01/29 16:28:52  daniel
 * Initial revision
 *
 **********************************************************************/

#include "avc.h"

#ifdef WIN32
#  include <direct.h>   /* getcwd() */
#else
#  include <unistd.h>   /* getcwd() */
#endif

#include <ctype.h>      /* toupper() */

static void _AVCE00ReadScanE00(AVCE00ReadE00Ptr psRead);
static int _AVCE00ReadBuildSqueleton(AVCE00ReadPtr psInfo,
                                     char **papszCoverDir);
static AVCCoverType _AVCE00ReadFindCoverType(char **papszCoverDir);


/**********************************************************************
 *                          AVCE00ReadOpen()
 *
 * Open a Arc/Info coverage to read it as if it was an E00 file.
 *
 * You can either pass the name of the coverage directory, or the path
 * to one of the files in the coverage directory.  The name of the
 * coverage MUST be included in pszCoverPath... this means that
 * passing "." is invalid.
 * The following are all valid values for pszCoverPath:
 *               /home/data/country
 *               /home/data/country/
 *               /home/data/country/arc.adf
 * (Of course you should replace the '/' with '\\' on DOS systems!)
 *
 * Returns a new AVCE00ReadPtr handle or NULL if the coverage could 
 * not be opened or if it does not appear to be a valid Arc/Info coverage.
 *
 * The handle will eventually have to be released with AVCE00ReadClose().
 **********************************************************************/
AVCE00ReadPtr  AVCE00ReadOpen(const char *pszCoverPath)
{
    AVCE00ReadPtr   psInfo;
    int             i, nLen, nCoverPrecision;
    VSIStatBuf      sStatBuf;
    char            **papszCoverDir = NULL;

    CPLErrorReset();

    /*-----------------------------------------------------------------
     * pszCoverPath must be either a valid directory name or a valid
     * file name.
     *----------------------------------------------------------------*/
    if (pszCoverPath == NULL || strlen(pszCoverPath) == 0 ||
        VSIStat(pszCoverPath, &sStatBuf) == -1)
    {
        CPLError(CE_Failure, CPLE_OpenFailed, 
                 "Invalid coverage path: %s.", 
                 pszCoverPath?pszCoverPath:"(NULL)");
        return NULL;
    }

    /*-----------------------------------------------------------------
     * Alloc the AVCE00ReadPtr handle
     *----------------------------------------------------------------*/
    psInfo = (AVCE00ReadPtr)CPLCalloc(1, sizeof(struct AVCE00ReadInfo_t));

    /*-----------------------------------------------------------------
     * 2 possibilities about the value passed in pszCoverPath:
     * - It can be the directory name of the coverage
     * - or it can be the path to one of the files in the coverage
     *
     * If the name passed in pszCoverPath is not a directory, then we
     * need to strip the last part of the filename to keep only the
     * path, terminated by a '/' (or a '\\').
     *----------------------------------------------------------------*/
    if (VSI_ISDIR(sStatBuf.st_mode))
    {
        /*-------------------------------------------------------------
         * OK, we have a valid directory name... make sure it is 
         * terminated with a '/' (or '\\')
         *------------------------------------------------------------*/
        nLen = strlen(pszCoverPath);

        if (pszCoverPath[nLen-1] == '/' || pszCoverPath[nLen-1] == '\\')
            psInfo->pszCoverPath = CPLStrdup(pszCoverPath);
        else
        {
#ifdef WIN32
            psInfo->pszCoverPath = CPLStrdup(CPLSPrintf("%s\\",pszCoverPath));
#else
            psInfo->pszCoverPath = CPLStrdup(CPLSPrintf("%s/",pszCoverPath));
#endif
        }
    }
    else
    {
        /*-------------------------------------------------------------
         * We are dealing with a filename.
         * Extract the coverage path component and store it.
         * The coverage path will remain terminated by a '/' or '\\' char.
         *------------------------------------------------------------*/
        psInfo->pszCoverPath = CPLStrdup(pszCoverPath);

        for( i = strlen(psInfo->pszCoverPath)-1; 
             i > 0 && psInfo->pszCoverPath[i] != '/' &&
                 psInfo->pszCoverPath[i] != '\\';
             i-- ) {}

        psInfo->pszCoverPath[i+1] = '\0';
    }


    /*-----------------------------------------------------------------
     * Extract the coverage name from the coverage path.  Note that
     * for this the coverage path must be in the form:
     * "dir1/dir2/dir3/covername/" ... if it is not the case, then
     * we would have to use getcwd() to find the current directory name...
     * but for now we'll just produce an error if this happens.
     *----------------------------------------------------------------*/
    nLen = 0;
    for( i = strlen(psInfo->pszCoverPath)-1; 
	 i > 0 && psInfo->pszCoverPath[i-1] != '/' &&
	          psInfo->pszCoverPath[i-1] != '\\'&&
	          psInfo->pszCoverPath[i-1] != ':';
	 i-- ) 
    {
        nLen++;
    }

    if (nLen > 0)
    {
        psInfo->pszCoverName = CPLStrdup(psInfo->pszCoverPath+i);
        psInfo->pszCoverName[nLen] = '\0';
    }
    else
    {
        CPLError(CE_Failure, CPLE_OpenFailed, 
                 "Invalid coverage path (%s): "
                 "coverage name must be included in path.", pszCoverPath);

        CPLFree(psInfo->pszCoverPath);
        CPLFree(psInfo);
        return NULL;
    }

    /*-----------------------------------------------------------------
     * Read the coverage directory listing and try to establish the cover type
     *----------------------------------------------------------------*/
    papszCoverDir = CPLReadDir(psInfo->pszCoverPath);

    psInfo->eCoverType = _AVCE00ReadFindCoverType(papszCoverDir);

    if (psInfo->eCoverType == AVCCoverTypeUnknown  )
    {
        CPLError(CE_Failure, CPLE_OpenFailed, 
                 "Invalid coverage (%s): directory does not appear to "
                 "contain any supported vector coverage file.",  pszCoverPath);
        CPLFree(psInfo->pszCoverName);
        CPLFree(psInfo->pszCoverPath);
        CPLFree(psInfo->pszInfoPath);
        CPLFree(psInfo);
        CSLDestroy(papszCoverDir);
        return NULL;
    }

   
    /*-----------------------------------------------------------------
     * INFO path: PC Coverages have all files in the same dir, and unix
     * covers have the INFO files in ../info
     *----------------------------------------------------------------*/
    if (psInfo->eCoverType == AVCCoverPC || psInfo->eCoverType == AVCCoverPC2)
    {
        psInfo->pszInfoPath = CPLStrdup(psInfo->pszCoverPath);
    }
    else
    {
        /*-------------------------------------------------------------
         * Lazy way to build the INFO path: simply add "../info/"...
         * this could probably be improved!
         *------------------------------------------------------------*/
        psInfo->pszInfoPath =(char*)CPLMalloc((strlen(psInfo->pszCoverPath)+9)*
                                           sizeof(char));
#ifdef WIN32
#  define AVC_INFOPATH "..\\info\\"
#else
#  define AVC_INFOPATH "../info/"
#endif
        sprintf(psInfo->pszInfoPath, "%s%s", psInfo->pszCoverPath, 
                                             AVC_INFOPATH);

        AVCAdjustCaseSensitiveFilename(psInfo->pszInfoPath);
    }

    /*-----------------------------------------------------------------
     * For Unix coverages, check that the info directory exists and 
     * contains the "arc.dir".  In AVCCoverWeird, the arc.dir is 
     * called "../INFO/ARCDR9".
     * PC Coverages have their info tables in the same direcotry as 
     * the coverage files.
     *----------------------------------------------------------------*/
    if (((psInfo->eCoverType == AVCCoverV7 || 
          psInfo->eCoverType == AVCCoverV7Tables) &&
         ! AVCFileExists(psInfo->pszInfoPath, "arc.dir") ) ||
         (psInfo->eCoverType == AVCCoverWeird &&
         ! AVCFileExists(psInfo->pszInfoPath, "arcdr9") ) )
    {
        CPLError(CE_Failure, CPLE_OpenFailed, 
             "Invalid coverage (%s): 'info' directory not found or invalid.", 
                                              pszCoverPath);
        CPLFree(psInfo->pszCoverName);
        CPLFree(psInfo->pszCoverPath);
        CPLFree(psInfo->pszInfoPath);
        CPLFree(psInfo);
        CSLDestroy(papszCoverDir);
        return NULL;
    }

    /*-----------------------------------------------------------------
     * Make sure there was no error until now before we build squeleton.
     *----------------------------------------------------------------*/
    if (CPLGetLastErrorNo() != 0)
    {
        CPLFree(psInfo->pszCoverName);
        CPLFree(psInfo->pszCoverPath);
        CPLFree(psInfo->pszInfoPath);
        CPLFree(psInfo);
        CSLDestroy(papszCoverDir);
        return NULL;
    }

    /*-----------------------------------------------------------------
     * Build the E00 file squeleton and be ready to return a E00 header...
     * We'll also read the coverage precision by the same way.
     *----------------------------------------------------------------*/
    nCoverPrecision = _AVCE00ReadBuildSqueleton(psInfo, papszCoverDir);

    /* Ignore warnings produced while building squeleton */
    CPLErrorReset();

    CSLDestroy(papszCoverDir);
    papszCoverDir = NULL;

    psInfo->iCurSection = 0;
    psInfo->iCurStep = AVC_GEN_NOTSTARTED;
    psInfo->bReadAllSections = TRUE;

    /*-----------------------------------------------------------------
     * Init the E00 generator.
     *----------------------------------------------------------------*/
    psInfo->hGenInfo = AVCE00GenInfoAlloc(nCoverPrecision);

    /*-----------------------------------------------------------------
     * Init multibyte encoding info
     *----------------------------------------------------------------*/
    psInfo->psDBCSInfo = AVCAllocDBCSInfo();

    /*-----------------------------------------------------------------
     * If an error happened during the open call, cleanup and return NULL.
     *----------------------------------------------------------------*/
    if (CPLGetLastErrorNo() != 0)
    {
        AVCE00ReadClose(psInfo);
        psInfo = NULL;
    }

    return psInfo;
}

/**********************************************************************
 *                          AVCE00ReadOpenE00()
 *
 * Open a E00 file for reading.
 *
 * Returns a new AVCE00ReadE00Ptr handle or NULL if the file could
 * not be opened or if it does not appear to be a valid E00 file.
 *
 * The handle will eventually have to be released with
 * AVCE00ReadCloseE00().
 **********************************************************************/
AVCE00ReadE00Ptr AVCE00ReadOpenE00(const char *pszE00FileName)
{
    AVCE00ReadE00Ptr psRead;
    VSIStatBuf       sStatBuf;
    FILE             *fp;
    char             *p;
    char             szHeader[10];

    CPLErrorReset();

    /*-----------------------------------------------------------------
     * pszE00FileName must be a valid file that can be opened for
     * reading
     *----------------------------------------------------------------*/
    if (pszE00FileName == NULL || strlen(pszE00FileName) == 0 ||
        VSIStat(pszE00FileName, &sStatBuf) == -1 ||
        VSI_ISDIR(sStatBuf.st_mode))
    {
        CPLError(CE_Failure, CPLE_OpenFailed, 
                 "Invalid E00 file path: %s.", 
                 pszE00FileName?pszE00FileName:"(NULL)");
        return NULL;
    }

    if (NULL == (fp = VSIFOpen(pszE00FileName, "r")))
        return NULL;

    /*-----------------------------------------------------------------
     * Make sure the file starts with a "EXP  0" or "EXP  1" header
     *----------------------------------------------------------------*/
    if (VSIFGets(szHeader, 5, fp) == NULL || !EQUALN("EXP ", szHeader, 4) )
    {
        CPLError(CE_Failure, CPLE_OpenFailed, 
                 "This does not look like a E00 file: does not start with "
                 "a EXP header." );
        VSIFClose(fp);
        return NULL;
    }
    VSIRewind(fp);

    /*-----------------------------------------------------------------
     * Alloc the AVCE00ReadE00Ptr handle
     *----------------------------------------------------------------*/
    psRead = (AVCE00ReadE00Ptr)CPLCalloc(1,
            sizeof(struct AVCE00ReadInfoE00_t));

    psRead->hFile = fp;
    psRead->pszCoverPath = CPLStrdup(pszE00FileName);
    psRead->eCurFileType = AVCFileUnknown;

    /*-----------------------------------------------------------------
     * Extract the coverage name from the coverage path.
     *----------------------------------------------------------------*/
    if (NULL != (p = strrchr(psRead->pszCoverPath, '/')) ||
        NULL != (p = strrchr(psRead->pszCoverPath, '\\')) ||
        NULL != (p = strrchr(psRead->pszCoverPath, ':')))
    {
        psRead->pszCoverName = CPLStrdup(p + 1);
    }
    else
    {
        psRead->pszCoverName = CPLStrdup(psRead->pszCoverPath);
    }
    if (NULL != (p = strrchr(psRead->pszCoverName, '.')))
    {
        *p = '\0';
    }

    /*-----------------------------------------------------------------
     * Make sure there was no error until now before we scan file.
     *----------------------------------------------------------------*/
    if (CPLGetLastErrorNo() != 0)
    {
        AVCE00ReadCloseE00(psRead);
        return NULL;
    }

    psRead->hParseInfo = AVCE00ParseInfoAlloc();

    /*-----------------------------------------------------------------
     * Scan the E00 file for sections
     *----------------------------------------------------------------*/
    _AVCE00ReadScanE00(psRead);
    if (CPLGetLastErrorNo() != 0)
    {
        AVCE00ReadCloseE00(psRead);
        return NULL;
    }

    AVCE00ReadRewindE00(psRead);
    CPLErrorReset();

    if (psRead->numSections < 1)
    {
        AVCE00ReadCloseE00(psRead);
        return NULL;
    }

    psRead->bReadAllSections = TRUE;

    /*-----------------------------------------------------------------
     * If an error happened during the open call, cleanup and return NULL.
     *----------------------------------------------------------------*/
    if (CPLGetLastErrorNo() != 0)
    {
        AVCE00ReadCloseE00(psRead);
        psRead = NULL;
    }

    return psRead;
}

/**********************************************************************
 *                          AVCE00ReadClose()
 *
 * Close a coverage and release all memory used by the AVCE00ReadPtr
 * handle.
 **********************************************************************/
void AVCE00ReadClose(AVCE00ReadPtr psInfo)
{
    CPLErrorReset();

    if (psInfo == NULL)
        return;

    CPLFree(psInfo->pszCoverPath);
    CPLFree(psInfo->pszInfoPath);
    CPLFree(psInfo->pszCoverName);

    if (psInfo->hFile)
        AVCBinReadClose(psInfo->hFile);

    if (psInfo->hGenInfo)
        AVCE00GenInfoFree(psInfo->hGenInfo);

    if (psInfo->pasSections)
    {
        int i;
        for(i=0; i<psInfo->numSections; i++)
        {
            CPLFree(psInfo->pasSections[i].pszName);
            CPLFree(psInfo->pasSections[i].pszFilename);
        }
        CPLFree(psInfo->pasSections);
    }

    AVCFreeDBCSInfo(psInfo->psDBCSInfo);

    CPLFree(psInfo);
}

/**********************************************************************
 *                          AVCE00ReadCloseE00()
 *
 * Close a coverage and release all memory used by the AVCE00ReadE00Ptr
 * handle.
 **********************************************************************/
void AVCE00ReadCloseE00(AVCE00ReadE00Ptr psRead)
{
    if (psRead == NULL)
        return;

    CPLFree(psRead->pszCoverPath);
    CPLFree(psRead->pszCoverName);

    if (psRead->hFile)
    {
        VSIFClose(psRead->hFile);
        psRead->hFile = 0;
    }

    if (psRead->pasSections)
    {
        int i;
        for(i=0; i<psRead->numSections; i++)
        {
            CPLFree(psRead->pasSections[i].pszName);
            CPLFree(psRead->pasSections[i].pszFilename);
        }
        CPLFree(psRead->pasSections);
    }

    /* These Free calls handle NULL's */
    AVCE00ParseInfoFree(psRead->hParseInfo);
    psRead->hParseInfo = NULL;

    CPLFree(psRead);
}


/**********************************************************************
 *                          _AVCIncreaseSectionsArray()
 *
 * Add a number of structures to the Sections array and return the
 * index of the first one that was added.  Note that the address of the
 * original array (*pasArray) is quite likely to change!
 *
 * The value of *pnumItems will be updated to reflect the new array size.
 **********************************************************************/
static int _AVCIncreaseSectionsArray(AVCE00Section **pasArray, int *pnumItems,
                                    int numToAdd)
{
    int i;

    *pasArray = (AVCE00Section*)CPLRealloc(*pasArray, 
                                           (*pnumItems+numToAdd)*
                                                    sizeof(AVCE00Section));

    for(i=0; i<numToAdd; i++)
    {
        (*pasArray)[*pnumItems+i].eType = AVCFileUnknown;
        (*pasArray)[*pnumItems+i].pszName = NULL;
        (*pasArray)[*pnumItems+i].pszFilename = NULL;
        (*pasArray)[*pnumItems+i].nLineNum = 0;
        (*pasArray)[*pnumItems+i].nFeatureCount = -1;
    }

    i = *pnumItems;
    (*pnumItems) += numToAdd;

    return i;
}

/**********************************************************************
 *                         _AVCE00ReadFindCoverType()
 *
 * This functions tries to establish the coverage type by looking
 * at the coverage directory listing passed as argument.
 *
 * Returns one of AVCCoverV7 for Arc/Info V7 (Unix) coverages, or
 *                AVCCoverPC for PC Arc/Info coverages.
 *                AVCCoverWeird for an hybrid between V7 and PC
 *
 * If coverage type cannot be established then AVCCoverTypeUnknown is 
 * returned.
 **********************************************************************/
static AVCCoverType _AVCE00ReadFindCoverType(char **papszCoverDir)
{
    int         i, nLen;
    GBool       bFoundAdfFile=FALSE, bFoundArcFile=FALSE, 
                bFoundTableFile=FALSE, bFoundDbfFile=FALSE,
                bFoundArcDirFile=FALSE;

    /*-----------------------------------------------------------------
     * Scan the list of files, looking for well known filenames.
     * Start with the funky types first...
     *----------------------------------------------------------------*/
    for(i=0; papszCoverDir && papszCoverDir[i]; i++)
    {
        nLen = strlen(papszCoverDir[i]);
        if (nLen > 4 && EQUAL(papszCoverDir[i]+nLen-4, ".adf") )
        {
            bFoundAdfFile = TRUE;
        }
        else if (nLen > 4 && EQUAL(papszCoverDir[i]+nLen-4, ".dbf") )
        {
            bFoundDbfFile = TRUE;
        }
        else if (EQUAL(papszCoverDir[i], "arc") ||
                 EQUAL(papszCoverDir[i], "cnt") ||
                 EQUAL(papszCoverDir[i], "pal") ||
                 EQUAL(papszCoverDir[i], "lab") ||
                 EQUAL(papszCoverDir[i], "prj") ||
                 EQUAL(papszCoverDir[i], "tol") )
        {
            bFoundArcFile = TRUE;
        }
        else if (EQUAL(papszCoverDir[i], "aat") ||
                 EQUAL(papszCoverDir[i], "pat") ||
                 EQUAL(papszCoverDir[i], "bnd") ||
                 EQUAL(papszCoverDir[i], "tic") )
        {
            bFoundTableFile = TRUE;
        }
        else if (EQUAL(papszCoverDir[i], "arc.dir") )
        {
            bFoundArcDirFile = TRUE;
        }

    }

    /*-----------------------------------------------------------------
     * Check for PC Arc/Info coverage - variant 1.
     * These PC coverages have files with no extension (e.g. "ARC","PAL",...)
     * and their tables filenames are in the form "???.dbf"
     *----------------------------------------------------------------*/
    if (bFoundArcFile && bFoundDbfFile)
        return AVCCoverPC;

    /*-----------------------------------------------------------------
     * Check for PC Arc/Info coverage - variant 2.
     * looks like a hybrid between AVCCoverPC and AVCCoverV7
     * These PC coverages have files with .adf extension (e.g."ARC.ADF"),
     * and their tables filenames are in the form "???.dbf"
     *----------------------------------------------------------------*/
    if (bFoundAdfFile && bFoundDbfFile)
        return AVCCoverPC2;

    /*-----------------------------------------------------------------
     * Check for the weird coverages.
     * Their coverage files have no extension just like PC Coverages, 
     * and their tables have 3 letters filenames with no extension
     * either (e.g. "AAT", "PAT", etc.)
     * They also have a ../info directory, but we don't really need
     * to check that (not yet!).
     *----------------------------------------------------------------*/
    if (bFoundArcFile && bFoundTableFile)
        return AVCCoverWeird;

    /*-----------------------------------------------------------------
     * V7 Coverages... they are the easiest to recognize
     * because of the ".adf" file extension
     *----------------------------------------------------------------*/
    if (bFoundAdfFile)
        return AVCCoverV7;

    /*-----------------------------------------------------------------
     * Standalone info tables.
     * We were pointed at the "info" directory. We'll treat this as
     * a coverage with just info tables.
     *----------------------------------------------------------------*/
    if (bFoundArcDirFile)
        return AVCCoverV7Tables;

    return AVCCoverTypeUnknown;
}


/**********************************************************************
 *                         _AVCE00ReadAddJabberwockySection()
 *
 * Add to the squeleton a section that contains subsections 
 * for all the files with a given extension.
 *
 * Returns Updated Coverage precision
 **********************************************************************/
static int _AVCE00ReadAddJabberwockySection(AVCE00ReadPtr psInfo,
                                            AVCFileType   eFileType,
                                            const char   *pszSectionName,
                                            int           nCoverPrecision,
                                            const char   *pszFileExtension,
                                            char        **papszCoverDir )
{
    int         iSect, iDirEntry, nLen, nExtLen;
    GBool       bFoundFiles = FALSE;
    AVCBinFile *psFile=NULL;

    nExtLen = strlen(pszFileExtension);

    /*-----------------------------------------------------------------
     * Scan the directory for files with a ".txt" extension.
     *----------------------------------------------------------------*/

    for (iDirEntry=0; papszCoverDir && papszCoverDir[iDirEntry]; iDirEntry++)
    {
        nLen = strlen(papszCoverDir[iDirEntry]);

        if (nLen > nExtLen && EQUAL(papszCoverDir[iDirEntry] + nLen-nExtLen, 
                                    pszFileExtension) &&
            (psFile = AVCBinReadOpen(psInfo->pszCoverPath, 
                                     papszCoverDir[iDirEntry],
                                     psInfo->eCoverType, eFileType,
                                     psInfo->psDBCSInfo)) != NULL)
        {
            if (nCoverPrecision == AVC_DEFAULT_PREC)
                nCoverPrecision = psFile->nPrecision;
            AVCBinReadClose(psFile);

            if (bFoundFiles == FALSE)
            {
                /* Insert a "TX6 #" header before the first TX6 file
                 */
                iSect = _AVCIncreaseSectionsArray(&(psInfo->pasSections), 
                                                  &(psInfo->numSections), 1);
                psInfo->pasSections[iSect].eType = AVCFileUnknown;

                psInfo->pasSections[iSect].pszName = 
                            CPLStrdup(CPLSPrintf("%s  %c", pszSectionName,
                                  (nCoverPrecision==AVC_DOUBLE_PREC)?'3':'2'));

                bFoundFiles = TRUE;
            }

            /* Add this file to the squeleton 
             */
            iSect = _AVCIncreaseSectionsArray(&(psInfo->pasSections), 
                                              &(psInfo->numSections), 1);

            psInfo->pasSections[iSect].eType = eFileType;
            psInfo->pasSections[iSect].pszFilename= 
                                   CPLStrdup(papszCoverDir[iDirEntry]);

            /* pszName will contain only the classname without the file 
             * extension */
            psInfo->pasSections[iSect].pszName =
                                   CPLStrdup(papszCoverDir[iDirEntry]);
            psInfo->pasSections[iSect].pszName[nLen-nExtLen] = '\0';
        }
    }

    if (bFoundFiles)
    {
        /* Add a line to close the TX6 section.
         */
        iSect = _AVCIncreaseSectionsArray(&(psInfo->pasSections), 
                                          &(psInfo->numSections), 1);
        psInfo->pasSections[iSect].eType = AVCFileUnknown;
        psInfo->pasSections[iSect].pszName = CPLStrdup("JABBERWOCKY");
    }

    return nCoverPrecision;
}

/**********************************************************************
 *                     _AVCE00ReadNextLineE00()
 *
 * Processes the next line of input from the E00 file.
 * (See AVCE00WriteNextLine() for similar processing.)
 *
 * Returns the next object from the E00 file, or NULL.
 **********************************************************************/
static void *_AVCE00ReadNextLineE00(AVCE00ReadE00Ptr psRead,
        const char *pszLine)
{
    int nStatus = 0;
    void *psObj = 0;

    AVCE00ParseInfo *psInfo = psRead->hParseInfo;

    CPLErrorReset();

    ++psInfo->nCurLineNum;

    if (psInfo->bForceEndOfSection)
    {
        /*-------------------------------------------------------------
         * The last call encountered an implicit end of section, so
         * we close the section now without waiting for an end-of-section
         * line (there won't be any!)... and get ready to proceed with
         * the next section.
         * This is used for TABLEs.
         *------------------------------------------------------------*/
        AVCE00ParseSectionEnd(psInfo, pszLine, TRUE);
        psRead->eCurFileType = AVCFileUnknown;
    }

    /*-----------------------------------------------------------------
     * If we're at the top level inside a supersection... check if this
     * supersection ends here.
     *----------------------------------------------------------------*/
    if (AVCE00ParseSuperSectionEnd(psInfo, pszLine) == TRUE)
    {
        /* Nothing to do... it's all been done by the call to 
         * AVCE00ParseSuperSectionEnd()
         */
    }
    else if (psRead->eCurFileType == AVCFileUnknown)
    {
        /*-------------------------------------------------------------
         * We're at the top level or inside a supersection... waiting 
         * to encounter a valid section or supersection header 
         * (i.e. "ARC  2", etc...)
         *------------------------------------------------------------*/

        /*-------------------------------------------------------------
         * First check for a supersection header (TX6, RXP, IFO, ...)
         *------------------------------------------------------------*/
        if ( AVCE00ParseSuperSectionHeader(psInfo,
                                           pszLine) == AVCFileUnknown )
        {
            /*---------------------------------------------------------
             * This was not a supersection header... check if it's a simple
             * section header
             *--------------------------------------------------------*/
            psRead->eCurFileType = AVCE00ParseSectionHeader(psInfo,
                    pszLine);
        }
        else
        {
            /* got supersection */
        }

        if (psRead->eCurFileType == AVCFileTABLE)
        {
            /*---------------------------------------------------------
             * send the first header line to the parser and wait until
             * the whole header has been read.
             *--------------------------------------------------------*/
            AVCE00ParseNextLine(psInfo, pszLine); 
        }
        else if (psRead->eCurFileType != AVCFileUnknown)
        {
            /*---------------------------------------------------------
             * found a valid section header
             *--------------------------------------------------------*/
        }
    }
    else if (psRead->eCurFileType == AVCFileTABLE &&
             ! psInfo->bTableHdrComplete )
    {
        /*-------------------------------------------------------------
         * We're reading a TABLE header... continue reading lines
         * from the header
         *
         * Note: When parsing a TABLE, the first object returned will 
         * be the AVCTableDef, then data records will follow.
         *------------------------------------------------------------*/
        psObj = AVCE00ParseNextLine(psInfo, pszLine); 
        if (psObj)
        {
			/* got table header */
            /* TODO: Enable return of table definition? */
            psObj = NULL;
        }
    }
    else
    {
        /*-------------------------------------------------------------
         * We're are in the middle of a section... first check if we
         * have reached the end.
         *
         * note: The first call to AVCE00ParseSectionEnd() with FALSE will 
         *       not reset the parser until we close the file... and then
         *       we call the function again to reset the parser.
         *------------------------------------------------------------*/
        if (AVCE00ParseSectionEnd(psInfo, pszLine, FALSE))
        {
            psRead->eCurFileType = AVCFileUnknown;
            AVCE00ParseSectionEnd(psInfo, pszLine, TRUE);
        }
        else
        /*-------------------------------------------------------------
         * ... not at the end yet, so continue reading objects.
         *------------------------------------------------------------*/
        {
            psObj = AVCE00ParseNextLine(psInfo, pszLine);

            if (psObj)
            {
				/* got object */
            }
        }
    }

    if (CPLGetLastErrorNo() != 0)
        nStatus = -1;

    return psObj;
}

/**********************************************************************
 *                         _AVCE00ReadBuildSqueleton()
 *
 * Build the squeleton of the E00 file corresponding to the specified
 * coverage and set the appropriate fields in the AVCE00ReadPtr struct.
 *
 * Note that the order of the sections in the squeleton is important
 * since some software may rely on this ordering when they read E00 files.
 *
 * The function returns the coverage precision that it will read from one
 * of the file headers.  
 **********************************************************************/
static int _AVCE00ReadBuildSqueleton(AVCE00ReadPtr psInfo, 
                                     char **papszCoverDir)
{
    int         iSect, iTable, numTables, iFile, nLen;
    char      **papszTables, **papszFiles, szCWD[75]="", *pcTmp;
    char       *pszEXPPath=NULL;
    int         nCoverPrecision = AVC_DEFAULT_PREC;
    char        cPrecisionCode = '2';
    const char *szFname = NULL;
    AVCBinFile *psFile=NULL;

    psInfo->numSections = 0;
    psInfo->pasSections = NULL;

    /*-----------------------------------------------------------------
     * Build the absolute coverage path to include in the EXP  0 line
     * This line usually contains the full path of the E00 file that
     * is being created, but since the lib does not write the output
     * file directly, there is no simple way to get that value.  Instead,
     * we will use the absolute coverage path to which we add a .E00
     * extension.
     * We need also make sure cover path is all in uppercase.
     *----------------------------------------------------------------*/
#ifdef WIN32
    if (psInfo->pszCoverPath[0] != '\\' &&
        !(isalpha(psInfo->pszCoverPath[0]) && psInfo->pszCoverPath[1] == ':'))
#else
    if (psInfo->pszCoverPath[0] != '/')
#endif
    {
        if (getcwd(szCWD, 74) == NULL)
            szCWD[0] = '\0';    /* Failed: buffer may be too small */

        nLen = strlen(szCWD);

#ifdef WIN32
        if (nLen > 0 && szCWD[nLen -1] != '\\')
            strcat(szCWD, "\\");
#else
        if (nLen > 0 && szCWD[nLen -1] != '/')
            strcat(szCWD, "/");
#endif
    }

    pszEXPPath = CPLStrdup(CPLSPrintf("EXP  0 %s%-.*s.E00", szCWD,
                                      (int)strlen(psInfo->pszCoverPath)-1,
                                      psInfo->pszCoverPath));
    pcTmp = pszEXPPath;
    for( ; *pcTmp != '\0'; pcTmp++)
        *pcTmp = toupper(*pcTmp);

    /*-----------------------------------------------------------------
     * EXP Header
     *----------------------------------------------------------------*/
    iSect = _AVCIncreaseSectionsArray(&(psInfo->pasSections), 
                                  &(psInfo->numSections), 1);
    psInfo->pasSections[iSect].eType = AVCFileUnknown;
    psInfo->pasSections[iSect].pszName = pszEXPPath;

    /*-----------------------------------------------------------------
     * We have to try to open each file as we go for 2 reasons:
     * - To validate the file's signature in order to detect cases like a user
     *   that places files such as "mystuff.txt" in the cover directory...
     *   this has already happened and obviously lead to problems!)
     * - We also need to find the coverage's precision from the headers
     *----------------------------------------------------------------*/

    /*-----------------------------------------------------------------
     * ARC section (arc.adf)
     *----------------------------------------------------------------*/
    szFname = (psInfo->eCoverType==AVCCoverV7 || 
               psInfo->eCoverType==AVCCoverPC2 ) ? "arc.adf": "arc";
    if ( (iFile=CSLFindString(papszCoverDir, szFname)) != -1 &&
         (psFile = AVCBinReadOpen(psInfo->pszCoverPath, szFname,
                                  psInfo->eCoverType, AVCFileARC,
                                  psInfo->psDBCSInfo)) != NULL)
    {
        if (nCoverPrecision == AVC_DEFAULT_PREC)
            nCoverPrecision = psFile->nPrecision;
        AVCBinReadClose(psFile);
        iSect = _AVCIncreaseSectionsArray(&(psInfo->pasSections), 
                                      &(psInfo->numSections), 1);

        psInfo->pasSections[iSect].eType = AVCFileARC;
        psInfo->pasSections[iSect].pszName = CPLStrdup("ARC");
        psInfo->pasSections[iSect].pszFilename=CPLStrdup(papszCoverDir[iFile]);
    }

    /*-----------------------------------------------------------------
     * CNT section (cnt.adf)
     *----------------------------------------------------------------*/
    szFname = (psInfo->eCoverType==AVCCoverV7 || 
               psInfo->eCoverType==AVCCoverPC2 ) ? "cnt.adf": "cnt";
    if ( (iFile=CSLFindString(papszCoverDir, szFname)) != -1 &&
         (psFile = AVCBinReadOpen(psInfo->pszCoverPath, szFname,
                                  psInfo->eCoverType, AVCFileCNT,
                                  psInfo->psDBCSInfo)) != NULL)
    {
        if (nCoverPrecision == AVC_DEFAULT_PREC)
            nCoverPrecision = psFile->nPrecision;
        AVCBinReadClose(psFile);
        iSect = _AVCIncreaseSectionsArray(&(psInfo->pasSections), 
                                      &(psInfo->numSections), 1);

        psInfo->pasSections[iSect].eType = AVCFileCNT;
        psInfo->pasSections[iSect].pszName = CPLStrdup("CNT");
        psInfo->pasSections[iSect].pszFilename=CPLStrdup(papszCoverDir[iFile]);
    }

    /*-----------------------------------------------------------------
     * LAB section (lab.adf)
     *----------------------------------------------------------------*/
    szFname = (psInfo->eCoverType==AVCCoverV7 || 
               psInfo->eCoverType==AVCCoverPC2 ) ? "lab.adf": "lab";
    if ( (iFile=CSLFindString(papszCoverDir, szFname)) != -1 &&
         (psFile = AVCBinReadOpen(psInfo->pszCoverPath, szFname,
                                  psInfo->eCoverType, AVCFileLAB,
                                  psInfo->psDBCSInfo)) != NULL)
    {
        if (nCoverPrecision == AVC_DEFAULT_PREC)
            nCoverPrecision = psFile->nPrecision;
        AVCBinReadClose(psFile);
        iSect = _AVCIncreaseSectionsArray(&(psInfo->pasSections), 
                                      &(psInfo->numSections), 1);

        psInfo->pasSections[iSect].eType = AVCFileLAB;
        psInfo->pasSections[iSect].pszName = CPLStrdup("LAB");
        psInfo->pasSections[iSect].pszFilename=CPLStrdup(papszCoverDir[iFile]);
    }

    /*-----------------------------------------------------------------
     * PAL section (pal.adf)
     *----------------------------------------------------------------*/
    szFname = (psInfo->eCoverType==AVCCoverV7 || 
               psInfo->eCoverType==AVCCoverPC2 ) ? "pal.adf": "pal";
    if ( (iFile=CSLFindString(papszCoverDir, szFname)) != -1 &&
         (psFile = AVCBinReadOpen(psInfo->pszCoverPath, szFname,
                                  psInfo->eCoverType, AVCFilePAL,
                                  psInfo->psDBCSInfo)) != NULL)
    {
        if (nCoverPrecision == AVC_DEFAULT_PREC)
            nCoverPrecision = psFile->nPrecision;
        AVCBinReadClose(psFile);
        iSect = _AVCIncreaseSectionsArray(&(psInfo->pasSections), 
                                      &(psInfo->numSections), 1);

        psInfo->pasSections[iSect].eType = AVCFilePAL;
        psInfo->pasSections[iSect].pszName = CPLStrdup("PAL");
        psInfo->pasSections[iSect].pszFilename=CPLStrdup(papszCoverDir[iFile]);
    }

    /*-----------------------------------------------------------------
     * TOL section (tol.adf for single precision, par.adf for double)
     *----------------------------------------------------------------*/
    szFname = (psInfo->eCoverType==AVCCoverV7 || 
               psInfo->eCoverType==AVCCoverPC2 ) ? "tol.adf": "tol";
    if ( (iFile=CSLFindString(papszCoverDir, szFname)) != -1 &&
         (psFile = AVCBinReadOpen(psInfo->pszCoverPath, szFname,
                                  psInfo->eCoverType, AVCFileTOL,
                                  psInfo->psDBCSInfo)) != NULL)
    {
        if (nCoverPrecision == AVC_DEFAULT_PREC)
            nCoverPrecision = psFile->nPrecision;
        AVCBinReadClose(psFile);
        iSect = _AVCIncreaseSectionsArray(&(psInfo->pasSections), 
                                      &(psInfo->numSections), 1);

        psInfo->pasSections[iSect].eType = AVCFileTOL;
        psInfo->pasSections[iSect].pszName = CPLStrdup("TOL");
        psInfo->pasSections[iSect].pszFilename=CPLStrdup(papszCoverDir[iFile]);
    }

    szFname = (psInfo->eCoverType==AVCCoverV7 || 
               psInfo->eCoverType==AVCCoverPC2 ) ? "par.adf": "par";
    if ( (iFile=CSLFindString(papszCoverDir, szFname)) != -1 &&
         (psFile = AVCBinReadOpen(psInfo->pszCoverPath, szFname,
                                  psInfo->eCoverType, AVCFileTOL,
                                  psInfo->psDBCSInfo)) != NULL)
    {
        if (nCoverPrecision == AVC_DEFAULT_PREC)
            nCoverPrecision = psFile->nPrecision;
        AVCBinReadClose(psFile);
        iSect = _AVCIncreaseSectionsArray(&(psInfo->pasSections), 
                                      &(psInfo->numSections), 1);

        psInfo->pasSections[iSect].eType = AVCFileTOL;
        psInfo->pasSections[iSect].pszName = CPLStrdup("TOL");
        psInfo->pasSections[iSect].pszFilename=CPLStrdup(papszCoverDir[iFile]);
    }

    /*-----------------------------------------------------------------
     * TXT section (txt.adf)
     *----------------------------------------------------------------*/
    szFname = (psInfo->eCoverType==AVCCoverV7 || 
               psInfo->eCoverType==AVCCoverPC2 ) ? "txt.adf": "txt";
    if ( (iFile=CSLFindString(papszCoverDir, szFname)) != -1 &&
         (psFile = AVCBinReadOpen(psInfo->pszCoverPath, szFname,
                                  psInfo->eCoverType, AVCFileTXT,
                                  psInfo->psDBCSInfo)) != NULL)
    {
        if (nCoverPrecision == AVC_DEFAULT_PREC)
            nCoverPrecision = psFile->nPrecision;
        AVCBinReadClose(psFile);
        iSect = _AVCIncreaseSectionsArray(&(psInfo->pasSections), 
                                      &(psInfo->numSections), 1);

        psInfo->pasSections[iSect].eType = AVCFileTXT;
        psInfo->pasSections[iSect].pszName = CPLStrdup("TXT");
        psInfo->pasSections[iSect].pszFilename=CPLStrdup(papszCoverDir[iFile]);
    }

    /*-----------------------------------------------------------------
     * TX6 section (*.txt)
     * Scan the directory for files with a ".txt" extension.
     * Note: Never seen those in a PC Arc/Info coverage!
     * In weird coverages, the filename ends with "txt" but there is no "."
     *----------------------------------------------------------------*/
    if (psInfo->eCoverType == AVCCoverV7)
        nCoverPrecision = _AVCE00ReadAddJabberwockySection(psInfo, AVCFileTX6,
                                                           "TX6", 
                                                           nCoverPrecision,
                                                           ".txt",
                                                           papszCoverDir);
    else if (psInfo->eCoverType == AVCCoverWeird)
        nCoverPrecision = _AVCE00ReadAddJabberwockySection(psInfo, AVCFileTX6,
                                                           "TX6", 
                                                           nCoverPrecision,
                                                           "txt",
                                                           papszCoverDir);

    /*-----------------------------------------------------------------
     * At this point, we should have read the coverage precsion... and if
     * we haven't yet then we'll just use single by default.
     * We'll need cPrecisionCode for some of the sections that follow.
     *----------------------------------------------------------------*/
    if (nCoverPrecision == AVC_DOUBLE_PREC)
        cPrecisionCode = '3';
    else
        cPrecisionCode = '2';

    /*-----------------------------------------------------------------
     * SIN  2/3 and EOX lines ... ???
     *----------------------------------------------------------------*/
    iSect = _AVCIncreaseSectionsArray(&(psInfo->pasSections), 
                                  &(psInfo->numSections), 2);
    psInfo->pasSections[iSect].eType = AVCFileUnknown;
    psInfo->pasSections[iSect].pszName = CPLStrdup("SIN  X");
    psInfo->pasSections[iSect].pszName[5] = cPrecisionCode;
    iSect++;
    psInfo->pasSections[iSect].eType = AVCFileUnknown;
    psInfo->pasSections[iSect].pszName = CPLStrdup("EOX");
    iSect++;

    /*-----------------------------------------------------------------
     * LOG section (log.adf) (ends with EOL)
     *----------------------------------------------------------------*/

    /*-----------------------------------------------------------------
     * PRJ section (prj.adf) (ends with EOP)
     *----------------------------------------------------------------*/
    szFname = (psInfo->eCoverType==AVCCoverV7 || 
               psInfo->eCoverType==AVCCoverPC2 ) ? "prj.adf": "prj";
    if ( (iFile=CSLFindString(papszCoverDir, szFname)) != -1 )
    {
        iSect = _AVCIncreaseSectionsArray(&(psInfo->pasSections), 
                                      &(psInfo->numSections), 1);

        psInfo->pasSections[iSect].eType = AVCFilePRJ;
        psInfo->pasSections[iSect].pszName = CPLStrdup("PRJ");
        psInfo->pasSections[iSect].pszFilename=CPLStrdup(papszCoverDir[iFile]);
    }

    /*-----------------------------------------------------------------
     * RXP section (*.rxp)
     * Scan the directory for files with a ".rxp" extension.
     *----------------------------------------------------------------*/
    if (psInfo->eCoverType == AVCCoverV7)
        _AVCE00ReadAddJabberwockySection(psInfo, AVCFileRXP, "RXP", 
                                         nCoverPrecision,".rxp",papszCoverDir);
    else if (psInfo->eCoverType == AVCCoverWeird)
        _AVCE00ReadAddJabberwockySection(psInfo, AVCFileRXP, "RXP", 
                                         nCoverPrecision,"rxp",papszCoverDir);


    /*-----------------------------------------------------------------
     * RPL section (*.pal)
     * Scan the directory for files with a ".rpl" extension.
     *----------------------------------------------------------------*/
    if (psInfo->eCoverType == AVCCoverV7)
        _AVCE00ReadAddJabberwockySection(psInfo, AVCFileRPL, "RPL", 
                                         nCoverPrecision,".pal",papszCoverDir);
    else if (psInfo->eCoverType == AVCCoverWeird)
        _AVCE00ReadAddJabberwockySection(psInfo, AVCFileRPL, "RPL", 
                                         nCoverPrecision,"rpl",papszCoverDir);

    /*-----------------------------------------------------------------
     * IFO section (tables)
     *----------------------------------------------------------------*/
    papszTables = papszFiles = NULL;
    if (psInfo->eCoverType == AVCCoverV7 || 
        psInfo->eCoverType == AVCCoverV7Tables || 
        psInfo->eCoverType == AVCCoverWeird)
    {
        /*-------------------------------------------------------------
         * Unix coverages: get tables from the ../info/arc.dir
         * Weird coverages: the arc.dir is similar but called "arcdr9"
         *------------------------------------------------------------*/
        papszTables = AVCBinReadListTables(psInfo->pszInfoPath, 
                                           psInfo->pszCoverName,
                                           &papszFiles, psInfo->eCoverType,
                                           psInfo->psDBCSInfo);
    }
    else if (psInfo->eCoverType == AVCCoverPC ||
             psInfo->eCoverType == AVCCoverPC2)
    {
        /*-------------------------------------------------------------
         * PC coverages: look for "???.dbf" in the coverage directory
         *               and build the table name using the coverage name
         *               as the table basename, and the dbf file basename
         *               as the table extension.
         *------------------------------------------------------------*/
        for(iFile=0; papszCoverDir && papszCoverDir[iFile]; iFile++)
        {
            if ((nLen = strlen(papszCoverDir[iFile])) == 7 &&
                EQUAL(papszCoverDir[iFile] + nLen -4, ".dbf"))
            {
                papszCoverDir[iFile][nLen - 4] = '\0';
                szFname = CPLSPrintf("%s.%s", psInfo->pszCoverName,
                                              papszCoverDir[iFile]);
                pcTmp = (char*)szFname;
                for( ; *pcTmp != '\0'; pcTmp++)
                    *pcTmp = toupper(*pcTmp);
                papszCoverDir[iFile][nLen - 4] = '.';

                papszTables = CSLAddString(papszTables, szFname);
                papszFiles = CSLAddString(papszFiles, papszCoverDir[iFile]);
            }
        }
    }

    if ((numTables = CSLCount(papszTables)) > 0)
    {
        iSect = _AVCIncreaseSectionsArray(&(psInfo->pasSections), 
                                      &(psInfo->numSections), numTables+2);

        psInfo->pasSections[iSect].eType = AVCFileUnknown;
        psInfo->pasSections[iSect].pszName = CPLStrdup("IFO  X");
        psInfo->pasSections[iSect].pszName[5] = cPrecisionCode;
        iSect++;

        for(iTable=0; iTable<numTables; iTable++)
        {
            psInfo->pasSections[iSect].eType = AVCFileTABLE;
            psInfo->pasSections[iSect].pszName=CPLStrdup(papszTables[iTable]);
            if (papszFiles)
            {
                psInfo->pasSections[iSect].pszFilename=
                                              CPLStrdup(papszFiles[iTable]);
            }
            iSect++;
        }

        psInfo->pasSections[iSect].eType = AVCFileUnknown;
        psInfo->pasSections[iSect].pszName = CPLStrdup("EOI");
        iSect++;

    }
    CSLDestroy(papszTables);
    CSLDestroy(papszFiles);

    /*-----------------------------------------------------------------
     * File ends with EOS
     *----------------------------------------------------------------*/
    iSect = _AVCIncreaseSectionsArray(&(psInfo->pasSections), 
                                  &(psInfo->numSections), 1);
    psInfo->pasSections[iSect].eType = AVCFileUnknown;
    psInfo->pasSections[iSect].pszName = CPLStrdup("EOS");


    return nCoverPrecision;
}


/**********************************************************************
 *                     _AVCE00ReadScanE00()
 *
 * Processes an entire E00 file to find all the interesting sections.
 **********************************************************************/
static void _AVCE00ReadScanE00(AVCE00ReadE00Ptr psRead)
{
    AVCE00ParseInfo *psInfo = psRead->hParseInfo;

    const char *pszLine;
    const char *pszName = 0;
    void       *obj;
    int        iSect = 0;
    GBool      bFirstLine = TRUE;

    while (CPLGetLastErrorNo() == 0 &&
            (pszLine = CPLReadLine(psRead->hFile) ) != NULL )
    {
        if (bFirstLine)
        {
            /* Look for the first non-empty line, after the EXP header,
             * trying to detect compressed E00 files. If the file is 
             * compressed, the first line of data should be 79 or 80 chars
             * long and contain several '~' characters.
             */
            int nLen = strlen(pszLine);
            if (nLen == 0 || EQUALN("EXP ", pszLine, 4))
                continue;  /* Skip empty and EXP header lines */
            else if ( (nLen == 79 || nLen == 80) &&
                      strchr(pszLine, '~') != NULL )
            {
                /* Looks like a compressed file. Just log an error and return.
                 * The caller should reject the file because it contains 0 
                 * sections 
                 */
                CPLError(CE_Failure, CPLE_OpenFailed, 
                         "This looks like a compressed E00 file and cannot be "
                         "processed directly. You may need to uncompress it "
                         "first using the E00compr library or the e00conv "
                         "program." );
                return;  
            }

            /* All seems fine. Continue with normal processing */
            bFirstLine = FALSE;
        }

        obj = _AVCE00ReadNextLineE00(psRead, pszLine);

        if (obj)
        {
            pszName = 0;
            switch (psInfo->eFileType)
            {
            case AVCFileARC:
                pszName = "ARC";
                break;

            case AVCFilePAL:
                pszName = "PAL";
                break;

            case AVCFileCNT:
                pszName = "CNT";
                break;

            case AVCFileLAB:
                pszName = "LAB";
                break;

            case AVCFileRPL:
                pszName = "RPL";
                break;

            case AVCFileTXT:
                pszName = "TXT";
                break;

            case AVCFileTX6:
                pszName = "TX6";
                break;

            case AVCFilePRJ:
                pszName = "PRJ";
                break;

            case AVCFileTABLE:
                pszName = psInfo->hdr.psTableDef->szTableName;
                break;

            default:
                break;
            }

            if (pszName && (psRead->numSections == 0 ||
                    psRead->pasSections[iSect].eType != psInfo->eFileType ||
                    !EQUAL(pszName, psRead->pasSections[iSect].pszName)))
            {
                iSect = _AVCIncreaseSectionsArray(&(psRead->pasSections), 
                                      &(psRead->numSections), 1);

                psRead->pasSections[iSect].eType = psInfo->eFileType;
                /* psRead->pasSections[iSect].pszName = CPLStrdup(psRead->pszCoverName); */
                psRead->pasSections[iSect].pszName = CPLStrdup(pszName);
                psRead->pasSections[iSect].pszFilename = CPLStrdup(psRead->pszCoverPath);
                psRead->pasSections[iSect].nLineNum = psInfo->nStartLineNum;
                psRead->pasSections[iSect].nFeatureCount = 0;
            }

            if (pszName && psRead->numSections)
            {
                /* increase feature count for current layer */
                ++psRead->pasSections[iSect].nFeatureCount;
            }
        }
    }
}

/**********************************************************************
 *                         _AVCE00ReadNextTableLine()
 *
 * Return the next line of the E00 representation of a info table.
 *
 * This function is used by AVCE00ReadNextLine() to generate table
 * output... it should never be called directly.
 **********************************************************************/
static const char *_AVCE00ReadNextTableLine(AVCE00ReadPtr psInfo)
{
    const char *pszLine = NULL;
    AVCE00Section *psSect;

    psSect = &(psInfo->pasSections[psInfo->iCurSection]);

    CPLAssert(psSect->eType == AVCFileTABLE);

    if (psInfo->iCurStep == AVC_GEN_NOTSTARTED)
    {
        /*---------------------------------------------------------
         * Open table and start returning header
         *--------------------------------------------------------*/
        if (psInfo->eCoverType == AVCCoverPC ||
            psInfo->eCoverType == AVCCoverPC2)
        {
            /*---------------------------------------------------------
             * PC Arc/Info: We pass the DBF table's full filename + the
             * Arc/Info table name (for E00 header)
             *--------------------------------------------------------*/
            char *pszFname;
            pszFname = CPLStrdup(CPLSPrintf("%s%s", psInfo->pszInfoPath,
                                                    psSect->pszFilename ));
            psInfo->hFile = AVCBinReadOpen(pszFname, psSect->pszName, 
                                           psInfo->eCoverType, psSect->eType,
                                           psInfo->psDBCSInfo);
            CPLFree(pszFname);
        }
        else
        {
            /*---------------------------------------------------------
             * AVCCoverV7 and AVCCoverWeird: 
             * We pass the INFO dir's path, and the Arc/Info table name
             * will be searched in the arc.dir
             *--------------------------------------------------------*/
            psInfo->hFile = AVCBinReadOpen(psInfo->pszInfoPath, 
                                           psSect->pszName, 
                                           psInfo->eCoverType, psSect->eType,
                                           psInfo->psDBCSInfo);
        }


        /* For some reason the file could not be opened... abort now.
         * An error message should have already been produced by 
         * AVCBinReadOpen()
         */
        if (psInfo->hFile == NULL)
            return NULL;

        psInfo->iCurStep = AVC_GEN_TABLEHEADER;

        pszLine = AVCE00GenTableHdr(psInfo->hGenInfo,
                                    psInfo->hFile->hdr.psTableDef,
                                    FALSE);
    }
        
    if (pszLine == NULL &&
        psInfo->iCurStep == AVC_GEN_TABLEHEADER)
    {
        /*---------------------------------------------------------
         * Continue table header
         *--------------------------------------------------------*/
        pszLine = AVCE00GenTableHdr(psInfo->hGenInfo,
                                    psInfo->hFile->hdr.psTableDef,
                                    TRUE);

        if (pszLine == NULL)
        {
            /* Finished with table header... time to proceed with the
             * table data.
             * Reset the AVCE00GenInfo struct. so that it returns NULL,
             * which will force reading of the first record from the 
             * file on the next call to AVCE00ReadNextLine()
             */
            AVCE00GenReset(psInfo->hGenInfo);
            psInfo->iCurStep = AVC_GEN_TABLEDATA;
        }

    }

    if (pszLine == NULL &&
        psInfo->iCurStep == AVC_GEN_TABLEDATA)
    {
        /*---------------------------------------------------------
         * Continue with records of data
         *--------------------------------------------------------*/

        pszLine = AVCE00GenTableRec(psInfo->hGenInfo, 
                                    psInfo->hFile->hdr.psTableDef->numFields,
                                    psInfo->hFile->hdr.psTableDef->pasFieldDef,
                                    psInfo->hFile->cur.pasFields,
                                    TRUE);

        if (pszLine == NULL)
        {
            /* Current record is finished generating... we need to read 
             * a new one from the file.
             */
            if (AVCBinReadNextObject(psInfo->hFile) != NULL)
            {
                pszLine = AVCE00GenTableRec(psInfo->hGenInfo, 
                                    psInfo->hFile->hdr.psTableDef->numFields,
                                    psInfo->hFile->hdr.psTableDef->pasFieldDef,
                                    psInfo->hFile->cur.pasFields,
                                    FALSE);
            }            

        }
    }

    if (pszLine == NULL)
    {
        /*---------------------------------------------------------
         * No more lines to output for this table ... Close it.
         *--------------------------------------------------------*/
        AVCBinReadClose(psInfo->hFile);
        psInfo->hFile = NULL;

        /*---------------------------------------------------------
         * And now proceed to the next section...
         * OK, I don't really like recursivity either... but it was
         * the simplest way to do this, and anyways we should never
         * have more than one level of recursivity.
         *--------------------------------------------------------*/
        if (psInfo->bReadAllSections)
            psInfo->iCurSection++;
        else
            psInfo->iCurSection = psInfo->numSections;
        psInfo->iCurStep = AVC_GEN_NOTSTARTED;

        pszLine = AVCE00ReadNextLine(psInfo);
    }

    /*-----------------------------------------------------------------
     * Check for errors... if any error happened, tehn return NULL
     *----------------------------------------------------------------*/
    if (CPLGetLastErrorNo() != 0)
    {
        pszLine = NULL;
    }

    return pszLine;
}


/**********************************************************************
 *                          AVCE00ReadNextLine()
 *
 * Returns the next line of the E00 representation of the coverage
 * or NULL when there are no more lines to generate, or if an error happened.
 * The returned line is a null-terminated string, and it does not
 * include a newline character.
 *
 * Call CPLGetLastErrorNo() after calling AVCE00ReadNextLine() to 
 * make sure that the line was generated succesfully.
 *
 * Note that AVCE00ReadNextLine() returns a reference to an
 * internal buffer whose contents will
 * be valid only until the next call to this function.  The caller should
 * not attempt to free() the returned pointer.
 **********************************************************************/
const char *AVCE00ReadNextLine(AVCE00ReadPtr psInfo)
{
    const char *pszLine = NULL;
    AVCE00Section *psSect;

    CPLErrorReset();

    /*-----------------------------------------------------------------
     * Check if we have finished generating E00 output
     *----------------------------------------------------------------*/
    if (psInfo->iCurSection >= psInfo->numSections)
        return NULL;

    psSect = &(psInfo->pasSections[psInfo->iCurSection]);

    /*-----------------------------------------------------------------
     * For simplicity, the generation of table output is in a separate
     * function.
     *----------------------------------------------------------------*/
    if (psSect->eType == AVCFileTABLE)
    {
        return _AVCE00ReadNextTableLine(psInfo);
    }

    if (psSect->eType == AVCFileUnknown)
    {
    /*-----------------------------------------------------------------
     * Section not attached to any file, used to hold header lines
     * or section separators, etc... just return the line directly and
     * move pointer to the next section.
     *----------------------------------------------------------------*/
        pszLine = psSect->pszName;
        if (psInfo->bReadAllSections)
            psInfo->iCurSection++;
        else
            psInfo->iCurSection = psInfo->numSections;
        psInfo->iCurStep = AVC_GEN_NOTSTARTED;
    }
    /*=================================================================
     *              ARC, PAL, CNT, LAB, TOL and TXT
     *================================================================*/
    else if (psInfo->iCurStep == AVC_GEN_NOTSTARTED &&
             (psSect->eType == AVCFileARC ||
              psSect->eType == AVCFilePAL ||
              psSect->eType == AVCFileRPL ||
              psSect->eType == AVCFileCNT ||
              psSect->eType == AVCFileLAB ||
              psSect->eType == AVCFileTOL ||
              psSect->eType == AVCFileTXT ||
              psSect->eType == AVCFileTX6 ||
              psSect->eType == AVCFileRXP   ) )
    {
    /*-----------------------------------------------------------------
     * Start processing of an ARC, PAL, CNT, LAB or TOL section:
     *   Open the file, get ready to read the first object from the 
     *   file, and return the header line.
     *  If the file fails to open then we will return NULL.
     *----------------------------------------------------------------*/
        psInfo->hFile = AVCBinReadOpen(psInfo->pszCoverPath, 
                                       psSect->pszFilename, 
                                       psInfo->eCoverType, psSect->eType,
                                       psInfo->psDBCSInfo);

        /*-------------------------------------------------------------
         * For some reason the file could not be opened... abort now.
         * An error message should have already been produced by 
         * AVCBinReadOpen()
         *------------------------------------------------------------*/
        if (psInfo->hFile == NULL)
            return NULL;

        pszLine = AVCE00GenStartSection(psInfo->hGenInfo, 
                                        psSect->eType, psSect->pszName);

        /*-------------------------------------------------------------
         * Reset the AVCE00GenInfo struct. so that it returns NULL,
         * which will force reading of the first object from the 
         * file on the next call to AVCE00ReadNextLine()
         *------------------------------------------------------------*/
        AVCE00GenReset(psInfo->hGenInfo);
        psInfo->iCurStep = AVC_GEN_DATA;
    }
    else if (psInfo->iCurStep == AVC_GEN_DATA &&
             (psSect->eType == AVCFileARC ||
              psSect->eType == AVCFilePAL ||
              psSect->eType == AVCFileRPL ||
              psSect->eType == AVCFileCNT ||
              psSect->eType == AVCFileLAB ||
              psSect->eType == AVCFileTOL ||
              psSect->eType == AVCFileTXT ||
              psSect->eType == AVCFileTX6 ||
              psSect->eType == AVCFileRXP    ) )
    {
    /*-----------------------------------------------------------------
     * Return the next line of an ARC/PAL/CNT/TOL/TXT object... 
     * if necessary, read the next object from the binary file.
     *----------------------------------------------------------------*/
        pszLine = AVCE00GenObject(psInfo->hGenInfo, 
                                  psSect->eType,
                  (psSect->eType==AVCFileARC?(void*)(psInfo->hFile->cur.psArc):
                   psSect->eType==AVCFilePAL?(void*)(psInfo->hFile->cur.psPal):
                   psSect->eType==AVCFileRPL?(void*)(psInfo->hFile->cur.psPal):
                   psSect->eType==AVCFileCNT?(void*)(psInfo->hFile->cur.psCnt):
                   psSect->eType==AVCFileLAB?(void*)(psInfo->hFile->cur.psLab):
                   psSect->eType==AVCFileTOL?(void*)(psInfo->hFile->cur.psTol):
                   psSect->eType==AVCFileTXT?(void*)(psInfo->hFile->cur.psTxt):
                   psSect->eType==AVCFileTX6?(void*)(psInfo->hFile->cur.psTxt):
                   psSect->eType==AVCFileRXP?(void*)(psInfo->hFile->cur.psRxp):
                   NULL),
                                  TRUE);
        if (pszLine == NULL)
        {
            /*---------------------------------------------------------
             * Current object is finished generating... we need to read 
             * a new one from the file.
             *--------------------------------------------------------*/
            if (AVCBinReadNextObject(psInfo->hFile) != NULL)
            {
                pszLine = AVCE00GenObject(psInfo->hGenInfo, 
                                          psSect->eType,
                  (psSect->eType==AVCFileARC?(void*)(psInfo->hFile->cur.psArc):
                   psSect->eType==AVCFilePAL?(void*)(psInfo->hFile->cur.psPal):
                   psSect->eType==AVCFileRPL?(void*)(psInfo->hFile->cur.psPal):
                   psSect->eType==AVCFileCNT?(void*)(psInfo->hFile->cur.psCnt):
                   psSect->eType==AVCFileLAB?(void*)(psInfo->hFile->cur.psLab):
                   psSect->eType==AVCFileTOL?(void*)(psInfo->hFile->cur.psTol):
                   psSect->eType==AVCFileTXT?(void*)(psInfo->hFile->cur.psTxt):
                   psSect->eType==AVCFileTX6?(void*)(psInfo->hFile->cur.psTxt):
                   psSect->eType==AVCFileRXP?(void*)(psInfo->hFile->cur.psRxp):
                   NULL),
                                          FALSE);
            }            
        }
        if (pszLine == NULL)
        {
            /*---------------------------------------------------------
             * Still NULL ??? This means we finished reading this file...
             * Start returning the "end of section" line(s)...
             *--------------------------------------------------------*/
            AVCBinReadClose(psInfo->hFile);
            psInfo->hFile = NULL;
            psInfo->iCurStep = AVC_GEN_ENDSECTION;
            pszLine = AVCE00GenEndSection(psInfo->hGenInfo, psSect->eType,
                                          FALSE);
        }
    }
    /*=================================================================
     *                          PRJ
     *================================================================*/
    else if (psInfo->iCurStep == AVC_GEN_NOTSTARTED &&
              psSect->eType == AVCFilePRJ   )
    {
        /*-------------------------------------------------------------
         * Start processing of PRJ section... return first header line.
         *------------------------------------------------------------*/
        pszLine = AVCE00GenStartSection(psInfo->hGenInfo, 
                                        psSect->eType, NULL);

        psInfo->hFile = NULL;
        psInfo->iCurStep = AVC_GEN_DATA;
    }
    else if (psInfo->iCurStep == AVC_GEN_DATA &&
             psSect->eType == AVCFilePRJ  )
    {
        /*-------------------------------------------------------------
         * Return the next line of a PRJ section
         *------------------------------------------------------------*/
        if (psInfo->hFile == NULL)
        {
            /*---------------------------------------------------------
             * File has not been read yet...
             * Read the PRJ file, and return the first PRJ line.
             *--------------------------------------------------------*/
            psInfo->hFile = AVCBinReadOpen(psInfo->pszCoverPath, 
                                           psSect->pszFilename, 
                                           psInfo->eCoverType, psSect->eType,
                                           psInfo->psDBCSInfo);

            /* For some reason the file could not be opened... abort now.
             * An error message should have already been produced by 
             * AVCBinReadOpen()
             */
            if (psInfo->hFile == NULL)
                return NULL;

            pszLine = AVCE00GenPrj(psInfo->hGenInfo, 
                                   psInfo->hFile->cur.papszPrj, FALSE);
        }
        else
        {
            /*---------------------------------------------------------
             * Generate the next line of output.
             *--------------------------------------------------------*/
            pszLine = AVCE00GenPrj(psInfo->hGenInfo, 
                                   psInfo->hFile->cur.papszPrj, TRUE);
        }

        if (pszLine == NULL)
        {
            /*---------------------------------------------------------
             * Still NULL ??? This means we finished generating this PRJ 
             * section...
             * Start returning the "end of section" line(s)...
             *--------------------------------------------------------*/
            AVCBinReadClose(psInfo->hFile);
            psInfo->hFile = NULL;
            psInfo->iCurStep = AVC_GEN_ENDSECTION;
            pszLine = AVCE00GenEndSection(psInfo->hGenInfo, psSect->eType,
                                          FALSE);
        }
    }
    else if (psInfo->iCurStep != AVC_GEN_ENDSECTION)
    {
        /* We should never get here! */
        CPLAssert(FALSE);
    }


    /*=================================================================
     *                End of section, for all files
     *================================================================*/

    /*-----------------------------------------------------------------
     * Finished processing of an ARC, PAL, CNT, LAB, TOL, PRJ file ...
     * continue returning the "end of section" line(s), and move the pointer
     * to the next section once we're done.
     *----------------------------------------------------------------*/
    if (psInfo->iCurStep == AVC_GEN_ENDSECTION && pszLine == NULL)
    {
        pszLine = AVCE00GenEndSection(psInfo->hGenInfo, psSect->eType, TRUE);

        if (pszLine == NULL)
        {
            /*---------------------------------------------------------
             * Finished returning the last lines of the section...
             * proceed to the next section...
             * OK, I don't really like recursivity either... but it was
             * the simplest way to do this, and anyways we should never
             * have more than one level of recursivity.
             *--------------------------------------------------------*/
            if (psInfo->bReadAllSections)
                psInfo->iCurSection++;
            else
                psInfo->iCurSection = psInfo->numSections;
            psInfo->iCurStep = AVC_GEN_NOTSTARTED;

            pszLine = AVCE00ReadNextLine(psInfo);
        }
    }

    return pszLine;
}



/**********************************************************************
 *                         AVCE00ReadSectionsList()
 *
 * Returns an array of AVCE00Section structures that describe the
 * squeleton of the whole coverage.  The value of *numSect will be
 * set to the number of sections in the array.
 *
 * You can scan the returned array, and use AVCE00ReadGotoSection() to move
 * the read pointer directly to the beginning of a given section
 * of the file.
 *
 * Sections of type AVCFileUnknown correspond to lines in the
 * E00 output that are not directly linked to any coverage file, like 
 * the "EXP 0" line, the "IFO X", "SIN X", etc.
 *
 * THE RETURNED ARRAY IS AN INTERNAL STRUCTURE AND SHOULD NOT BE
 * MODIFIED OR FREED BY THE CALLER... its contents will be valid
 * for as long as the coverage will remain open.
 **********************************************************************/
AVCE00Section *AVCE00ReadSectionsList(AVCE00ReadPtr psInfo, int *numSect)
{
    CPLErrorReset();

    *numSect = psInfo->numSections;
    return psInfo->pasSections;
}

/**********************************************************************
 *                         AVCE00ReadGotoSection()
 *
 * Move the read pointer to the E00 section (coverage file) described in 
 * the psSect structure.  Call AVCE00ReadSectionsList() to get the list of
 * sections for the current coverage.
 *
 * if bContinue=TRUE, then reading will automatically continue with the
 * next sections of the file once the requested section is finished.
 * Otherwise, if bContinue=FALSE then reading will stop at the end
 * of this section (i.e. AVCE00ReadNextLine() will return NULL when 
 * it reaches the end of this section)
 *
 * Sections of type AVCFileUnknown returned by AVCE00ReadSectionsList()
 * correspond to lines in the E00 output that are not directly linked
 * to any coverage file, like the "EXP 0" line, the "IFO X", "SIN X", etc.
 * You can jump to these sections or any other one without problems.
 *
 * This function returns 0 on success or -1 on error.
 **********************************************************************/
int AVCE00ReadGotoSection(AVCE00ReadPtr psInfo, AVCE00Section *psSect,
                          GBool bContinue)
{
    int     iSect;
    GBool   bFound = FALSE;

    CPLErrorReset();

    /*-----------------------------------------------------------------
     * Locate the requested section in the array.
     *----------------------------------------------------------------*/
    for(iSect=0; iSect<psInfo->numSections; iSect++)
    {
        if (psInfo->pasSections[iSect].eType == psSect->eType &&
            EQUAL(psInfo->pasSections[iSect].pszName, psSect->pszName))
        {
            bFound = TRUE;
            break;
        }
    }

    /*-----------------------------------------------------------------
     * Not found ... generate an error...
     *----------------------------------------------------------------*/
    if (!bFound)
    {
        CPLError(CE_Failure, CPLE_IllegalArg, 
                 "Requested E00 section does not exist!");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Found it ... close current section and get ready to read 
     * the new one.
     *----------------------------------------------------------------*/
    if (psInfo->hFile)
    {
        AVCBinReadClose(psInfo->hFile);
        psInfo->hFile = NULL;
    }

    psInfo->bReadAllSections = bContinue;
    psInfo->iCurSection = iSect;
    psInfo->iCurStep = AVC_GEN_NOTSTARTED;

    return 0;
}

/**********************************************************************
 *                         AVCE00ReadRewind()
 *
 * Rewinds the AVCE00ReadPtr just like the stdio rewind() 
 * function would do if you were reading an ASCII E00 file.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
int  AVCE00ReadRewind(AVCE00ReadPtr psInfo)
{
    CPLErrorReset();

    return AVCE00ReadGotoSection(psInfo, &(psInfo->pasSections[0]), TRUE);
}

/**********************************************************************
 *                         AVCE00ReadRewindE00()
 *
 * Rewinds the AVCE00ReadE00Ptr just like the stdio rewind() 
 * function would do if you were reading an ASCII E00 file.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
int  AVCE00ReadRewindE00(AVCE00ReadE00Ptr psRead)
{
    CPLErrorReset();

    psRead->bReadAllSections = TRUE;
    psRead->eCurFileType = AVCFileUnknown;

    psRead->hParseInfo->nCurLineNum = 0;
    psRead->hParseInfo->nStartLineNum = 0;
    psRead->hParseInfo->bForceEndOfSection = TRUE;
    psRead->hParseInfo->eSuperSectionType = AVCFileUnknown;
    AVCE00ParseSectionEnd(psRead->hParseInfo, NULL, 1);

    return fseek(psRead->hFile, 0, SEEK_SET);
}

/**********************************************************************
 *                        _AVCE00ReadSeekE00()
 *
 * Seeks to a new location in the E00 file, keeping parse state
 * appropriately.
 *
 * NOTE: This is a pretty slow implementation.
 * NOTE: The SEEK_END is not implemented.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
static int _AVCE00ReadSeekE00(AVCE00ReadE00Ptr psRead, int nOffset,
        int nWhence)
{
    const char *pszLine;
    void       *obj;

    switch (nWhence)
    {
    case SEEK_CUR:
        break;

    case SEEK_SET:
        AVCE00ReadRewindE00(psRead);
        break;

    default:
        CPLAssert(nWhence == SEEK_CUR || nWhence == SEEK_SET);
        break;
    }

    while (nOffset-- &&
            CPLGetLastErrorNo() == 0 &&
            (pszLine = CPLReadLine(psRead->hFile) ) != NULL )
    {
        obj = _AVCE00ReadNextLineE00(psRead, pszLine);
    }

    return nOffset ? -1 : 0;
}

/**********************************************************************
 *                      AVCE00ReadNextObjectE00()
 *
 * Returns the next object in an E00 file or NULL when there are no
 * more objects, or if an error happened.  The object type can be
 * determined via the eCurFileType attribute of the
 * AVCE00ReadE00Ptr object.
 *
 * Note that AVCE00ReadNextLine() returns a reference to an internal
 * buffer whose contents will be valid only until the next call to
 * this function.  The caller should not attempt to free() the
 * returned pointer.
 **********************************************************************/
void *AVCE00ReadNextObjectE00(AVCE00ReadE00Ptr psRead)
{
    const char *pszLine;
    void       *obj = NULL;

    do
    {
        pszLine = CPLReadLine(psRead->hFile);
        if (pszLine == 0)
            break;
        obj = _AVCE00ReadNextLineE00(psRead, pszLine);
    }
    while (obj == NULL &&
            (psRead->bReadAllSections ||
             psRead->eCurFileType != AVCFileUnknown) &&
            CPLGetLastErrorNo() == 0);
    return obj;
}

/**********************************************************************
 *                         AVCE00ReadSectionsListE00()
 *
 * Returns an array of AVCE00Section structures that describe the
 * sections in the E00 file.  The value of *numSect will be set to the
 * number of sections in the array.
 *
 * You can scan the returned array, and use AVCE00ReadGotoSectionE00()
 * to move the read pointer directly to the beginning of a given
 * section of the file.
 *
 * THE RETURNED ARRAY IS AN INTERNAL STRUCTURE AND SHOULD NOT BE
 * MODIFIED OR FREED BY THE CALLER... its contents will be valid
 * for as long as the coverage will remain open.
 **********************************************************************/
AVCE00Section *AVCE00ReadSectionsListE00(AVCE00ReadE00Ptr psRead,
        int *numSect)
{
    CPLErrorReset();

    *numSect = psRead->numSections;
    return psRead->pasSections;
}

/**********************************************************************
 *                         AVCE00ReadGotoSectionE00()
 *
 * Move the read pointer to the E00 section described in the psSect
 * structure.  Call AVCE00ReadSectionsListE00() to get the list of
 * sections for the current coverage.
 *
 * If bContinue is TRUE, then reading will automatically continue with
 * the next section of the file once the requested section is finished.
 * Otherwise, if bContinue is FALSE then reading will stop at the end
 * of this section (i.e. AVCE00ReadNextObjectE00() will return NULL
 * when the end of this section is reached)
 *
 * This function returns 0 on success or -1 on error.
 **********************************************************************/
int AVCE00ReadGotoSectionE00(AVCE00ReadE00Ptr psRead,
        AVCE00Section *psSect, GBool bContinue)
{
    int     iSect;
    GBool   bFound = FALSE;

    CPLErrorReset();

    /*-----------------------------------------------------------------
     * Locate the requested section in the array.
     *----------------------------------------------------------------*/
    for(iSect=0; iSect<psRead->numSections; iSect++)
    {
        if (psRead->pasSections[iSect].eType == psSect->eType &&
            EQUAL(psRead->pasSections[iSect].pszName, psSect->pszName))
        {
            bFound = TRUE;
            break;
        }
    }

    /*-----------------------------------------------------------------
     * Not found ... generate an error...
     *----------------------------------------------------------------*/
    if (!bFound)
    {
        CPLError(CE_Failure, CPLE_IllegalArg, 
                 "Requested E00 section does not exist!");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Found it ... advance parser to line number of start of section
     *----------------------------------------------------------------*/
    _AVCE00ReadSeekE00(psRead, psRead->pasSections[iSect].nLineNum, SEEK_SET);

    psRead->bReadAllSections = bContinue;

    return 0;
}
