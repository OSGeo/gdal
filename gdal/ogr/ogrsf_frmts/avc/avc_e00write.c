/**********************************************************************
 * $Id: avc_e00write.c,v 1.21 2008/07/23 20:51:38 dmorissette Exp $
 *
 * Name:     avc_e00write.c
 * Project:  Arc/Info vector coverage (AVC)  E00->BIN conversion library
 * Language: ANSI C
 * Purpose:  Functions to create a binary coverage from a stream of
 *           ASCII E00 lines.
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
 * $Log: avc_e00write.c,v $
 * Revision 1.21  2008/07/23 20:51:38  dmorissette
 * Fixed GCC 4.1.x compile warnings related to use of char vs unsigned char
 * (GDAL/OGR ticket http://trac.osgeo.org/gdal/ticket/2495)
 *
 * Revision 1.20  2006/06/27 18:38:43  dmorissette
 * Cleaned up E00 reading (bug 1497, patch from James F.)
 *
 * Revision 1.19  2006/06/14 16:31:28  daniel
 * Added support for AVCCoverPC2 type (bug 1491)
 *
 * Revision 1.18  2006/03/02 22:46:26  daniel
 * Accept empty subclass names for TX6/TX7 sections (bug 1261)
 *
 * Revision 1.17  2005/06/03 03:49:59  daniel
 * Update email address, website url, and copyright dates
 *
 * Revision 1.16  2002/08/27 15:46:15  daniel
 * Applied fix made in GDAL/OGR by 'aubin' (moved include ctype.h after avc.h)
 *
 * Revision 1.15  2002/04/16 21:19:10  daniel
 * Use VSIRmdir()
 *
 * Revision 1.14  2002/03/18 19:00:44  daniel
 * Use VSIMkdir() and not VSIMkDir()
 *
 * Revision 1.13  2002/02/18 21:16:33  warmerda
 * modified to use VSIMkDir
 *
 * Revision 1.12  2001/05/23 15:23:17  daniel
 * Remove trailing '/' in info directory path when creating the info dir.
 *
 * Revision 1.11  2000/09/26 20:21:04  daniel
 * Added AVCCoverPC write
 *
 * Revision 1.10  2000/09/22 19:45:21  daniel
 * Switch to MIT-style license
 *
 * Revision 1.9  2000/05/29 22:47:39  daniel
 * Made validation on new coverage name more flexible.
 *
 * Revision 1.8  2000/05/29 15:31:31  daniel
 * Added Japanese DBCS support
 *
 * Revision 1.7  2000/02/14 17:19:53  daniel
 * Accept '-' cahracter in new coverage name
 *
 * Revision 1.6  2000/01/10 02:57:44  daniel
 * Little changes to accommodate read support for "weird" coverages
 *
 * Revision 1.5  1999/12/24 07:18:34  daniel
 * Added PC Arc/Info coverages support
 *
 * Revision 1.4  1999/08/26 17:36:36  daniel
 * Avoid overwriting arc.dir on Windows... happened only when several
 * coverages are created by the same process on Windows.
 *
 * Revision 1.3  1999/08/23 18:23:35  daniel
 * Added AVCE00DeleteCoverage()
 *
 * Revision 1.2  1999/05/17 16:23:36  daniel
 * Added AVC_DEFAULT_PREC + more cover name validation in AVCE00WriteOpen().
 *
 * Revision 1.1  1999/05/11 02:34:46  daniel
 * Initial revision
 *
 **********************************************************************/

#include "cpl_vsi.h"
#include "avc.h"
#include <ctype.h>      /* tolower() */

static GBool _IsStringAlnum(const char *pszFname);

/**********************************************************************
 *                          AVCE00WriteOpen()
 *
 * Open (create) an Arc/Info coverage, ready to be receive a stream
 * of ASCII E00 lines and convert that to the binary coverage format.
 *
 * For now, writing to or overwriting existing coverages is not supported
 * (and may quite well never be!)... you can only create new coverages.
 *
 * Important Note: The E00 source lines are assumed to be valid... the
 * library performs no validation on the consistency of what it is 
 * given as input (i.e. topology, polygons consistency, etc.).
 * So the coverage that will be created will be only as good as the 
 * E00 input that is used to generate it.
 *
 * pszCoverPath MUST be the name of the coverage directory, including 
 * the path to it.
 * (contrary to AVCE00ReadOpen(), you cannot pass the name of one of
 *  the files in the coverage directory).
 * The name of the coverage MUST be included in pszCoverPath... this 
 * means that passing "." is invalid.
 *
 * eNewCoverType is the type of coverage to create.  
 *               Either AVCCoverV7 (Arc/Info V7 (Unix) coverage)
 *               or     AVCCoverPC (PC Arc/Info coverage)
 *
 * nPrecision should always be AVC_DEFAULT_PREC to automagically detect the
 *            source coverage's precision and use that same precision
 *            for the new coverage.  
 *
 *            This parameter has been included to allow adding the 
 *            possibility to eventually create coverages with a precision 
 *            different from the source E00.
 *            Given the way the lib is built, it could be possible to
 *            also pass  AVC_SINGLE_PREC or AVC_DOUBLE_PREC to explicitly
 *            request the creation of a coverage with that precision, 
 *            but the library does not (not yet!) properly convert the 
 *            TABLE attributes' precision, and the resulting coverage may
 *            be invalid in some cases.  
 *            This improvement is on the ToDo list!
 *
 * Returns a new AVCE00WritePtr handle or NULL if the coverage could 
 * not be created or if a coverage with that name already exists.
 *
 * The handle will eventually have to be released with AVCE00ReadClose().
 **********************************************************************/
AVCE00WritePtr  AVCE00WriteOpen(const char *pszCoverPath, 
                                AVCCoverType eNewCoverType, int nPrecision )
{
    AVCE00WritePtr  psInfo;
    int             i, nLen;
    VSIStatBuf      sStatBuf;

    CPLErrorReset();

    /*-----------------------------------------------------------------
     * Create pszCoverPath directory.  
     *----------------------------------------------------------------*/
    if (pszCoverPath == NULL || strlen(pszCoverPath) == 0)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed, 
                 "Invalid (empty) coverage directory name.");
        return NULL;
    }
    else if ( VSIStat(pszCoverPath, &sStatBuf) == 0 &&
              VSI_ISDIR(sStatBuf.st_mode) )
    {
        /*-------------------------------------------------------------
         * Directory already exists... make sure it is empty
         * otherwise we can't use it as a coverage directory.
         *------------------------------------------------------------*/
        char **papszFiles;
        papszFiles = CPLReadDir(pszCoverPath);
        for(i=0; papszFiles && papszFiles[i]; i++)
        {
            if (!EQUAL(".", papszFiles[i]) &&
                !EQUAL("..", papszFiles[i]))
            {
                CPLError(CE_Failure, CPLE_OpenFailed, 
                         "Cannot create coverage %s: directory already exists "
                         "and is not empty.", pszCoverPath);
                CSLDestroy(papszFiles);
                papszFiles = NULL;
                return NULL;
            }
        }

        CSLDestroy(papszFiles);
        papszFiles = NULL;
    }
    else
    {
        /*-------------------------------------------------------------
         * Create new pszCoverPath directory.  
         * This will fail if a file with the same name already exists.
         *------------------------------------------------------------*/
        if( VSIMkdir(pszCoverPath, 0777) != 0 )
        {
            CPLError(CE_Failure, CPLE_OpenFailed, 
                     "Unable to create coverage directory: %s.", pszCoverPath);
            return NULL;
        }
    }

    /*-----------------------------------------------------------------
     * Alloc the AVCE00WritePtr handle
     *----------------------------------------------------------------*/
    psInfo = (AVCE00WritePtr)CPLCalloc(1, sizeof(struct AVCE00WriteInfo_t));

    /*-----------------------------------------------------------------
     * Validate and store coverage type
     *----------------------------------------------------------------*/
    if (eNewCoverType == AVCCoverV7 || eNewCoverType == AVCCoverPC)
        psInfo->eCoverType = eNewCoverType;
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported, 
                 "Requested coverage type cannot be created.  Please use "
                 "the AVCCoverV7 or AVCCoverPC coverage type.");
        CPLFree(psInfo);
        return NULL;
    }

    /*-----------------------------------------------------------------
     * Requested precision for the new coverage... for now only
     * AVC_DEFAULT_PREC is supported.  When the first section is
     * read, then this section's precision will be used for the whole
     * coverage.  (This is done inside AVCE00WriteNextLine())
     *----------------------------------------------------------------*/
    if (psInfo->eCoverType == AVCCoverPC)
        psInfo->nPrecision = AVC_SINGLE_PREC; /* PC Cover always single prec.*/
    else if (nPrecision == AVC_DEFAULT_PREC)
        psInfo->nPrecision = nPrecision;
    else
    {
        CPLError(CE_Failure, CPLE_IllegalArg, 
                 "Coverages can only be created using AVC_DEFAULT_PREC. "
                 "Please see the documentation for AVCE00WriteOpen().");
        CPLFree(psInfo);
        return NULL;
    }

    /*-----------------------------------------------------------------
     * Make sure coverage directory name is terminated with a '/' (or '\\')
     *----------------------------------------------------------------*/
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

    if (strlen(psInfo->pszCoverName) > 13 ||
        !_IsStringAlnum(psInfo->pszCoverName) )
    {
        CPLError(CE_Failure, CPLE_OpenFailed, 
                 "Invalid coverage name (%s): "
                 "coverage name must be 13 chars or less and contain only "
                 "alphanumerical characters, '-' or '_'.", 
                 psInfo->pszCoverName);

        CPLFree(psInfo->pszCoverPath);
        CPLFree(psInfo->pszCoverName);
        CPLFree(psInfo);
        return NULL;
    }

    if (psInfo->eCoverType == AVCCoverPC || psInfo->eCoverType == AVCCoverPC2)
    {
        /*-------------------------------------------------------------
         * No 'info' directory is required for PC coverages
         *------------------------------------------------------------*/
        psInfo->pszInfoPath = NULL;
    }
    else
    {
        /*-------------------------------------------------------------
         * Lazy way to build the INFO path: simply add "../info/"...
         * this could probably be improved!
         *------------------------------------------------------------*/
        psInfo->pszInfoPath = (char*)CPLMalloc((strlen(psInfo->pszCoverPath)+9)
                                               *sizeof(char));
#ifdef WIN32
#  define AVC_INFOPATH "..\\info\\"
#else
#  define AVC_INFOPATH "../info/"
#endif
        sprintf(psInfo->pszInfoPath, "%s%s", psInfo->pszCoverPath, 
                                             AVC_INFOPATH);

        /*-------------------------------------------------------------
         * Check if the info directory exists and contains the "arc.dir"
         * if the info dir does not exist, then make sure we can create
         * the arc.dir file (i.e. try to create an empty one)
         *
         * Note: On Windows, this VSIStat() call seems to sometimes fail even 
         *       when the directory exists (buffering issue?), and the 
         *       following if() block is sometimes executed even if it 
         *       should not, but this should not cause problems since the 
         *       arc.dir is opened with "a+b" access.
         *------------------------------------------------------------*/
        if ( VSIStat(psInfo->pszInfoPath, &sStatBuf) == -1)
        {
            FILE *fp;
            char *pszArcDir;
            char *pszInfoDir;

            pszArcDir = CPLStrdup(CPLSPrintf("%s%s", 
                                             psInfo->pszInfoPath, "arc.dir"));

            /* Remove the trailing "/" from pszInfoPath.  Most OSes are
             * forgiving, and allow mkdir to include the trailing character,
             * but some UNIXes are not. [GEH 2001/05/17]
             */
            pszInfoDir = CPLStrdup(psInfo->pszInfoPath);
            pszInfoDir[strlen(pszInfoDir)-1] = '\0';
            
            VSIMkdir(pszInfoDir, 0777);
            fp = VSIFOpen(pszArcDir, "a+b");

            CPLFree(pszArcDir);
            CPLFree(pszInfoDir);
            if (fp)
            {
                VSIFClose(fp);
            }
            else
            {
                CPLError(CE_Failure, CPLE_OpenFailed, 
                         "Unable to create (or write to) 'info' directory %s", 
                         psInfo->pszInfoPath);
                CPLFree(psInfo->pszCoverPath);
                CPLFree(psInfo->pszInfoPath);
                CPLFree(psInfo);
                return NULL;
            }
        }
    }

    /*-----------------------------------------------------------------
     * Init the E00 parser.
     *----------------------------------------------------------------*/
    psInfo->hParseInfo = AVCE00ParseInfoAlloc();
    psInfo->eCurFileType = AVCFileUnknown;

    /*-----------------------------------------------------------------
     * Init multibyte encoding info
     *----------------------------------------------------------------*/
    psInfo->psDBCSInfo = AVCAllocDBCSInfo();

    /*-----------------------------------------------------------------
     * If an error happened during the open call, cleanup and return NULL.
     *----------------------------------------------------------------*/
    if (CPLGetLastErrorNo() != 0)
    {
        AVCE00WriteClose(psInfo);
        psInfo = NULL;
    }

    return psInfo;
}

/**********************************************************************
 *                          AVCE00WriteClose()
 *
 * Close a coverage and release all memory used by the AVCE00WritePtr
 * handle.
 **********************************************************************/
void AVCE00WriteClose(AVCE00WritePtr psInfo)
{
    CPLErrorReset();

    if (psInfo == NULL)
        return;

    CPLFree(psInfo->pszCoverPath);
    CPLFree(psInfo->pszCoverName);
    CPLFree(psInfo->pszInfoPath);

    if (psInfo->hFile)
        AVCBinWriteClose(psInfo->hFile);

    if (psInfo->hParseInfo)
        AVCE00ParseInfoFree(psInfo->hParseInfo);

    AVCFreeDBCSInfo(psInfo->psDBCSInfo);

    CPLFree(psInfo);
}


/**********************************************************************
 *                          _IsStringAlnum()
 *
 * Scan a string, and return TRUE if it contains only valid characters, 
 * Return FALSE otherwise.
 *
 * We used to accept only isalnum() chars, but since extended chars with
 * accents seem to be accepted, we will only check for chars that 
 * could confuse the lib.
 **********************************************************************/
static GBool _IsStringAlnum(const char *pszFname)
{
    GBool bOK = TRUE;

    while(bOK && *pszFname != '\0')
    {
        if (strchr(" \t.,/\\", (unsigned char)*pszFname) != NULL)
            bOK = FALSE;
        pszFname ++;
    }

    return bOK;
}

/**********************************************************************
 *                          _AVCE00WriteRenameTable()
 *
 * Rename the table and the system fields in a tabledef that will
 * be written to a new coverage.
 **********************************************************************/
static void _AVCE00WriteRenameTable(AVCTableDef *psTableDef, 
                                    const char *pszNewCoverName)
{
    char szOldName[40], szOldExt[40], szNewName[40], *pszTmp;
    char szSysId[40], szUserId[40];
    int  i;

    strcpy(szNewName, pszNewCoverName);
    for(i=0; szNewName[i] != '\0'; i++)
        szNewName[i] = toupper(szNewName[i]);

    /*-----------------------------------------------------------------
     * Extract components from the current table name.
     *----------------------------------------------------------------*/
    strcpy(szOldName, psTableDef->szTableName);

    if ( !EQUAL(psTableDef->szExternal, "XX") ||
         (pszTmp = strchr(szOldName, '.')) == NULL )
        return;  /* We don't deal with that table */

    *pszTmp = '\0';
    pszTmp++;

    strcpy(szOldExt, pszTmp);
    if ( (pszTmp = strchr(szOldExt, ' ')) != NULL )
        *pszTmp = '\0';

    if (strlen(szOldExt) < 3)
        return;  /* We don't deal with that table */

    /*-----------------------------------------------------------------
     * Look for system attributes with same name as table
     * If the table name extension is followed by a subclass name 
     * (e.g. "TEST.PATCOUNTY") then this subclass is used to build
     * the system attributes (COUNTY# and COUNTY-ID) and thus we do 
     * not need to rename them
     * Otherwise (e.g. COUNTY.PAT) the coverage name is used and then 
     * we need to rename these attribs for the new coverage name.
     *----------------------------------------------------------------*/
    if (strlen(szOldExt) == 3)
    {
        sprintf(szSysId, "%s#", szOldName);
        sprintf(szUserId, "%s-ID", szOldName);

        for(i=0; i<psTableDef->numFields; i++)
        {
            /* Remove trailing spaces */
            if ((pszTmp=strchr(psTableDef->pasFieldDef[i].szName,' '))!=NULL)
                *pszTmp = '\0';

            if (EQUAL(psTableDef->pasFieldDef[i].szName, szSysId))
            {
                sprintf(psTableDef->pasFieldDef[i].szName, "%s#", szNewName);
            }
            else if (EQUAL(psTableDef->pasFieldDef[i].szName, szUserId))
            {
                sprintf(psTableDef->pasFieldDef[i].szName, "%s-ID", szNewName);
            }
        }
    }

    /*-----------------------------------------------------------------
     * Build new table name
     *----------------------------------------------------------------*/
    sprintf(psTableDef->szTableName, "%s.%s", szNewName, szOldExt);

}

/**********************************************************************
 *                          _AVCE00WriteCreateCoverFile()
 *
 * Create a coverage file for the specified file type.
 *
 * The main part of the work is to find the right filename to use based on
 * the file type, the coverage precision, etc... the rest of job is 
 * done by AVCBinWriteCreate().
 *
 * Returns 0 on success, or -1 if an error happened.
 *
 * AVCWriteCloseCoverFile() will eventually have to be called to release the 
 * resources used by the AVCBinFile structure.
 **********************************************************************/
int  _AVCE00WriteCreateCoverFile(AVCE00WritePtr psInfo, AVCFileType eType,
                                 const char *pszLine, AVCTableDef *psTableDef)
{
    char        *pszPath, szFname[50]="";
    int         i, nStatus = 0;

    /* By now, new coverage precision should have been established */
    CPLAssert(psInfo->nPrecision != AVC_DEFAULT_PREC);

    /*-----------------------------------------------------------------
     * Establish filename based on file type, precision, and possibly the
     * contents of the header line.
     *----------------------------------------------------------------*/
    pszPath = psInfo->pszCoverPath;
    switch(eType)
    {
      case AVCFileARC:
        strcpy(szFname, "arc");
        break;
      case AVCFilePAL:
        strcpy(szFname, "pal");
        break;
      case AVCFileCNT:
        strcpy(szFname, "cnt");
        break;
      case AVCFileLAB:
        strcpy(szFname, "lab");
        break;
      case AVCFileTOL:
        if (psInfo->nPrecision == AVC_SINGLE_PREC)
            strcpy(szFname, "tol");
        else
            strcpy(szFname, "par");
        break;
      case AVCFilePRJ:
        strcpy(szFname, "prj");
        break;
      case AVCFileTXT:
        strcpy(szFname, "txt");
        break;
      case AVCFileTX6:
      /* For TX6/TX7: the filename is subclass_name.txt 
       */

        /* See bug 1261: It seems that empty subclass names are valid
         * for TX7. In this case we'll default the filename to txt.txt
         */
        if (pszLine[0] == '\0')
        {
            strcpy(szFname, "txt.txt");
        }
        else if (strlen(pszLine) > 30 || strchr(pszLine, ' ') != NULL)
            CPLError(CE_Failure, CPLE_IllegalArg, 
                     "Invalid TX6/TX7 subclass name \"%s\"", pszLine);
        else
            sprintf(szFname, "%s.txt", pszLine);
        break;
      case AVCFileRPL:
      /* For RPL and RXP: the filename is region_name.pal or region_name.rxp
       */
        if (strlen(pszLine) > 30 || strchr(pszLine, ' ') != NULL)
            CPLError(CE_Failure, CPLE_IllegalArg, 
                     "Invalid RPL region name \"%s\"", pszLine);
        else
            sprintf(szFname, "%s.pal", pszLine);
        break;
      case AVCFileRXP:
        if (strlen(pszLine) > 30 || strchr(pszLine, ' ') != NULL)
            CPLError(CE_Failure, CPLE_IllegalArg, 
                     "Invalid RXP name \"%s\"", pszLine);
        else
            sprintf(szFname, "%s.rxp", pszLine);
        break;
      case AVCFileTABLE:
        /*-------------------------------------------------------------
         * For tables, Filename will be based on info in the psTableDef 
         * but we need to rename the table and the system attributes 
         * based on the new coverage name.
         *------------------------------------------------------------*/
        if (psInfo->eCoverType != AVCCoverPC && 
            psInfo->eCoverType != AVCCoverPC2)
            pszPath = psInfo->pszInfoPath;
        _AVCE00WriteRenameTable(psTableDef, psInfo->pszCoverName);
        break;
      default:
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "_AVCE00WriteCreateCoverFile(): Unsupported file type!");
        nStatus = -1;
        break;
    }

    /*-----------------------------------------------------------------
     * V7 coverage filenames default to have a .adf extension
     * but PC coverage filenames (except .dbf tables) have no extensions.
     *----------------------------------------------------------------*/
    if (psInfo->eCoverType == AVCCoverV7 && strchr(szFname, '.') == NULL)
        strcat(szFname, ".adf");

    /*-----------------------------------------------------------------
     * Make sure filename is all lowercase and attempt to create the file
     *----------------------------------------------------------------*/
    for(i=0; szFname[i] != '\0'; i++)
        szFname[i] = tolower(szFname[i]);

    if (nStatus == 0)
    {
        psInfo->eCurFileType = eType;

        if (eType == AVCFileTABLE)
            psInfo->hFile = AVCBinWriteCreateTable(pszPath, 
                                                   psInfo->pszCoverName, 
                                                   psTableDef,
                                                   psInfo->eCoverType,
                                                   psInfo->nPrecision,
                                                   psInfo->psDBCSInfo);
        else

            psInfo->hFile = AVCBinWriteCreate(pszPath, szFname, 
                                              psInfo->eCoverType,
                                              eType, psInfo->nPrecision,
                                              psInfo->psDBCSInfo);

        if (psInfo->hFile == NULL)
        {
            nStatus = -1;
            psInfo->eCurFileType = AVCFileUnknown;
        }
    }

    return nStatus;
}

/**********************************************************************
 *                          _AVCE00WriteCloseCoverFile()
 *
 * Close current coverage file and reset the contents of psInfo.
 *
 * File should have been previously opened by _AVCE00WriteCreateCoverFile().
 *
 **********************************************************************/
void  _AVCE00WriteCloseCoverFile(AVCE00WritePtr psInfo)
{
    /*-----------------------------------------------------------------
     * PRJ sections behave differently... since there is only one "object"
     * per section, they accumulate lines while we read them, and we 
     * write everything at once when we reach the end-of-section (EOP) line.
     *----------------------------------------------------------------*/
    if (psInfo->eCurFileType == AVCFilePRJ)
    {
        AVCBinWriteObject(psInfo->hFile, psInfo->hParseInfo->cur.papszPrj);
    }

    AVCBinWriteClose(psInfo->hFile);
    psInfo->hFile = NULL;
    psInfo->eCurFileType = AVCFileUnknown;
}

/**********************************************************************
 *                          AVCE00WriteNextLine()
 *
 * Take the next line of E00 input for this coverage, parse it and 
 * write the result to the coverage.
 *
 * Important Note: The E00 source lines are assumed to be valid... the
 * library performs no validation on the consistency of what it is 
 * given as input (i.e. topology, polygons consistency, etc.).
 * So the coverage that will be created will be only as good as the 
 * E00 input that is used to generate it.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
int     AVCE00WriteNextLine(AVCE00WritePtr psInfo, const char *pszLine)
{
    /*-----------------------------------------------------------------
     * TODO: Update this call to use _AVCE00ReadNextLineE00(), if
     * possible.
     *----------------------------------------------------------------*/

    int nStatus = 0;

    CPLErrorReset();

    /*-----------------------------------------------------------------
     * If we're at the top level inside a supersection... check if this
     * supersection ends here.
     *----------------------------------------------------------------*/
    if (AVCE00ParseSuperSectionEnd(psInfo->hParseInfo, pszLine) == TRUE)
    {
        /* Nothing to do... it's all been done by the call to 
         * AVCE00ParseSuperSectionEnd()
         */
    }
    else if (psInfo->eCurFileType == AVCFileUnknown)
    {
        /*-------------------------------------------------------------
         * We're at the top level or inside a supersection... waiting 
         * to encounter a valid section or supersection header 
         * (i.e. "ARC  2", etc...)
         *------------------------------------------------------------*/

        /*-------------------------------------------------------------
         * First check for a supersection header (TX6, RXP, IFO, ...)
         *------------------------------------------------------------*/
        if ( AVCE00ParseSuperSectionHeader(psInfo->hParseInfo,
                                           pszLine) == AVCFileUnknown )
        {
            /*---------------------------------------------------------
             * This was not a supersection header... check if it's a simple
             * section header
             *--------------------------------------------------------*/
            psInfo->eCurFileType=AVCE00ParseSectionHeader(psInfo->hParseInfo,
                                                          pszLine);
        }

        /*-------------------------------------------------------------
         * If the coverage was created using AVC_DEFAULT_PREC and we are
         * processing the first section header, then use this section's
         * precision for the new coverage.
         * (Note: this code segment will be executed only once per 
         *        coverage and only if AVC_DEFAULT_PREC was selected)
         *------------------------------------------------------------*/
        if (psInfo->nPrecision == AVC_DEFAULT_PREC &&
            psInfo->eCurFileType != AVCFileUnknown)
        {
            psInfo->nPrecision = psInfo->hParseInfo->nPrecision;
        }

        if (psInfo->eCurFileType == AVCFileTABLE)
        {
            /*---------------------------------------------------------
             * We can't create the file for a TABLE until the
             * whole header has been read... send the first header
             * line to the parser and wait until the whole header has 
             * been read.
             *--------------------------------------------------------*/
            AVCE00ParseNextLine(psInfo->hParseInfo, pszLine); 
        }
        else if (psInfo->eCurFileType != AVCFileUnknown)
        {
            /*---------------------------------------------------------
             * OK, we've found a valid section header... create the 
             * corresponding file in the coverage.
             * Note: supersection headers don't trigger the creation
             *       of any output file... they just alter the psInfo state.
             *--------------------------------------------------------*/

            nStatus = _AVCE00WriteCreateCoverFile(psInfo, 
                                                  psInfo->eCurFileType,
                                       psInfo->hParseInfo->pszSectionHdrLine,
                                                  NULL);
        }
    }
    else if (psInfo->eCurFileType == AVCFileTABLE &&
             ! psInfo->hParseInfo->bTableHdrComplete )
    {
        /*-------------------------------------------------------------
         * We're reading a TABLE header... continue reading lines
         * from the header, and create the output file only once
         * the header will have been completely read.
         *
         * Note: When parsing a TABLE, the first object returned will 
         * be the AVCTableDef, then data records will follow.
         *------------------------------------------------------------*/
        AVCTableDef *psTableDef;
        psTableDef = (AVCTableDef*)AVCE00ParseNextLine(psInfo->hParseInfo, 
                                                       pszLine); 
        if (psTableDef)
        {
            nStatus = _AVCE00WriteCreateCoverFile(psInfo, 
                                                  psInfo->eCurFileType,
                                       psInfo->hParseInfo->pszSectionHdrLine,
                                                  psTableDef);
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
        if (AVCE00ParseSectionEnd(psInfo->hParseInfo, pszLine, FALSE))
        {
            _AVCE00WriteCloseCoverFile(psInfo);
            AVCE00ParseSectionEnd(psInfo->hParseInfo, pszLine, TRUE);
        }
        else
        /*-------------------------------------------------------------
         * ... not at the end yet, so continue reading objects.
         *------------------------------------------------------------*/
        {
            void *psObj;
            psObj = AVCE00ParseNextLine(psInfo->hParseInfo, pszLine);

            if (psObj)
                AVCBinWriteObject(psInfo->hFile, psObj);
        }
    }


    if (psInfo->hParseInfo->bForceEndOfSection)
    {
        /*-------------------------------------------------------------
         * The last call encountered an implicit end of section, so
         * we close the section now without waiting for an end-of-section
         * line (there won't be any!)... and get ready to proceed with
         * the next section.
         * This is used for TABLEs.
         *------------------------------------------------------------*/
        _AVCE00WriteCloseCoverFile(psInfo);
        AVCE00ParseSectionEnd(psInfo->hParseInfo, pszLine, TRUE);
        /* psInfo->hParseInfo->bForceEndOfSection = FALSE; */
    }

    if (CPLGetLastErrorNo() != 0)
        nStatus = -1;

    return nStatus;
}


/**********************************************************************
 *                          AVCE00DeleteCoverage()
 *
 * Delete a coverage directory, its contents, and the associated info
 * tables.
 *
 * Note:
 * When deleting tables, only the ../info/arc????.nit and arc????.dat
 * need to be deleted; the arc.dir does not need to be updated.  This
 * is exactly what Arc/Info's KILL command does.
 *
 * Returns 0 on success or -1 on error.
 **********************************************************************/
int     AVCE00DeleteCoverage(const char *pszCoverToDelete)
{
    int i, j, nStatus = 0;
    char *pszInfoPath, *pszCoverPath, *pszCoverName;
    const char *pszFname;
    char **papszTables=NULL, **papszFiles=NULL;
    AVCE00ReadPtr   psInfo;
    VSIStatBuf      sStatBuf;
    AVCCoverType    eCoverType;

    CPLErrorReset();

    /*-----------------------------------------------------------------
     * Since we don't want to duplicate all the logic to figure coverage
     * and info dir name, etc... we'll simply open the coverage and
     * grab the info we need from the coverage handle.
     * By the same way, this will verify that the coverage exists and is
     * valid.
     *----------------------------------------------------------------*/
    psInfo = AVCE00ReadOpen(pszCoverToDelete);

    if (psInfo == NULL)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Cannot delete coverage %s: it does not appear to be valid\n",
                 pszCoverToDelete);
        return -1;
    }

    pszCoverPath = CPLStrdup(psInfo->pszCoverPath);
    pszInfoPath = CPLStrdup(psInfo->pszInfoPath);
    pszCoverName = CPLStrdup(psInfo->pszCoverName);
    eCoverType = psInfo->eCoverType;

    AVCE00ReadClose(psInfo);

    /*-----------------------------------------------------------------
     * Delete files in cover directory.
     *----------------------------------------------------------------*/
    papszFiles = CPLReadDir(pszCoverPath);
    for(i=0; nStatus==0 && papszFiles && papszFiles[i]; i++)
    {
        if (!EQUAL(".", papszFiles[i]) &&
            !EQUAL("..", papszFiles[i]))
        {
            pszFname = CPLSPrintf("%s%s", pszCoverPath, papszFiles[i]);
            if (unlink(pszFname) != 0)
            {
                CPLError(CE_Failure, CPLE_FileIO, 
                         "Failed deleting %s%s", 
                         pszCoverPath, papszFiles[i]);
                nStatus = -1;
                break;
            }
        }
    }

    CSLDestroy(papszFiles);
    papszFiles = NULL;

    /*-----------------------------------------------------------------
     * Get the list of info files (ARC????) to delete and delete them
     * (No 'info' directory for PC coverages)
     *----------------------------------------------------------------*/
    if (nStatus == 0 && eCoverType != AVCCoverPC && eCoverType != AVCCoverPC2)
    {
        papszTables = AVCBinReadListTables(pszInfoPath, pszCoverName, 
                                           &papszFiles, eCoverType,
                                           NULL /*DBCSInfo*/);

        for(i=0; nStatus==0 && papszFiles && papszFiles[i]; i++)
        {
            /* Convert table filename to lowercases */
            for(j=0; papszFiles[i] && papszFiles[i][j]!='\0'; j++)
                papszFiles[i][j] = tolower(papszFiles[i][j]);

            /* Delete the .DAT file */
            pszFname = CPLSPrintf("%s%s.dat", pszInfoPath, papszFiles[i]);
            if ( VSIStat(pszFname, &sStatBuf) != -1 &&
                 unlink(pszFname) != 0)
            {
                CPLError(CE_Failure, CPLE_FileIO, 
                         "Failed deleting %s%s", 
                         pszInfoPath, papszFiles[i]);
                nStatus = -1;
                break;
            }

            /* Delete the .DAT file */
            pszFname = CPLSPrintf("%s%s.nit", pszInfoPath, papszFiles[i]);
            if ( VSIStat(pszFname, &sStatBuf) != -1 &&
                 unlink(pszFname) != 0)
            {
                CPLError(CE_Failure, CPLE_FileIO, 
                         "Failed deleting %s%s", 
                         pszInfoPath, papszFiles[i]);
                nStatus = -1;
                break;
            }
        }

        CSLDestroy(papszTables);
        CSLDestroy(papszFiles);
    }

    /*-----------------------------------------------------------------
     * Delete the coverage directory itself
     * In some cases, the directory could be locked by another application 
     * on the same system or somewhere on the network.  
     * Define AVC_IGNORE_RMDIR_ERROR at compile time if you want this 
     * error to be ignored.
     *----------------------------------------------------------------*/
    if (VSIRmdir(pszCoverPath) != 0)
    {
#ifndef AVC_IGNORE_RMDIR_ERROR
        CPLError(CE_Failure, CPLE_FileIO, 
                 "Failed deleting directory %s", pszCoverPath);
        nStatus = -1;
#endif
    }

    CPLFree(pszCoverPath);
    CPLFree(pszInfoPath);
    CPLFree(pszCoverName);

    return nStatus;
}

