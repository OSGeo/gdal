/**********************************************************************
 * $Id: mitab_tabview.cpp,v 1.3 1999/12/14 05:53:00 daniel Exp $
 *
 * Name:     mitab_tabfile.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the TABView class, used to handle .TAB
 *           datasets composed of a number of .TAB files linked through 
 *           indexed fields.
 * Author:   Daniel Morissette, danmo@videotron.ca
 *
 **********************************************************************
 * Copyright (c) 1999, Daniel Morissette
 *
 * All rights reserved.  This software may be copied or reproduced, in
 * all or in part, without the prior written consent of its author,
 * Daniel Morissette (danmo@videotron.ca).  However, any material copied
 * or reproduced must bear the original copyright notice (above), this 
 * original paragraph, and the original disclaimer (below).
 *  
 * The entire risk as to the results and performance of the software,
 * supporting text and other information contained in this file
 * (collectively called the "Software") is with the user.  Although 
 * considerable efforts have been used in preparing the Software, the 
 * author does not warrant the accuracy or completeness of the Software.
 * In no event will the author be liable for damages, including loss of
 * profits or consequential damages, arising out of the use of the 
 * Software.
 * 
 **********************************************************************
 *
 * $Log: mitab_tabview.cpp,v $
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
    m_pszCharset = NULL;

    m_numTABFiles = 0;
    m_papszTABFnames = NULL;
    m_papoTABFiles = NULL;
    m_nMainTableIndex = -1;

    m_papszFieldNames = NULL;
    m_papszWhereClause = NULL;
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


int TABView::GetFeatureCount (int bForce)
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
 * The only supported access mode for this class is "r" (read-only).
 *
 * Set bTestOpenNoError=TRUE to silently return -1 with no error message
 * if the file cannot be opened.  This is intended to be used in the
 * context of a TestOpen() function.  The default value is FALSE which
 * means that an error is reported if the file cannot be opened.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABView::Open(const char *pszFname, const char *pszAccess,
                  GBool bTestOpenNoError /*= FALSE*/ )
{
    char *pszPath = NULL;
    int nFnameLen = 0;
   
    if (m_numTABFiles > 0)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Open() failed: object already contains an open file");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Validate access mode... only READ access is supported
     *----------------------------------------------------------------*/
    if (EQUALN(pszAccess, "r", 1))
    {
        m_eAccessMode = TABRead;
        pszAccess = "rb";
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Open() failed: access mode \"%s\" not supported", pszAccess);
        return -1;
    }

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
        while(*pszStr != '\0' && isspace(*pszStr))
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
                                         pszAccess, bTestOpenNoError) != 0)
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
            m_pszCharset = CPLStrdup(papszTok[1]);
        }
        else if (EQUAL(papszTok[0], "open") &&
                 EQUAL(papszTok[1], "table") &&
                 CSLCount(papszTok) >= 3)
        {
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
        m_pszVersion = CPLStrdup("300");

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
 *                   TABView::Close()
 *
 * Close current file, and release all memory used.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABView::Close()
{

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

    for(int i=0; m_papoTABFiles && i<m_numTABFiles; i++)
    {
        if (m_papoTABFiles[i])
            delete m_papoTABFiles[i];
    }
    CPLFree(m_papoTABFiles);
    m_papoTABFiles = NULL;
    m_numTABFiles = 0;

    CSLDestroy(m_papszFieldNames);
    m_papszFieldNames = NULL;
    CSLDestroy(m_papszWhereClause);
    m_papszWhereClause = NULL;

    m_nMainTableIndex = -1;

    return 0;
}

/**********************************************************************
 *                   TABView::GetNextFeatureId()
 *
 * Returns feature id that follows nPrevId, or -1 if it is the
 * last feature id.  Pass nPrevId=-1 to fetch the first valid feature id.
 **********************************************************************/
int TABView::GetNextFeatureId(int nPrevId)
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
TABFeature *TABView::GetFeatureRef(int nFeatureId)
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

    return m_poRelation->GetFeatureRef(nFeatureId);
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

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int TABView::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCSequentialWrite) 
             || EQUAL(pszCap,OLCRandomWrite) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return m_poFilterGeom == NULL;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return FALSE;

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

    m_panMainTableFieldMap = NULL;
    m_panRelTableFieldMap = NULL;

    m_poDefn = NULL;
    m_poCurFeature = NULL;
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

    // No need to close m_poRelINDFileRef since we only got a ref. to it
    m_poRelINDFileRef = NULL;

    CPLFree(m_panMainTableFieldMap);
    m_panMainTableFieldMap = NULL;
    CPLFree(m_panRelTableFieldMap);
    m_panRelTableFieldMap = NULL;

    if (m_poCurFeature)
    {
        delete m_poCurFeature;
        m_poCurFeature = NULL;
    }
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
    m_pszMainFieldName = CPLStrdup(pszMainFieldName);
    m_nMainFieldNo = poMainDefn->GetFieldIndex(pszMainFieldName);

    m_poRelTable = poRelTable;
    m_pszRelFieldName = CPLStrdup(pszRelFieldName);
    m_nRelFieldNo = poRelDefn->GetFieldIndex(pszRelFieldName);
    m_nRelFieldIndexNo = poRelTable->GetFieldIndexNumber(m_nRelFieldNo);
    m_poRelINDFileRef = poRelTable->GetINDFileRef();

    /*-----------------------------------------------------------------
     * Init field maps.  For each field in each table, a -1 means that
     * the field is not selected, and a value >=0 is the index of the 
     * field in the view's FeatureDefn
     *----------------------------------------------------------------*/
    int i;

    m_panMainTableFieldMap = (int*)CPLMalloc(poMainDefn->GetFieldCount()*
                                             sizeof(int));
    for(i=0; i<poMainDefn->GetFieldCount(); i++)
        m_panMainTableFieldMap[i] = -1;
    m_panRelTableFieldMap = (int*)CPLMalloc(poRelDefn->GetFieldCount()*
                                             sizeof(int));
    for(i=0; i<poRelDefn->GetFieldCount(); i++)
        m_panRelTableFieldMap[i] = -1;

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
        if ((nIndex=poMainDefn->GetFieldIndex(papszSelectedFields[i])) >=0)
        {
            /* Field from the main table
             */
            poFieldDefn = poMainDefn->GetFieldDefn(nIndex);
            m_poDefn->AddFieldDefn(poFieldDefn);
            m_panMainTableFieldMap[nIndex] = m_poDefn->GetFieldCount()-1;
        }
        else if ((nIndex=poRelDefn->GetFieldIndex(papszSelectedFields[i]))>=0)
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
 *                   TABRelation::GetFeatureRef()
 *
 * Fill and return a TABFeature object for the specified feature id.
 *
 * The retuned pointer is a reference to an object owned and maintained
 * by this TABRelation object.  It should not be altered or freed by the 
 * caller and its contents is guaranteed to be valid only until the next
 * call to GetFeatureRef() or until the object is deleted.
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
TABFeature *TABRelation::GetFeatureRef(int nFeatureId)
{
    TABFeature *poMainFeature;

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
     * Flush current feature object
     *----------------------------------------------------------------*/
    if (m_poCurFeature)
    {
        delete m_poCurFeature;
        m_poCurFeature = NULL;
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

    m_poCurFeature = poMainFeature->CloneTABFeature(m_poDefn);

#ifdef __TODO__DELETE__
    switch(poMainFeature->GetFeatureClass())
    {
      case TABFCPoint:
        m_poCurFeature = new TABPoint(m_poDefn);
        break;
      case TABFCFontPoint:
        m_poCurFeature = new TABFontPoint(m_poDefn);
        break;
      case TABFCCustomPoint:
        m_poCurFeature = new TABCustomPoint(m_poDefn);
        break;
      case TABFCPolyline:
        m_poCurFeature = new TABPolyline(m_poDefn);
        break;
      case TABFCArc:
        m_poCurFeature = new TABArc(m_poDefn);
        break;
      case TABFCRegion:
        m_poCurFeature = new TABRegion(m_poDefn);
        break;
      case TABFCRectangle:
        m_poCurFeature = new TABRectangle(m_poDefn);
        break;
      case TABFCEllipse:
        m_poCurFeature = new TABEllipse(m_poDefn);
        break;
      case TABFCText:
        m_poCurFeature = new TABText(m_poDefn);
        break;
      default:
        // New feature type was added but has not been included here...
        // Assert in debug mode, but handle as NONE geometry otherwise.
        CPLAssert(FALSE);
        // no-break here
      case TABFCNoGeomFeature:
        m_poCurFeature = new TABFeature(m_poDefn);
    }
    CPLAssert(m_poCurFeature);
#endif

    /*-----------------------------------------------------------------
     * Keep track of FID and copy the geometry 
     *----------------------------------------------------------------*/
    m_poCurFeature->SetFID(nFeatureId);

    if (m_poCurFeature->GetFeatureClass() != TABFCNoGeomFeature)
    {
        OGRGeometry *poGeom;
        poGeom = poMainFeature->GetGeometryRef();
        m_poCurFeature->SetGeometry(poGeom);
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
    GByte *pKey = BuildMainKey(poMainFeature);
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
            m_poCurFeature->SetField(m_panMainTableFieldMap[i], 
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
            m_poCurFeature->SetField(m_panRelTableFieldMap[i], 
                                     poRelFeature->GetRawFieldRef(i));
        }
    }

    return m_poCurFeature;
}



/**********************************************************************
 *                   TABRelation::BuildMainKey()
 *
 * Return the encoded field key for field m_nMainFieldNo in the 
 * feature in argument.  Simply maps the call to the proper method
 * in the TABINDFile class.
 *
 * Returns a reference to a TABINDFile internal buffer that should not
 * be freed by the caller.
 **********************************************************************/
GByte *TABRelation::BuildMainKey(TABFeature *poFeature)
{
    GByte *pKey = NULL;

    switch(m_poMainTable->GetNativeFieldType(m_nMainFieldNo))
    {
      case TABFChar:
        pKey = m_poRelINDFileRef->BuildKey(m_nRelFieldIndexNo,
                             poFeature->GetFieldAsString(m_nMainFieldNo));
        break;

      case TABFDecimal:
      case TABFFloat:
        pKey = m_poRelINDFileRef->BuildKey(m_nRelFieldIndexNo,
                             poFeature->GetFieldAsDouble(m_nMainFieldNo));
        break;

      case TABFInteger:
      case TABFSmallInt:
      case TABFDate:
      case TABFLogical:
      default:
        pKey = m_poRelINDFileRef->BuildKey(m_nRelFieldIndexNo,
                             poFeature->GetFieldAsInteger(m_nMainFieldNo));
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

