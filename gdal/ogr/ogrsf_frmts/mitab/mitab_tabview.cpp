/**********************************************************************
 * $Id: mitab_tabview.cpp,v 1.22 2010-07-07 19:00:15 aboudreault Exp $
 *
 * Name:     mitab_tabfile.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the TABView class, used to handle .TAB
 *           datasets composed of a number of .TAB files linked through 
 *           indexed fields.
 * Author:   Daniel Morissette, dmorissette@dmsolutions.ca
 *
 **********************************************************************
 * Copyright (c) 1999-2002, Daniel Morissette
 * Copyright (c) 2014, Even Rouault <even.rouault at spatialys.com>
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
 * $Log: mitab_tabview.cpp,v $
 * Revision 1.22  2010-07-07 19:00:15  aboudreault
 * Cleanup Win32 Compile Warnings (GDAL bug #2930)
 *
 * Revision 1.21  2010-07-05 19:01:20  aboudreault
 * Reverted last SetFeature change in mitab_capi.cpp and fixed another memory leak
 *
 * Revision 1.20  2010-01-07 20:39:12  aboudreault
 * Added support to handle duplicate field names, Added validation to check if a field name start with a number (bug 2141)
 *
 * Revision 1.19  2008-03-05 20:35:39  dmorissette
 * Replace MITAB 1.x SetFeature() with a CreateFeature() for V2.x (bug 1859)
 *
 * Revision 1.18  2008/01/29 20:46:32  dmorissette
 * Added support for v9 Time and DateTime fields (byg 1754)
 *
 * Revision 1.17  2007/06/21 14:00:23  dmorissette
 * Added missing cast in isspace() calls to avoid failed assertion on Windows
 * (MITAB bug 1737, GDAL ticket 1678))
 *
 * Revision 1.16  2007/06/12 13:52:38  dmorissette
 * Added IMapInfoFile::SetCharset() method (bug 1734)
 *
 * Revision 1.15  2007/06/12 12:50:40  dmorissette
 * Use Quick Spatial Index by default until bug 1732 is fixed (broken files
 * produced by current coord block splitting technique).
 *
 * Revision 1.14  2007/03/21 21:15:56  dmorissette
 * Added SetQuickSpatialIndexMode() which generates a non-optimal spatial
 * index but results in faster write time (bug 1669)
 *
 * Revision 1.13  2004/06/30 20:29:04  dmorissette
 * Fixed refs to old address danmo@videotron.ca
 *
 * Revision 1.12  2002/02/22 20:44:51  julien
 * Prevent infinite loop with TABRelation by suppress the m_poCurFeature object
 * from the class and setting it in the calling function and add GetFeature in
 * the class. (bug 706)
 *
 * Revision 1.11  2002/01/10 05:13:22  daniel
 * Prevent crash if .IND file is deleted (but 703)
 *
 * Revision 1.10  2002/01/10 04:52:58  daniel
 * Support 'select * ...' syntax + 'open table..." directives with/without .tab
 *
 * Revision 1.9  2001/06/27 19:52:26  warmerda
 * use VSIUnlink() instead of unlink()
 *
 * Revision 1.8  2001/03/15 03:57:51  daniel
 * Added implementation for new OGRLayer::GetExtent(), returning data MBR.
 *
 * Revision 1.7  2000/09/28 16:39:44  warmerda
 * avoid warnings for unused, and unitialized variables
 *
 * Revision 1.6  2000/02/28 17:12:22  daniel
 * Write support for joined tables and indexed fields
 *
 * Revision 1.5  2000/01/15 22:30:45  daniel
 * Switch to MIT/X-Consortium OpenSource license
 *
 * Revision 1.4  1999/12/19 17:40:16  daniel
 * Init + delete m_poRelation properly
 *
 * Revision 1.3  1999/12/14 05:53:00  daniel
 * Fixed compile warnings
 *
 * Revision 1.2  1999/12/14 04:04:10  daniel
 * Added bforceFlags to GetBounds() and GetFeatureCountByType()
 *
 * Revision 1.1  1999/12/14 02:10:32  daniel
 * Initial revision
 *
 **********************************************************************/

#include "mitab.h"
#include "mitab_utils.h"

#include <ctype.h>      /* isspace() */

/*=====================================================================
 *                      class TABView
 *====================================================================*/


/**********************************************************************
 *                   TABView::TABView()
 *
 * Constructor.
 **********************************************************************/
TABView::TABView()
{
    m_pszFname = NULL;
    m_eAccessMode = TABRead;
    m_papszTABFile = NULL;
    m_pszVersion = NULL;

    m_numTABFiles = 0;
    m_papszTABFnames = NULL;
    m_papoTABFiles = NULL;
    m_nMainTableIndex = -1;

    m_papszFieldNames = NULL;
    m_papszWhereClause = NULL;

    m_poRelation = NULL;
    m_bRelFieldsCreated = FALSE;
}

/**********************************************************************
 *                   TABView::~TABView()
 *
 * Destructor.
 **********************************************************************/
TABView::~TABView()
{
    Close();
}


GIntBig TABView::GetFeatureCount (int bForce)
{

    if (m_nMainTableIndex != -1)
        return m_papoTABFiles[m_nMainTableIndex]->GetFeatureCount( bForce );

    return 0;
}

void TABView::ResetReading()
{
    if (m_nMainTableIndex != -1)
        m_papoTABFiles[m_nMainTableIndex]->ResetReading();
}


/**********************************************************************
 *                   TABView::Open()
 *
 * Open a .TAB dataset and the associated files, and initialize the 
 * structures to be ready to read features from it.
 *
 * This class is used to open .TAB files that define a view on
 * two other .TAB files.  Regular .TAB datasets should be opened using
 * the TABFile class instead.
 *
 * Set bTestOpenNoError=TRUE to silently return -1 with no error message
 * if the file cannot be opened.  This is intended to be used in the
 * context of a TestOpen() function.  The default value is FALSE which
 * means that an error is reported if the file cannot be opened.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABView::Open(const char *pszFname, TABAccess eAccess,
                  GBool bTestOpenNoError /*= FALSE*/ )
{
    char nStatus = 0;
   
    if (m_numTABFiles > 0)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Open() failed: object already contains an open file");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Validate access mode and call the right open method
     *----------------------------------------------------------------*/
    if (eAccess == TABRead)
    {
        m_eAccessMode = TABRead;
        nStatus = (char)OpenForRead(pszFname, bTestOpenNoError);
    }
    else if (eAccess == TABWrite)
    {
        m_eAccessMode = TABWrite;
        nStatus = (char)OpenForWrite(pszFname);
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Open() failed: access mode \"%d\" not supported", eAccess);
        return -1;
    }

    return nStatus;
}


/**********************************************************************
 *                   TABView::OpenForRead()
 *
 * Open for reading
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABView::OpenForRead(const char *pszFname, 
                         GBool bTestOpenNoError /*= FALSE*/ )
{
    char *pszPath = NULL;
    int nFnameLen = 0;
   
    m_eAccessMode = TABRead;

    /*-----------------------------------------------------------------
     * Read main .TAB (text) file
     *----------------------------------------------------------------*/
    m_pszFname = CPLStrdup(pszFname);

#ifndef _WIN32
    /*-----------------------------------------------------------------
     * On Unix, make sure extension uses the right cases
     * We do it even for write access because if a file with the same
     * extension already exists we want to overwrite it.
     *----------------------------------------------------------------*/
    TABAdjustFilenameExtension(m_pszFname);
#endif

    /*-----------------------------------------------------------------
     * Open .TAB file... since it's a small text file, we will just load
     * it as a stringlist in memory.
     *----------------------------------------------------------------*/
    m_papszTABFile = TAB_CSLLoad(m_pszFname);
    if (m_papszTABFile == NULL)
    {
        if (!bTestOpenNoError)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed opening %s.", m_pszFname);
        }
        
        CPLFree(m_pszFname);
        return -1;
    }

    /*-------------------------------------------------------------
     * Look for a line with the "create view" keyword.
     * If there is no "create view", then we may have a valid .TAB file,
     * but we do not support it in this class.
     *------------------------------------------------------------*/
    GBool bCreateViewFound = FALSE;
    for (int i=0; 
         !bCreateViewFound && m_papszTABFile && m_papszTABFile[i];
         i++)
    {
        const char *pszStr = m_papszTABFile[i];
        while(*pszStr != '\0' && isspace((unsigned char)*pszStr))
            pszStr++;
        if (EQUALN(pszStr, "create view", 11))
            bCreateViewFound = TRUE;
    }

    if ( !bCreateViewFound )
    {
        if (!bTestOpenNoError)
            CPLError(CE_Failure, CPLE_NotSupported,
                     "%s contains no table view definition.  "
                     "This type of .TAB file cannot be read by this library.",
                     m_pszFname);
        else
            CPLErrorReset();

        CPLFree(m_pszFname);

        return -1;
    }

    /*-----------------------------------------------------------------
     * OK, this appears to be a valid TAB view dataset...
     * Extract the path component from the main .TAB filename
     * to build the filename of the sub-tables
     *----------------------------------------------------------------*/
    pszPath = CPLStrdup(m_pszFname);
    nFnameLen = strlen(pszPath);
    for( ; nFnameLen > 0; nFnameLen--)
    {
        if (pszPath[nFnameLen-1] == '/' || 
            pszPath[nFnameLen-1] == '\\' )
        {
            break;
        }
        pszPath[nFnameLen-1] = '\0';
    }

    /*-----------------------------------------------------------------
     * Extract the useful info from the TAB header
     *----------------------------------------------------------------*/
    if (ParseTABFile(pszPath, bTestOpenNoError) != 0)
    {
        // Failed parsing... an error has already been produced if necessary
        CPLFree(pszPath);
        Close();
        return -1;
    }
    CPLFree(pszPath);
    pszPath = NULL;

    /*-----------------------------------------------------------------
     * __TODO__ For now, we support only 2 files linked through a single
     *          field... so we'll do some validation first to make sure
     *          that what we found in the header respects these limitations.
     *----------------------------------------------------------------*/
    if (m_numTABFiles != 2)
    {
        if (!bTestOpenNoError)
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Open Failed: Dataset %s defines a view on %d tables. "
                     "This is not currently supported.",
                     m_pszFname, m_numTABFiles);
        Close();
        return -1;
    }

    /*-----------------------------------------------------------------
     * Open all the tab files listed in the view
     *----------------------------------------------------------------*/
    m_papoTABFiles = (TABFile**)CPLCalloc(m_numTABFiles, sizeof(TABFile*));

    for (int iFile=0; iFile < m_numTABFiles; iFile++)
    {
#ifndef _WIN32
        TABAdjustFilenameExtension(m_papszTABFnames[iFile]);
#endif
        
        m_papoTABFiles[iFile] = new TABFile;
   
        if ( m_papoTABFiles[iFile]->Open(m_papszTABFnames[iFile],
                                         m_eAccessMode, bTestOpenNoError) != 0)
        {
            // Open Failed... an error has already been reported, just return.
            if (bTestOpenNoError)
                CPLErrorReset();
            Close();
            return -1;
        }
    }

    /*-----------------------------------------------------------------
     * Create TABRelation... this will build FeatureDefn, etc.
     * __TODO__ For now this assumes only 2 tables in the view...
     *----------------------------------------------------------------*/
    m_poRelation = new TABRelation;
    
    CPLAssert(m_nMainTableIndex == 0);
    CPLAssert(CSLCount(m_papszWhereClause) == 5);
    char *pszTableName = TABGetBasename(m_pszFname);
    if ( m_poRelation->Init(pszTableName,
                            m_papoTABFiles[0], m_papoTABFiles[1],
                            m_papszWhereClause[4], m_papszWhereClause[2],
                            m_papszFieldNames)  != 0 )
    {
        // An error should already have been reported
        CPLFree(pszTableName);
        Close();
        return -1;
    }
    CPLFree(pszTableName);

    return 0;
}


/**********************************************************************
 *                   TABView::OpenForWrite()
 *
 * Create a new TABView dataset
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABView::OpenForWrite(const char *pszFname)
{
    int nFnameLen = 0;
   
    m_eAccessMode = TABWrite;

    /*-----------------------------------------------------------------
     * Read main .TAB (text) file
     *----------------------------------------------------------------*/
    m_pszFname = CPLStrdup(pszFname);

#ifndef _WIN32
    /*-----------------------------------------------------------------
     * On Unix, make sure extension uses the right cases
     * We do it even for write access because if a file with the same
     * extension already exists we want to overwrite it.
     *----------------------------------------------------------------*/
    TABAdjustFilenameExtension(m_pszFname);
#endif

    /*-----------------------------------------------------------------
     * Extract the path component from the main .TAB filename
     *----------------------------------------------------------------*/
    char *pszPath = CPLStrdup(m_pszFname);
    nFnameLen = strlen(pszPath);
    for( ; nFnameLen > 0; nFnameLen--)
    {
        if (pszPath[nFnameLen-1] == '/' || 
            pszPath[nFnameLen-1] == '\\' )
        {
            break;
        }
        pszPath[nFnameLen-1] = '\0';
    }

    char *pszBasename = TABGetBasename(m_pszFname);

    /*-----------------------------------------------------------------
     * Create the 2 TAB files for the view.
     *
     * __TODO__ For now, we support only 2 files linked through a single
     *          field... not sure if anything else than that can be useful
     *          anyways.
     *----------------------------------------------------------------*/
    m_numTABFiles = 2;
    m_papszTABFnames = NULL;
    m_nMainTableIndex = 0;
    m_bRelFieldsCreated = FALSE;

    m_papoTABFiles = (TABFile**)CPLCalloc(m_numTABFiles, sizeof(TABFile*));

    for (int iFile=0; iFile < m_numTABFiles; iFile++)
    {
        m_papszTABFnames = CSLAppendPrintf(m_papszTABFnames, "%s%s%d.tab", 
                                               pszPath, pszBasename, iFile+1);
#ifndef _WIN32
        TABAdjustFilenameExtension(m_papszTABFnames[iFile]);
#endif
        
        m_papoTABFiles[iFile] = new TABFile;
   
        if ( m_papoTABFiles[iFile]->Open(m_papszTABFnames[iFile], m_eAccessMode) != 0)
        {
            // Open Failed... an error has already been reported, just return.
            CPLFree(pszPath);
            CPLFree(pszBasename);
            Close();
            return -1;
        }
    }

    /*-----------------------------------------------------------------
     * Create TABRelation... 
     *----------------------------------------------------------------*/
    m_poRelation = new TABRelation;
    
    if ( m_poRelation->Init(pszBasename,
                            m_papoTABFiles[0], m_papoTABFiles[1],
                            NULL, NULL, NULL)  != 0 )
    {
        // An error should already have been reported
        CPLFree(pszPath);
        CPLFree(pszBasename);
        Close();
        return -1;
    }

    CPLFree(pszPath);
    CPLFree(pszBasename);

    return 0;
}



/**********************************************************************
 *                   TABView::ParseTABFile()
 *
 * Scan the lines of the TAB file, and store any useful information into
 * class members.  The main piece of information being the sub-table
 * names, and the list of fields to include in the view that we will
 * use to build the OGRFeatureDefn for this file.
 *
 * It is assumed that the TAB header file is already loaded in m_papszTABFile
 *
 * This private method should be used only during the Open() call.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABView::ParseTABFile(const char *pszDatasetPath, 
                          GBool bTestOpenNoError /*=FALSE*/)
{
    int         iLine, numLines;
    char        **papszTok=NULL;
    GBool       bInsideTableDef = FALSE;

    if (m_eAccessMode != TABRead)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "ParseTABFile() can be used only with Read access.");
        return -1;
    }

    numLines = CSLCount(m_papszTABFile);

    for(iLine=0; iLine<numLines; iLine++)
    {
        /*-------------------------------------------------------------
         * Tokenize the next .TAB line, and check first keyword
         *------------------------------------------------------------*/
        CSLDestroy(papszTok);
        papszTok = CSLTokenizeStringComplex(m_papszTABFile[iLine], " \t(),;",
                                            TRUE, FALSE);
        if (CSLCount(papszTok) < 2)
            continue;   // All interesting lines have at least 2 tokens

        if (EQUAL(papszTok[0], "!version"))
        {
            m_pszVersion = CPLStrdup(papszTok[1]);
        }
        else if (EQUAL(papszTok[0], "!charset"))
        {
            CPLFree(m_pszCharset);
            m_pszCharset = CPLStrdup(papszTok[1]);
        }
        else if (EQUAL(papszTok[0], "open") &&
                 EQUAL(papszTok[1], "table") &&
                 CSLCount(papszTok) >= 3)
        {
            // Source table name may be either "filename" or "filename.tab"
            int nLen = strlen(papszTok[2]);
            if (nLen > 4 && EQUAL(papszTok[2]+nLen-4, ".tab"))
                papszTok[2][nLen-4] = '\0';

            m_papszTABFnames = CSLAppendPrintf(m_papszTABFnames, 
                                               "%s%s.tab", 
                                               pszDatasetPath, papszTok[2]);
        }
        else if (EQUAL(papszTok[0], "create") &&
                 EQUAL(papszTok[1], "view") )
        {
            bInsideTableDef = TRUE;
        }
        else if (bInsideTableDef &&
                 (EQUAL(papszTok[0],"Select")))
        {
            /*---------------------------------------------------------
             * We found the list of table fields (comma-delimited list)
             *--------------------------------------------------------*/
            int iTok;
            for(iTok=1; papszTok[iTok] != NULL; iTok++)
                m_papszFieldNames = CSLAddString(m_papszFieldNames, 
                                                 papszTok[iTok]);

        }
        else if (bInsideTableDef &&
                 (EQUAL(papszTok[0],"where")))
        {
            /*---------------------------------------------------------
             * We found the where clause that relates the 2 tables
             * Something in the form:
             *   where table1.field1=table2.field2
             * The tokenized array will contain:
             *  {"where", "table1", "field1", "table2", "field2"}
             *--------------------------------------------------------*/
            m_papszWhereClause =CSLTokenizeStringComplex(m_papszTABFile[iLine],
                                                         " \t(),;=.",
                                                         TRUE, FALSE);

            /*---------------------------------------------------------
             * For now we are very limiting on the format of the WHERE
             * clause... we will be more permitting as we learn more about
             * what it can contain... (I don't want to implement a full SQL 
             * parser here!!!).  If you encountered this error,
             * (and are reading this!) please report the test dataset 
             * that produced the error and I'll see if we can support it.
             *--------------------------------------------------------*/
            if (CSLCount( m_papszWhereClause ) != 5)
            {
                if (!bTestOpenNoError)
                    CPLError(CE_Failure, CPLE_NotSupported,
                     "WHERE clause in %s is not in a supported format: \"%s\"",
                             m_pszFname, m_papszTABFile[iLine]);
                return -1;
            }
        }
        else
        {
            // Simply Ignore unrecognized lines
        }
    }

    CSLDestroy(papszTok);

    /*-----------------------------------------------------------------
     * The main table is the one from which we read the geometries, etc...
     * For now we assume it is always the first one in the list
     *----------------------------------------------------------------*/
    m_nMainTableIndex = 0;

    /*-----------------------------------------------------------------
     * Make sure all required class members are set
     *----------------------------------------------------------------*/
    m_numTABFiles = CSLCount(m_papszTABFnames);

    if (m_pszCharset == NULL)
        m_pszCharset = CPLStrdup("Neutral");
    if (m_pszVersion == NULL)
        m_pszVersion = CPLStrdup("100");

    if (CSLCount(m_papszFieldNames) == 0 )
    {
        if (!bTestOpenNoError)
            CPLError(CE_Failure, CPLE_NotSupported,
                     "%s: header contains no table field definition.  "
                     "This type of .TAB file cannot be read by this library.",
                     m_pszFname);
        return -1;
    }

    if (CSLCount(m_papszWhereClause) == 0 )
    {
        if (!bTestOpenNoError)
            CPLError(CE_Failure, CPLE_NotSupported,
                     "%s: WHERE clause not found or missing in header.  "
                     "This type of .TAB file cannot be read by this library.",
                     m_pszFname);
        return -1;
    }
    return 0;
}


/**********************************************************************
 *                   TABView::WriteTABFile()
 *
 * Generate the TAB header file.  This is usually done during the 
 * Close() call.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABView::WriteTABFile()
{
    VSILFILE *fp;

    CPLAssert(m_eAccessMode == TABWrite);
    CPLAssert(m_numTABFiles == 2);
    CPLAssert(GetLayerDefn());

    char *pszTable  = TABGetBasename(m_pszFname);
    char *pszTable1 = TABGetBasename(m_papszTABFnames[0]);
    char *pszTable2 = TABGetBasename(m_papszTABFnames[1]);

    if ( (fp = VSIFOpenL(m_pszFname, "wt")) != NULL)
    {
        // Version is always 100, no matter what the sub-table's version is
        VSIFPrintfL(fp, "!Table\n");
        VSIFPrintfL(fp, "!Version 100\n");

        VSIFPrintfL(fp, "Open Table \"%s\" Hide\n", pszTable1);
        VSIFPrintfL(fp, "Open Table \"%s\" Hide\n", pszTable2);
        VSIFPrintfL(fp, "\n");
        VSIFPrintfL(fp, "Create View %s As\n", pszTable);
        VSIFPrintfL(fp, "Select ");

        OGRFeatureDefn *poDefn = GetLayerDefn();
        for(int iField=0; iField<poDefn->GetFieldCount(); iField++)
        {
            OGRFieldDefn *poFieldDefn = poDefn->GetFieldDefn(iField);
            if (iField == 0)
                VSIFPrintfL(fp, "%s", poFieldDefn->GetNameRef());
            else
                VSIFPrintfL(fp, ",%s", poFieldDefn->GetNameRef());
        }
        VSIFPrintfL(fp, "\n");

        VSIFPrintfL(fp, "From %s, %s\n", pszTable2, pszTable1);
        VSIFPrintfL(fp, "Where %s.%s=%s.%s\n", pszTable2, 
                                           m_poRelation->GetRelFieldName(),
                                           pszTable1, 
                                           m_poRelation->GetMainFieldName());


        VSIFCloseL(fp);
    }
    else
    {
        CPLFree(pszTable);
        CPLFree(pszTable1);
        CPLFree(pszTable2);
        
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed to create file `%s'", m_pszFname);
        return -1;
    }

    CPLFree(pszTable);
    CPLFree(pszTable1);
    CPLFree(pszTable2);

    return 0;
}


/**********************************************************************
 *                   TABView::Close()
 *
 * Close current file, and release all memory used.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABView::Close()
{
    // In write access, the main .TAB file has not been written yet.
    if (m_eAccessMode == TABWrite && m_poRelation)
        WriteTABFile();

    for(int i=0; m_papoTABFiles && i<m_numTABFiles; i++)
    {
        if (m_papoTABFiles[i])
            delete m_papoTABFiles[i];  // Automatically closes.
    }
    CPLFree(m_papoTABFiles);
    m_papoTABFiles = NULL;
    m_numTABFiles = 0;

    
    /*-----------------------------------------------------------------
     * __TODO__ OK, MapInfo does not like to see a .map and .id file
     * attached to the second table, even if they're empty.
     * We'll use a little hack to delete them now, but eventually we
     * should avoid creating them at all.
     *----------------------------------------------------------------*/
    if (m_eAccessMode == TABWrite && m_pszFname)
    {
        m_pszFname[strlen(m_pszFname)-4] = '\0';
        char *pszFile = CPLStrdup(CPLSPrintf("%s2.map", m_pszFname));
        TABAdjustFilenameExtension(pszFile);
        VSIUnlink(pszFile);

        sprintf(pszFile, "%s2.id", m_pszFname);
        TABAdjustFilenameExtension(pszFile);
        VSIUnlink(pszFile);

        CPLFree(pszFile);
    }
    // End of hack!


    CPLFree(m_pszFname);
    m_pszFname = NULL;

    CSLDestroy(m_papszTABFile);
    m_papszTABFile = NULL;

    CPLFree(m_pszVersion);
    m_pszVersion = NULL;
    CPLFree(m_pszCharset);
    m_pszCharset = NULL;

    CSLDestroy(m_papszTABFnames);
    m_papszTABFnames = NULL;

    CSLDestroy(m_papszFieldNames);
    m_papszFieldNames = NULL;
    CSLDestroy(m_papszWhereClause);
    m_papszWhereClause = NULL;

    m_nMainTableIndex = -1;

    if (m_poRelation)
        delete m_poRelation;
    m_poRelation = NULL;

    m_bRelFieldsCreated = FALSE;

    return 0;
}

/**********************************************************************
 *                   TABView::SetQuickSpatialIndexMode()
 *
 * Select "quick spatial index mode". 
 *
 * The default behavior of MITAB is to generate an optimized spatial index,
 * but this results in slower write speed. 
 *
 * Applications that want faster write speed and do not care
 * about the performance of spatial queries on the resulting file can
 * use SetQuickSpatialIndexMode() to require the creation of a non-optimal
 * spatial index (actually emulating the type of spatial index produced
 * by MITAB before version 1.6.0). In this mode writing files can be 
 * about 5 times faster, but spatial queries can be up to 30 times slower.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABView::SetQuickSpatialIndexMode(GBool bQuickSpatialIndexMode/*=TRUE*/)
{
    if (m_eAccessMode != TABWrite || m_numTABFiles == 0)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "SetQuickSpatialIndexMode() failed: file not opened for write access.");
        return -1;
    }

    for (int iFile=0; iFile < m_numTABFiles; iFile++)
    {
        if ( m_papoTABFiles[iFile]->SetQuickSpatialIndexMode(bQuickSpatialIndexMode) != 0)
        {
            // An error has already been reported, just return.
            return -1;
        }
    }

    return 0;
}


/**********************************************************************
 *                   TABView::GetNextFeatureId()
 *
 * Returns feature id that follows nPrevId, or -1 if it is the
 * last feature id.  Pass nPrevId=-1 to fetch the first valid feature id.
 **********************************************************************/
GIntBig TABView::GetNextFeatureId(GIntBig nPrevId)
{
    if (m_nMainTableIndex != -1)
        return m_papoTABFiles[m_nMainTableIndex]->GetNextFeatureId(nPrevId);

    return -1;
}

/**********************************************************************
 *                   TABView::GetFeatureRef()
 *
 * Fill and return a TABFeature object for the specified feature id.
 *
 * The retruned pointer is a reference to an object owned and maintained
 * by this TABView object.  It should not be altered or freed by the 
 * caller and its contents is guaranteed to be valid only until the next
 * call to GetFeatureRef() or Close().
 *
 * Returns NULL if the specified feature id does not exist of if an
 * error happened.  In any case, CPLError() will have been called to
 * report the reason of the failure.
 **********************************************************************/
TABFeature *TABView::GetFeatureRef(GIntBig nFeatureId)
{
    
    /*-----------------------------------------------------------------
     * Make sure file is opened 
     *----------------------------------------------------------------*/
    if (m_poRelation == NULL)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "GetFeatureRef() failed: file is not opened!");
        return NULL;
    }

    if(m_poCurFeature)
    {
        delete m_poCurFeature;
        m_poCurFeature = NULL;
    }

    m_poCurFeature = m_poRelation->GetFeature(nFeatureId);
    m_nCurFeatureId = nFeatureId;
    m_poCurFeature->SetFID(m_nCurFeatureId);
    return m_poCurFeature;
}


/**********************************************************************
 *                   TABView::CreateFeature()
 *
 * Write a new feature to this dataset. The passed in feature is updated 
 * with the new feature id.
 *
 * Returns OGRERR_NONE on success, or an appropriate OGRERR_ code if an
 * error happened in which case, CPLError() will have been called to
 * report the reason of the failure.
 **********************************************************************/
OGRErr TABView::CreateFeature(TABFeature *poFeature)
{
    if (m_eAccessMode != TABWrite)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "CreateFeature() can be used only with Write access.");
        return OGRERR_UNSUPPORTED_OPERATION;
    }

    if (m_poRelation == NULL)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "CreateFeature() failed: file is not opened!");
        return OGRERR_FAILURE;
    }

    /*-----------------------------------------------------------------
     * If we're about to write the first feature, then we must finish
     * the initialization of the view first by creating the MI_refnum fields
     *----------------------------------------------------------------*/
    if (!m_bRelFieldsCreated)
    {
        if (m_poRelation->CreateRelFields() != 0)
            return OGRERR_FAILURE;
        m_bRelFieldsCreated = TRUE;
    }

    int nFeatureId = m_poRelation->WriteFeature(poFeature);
    if (nFeatureId < 0)
        return OGRERR_FAILURE;

    poFeature->SetFID(nFeatureId);

    return OGRERR_NONE;
}



/**********************************************************************
 *                   TABView::GetLayerDefn()
 *
 * Returns a reference to the OGRFeatureDefn that will be used to create
 * features in this dataset.
 *
 * Returns a reference to an object that is maintained by this TABView
 * object (and thus should not be modified or freed by the caller) or
 * NULL if the OGRFeatureDefn has not been initialized yet (i.e. no file
 * opened yet)
 **********************************************************************/
OGRFeatureDefn *TABView::GetLayerDefn()
{
    if (m_poRelation)
        return m_poRelation->GetFeatureDefn();

    return NULL;
}

/**********************************************************************
 *                   TABView::SetFeatureDefn()
 *
 * Set the FeatureDefn for this dataset.
 *
 * For now, fields passed through SetFeatureDefn will not be mapped
 * properly, so this function can be used only with an empty feature defn.
 **********************************************************************/
int TABView::SetFeatureDefn(OGRFeatureDefn *poFeatureDefn,
                            CPL_UNUSED TABFieldType *paeMapInfoNativeFieldTypes /* =NULL */)
{
    if (m_poRelation)
        return m_poRelation->SetFeatureDefn(poFeatureDefn);

    return -1;
}


/**********************************************************************
 *                   TABView::GetNativeFieldType()
 *
 * Returns the native MapInfo field type for the specified field.
 *
 * Returns TABFUnknown if file is not opened, or if specified field index is
 * invalid.
 *
 * Note that field ids are positive and start at 0.
 **********************************************************************/
TABFieldType TABView::GetNativeFieldType(int nFieldId)
{
    if (m_poRelation)
        return m_poRelation->GetNativeFieldType(nFieldId);

    return TABFUnknown;
}


/**********************************************************************
 *                   TABView::AddFieldNative()
 *
 * Create a new field using a native mapinfo data type... this is an 
 * alternative to defining fields through the OGR interface.
 * This function should be called after creating a new dataset, but before 
 * writing the first feature.
 *
 * This function will build/update the OGRFeatureDefn that will have to be
 * used when writing features to this dataset.
 *
 * A reference to the OGRFeatureDefn can be obtained using GetLayerDefn().
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABView::AddFieldNative(const char *pszName, TABFieldType eMapInfoType,
                            int nWidth /*=0*/, int nPrecision /*=0*/,
                            GBool bIndexed /*=FALSE*/, GBool bUnique/*=FALSE*/, int bApproxOK)
{
    if (m_poRelation)
        return m_poRelation->AddFieldNative(pszName, eMapInfoType,
                                            nWidth, nPrecision,
                                            bIndexed, bUnique, bApproxOK);

    return -1;
}

/**********************************************************************
 *                   TABView::SetFieldIndexed()
 *
 * Request that a field be indexed.  This will create the .IND file if
 * necessary, etc.
 *
 * Note that field ids are positive and start at 0.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABView::SetFieldIndexed(int nFieldId)
{
    if (m_poRelation)
        return m_poRelation->SetFieldIndexed(nFieldId);

    return -1;
}

/**********************************************************************
 *                   TABView::IsFieldIndexed()
 *
 * Returns TRUE if field is indexed, or FALSE otherwise.
 **********************************************************************/
GBool TABView::IsFieldIndexed(int nFieldId)
{
    if (m_poRelation)
        return m_poRelation->IsFieldIndexed(nFieldId);

    return FALSE;
}

/**********************************************************************
 *                   TABView::IsFieldUnique()
 *
 * Returns TRUE if field is in the Unique table, or FALSE otherwise.
 **********************************************************************/
GBool TABView::IsFieldUnique(int nFieldId)
{
    if (m_poRelation)
        return m_poRelation->IsFieldUnique(nFieldId);

    return FALSE;
}


/**********************************************************************
 *                   TABView::GetBounds()
 *
 * Fetch projection coordinates bounds of a dataset.
 *
 * The bForce flag has no effect on TAB files since the bounds are
 * always in the header.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABView::GetBounds(double &dXMin, double &dYMin, 
                       double &dXMax, double &dYMax,
                       GBool bForce /*= TRUE*/)
{
    if (m_nMainTableIndex == -1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
             "GetBounds() can be called only after dataset has been opened.");
        return -1;
    }

    return m_papoTABFiles[m_nMainTableIndex]->GetBounds(dXMin, dYMin,
                                                        dXMax, dYMax,
                                                        bForce);
}

/**********************************************************************
 *                   TABView::GetExtent()
 *
 * Fetch extent of the data currently stored in the dataset.
 *
 * The bForce flag has no effect on TAB files since that value is
 * always in the header.
 *
 * Returns OGRERR_NONE/OGRRERR_FAILURE.
 **********************************************************************/
OGRErr TABView::GetExtent (OGREnvelope *psExtent, int bForce)
{
    if (m_nMainTableIndex == -1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
             "GetExtent() can be called only after dataset has been opened.");
        return -1;
    }

    return m_papoTABFiles[m_nMainTableIndex]->GetExtent(psExtent, bForce);

}

/**********************************************************************
 *                   TABView::GetFeatureCountByType()
 *
 * Return number of features of each type.
 *
 * Note that the sum of the 4 returned values may be different from
 * the total number of features since features with NONE geometry
 * are not taken into account here.
 *
 * Note: the bForce flag has nmo effect on .TAB files since the info
 * is always in the header.
 *
 * Returns 0 on success, or silently returns -1 (with no error) if this
 * information is not available.
 **********************************************************************/
int TABView::GetFeatureCountByType(int &numPoints, int &numLines,
                                   int &numRegions, int &numTexts,
                                   GBool bForce /*= TRUE*/)
{
    if (m_nMainTableIndex == -1)
        return -1;

    return m_papoTABFiles[m_nMainTableIndex]->GetFeatureCountByType(numPoints,
                                                                    numLines,
                                                                    numRegions,
                                                                    numTexts,
                                                                    bForce);
}


/**********************************************************************
 *                   TABView::GetSpatialRef()
 *
 * Returns a reference to an OGRSpatialReference for this dataset.
 * If the projection parameters have not been parsed yet, then we will
 * parse them before returning.
 *
 * The returned object is owned and maintained by this TABFile and
 * should not be modified or freed by the caller.
 *
 * Returns NULL if the SpatialRef cannot be accessed.
 **********************************************************************/
OGRSpatialReference *TABView::GetSpatialRef()
{
    if (m_nMainTableIndex == -1)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "GetSpatialRef() failed: file has not been opened yet.");
        return NULL;
    }

    return m_papoTABFiles[m_nMainTableIndex]->GetSpatialRef();
}

/**********************************************************************
 *                   TABView::SetSpatialRef()
 **********************************************************************/
int TABView::SetSpatialRef(OGRSpatialReference *poSpatialRef)
{
    if (m_nMainTableIndex == -1)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "SetSpatialRef() failed: file has not been opened yet.");
        return -1;
    }

    return m_papoTABFiles[m_nMainTableIndex]->SetSpatialRef(poSpatialRef);
}



/**********************************************************************
 *                   TABView::SetBounds()
 **********************************************************************/
int TABView::SetBounds(double dXMin, double dYMin, 
                       double dXMax, double dYMax)
{
    if (m_nMainTableIndex == -1)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "SetBounds() failed: file has not been opened yet.");
        return -1;
    }

    return m_papoTABFiles[m_nMainTableIndex]->SetBounds(dXMin, dYMin,
                                                        dXMax, dYMax);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int TABView::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCSequentialWrite))
        return TRUE;

    else if( EQUAL(pszCap,OLCRandomWrite))
        return FALSE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return m_poFilterGeom == NULL;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastGetExtent) )
        return TRUE;

    else 
        return FALSE;
}






/**********************************************************************
 *                   TABView::Dump()
 *
 * Dump block contents... available only in DEBUG mode.
 **********************************************************************/
#ifdef DEBUG

void TABView::Dump(FILE *fpOut /*=NULL*/)
{
    if (fpOut == NULL)
        fpOut = stdout;

    fprintf(fpOut, "----- TABView::Dump() -----\n");

    if (m_numTABFiles > 0)
    {
        fprintf(fpOut, "File is not opened.\n");
    }
    else
    {
        fprintf(fpOut, "File is opened: %s\n", m_pszFname);
        fprintf(fpOut, "View contains %d tables\n", m_numTABFiles);

    }

    fflush(fpOut);
}

#endif // DEBUG



/*=====================================================================
 *                      class TABRelation
 *====================================================================*/


/**********************************************************************
 *                   TABRelation::TABRelation()
 *
 * Constructor.
 **********************************************************************/
TABRelation::TABRelation()
{
    m_poMainTable = NULL;
    m_pszMainFieldName = NULL;
    m_nMainFieldNo = -1;

    m_poRelTable = NULL;
    m_pszRelFieldName = NULL;
    m_nRelFieldNo = -1;
    m_nRelFieldIndexNo = -1;
    m_poRelINDFileRef = NULL;

    m_nUniqueRecordNo = 0;

    m_panMainTableFieldMap = NULL;
    m_panRelTableFieldMap = NULL;

    m_poDefn = NULL;
}

/**********************************************************************
 *                   TABRelation::~TABRelation()
 *
 * Destructor.
 **********************************************************************/
TABRelation::~TABRelation()
{
    ResetAllMembers();
}

/**********************************************************************
 *                   TABRelation::ResetAllMembers()
 *
 * Reset all class members.
 **********************************************************************/
void TABRelation::ResetAllMembers()
{
    m_poMainTable = NULL;
    CPLFree(m_pszMainFieldName);
    m_pszMainFieldName = NULL;
    m_nMainFieldNo = -1;

    m_poRelTable = NULL;
    CPLFree(m_pszRelFieldName);
    m_pszRelFieldName = NULL;
    m_nRelFieldNo = -1;
    m_nRelFieldIndexNo = -1;

    m_nUniqueRecordNo = 0;

    // No need to close m_poRelINDFileRef since we only got a ref. to it
    m_poRelINDFileRef = NULL;

    CPLFree(m_panMainTableFieldMap);
    m_panMainTableFieldMap = NULL;
    CPLFree(m_panRelTableFieldMap);
    m_panRelTableFieldMap = NULL;

    /*-----------------------------------------------------------------
     * Note: we have to check the reference count before deleting m_poDefn
     *----------------------------------------------------------------*/
    if (m_poDefn && m_poDefn->Dereference() == 0)
        delete m_poDefn;
    m_poDefn = NULL;

}

/**********************************************************************
 *                   TABRelation::Init()
 *
 * Set the details of the relation: the main and related tables, the fields
 * through which they will be connected, and the list of fields to select.
 * After this call, we are ready to read data records.
 *
 * For write access, Init() is called with pszMain/RelFieldName and
 * **papszSelectedFields passed as NULL.  They will have to be set through
 * other methods before a first feature can be written.
 *
 * A new OGRFeatureDefn is also built for the combined tables.
 *
 * Returns 0 on success, or -1 or error.
 **********************************************************************/
int  TABRelation::Init(const char *pszViewName,
                       TABFile *poMainTable, TABFile *poRelTable,
                       const char *pszMainFieldName,
                       const char *pszRelFieldName,
                       char **papszSelectedFields)
{
    if (poMainTable == NULL || poRelTable == NULL)
        return -1;

    // We'll need the feature Defn later...
    OGRFeatureDefn *poMainDefn, *poRelDefn;

    poMainDefn = poMainTable->GetLayerDefn();
    poRelDefn = poRelTable->GetLayerDefn();

    /*-----------------------------------------------------------------
     * Keep info for later use about source tables, etc.
     *----------------------------------------------------------------*/
    ResetAllMembers();

    m_poMainTable = poMainTable;
    if (pszMainFieldName)
    {
        m_pszMainFieldName = CPLStrdup(pszMainFieldName);
        m_nMainFieldNo = poMainDefn->GetFieldIndex(pszMainFieldName);
    }

    m_poRelTable = poRelTable;
    if (pszRelFieldName)
    {
        m_pszRelFieldName = CPLStrdup(pszRelFieldName);
        m_nRelFieldNo = poRelDefn->GetFieldIndex(pszRelFieldName);
        m_nRelFieldIndexNo = poRelTable->GetFieldIndexNumber(m_nRelFieldNo);
        m_poRelINDFileRef = poRelTable->GetINDFileRef();

        if (m_nRelFieldIndexNo >= 0 && m_poRelINDFileRef == NULL)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Field %s is indexed but the .IND file is missing.",
                     pszRelFieldName);
            return -1;
        }
    }

    /*-----------------------------------------------------------------
     * Init field maps.  For each field in each table, a -1 means that
     * the field is not selected, and a value >=0 is the index of the 
     * field in the view's FeatureDefn
     *----------------------------------------------------------------*/
    int i;
    int numFields1 = (poMainDefn?poMainDefn->GetFieldCount():0);
    int numFields2 = (poRelDefn?poRelDefn->GetFieldCount():0);

    m_panMainTableFieldMap = (int*)CPLMalloc((numFields1+1)*sizeof(int));
    for(i=0; i<numFields1; i++)
        m_panMainTableFieldMap[i] = -1;
    m_panRelTableFieldMap = (int*)CPLMalloc((numFields2+1)*sizeof(int));
    for(i=0; i<numFields2; i++)
        m_panRelTableFieldMap[i] = -1;

    /*-----------------------------------------------------------------
     * If selectedFields = "*" then select all fields from both tables
     *----------------------------------------------------------------*/
    if (CSLCount(papszSelectedFields) == 1 && 
        EQUAL(papszSelectedFields[0], "*") )
    {
        CSLDestroy(papszSelectedFields);
        papszSelectedFields = NULL;

        for(i=0; i<numFields1; i++)
        {
            OGRFieldDefn *poFieldDefn = poMainDefn->GetFieldDefn(i);

            papszSelectedFields = CSLAddString(papszSelectedFields, 
                                               poFieldDefn->GetNameRef());
        }

        for(i=0; i<numFields2; i++)
        {
            OGRFieldDefn *poFieldDefn = poRelDefn->GetFieldDefn(i);

            if (CSLFindString(papszSelectedFields, 
                              poFieldDefn->GetNameRef()) != -1)
                continue;  // Avoid duplicate field name in view

            papszSelectedFields = CSLAddString(papszSelectedFields, 
                                               poFieldDefn->GetNameRef());
        }

    }

    /*-----------------------------------------------------------------
     * Create new FeatureDefn and copy selected fields definitions
     * while updating the appropriate field maps.
     *----------------------------------------------------------------*/
    int nIndex, numSelFields = CSLCount(papszSelectedFields);
    OGRFieldDefn *poFieldDefn;

    m_poDefn = new OGRFeatureDefn(pszViewName);
    // Ref count defaults to 0... set it to 1
    m_poDefn->Reference();

    for(i=0; i<numSelFields ; i++)
    {
        if (poMainDefn &&
            (nIndex=poMainDefn->GetFieldIndex(papszSelectedFields[i])) >=0)
        {
            /* Field from the main table
             */
            poFieldDefn = poMainDefn->GetFieldDefn(nIndex);
            m_poDefn->AddFieldDefn(poFieldDefn);
            m_panMainTableFieldMap[nIndex] = m_poDefn->GetFieldCount()-1;
        }
        else if (poRelDefn &&
                 (nIndex=poRelDefn->GetFieldIndex(papszSelectedFields[i]))>=0)
        {
            /* Field from the related table
             */
            poFieldDefn = poRelDefn->GetFieldDefn(nIndex);
            m_poDefn->AddFieldDefn(poFieldDefn);
            m_panRelTableFieldMap[nIndex] = m_poDefn->GetFieldCount()-1;
        }
        else
        {
            // Hummm... field does not exist... likely an unsupported feature!
            // At least send a warning and ignore the field.
            CPLError(CE_Warning, CPLE_IllegalArg,
                     "Selected Field %s not found in source tables %s and %s",
                     papszSelectedFields[i], 
                     poMainDefn->GetName(), poRelDefn->GetName());
        }
    }

    return 0;
}


/**********************************************************************
 *                   TABRelation::CreateRelFields()
 *
 * For write access, create the integer fields in each table that will
 * link them, and setup everything to be ready to write the first feature.
 *
 * This function should be called just before writing the first feature.
 *
 * Returns 0 on success, or -1 or error.
 **********************************************************************/
int  TABRelation::CreateRelFields()
{
    int i;

    /*-----------------------------------------------------------------
     * Create the field in each table.  
     * The default name is "MI_refnum" but if a field with the same name
     * already exists then we'll try to generate a unique name.
     *----------------------------------------------------------------*/
    m_pszMainFieldName = CPLStrdup("MI_Refnum      ");
    strcpy(m_pszMainFieldName, "MI_Refnum");
    i = 1;
    while(m_poDefn->GetFieldIndex(m_pszMainFieldName) >= 0)
    {
        sprintf(m_pszMainFieldName, "MI_Refnum_%d", i++);
    }
    m_pszRelFieldName = CPLStrdup(m_pszMainFieldName);

    m_nMainFieldNo = m_nRelFieldNo = -1;
    if (m_poMainTable->AddFieldNative(m_pszMainFieldName,
                                      TABFInteger, 0, 0) == 0)
        m_nMainFieldNo = m_poMainTable->GetLayerDefn()->GetFieldCount()-1;

    if (m_poRelTable->AddFieldNative(m_pszRelFieldName,
                                     TABFInteger, 0, 0) == 0)
        m_nRelFieldNo = m_poRelTable->GetLayerDefn()->GetFieldCount()-1;

    if (m_nMainFieldNo == -1 || m_nRelFieldNo == -1)
        return -1;

    if (m_poMainTable->SetFieldIndexed(m_nMainFieldNo) == -1)
        return -1;

    if ((m_nRelFieldIndexNo=m_poRelTable->SetFieldIndexed(m_nRelFieldNo)) ==-1)
        return -1;

    m_poRelINDFileRef = m_poRelTable->GetINDFileRef();

    /*-----------------------------------------------------------------
     * Update field maps
     *----------------------------------------------------------------*/
    OGRFeatureDefn *poMainDefn, *poRelDefn;

    poMainDefn = m_poMainTable->GetLayerDefn();
    poRelDefn = m_poRelTable->GetLayerDefn();

    m_panMainTableFieldMap = (int*)CPLRealloc(m_panMainTableFieldMap,
                                      poMainDefn->GetFieldCount()*sizeof(int));
    m_panMainTableFieldMap[poMainDefn->GetFieldCount()-1] = -1;

    m_panRelTableFieldMap = (int*)CPLRealloc(m_panRelTableFieldMap,
                                      poRelDefn->GetFieldCount()*sizeof(int));
    m_panRelTableFieldMap[poRelDefn->GetFieldCount()-1] = -1;

    /*-----------------------------------------------------------------
     * Make sure the first unique field (in poRelTable) is indexed since
     * it is the one against which we will try to match records.
     *----------------------------------------------------------------*/
    if ( m_poRelTable->SetFieldIndexed(0) == -1)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABRelation::GetFeature()
 *
 * Fill and return a TABFeature object for the specified feature id.
 *
 * The retuned pointer is a new TABFeature that will have to be freed
 * by the caller.
 *
 * Returns NULL if the specified feature id does not exist of if an
 * error happened.  In any case, CPLError() will have been called to
 * report the reason of the failure.
 *
 * __TODO__ The current implementation fetches the features from each table
 * and creates a 3rd feature to merge them.  There would be room for 
 * optimization, at least by avoiding the duplication of the geometry 
 * which can be big sometimes... but this would imply changes at the
 * lower-level in the lib. and we won't go there yet.
 **********************************************************************/
TABFeature *TABRelation::GetFeature(int nFeatureId)
{
    TABFeature *poMainFeature;
    TABFeature *poCurFeature;

    /*-----------------------------------------------------------------
     * Make sure init() has been called
     *----------------------------------------------------------------*/
    if (m_poMainTable == NULL || m_poRelTable == NULL)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "GetFeatureRef() failed: object not initialized yet!");
        return NULL;
    }

    /*-----------------------------------------------------------------
     * Read main feature and create a new one of the right type
     *----------------------------------------------------------------*/
    if ((poMainFeature = m_poMainTable->GetFeatureRef(nFeatureId)) == NULL)
    {
        // Feature cannot be read from main table... 
        // an error has already been reported.
        return NULL;
    }

    poCurFeature = poMainFeature->CloneTABFeature(m_poDefn);

    /*-----------------------------------------------------------------
     * Keep track of FID and copy the geometry 
     *----------------------------------------------------------------*/
    poCurFeature->SetFID(nFeatureId);

    if (poCurFeature->GetFeatureClass() != TABFCNoGeomFeature)
    {
        OGRGeometry *poGeom;
        poGeom = poMainFeature->GetGeometryRef();
        poCurFeature->SetGeometry(poGeom);
    }

    /*-----------------------------------------------------------------
     * Fetch feature from related table
     *
     * __TODO__ Right now we support only many-to-1 relationships, but
     *          it might be possible to have several related entries
     *          for a single key, and in this case we should return
     *          one new feature for each of them.
     *----------------------------------------------------------------*/
    TABFeature *poRelFeature=NULL;
    GByte *pKey = BuildFieldKey(poMainFeature, m_nMainFieldNo,
                            m_poMainTable->GetNativeFieldType(m_nMainFieldNo),
                                m_nRelFieldIndexNo);
    int i;
    int nRelFeatureId = m_poRelINDFileRef->FindFirst(m_nRelFieldIndexNo, pKey);
    
    if (nRelFeatureId > 0)
        poRelFeature = m_poRelTable->GetFeatureRef(nRelFeatureId);

    /*-----------------------------------------------------------------
     * Copy fields from poMainFeature
     *----------------------------------------------------------------*/
    for(i=0; i<poMainFeature->GetFieldCount(); i++)
    {
        if (m_panMainTableFieldMap[i] != -1)
        {
            poCurFeature->SetField(m_panMainTableFieldMap[i], 
                                     poMainFeature->GetRawFieldRef(i));
        }
    }

    /*-----------------------------------------------------------------
     * Copy fields from poRelFeature...
     *
     * NOTE: For now, if no corresponding feature is found in RelTable
     *       then we will just leave the corresponding fields unset.
     *----------------------------------------------------------------*/
    for(i=0; poRelFeature && i<poRelFeature->GetFieldCount(); i++)
    {
        if (m_panRelTableFieldMap[i] != -1)
        {
            poCurFeature->SetField(m_panRelTableFieldMap[i], 
                                     poRelFeature->GetRawFieldRef(i));
        }
    }

    return poCurFeature;
}



/**********************************************************************
 *                   TABRelation::BuildFieldKey()
 *
 * Return the index key for the specified field in poFeature.
 * Simply maps the call to the proper method in the TABINDFile class.
 *
 * Returns a reference to a TABINDFile internal buffer that should not
 * be freed by the caller.
 **********************************************************************/
GByte *TABRelation::BuildFieldKey(TABFeature *poFeature, int nFieldNo,
                                  TABFieldType eType, int nIndexNo)
{
    GByte *pKey = NULL;

    switch(eType)
    {
      case TABFChar:
        pKey = m_poRelINDFileRef->BuildKey(nIndexNo,
                             poFeature->GetFieldAsString(nFieldNo));
        break;

      case TABFDecimal:
      case TABFFloat:
        pKey = m_poRelINDFileRef->BuildKey(nIndexNo,
                             poFeature->GetFieldAsDouble(nFieldNo));
        break;

      // __TODO__ DateTime fields are 8 bytes long, not supported yet by
      // the indexing code (see bug #1844).
      case TABFDateTime:
        CPLError(CE_Failure, CPLE_NotSupported,
                 "TABRelation on field of type DateTime not supported yet.");
        break;

      case TABFInteger:
      case TABFSmallInt:
      case TABFDate:
      case TABFTime:
      case TABFLogical:
      default:
        pKey = m_poRelINDFileRef->BuildKey(nIndexNo,
                             poFeature->GetFieldAsInteger(nFieldNo));
        break;
    }

    return pKey;
}


/**********************************************************************
 *                   TABRelation::GetNativeFieldType()
 *
 * Returns the native MapInfo field type for the specified field.
 *
 * Returns TABFUnknown if file is not opened, or if specified field index is
 * invalid.
 *
 * Note that field ids are positive and start at 0.
 **********************************************************************/
TABFieldType TABRelation::GetNativeFieldType(int nFieldId)
{
    int i, numFields;

    if (m_poMainTable==NULL || m_poRelTable==NULL ||
        m_panMainTableFieldMap==NULL || m_panRelTableFieldMap==NULL)
        return TABFUnknown;

    /*-----------------------------------------------------------------
     * Look for nFieldId in the field maps and call the corresponding
     * TAB file's GetNativeFieldType()
     *----------------------------------------------------------------*/
    numFields = m_poMainTable->GetLayerDefn()->GetFieldCount();
    for(i=0; i<numFields; i++)
    {
        if (m_panMainTableFieldMap[i] == nFieldId)
        {
            return m_poMainTable->GetNativeFieldType(i);
        }
    }

    numFields = m_poRelTable->GetLayerDefn()->GetFieldCount();
    for(i=0; i<numFields; i++)
    {
        if (m_panRelTableFieldMap[i] == nFieldId)
        {
            return m_poRelTable->GetNativeFieldType(i);
        }
    }

    return TABFUnknown;
}


/**********************************************************************
 *                   TABRelation::AddFieldNative()
 *
 * Create a new field using a native mapinfo data type... this is an 
 * alternative to defining fields through the OGR interface.
 * This function should be called after creating a new dataset, but before 
 * writing the first feature.
 *
 * This function will build/update the OGRFeatureDefn that will have to be
 * used when writing features to this dataset.
 *
 * A reference to the OGRFeatureDefn can be obtained using GetLayerDefn().
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABRelation::AddFieldNative(const char *pszName, TABFieldType eMapInfoType,
                                int nWidth /*=0*/, int nPrecision /*=0*/,
                                GBool bIndexed /*=FALSE*/, GBool bUnique/*=FALSE*/, int bApproxOK)
{
    if (m_poMainTable==NULL || m_poRelTable==NULL ||
        m_panMainTableFieldMap==NULL || m_panRelTableFieldMap==NULL)
        return -1;

    if (!bUnique)
    {
        /*-------------------------------------------------------------
         * Add field to poMainTable and to m_poDefn
         *------------------------------------------------------------*/
        if (m_poMainTable->AddFieldNative(pszName, eMapInfoType,
                                          nWidth, nPrecision,
                                          bIndexed, bUnique, bApproxOK) != 0)
            return -1;

        OGRFeatureDefn *poMainDefn = m_poMainTable->GetLayerDefn();

        m_panMainTableFieldMap = (int*)CPLRealloc(m_panMainTableFieldMap,
                                      poMainDefn->GetFieldCount()*sizeof(int));

        m_poDefn->AddFieldDefn(poMainDefn->GetFieldDefn(poMainDefn->
                                                          GetFieldCount()-1));

        m_panMainTableFieldMap[poMainDefn->GetFieldCount()-1] = 
                                            m_poDefn->GetFieldCount()-1;
    }
    else
    {
        /*-------------------------------------------------------------
         * Add field to poRelTable and to m_poDefn
         *------------------------------------------------------------*/
        if (m_poRelTable->AddFieldNative(pszName, eMapInfoType,
                                         nWidth, nPrecision,
                                         bIndexed, bUnique, bApproxOK) != 0)
            return -1;

        OGRFeatureDefn *poRelDefn = m_poRelTable->GetLayerDefn();

        m_panRelTableFieldMap = (int*)CPLRealloc(m_panRelTableFieldMap,
                                      poRelDefn->GetFieldCount()*sizeof(int));

        m_poDefn->AddFieldDefn(poRelDefn->GetFieldDefn(poRelDefn->
                                                         GetFieldCount()-1));

        m_panRelTableFieldMap[poRelDefn->GetFieldCount()-1] =  
                                            m_poDefn->GetFieldCount()-1;

        // The first field in this table must be indexed.
        if (poRelDefn->GetFieldCount() == 1)
            m_poRelTable->SetFieldIndexed(0);
    }

    return 0;
}


/**********************************************************************
 *                   TABRelation::IsFieldIndexed()
 *
 * Returns TRUE is specified field is indexed.
 *
 * Note that field ids are positive and start at 0.
 **********************************************************************/
GBool TABRelation::IsFieldIndexed(int nFieldId)
{
    int i, numFields;

    if (m_poMainTable==NULL || m_poRelTable==NULL ||
        m_panMainTableFieldMap==NULL || m_panRelTableFieldMap==NULL)
        return FALSE;

    /*-----------------------------------------------------------------
     * Look for nFieldId in the field maps and call the corresponding
     * TAB file's GetNativeFieldType()
     *----------------------------------------------------------------*/
    numFields = m_poMainTable->GetLayerDefn()->GetFieldCount();
    for(i=0; i<numFields; i++)
    {
        if (m_panMainTableFieldMap[i] == nFieldId)
        {
            return m_poMainTable->IsFieldIndexed(i);
        }
    }

    numFields = m_poRelTable->GetLayerDefn()->GetFieldCount();
    for(i=0; i<numFields; i++)
    {
        if (m_panRelTableFieldMap[i] == nFieldId)
        {
            return m_poRelTable->IsFieldIndexed(i);
        }
    }

    return FALSE;
}

/**********************************************************************
 *                   TABRelation::SetFieldIndexed()
 *
 * Request that the specified field be indexed.  This will create the .IND
 * file, etc.
 *
 * Note that field ids are positive and start at 0.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABRelation::SetFieldIndexed(int nFieldId)
{
    int i, numFields;

    if (m_poMainTable==NULL || m_poRelTable==NULL ||
        m_panMainTableFieldMap==NULL || m_panRelTableFieldMap==NULL)
        return -1;

    /*-----------------------------------------------------------------
     * Look for nFieldId in the field maps and call the corresponding
     * TAB file's GetNativeFieldType()
     *----------------------------------------------------------------*/
    numFields = m_poMainTable->GetLayerDefn()->GetFieldCount();
    for(i=0; i<numFields; i++)
    {
        if (m_panMainTableFieldMap[i] == nFieldId)
        {
            return m_poMainTable->SetFieldIndexed(i);
        }
    }

    numFields = m_poRelTable->GetLayerDefn()->GetFieldCount();
    for(i=0; i<numFields; i++)
    {
        if (m_panRelTableFieldMap[i] == nFieldId)
        {
            return m_poRelTable->SetFieldIndexed(i);
        }
    }

    return -1;
}

/**********************************************************************
 *                   TABRelation::IsFieldUnique()
 *
 * Returns TRUE is specified field is part of the unique table (poRelTable).
 *
 * Note that field ids are positive and start at 0.
 **********************************************************************/
GBool TABRelation::IsFieldUnique(int nFieldId)
{
    int i, numFields;

    if (m_poMainTable==NULL || m_poRelTable==NULL ||
        m_panMainTableFieldMap==NULL || m_panRelTableFieldMap==NULL)
        return FALSE;

    /*-----------------------------------------------------------------
     * Look for nFieldId in the poRelTable field map
     *----------------------------------------------------------------*/
    numFields = m_poRelTable->GetLayerDefn()->GetFieldCount();
    for(i=0; i<numFields; i++)
    {
        if (m_panRelTableFieldMap[i] == nFieldId)
        {
            return TRUE;  // If it's here then it is unique!
        }
    }

    return FALSE;
}

/**********************************************************************
 *                   TABRelation::WriteFeature()
 *
 * Write a feature to this dataset.  
 *
 * For now only sequential writes are supported (i.e. with nFeatureId=-1)
 * but eventually we should be able to do random access by specifying
 * a value through nFeatureId.
 *
 * Returns the new featureId (> 0) on success, or -1 if an
 * error happened in which case, CPLError() will have been called to
 * report the reason of the failure.
 **********************************************************************/
int TABRelation::WriteFeature(TABFeature *poFeature, int nFeatureId /*=-1*/)
{
    TABFeature *poMainFeature=NULL;

    if (nFeatureId != -1)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "WriteFeature(): random access not implemented yet.");
        return -1;
    }

    CPLAssert(m_poMainTable && m_poRelTable);

    // We'll need the feature Defn later...
    OGRFeatureDefn *poMainDefn, *poRelDefn;

    poMainDefn = m_poMainTable->GetLayerDefn();
    poRelDefn = m_poRelTable->GetLayerDefn();

    /*-----------------------------------------------------------------
     * Create one feature for each table
     * Copy the geometry only to the feature from the main table
     *----------------------------------------------------------------*/
    poMainFeature = poFeature->CloneTABFeature(poMainDefn);

    if (poFeature->GetFeatureClass() != TABFCNoGeomFeature)
    {
        OGRGeometry *poGeom;
        poGeom = poFeature->GetGeometryRef();
        poMainFeature->SetGeometry(poGeom);
    }

    /*-----------------------------------------------------------------
     * Copy fields to poMainFeature
     *----------------------------------------------------------------*/
    for(int i=0; i<poMainDefn->GetFieldCount(); i++)
    {
        if (m_panMainTableFieldMap[i] != -1)
        {
            poMainFeature->SetField(i, 
                      poFeature->GetRawFieldRef(m_panMainTableFieldMap[i]));
        }
    }

    /*-----------------------------------------------------------------
     * Look for a record id for the unique fields, and write a new 
     * record if necessary
     *----------------------------------------------------------------*/
    int nRecordNo = 0;
    int nUniqueIndexNo=-1;
    if (m_panMainTableFieldMap[0] != -1)
        nUniqueIndexNo =m_poRelTable->GetFieldIndexNumber( 0 );

    if (nUniqueIndexNo > 0)
    {
        GByte *pKey = BuildFieldKey(poFeature, 0,
                                    m_poRelTable->GetNativeFieldType(0),
                                    nUniqueIndexNo);

        if ((nRecordNo=m_poRelINDFileRef->FindFirst(nUniqueIndexNo, pKey))==-1)
            return -1;

        if (nRecordNo == 0)
        {
            /*---------------------------------------------------------
             * No record in poRelTable yet for this unique value...
             * add one now...
             *--------------------------------------------------------*/
            TABFeature *poRelFeature = new TABFeature(poRelDefn);

            for(int i=0;  i<poRelDefn->GetFieldCount(); i++)
            {
                if (m_panRelTableFieldMap[i] != -1)
                {
                    poRelFeature->SetField(i, 
                          poFeature->GetRawFieldRef(m_panRelTableFieldMap[i]));
                }
            }

            nRecordNo = ++m_nUniqueRecordNo;

            poRelFeature->SetField(m_nRelFieldNo, nRecordNo);

            if (m_poRelTable->CreateFeature(poRelFeature) == OGRERR_NONE)
                return -1;

            delete poRelFeature;
        }
    }


    /*-----------------------------------------------------------------
     * Write poMainFeature to the main table
     *----------------------------------------------------------------*/
    poMainFeature->SetField(m_nMainFieldNo, nRecordNo);

    if (m_poMainTable->CreateFeature(poMainFeature) != OGRERR_NONE)
        nFeatureId = poMainFeature->GetFID();
    else
        nFeatureId = -1;

    delete poMainFeature;

    return nFeatureId;
}


/**********************************************************************
 *                   TABFile::SetFeatureDefn()
 *
 * NOT FULLY IMPLEMENTED YET... 
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABRelation::SetFeatureDefn(OGRFeatureDefn *poFeatureDefn,
                                CPL_UNUSED TABFieldType *paeMapInfoNativeFieldTypes /* =NULL */)
{
    if (m_poDefn && m_poDefn->GetFieldCount() > 0)
    {
        CPLAssert(m_poDefn==NULL);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Keep a reference to the OGRFeatureDefn... we'll have to take the
     * reference count into account when we are done with it.
     *----------------------------------------------------------------*/
    if (m_poDefn && m_poDefn->Dereference() == 0)
        delete m_poDefn;

    m_poDefn = poFeatureDefn;
    m_poDefn->Reference();

    return 0;
}
