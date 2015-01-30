/**********************************************************************
 * $Id: mitab_tabfile.cpp,v 1.78 2010-10-08 18:40:12 aboudreault Exp $
 *
 * Name:     mitab_tabfile.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the TABFile class, the main class of the lib.
 *           To be used by external programs to handle reading/writing of
 *           features from/to TAB datasets.
 * Author:   Daniel Morissette, dmorissette@dmsolutions.ca
 *
 **********************************************************************
 * Copyright (c) 1999-2003, Daniel Morissette
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
 * $Log: mitab_tabfile.cpp,v $
 * Revision 1.78  2010-10-08 18:40:12  aboudreault
 * Fixed missing initializations that cause crashes
 *
 * Revision 1.77  2010-10-08 18:38:13  aboudreault
 * Added attribute index support for the sql queries in mapinfo tab format (GDAL bug #3687)
 *
 * Revision 1.76  2010-07-07 19:00:15  aboudreault
 * Cleanup Win32 Compile Warnings (GDAL bug #2930)
 *
 * Revision 1.75  2010-07-05 14:58:33  aboudreault
 * Fixed bad feature count after we deleted a feature in MapInfo (bug 2227)
 *
 * Revision 1.74  2010-01-07 20:39:12  aboudreault
 * Added support to handle duplicate field names, Added validation to check if a field name start with a number (bug 2141)
 *
 * Revision 1.73  2008-11-27 20:50:23  aboudreault
 * Improved support for OGR date/time types. New Read/Write methods (bug 1948)
 * Added support of OGR date/time types for MIF features.
 *
 * Revision 1.72  2008/11/17 22:06:21  aboudreault
 * Added support to use OFTDateTime/OFTDate/OFTTime type when compiled with
 * OGR and fixed reading/writing support for these types.
 *
 * Revision 1.71  2008/09/26 14:40:24  aboudreault
 * Fixed bug: MITAB doesn't support writing DateTime type (bug 1948)
 *
 * Revision 1.70  2008/06/13 18:39:21  aboudreault
 * Fixed problem with corrupt pointer if file not found (bug 1899) and
 * fixed tabdump build problem if DEBUG option not provided (bug 1898)
 *
 * Revision 1.69  2008/03/05 20:59:10  dmorissette
 * Purged CVS logs in header
 *
 * Revision 1.68  2008/03/05 20:35:39  dmorissette
 * Replace MITAB 1.x SetFeature() with a CreateFeature() for V2.x (bug 1859)
 *
 * Revision 1.67  2008/01/29 21:56:39  dmorissette
 * Update dataset version properly for Date/Time/DateTime field types (#1754)
 *
 * Revision 1.66  2008/01/29 20:46:32  dmorissette
 * Added support for v9 Time and DateTime fields (byg 1754)
 *
 * Revision 1.65  2007/09/12 20:22:31  dmorissette
 * Added TABFeature::CreateFromMapInfoType()
 *
 * Revision 1.64  2007/06/21 14:00:23  dmorissette
 * Added missing cast in isspace() calls to avoid failed assertion on Windows
 * (MITAB bug 1737, GDAL ticket 1678))
 *
 * Revision 1.63  2007/06/12 13:52:38  dmorissette
 * Added IMapInfoFile::SetCharset() method (bug 1734)
 *
 * Revision 1.62  2007/06/12 12:50:40  dmorissette
 * Use Quick Spatial Index by default until bug 1732 is fixed (broken files
 * produced by current coord block splitting technique).
 *
 * Revision 1.61  2007/03/21 21:15:56  dmorissette
 * Added SetQuickSpatialIndexMode() which generates a non-optimal spatial
 * index but results in faster write time (bug 1669)
 *
 * ...
 *
 * Revision 1.1  1999/07/12 04:18:25  daniel
 * Initial checkin
 *
 **********************************************************************/

#include "mitab.h"
#include "mitab_utils.h"
#include "cpl_minixml.h"
#include "ogr_p.h"

#include <ctype.h>      /* isspace() */

#define UNSUPPORTED_OP_READ_ONLY "%s : unsupported operation on a read-only datasource."

/*=====================================================================
 *                      class TABFile
 *====================================================================*/


/**********************************************************************
 *                   TABFile::TABFile()
 *
 * Constructor.
 **********************************************************************/
TABFile::TABFile()
{
    m_eAccessMode = TABRead;
    m_pszFname = NULL;
    m_papszTABFile = NULL;
    m_nVersion = 300;
    m_eTableType = TABTableNative;

    m_poMAPFile = NULL;
    m_poDATFile = NULL;
    m_poINDFile = NULL;
    m_poDefn = NULL;
    m_poSpatialRef = NULL;
    m_poCurFeature = NULL;
    m_nCurFeatureId = 0;
    m_nLastFeatureId = 0;
    m_panIndexNo = NULL;

    bUseSpatialTraversal = FALSE;

    m_panMatchingFIDs = NULL; 
    m_iMatchingFID = 0; 
    
    m_bNeedTABRewrite = FALSE;
    m_bLastOpWasRead = FALSE;
    m_bLastOpWasWrite = FALSE;
}

/**********************************************************************
 *                   TABFile::~TABFile()
 *
 * Destructor.
 **********************************************************************/
TABFile::~TABFile()
{
    Close();
}


/************************************************************************/
/*                         GetFeatureCount()                          */
/************************************************************************/

GIntBig TABFile::GetFeatureCount (int bForce)
{
    
    if( m_poFilterGeom != NULL || m_poAttrQuery != NULL || bForce)
        return OGRLayer::GetFeatureCount( bForce );
    else
        return m_nLastFeatureId;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/
void TABFile::ResetReading()
{
    CPLFree(m_panMatchingFIDs);
    m_panMatchingFIDs = NULL;
    m_iMatchingFID = 0;
    
    m_nCurFeatureId = 0;
    if( m_poMAPFile != NULL )
        m_poMAPFile->ResetReading();

/* -------------------------------------------------------------------- */
/*      Decide whether to operate in spatial traversal mode or not,     */
/*      and ensure the current spatial filter is applied to the map     */
/*      file object.                                                    */
/* -------------------------------------------------------------------- */
    if( m_poMAPFile )
    {
        bUseSpatialTraversal = FALSE;
    
        m_poMAPFile->ResetCoordFilter();

        if( m_poFilterGeom != NULL )
        {
            OGREnvelope  sEnvelope;
            TABVertex sMin, sMax;
            /* TABMAPHeaderBlock *poHeader; */
            /* poHeader = m_poMAPFile->GetHeaderBlock(); */

            m_poFilterGeom->getEnvelope( &sEnvelope );
            m_poMAPFile->GetCoordFilter( sMin, sMax );

            if( sEnvelope.MinX > sMin.x
                || sEnvelope.MinY > sMin.y
                || sEnvelope.MaxX < sMax.x
                || sEnvelope.MaxY < sMax.y )
            {
                bUseSpatialTraversal = TRUE;
                sMin.x = sEnvelope.MinX;
                sMin.y = sEnvelope.MinY;
                sMax.x = sEnvelope.MaxX;
                sMax.y = sEnvelope.MaxY;
                m_poMAPFile->SetCoordFilter( sMin, sMax );
            }
        }
    }

    m_bLastOpWasRead = FALSE;
    m_bLastOpWasWrite = FALSE;
}

/**********************************************************************
 *                   TABFile::Open()
 *
 * Open a .TAB dataset and the associated files, and initialize the 
 * structures to be ready to read features from (or write to) it.
 *
 * Supported access modes are "r" (read-only) and "w" (create new dataset or
 * update).
 *
 * Set bTestOpenNoError=TRUE to silently return -1 with no error message
 * if the file cannot be opened.  This is intended to be used in the
 * context of a TestOpen() function.  The default value is FALSE which
 * means that an error is reported if the file cannot be opened.
 *
 * Note that dataset extents will have to be set using SetBounds() before
 * any feature can be written to a newly created dataset.
 *
 * In read mode, a valid dataset must have at least a .TAB and a .DAT file.
 * The .MAP and .ID files are optional and if they do not exist then
 * all features will be returned with NONE geometry.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABFile::Open(const char *pszFname, TABAccess eAccess,
                  GBool bTestOpenNoError /*=FALSE*/ )
{
    char *pszTmpFname = NULL;
    int nFnameLen = 0;

    CPLErrorReset();
   
    if (m_poMAPFile)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Open() failed: object already contains an open file");

        return -1;
    }
    
    m_eAccessMode = eAccess;

    /*-----------------------------------------------------------------
     * Make sure filename has a .TAB extension... 
     *----------------------------------------------------------------*/
    m_pszFname = CPLStrdup(pszFname);
    nFnameLen = strlen(m_pszFname);

    if (nFnameLen > 4 && (strcmp(m_pszFname+nFnameLen-4, ".TAB")==0 ||
                     strcmp(m_pszFname+nFnameLen-4, ".MAP")==0 ||
                     strcmp(m_pszFname+nFnameLen-4, ".DAT")==0 ) )
        strcpy(m_pszFname+nFnameLen-4, ".TAB");
    else if (nFnameLen > 4 && (EQUAL(m_pszFname+nFnameLen-4, ".tab") ||
                               EQUAL(m_pszFname+nFnameLen-4, ".map") ||
                               EQUAL(m_pszFname+nFnameLen-4, ".dat") ) )
        strcpy(m_pszFname+nFnameLen-4, ".tab");
    else
    {
        if (!bTestOpenNoError)
            CPLError(CE_Failure, CPLE_FileIO,
                     "Open() failed for %s: invalid filename extension",
                     m_pszFname);
        else
            CPLErrorReset();

        CPLFree(m_pszFname);
        m_pszFname = NULL;
        return -1;
    }

    pszTmpFname = CPLStrdup(m_pszFname);


#ifndef _WIN32
    /*-----------------------------------------------------------------
     * On Unix, make sure extension uses the right cases
     * We do it even for write access because if a file with the same
     * extension already exists we want to overwrite it.
     *----------------------------------------------------------------*/
    TABAdjustFilenameExtension(m_pszFname);
#endif

    /*-----------------------------------------------------------------
     * Handle .TAB file... depends on access mode.
     *----------------------------------------------------------------*/
    if (m_eAccessMode == TABRead || m_eAccessMode == TABReadWrite )
    {
        /*-------------------------------------------------------------
         * Open .TAB file... since it's a small text file, we will just load
         * it as a stringlist in memory.
         *------------------------------------------------------------*/
        m_papszTABFile = TAB_CSLLoad(m_pszFname);
        if (m_papszTABFile == NULL)
        {
            if (!bTestOpenNoError)
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Failed opening %s.", m_pszFname);
            }
            CPLFree(m_pszFname);
            m_pszFname = NULL;
            CSLDestroy(m_papszTABFile);
            m_papszTABFile = NULL;
            CPLFree( pszTmpFname );
            return -1;
        }

        /*-------------------------------------------------------------
         * Do a first pass on the TAB header to establish the type of 
         * dataset we have (NATIVE, DBF, etc.)... and also to know if
         * it is a supported type.
         *------------------------------------------------------------*/
        if ( ParseTABFileFirstPass(bTestOpenNoError) != 0 )
        {
            // No need to produce an error... it's already been done if 
            // necessary... just cleanup and exit.

            CPLFree(m_pszFname);
            m_pszFname = NULL;
            CSLDestroy(m_papszTABFile);
            m_papszTABFile = NULL;
            CPLFree( pszTmpFname );

            return -1;
        }
    }
    else
    {
        /*-------------------------------------------------------------
         * In Write access mode, the .TAB file will be written during the 
         * Close() call... we will just set some defaults here.
         *------------------------------------------------------------*/
        m_nVersion = 300;
        CPLFree(m_pszCharset);
        m_pszCharset = CPLStrdup("Neutral");
        m_eTableType = TABTableNative;

        /*-------------------------------------------------------------
         * Do initial setup of feature definition.
         *------------------------------------------------------------*/
        char *pszFeatureClassName = TABGetBasename(m_pszFname);
        m_poDefn = new OGRFeatureDefn(pszFeatureClassName);
        m_poDefn->Reference();
        CPLFree(pszFeatureClassName);

        m_bNeedTABRewrite = TRUE;
    }


    /*-----------------------------------------------------------------
     * Open .DAT file (or .DBF)
     *----------------------------------------------------------------*/
    if (nFnameLen > 4 && strcmp(pszTmpFname+nFnameLen-4, ".TAB")==0)
    {
        if (m_eTableType == TABTableDBF)
            strcpy(pszTmpFname+nFnameLen-4, ".DBF");
        else  // Default is NATIVE
            strcpy(pszTmpFname+nFnameLen-4, ".DAT");
    }
    else 
    {
        if (m_eTableType == TABTableDBF)
            strcpy(pszTmpFname+nFnameLen-4, ".dbf");
        else  // Default is NATIVE
            strcpy(pszTmpFname+nFnameLen-4, ".dat");
    }

#ifndef _WIN32
    TABAdjustFilenameExtension(pszTmpFname);
#endif

    m_poDATFile = new TABDATFile;
   
    if ( m_poDATFile->Open(pszTmpFname, eAccess, m_eTableType) != 0)
    {
        // Open Failed... an error has already been reported, just return.
        CPLFree(pszTmpFname);
        Close();
        if (bTestOpenNoError)
            CPLErrorReset();

        return -1;
    }

    m_nLastFeatureId = m_poDATFile->GetNumRecords();


    /*-----------------------------------------------------------------
     * Parse .TAB file field defs and build FeatureDefn (only in read access)
     *----------------------------------------------------------------*/
    if ( (m_eAccessMode == TABRead || m_eAccessMode == TABReadWrite) && ParseTABFileFields() != 0)
    {
        // Failed... an error has already been reported, just return.
        CPLFree(pszTmpFname);
        Close();
        if (bTestOpenNoError)
            CPLErrorReset();

        return -1;
    }


    /*-----------------------------------------------------------------
     * Open .MAP (and .ID) file
     * Note that the .MAP and .ID files are optional.  Failure to open them
     * is not an error... it simply means that all features will be returned
     * with NONE geometry.
     *----------------------------------------------------------------*/
    if (nFnameLen > 4 && strcmp(pszTmpFname+nFnameLen-4, ".DAT")==0)
        strcpy(pszTmpFname+nFnameLen-4, ".MAP");
    else 
        strcpy(pszTmpFname+nFnameLen-4, ".map");

#ifndef _WIN32
    TABAdjustFilenameExtension(pszTmpFname);
#endif

    m_poMAPFile = new TABMAPFile;
    if (m_eAccessMode == TABRead || m_eAccessMode == TABReadWrite)
    {
        /*-------------------------------------------------------------
         * Read access: .MAP/.ID are optional... try to open but return
         * no error if files do not exist.
         *------------------------------------------------------------*/
        if (m_poMAPFile->Open(pszTmpFname, eAccess, TRUE) < 0)
        {
            // File exists, but Open Failed... 
            // we have to produce an error message
            if (!bTestOpenNoError)
                CPLError(CE_Failure, CPLE_FileIO, 
                         "Open() failed for %s", pszTmpFname);
            else
                CPLErrorReset();

            CPLFree(pszTmpFname);
            Close();
            return -1;
        }

        /*-------------------------------------------------------------
         * Set geometry type if the geometry objects are uniform.
         *------------------------------------------------------------*/
        int numPoints=0, numRegions=0, numTexts=0, numLines=0;

        GetFeatureCountByType( numPoints, numLines, numRegions, numTexts);

        numPoints += numTexts;
        if( numPoints > 0 && numLines == 0 && numRegions == 0 )
            m_poDefn->SetGeomType( wkbPoint );
        else if( numPoints == 0 && numLines > 0 && numRegions == 0 )
            m_poDefn->SetGeomType( wkbLineString );
        else {
            /* we leave it unknown indicating a mixture */
        }
    }
    else if (m_poMAPFile->Open(pszTmpFname, eAccess) != 0)
    {
        // Open Failed for write... 
        // an error has already been reported, just return.
        CPLFree(pszTmpFname);
        Close();
        if (bTestOpenNoError)
            CPLErrorReset();

        return -1;
    }

    /*-----------------------------------------------------------------
     * Initializing the attribute index (.IND) support
     *----------------------------------------------------------------*/

    CPLXMLNode *psRoot = CPLCreateXMLNode( NULL, CXT_Element, "OGRMILayerAttrIndex" );
    CPLCreateXMLElementAndValue( psRoot, "MIIDFilename", CPLResetExtension( pszFname, "IND" ) );
    OGRFeatureDefn *poLayerDefn = GetLayerDefn();
    int iField, iIndexIndex, bHasIndex = 0;
    for( iField = 0; iField < poLayerDefn->GetFieldCount(); iField++ )
    {
        iIndexIndex = GetFieldIndexNumber(iField);
        if (iIndexIndex > 0)
        {
            CPLXMLNode *psIndex = CPLCreateXMLNode( psRoot, CXT_Element, "OGRMIAttrIndex" );
            CPLCreateXMLElementAndValue( psIndex, "FieldIndex", CPLSPrintf( "%d", iField ) );
            CPLCreateXMLElementAndValue( psIndex, "FieldName", 
                                     poLayerDefn->GetFieldDefn(iField)->GetNameRef() );
            CPLCreateXMLElementAndValue( psIndex, "IndexIndex", CPLSPrintf( "%d", iIndexIndex ) );
            bHasIndex = 1;
        }     
    }

    if (bHasIndex)
    {
        char *pszRawXML = CPLSerializeXMLTree( psRoot );
        InitializeIndexSupport( pszRawXML );
        CPLFree( pszRawXML );
    }

    CPLDestroyXMLNode( psRoot );

    CPLFree(pszTmpFname);
    pszTmpFname = NULL;

    if( m_poDefn != NULL && m_eAccessMode != TABWrite )
        m_poDefn->GetGeomFieldDefn(0)->SetSpatialRef(GetSpatialRef());

    return 0;
}


/**********************************************************************
 *                   TABFile::ParseTABFileFirstPass()
 *
 * Do a first pass in the TAB header file to establish the table type, etc.
 * and store any useful information into class members.
 *
 * This private method should be used only during the Open() call.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABFile::ParseTABFileFirstPass(GBool bTestOpenNoError)
{
    int         iLine, numLines, numFields = 0;
    char        **papszTok=NULL;
    GBool       bInsideTableDef = FALSE, bFoundTableFields=FALSE;

    if (m_eAccessMode == TABWrite)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
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
            m_nVersion = atoi(papszTok[1]);
            if (m_nVersion == 100)
            {
                /* Version 100 files contain only the fields definition,
                 * so we set default values for the other params.
                 */
                bInsideTableDef = TRUE;
                CPLFree(m_pszCharset);
                m_pszCharset = CPLStrdup("Neutral");
                m_eTableType = TABTableNative;
            }

        }
        else if (EQUAL(papszTok[0], "!edit_version"))
        {
            /* Sometimes, V450 files have version 300 + edit_version 450
             * for us version and edit_version are the same 
             */
            m_nVersion = atoi(papszTok[1]);
        }
        else if (EQUAL(papszTok[0], "!charset"))
        {
            CPLFree(m_pszCharset);
            m_pszCharset = CPLStrdup(papszTok[1]);
        }
        else if (EQUAL(papszTok[0], "Definition") &&
                 EQUAL(papszTok[1], "Table") )
        {
            bInsideTableDef = TRUE;
        }
        else if (bInsideTableDef && !bFoundTableFields &&
                 (EQUAL(papszTok[0], "Type") || EQUAL(papszTok[0],"FORMAT:")) )
        {
            if (EQUAL(papszTok[1], "NATIVE") || EQUAL(papszTok[1], "LINKED"))
                m_eTableType = TABTableNative;
            else if (EQUAL(papszTok[1], "DBF"))
                m_eTableType = TABTableDBF;
            else
            {
                // Type=ACCESS, or other unsupported type... cannot open!
                if (!bTestOpenNoError)
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Unsupported table type '%s' in file %s.  "
                      "This type of .TAB file cannot be read by this library.",
                             papszTok[1], m_pszFname);
                CSLDestroy(papszTok);
                return -1;
            }
        }
        else if (bInsideTableDef && !bFoundTableFields &&
                 (EQUAL(papszTok[0],"Fields") || EQUAL(papszTok[0],"FIELDS:")))
        {
            /*---------------------------------------------------------
             * We found the list of table fields
             * Just remember number of fields... the field types will be
             * parsed inside ParseTABFileFields() later...
             *--------------------------------------------------------*/
            bFoundTableFields = TRUE;
            numFields = atoi(papszTok[1]);

            if (numFields < 1 || numFields>2048 || iLine+numFields >= numLines)
            {
                if (!bTestOpenNoError)
                    CPLError(CE_Failure, CPLE_FileIO,
                         "Invalid number of fields (%s) at line %d in file %s",
                             papszTok[1], iLine+1, m_pszFname);

                CSLDestroy(papszTok);
                return -1;
            }

            bInsideTableDef = FALSE;
        }/* end of fields section*/
        else
        {
            // Simply Ignore unrecognized lines
        }
    }

    CSLDestroy(papszTok);

    if (m_pszCharset == NULL)
        m_pszCharset = CPLStrdup("Neutral");

    if (numFields == 0)
    {
        if (!bTestOpenNoError)
            CPLError(CE_Failure, CPLE_NotSupported,
                     "%s contains no table field definition.  "
                     "This type of .TAB file cannot be read by this library.",
                     m_pszFname);
        return -1;
    }

    return 0;
}

/**********************************************************************
 *                   TABFile::ParseTABFileFields()
 *
 * Extract the field definition from the TAB header file, validate
 * with what we have in the previously opened .DAT or .DBF file, and 
 * finally build the m_poDefn OGRFeatureDefn for this dataset.
 *
 * This private method should be used only during the Open() call and after
 * ParseTABFileFirstPass() has been called.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABFile::ParseTABFileFields()
{
    int         iLine, numLines=0, numTok, nStatus;
    char        **papszTok=NULL;
    OGRFieldDefn *poFieldDefn;

    if (m_eAccessMode == TABWrite)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "ParseTABFile() can be used only with Read access.");
        return -1;
    }

    char *pszFeatureClassName = TABGetBasename(m_pszFname);
    m_poDefn = new OGRFeatureDefn(pszFeatureClassName);
    CPLFree(pszFeatureClassName);
    // Ref count defaults to 0... set it to 1
    m_poDefn->Reference();

    /*-------------------------------------------------------------
     * Scan for fields.
     *------------------------------------------------------------*/
    numLines = CSLCount(m_papszTABFile);
    for(iLine=0; iLine<numLines; iLine++)
    {
        /*-------------------------------------------------------------
         * Tokenize the next .TAB line, and check first keyword
         *------------------------------------------------------------*/
        const char *pszStr = m_papszTABFile[iLine];
        while(*pszStr != '\0' && isspace((unsigned char)*pszStr))
            pszStr++;

        if (EQUALN(pszStr, "Fields", 6))
        {
            /*---------------------------------------------------------
             * We found the list of table fields
             *--------------------------------------------------------*/
            int iField, numFields;
            numFields = atoi(pszStr+7);
            if (numFields < 1 || numFields > 2048 || 
                iLine+numFields >= numLines)
            {
                CPLError(CE_Failure, CPLE_FileIO,
                         "Invalid number of fields (%s) at line %d in file %s",
                         pszStr+7, iLine+1, m_pszFname);
                CSLDestroy(papszTok);
                return -1;
            }

            // Alloc the array to keep track of indexed fields
            m_panIndexNo = (int *)CPLCalloc(numFields, sizeof(int));

            iLine++;
            poFieldDefn = NULL;
            for(iField=0; iField<numFields; iField++, iLine++)
            {
                /*-----------------------------------------------------
                 * For each field definition found in the .TAB:
                 * Pass the info to the DAT file object.  It will validate
                 * the info with what is found in the .DAT header, and will
                 * also use this info later to interpret field values.
                 *
                 * We also create the OGRFieldDefn at the same time to 
                 * initialize the OGRFeatureDefn
                 *----------------------------------------------------*/
                CSLDestroy(papszTok);
                papszTok = CSLTokenizeStringComplex(m_papszTABFile[iLine], 
                                                    " \t(),;",
                                                    TRUE, FALSE);
                numTok = CSLCount(papszTok);
                nStatus = -1;
                CPLAssert(m_poDefn);
                poFieldDefn = NULL;
                if (numTok >= 3 && EQUAL(papszTok[1], "char"))
                {
                    /*-------------------------------------------------
                     * CHAR type
                     *------------------------------------------------*/
                    nStatus = m_poDATFile->ValidateFieldInfoFromTAB(iField, 
                                                               papszTok[0],
                                                               TABFChar,
                                                            atoi(papszTok[2]),
                                                               0);
                    poFieldDefn = new OGRFieldDefn(papszTok[0], OFTString);
                    poFieldDefn->SetWidth(atoi(papszTok[2]));
                }
                else if (numTok >= 2 && EQUAL(papszTok[1], "integer"))
                {
                    /*-------------------------------------------------
                     * INTEGER type
                     *------------------------------------------------*/
                    nStatus = m_poDATFile->ValidateFieldInfoFromTAB(iField, 
                                                               papszTok[0],
                                                               TABFInteger,
                                                               0,
                                                               0);
                    poFieldDefn = new OGRFieldDefn(papszTok[0], OFTInteger);
                    if( numTok > 2 && atoi(papszTok[2]) > 0 )
                        poFieldDefn->SetWidth( atoi(papszTok[2]) );
                }
                else if (numTok >= 2 && EQUAL(papszTok[1], "smallint"))
                {
                    /*-------------------------------------------------
                     * SMALLINT type
                     *------------------------------------------------*/
                    nStatus = m_poDATFile->ValidateFieldInfoFromTAB(iField, 
                                                               papszTok[0],
                                                               TABFSmallInt,
                                                               0,
                                                               0);
                    poFieldDefn = new OGRFieldDefn(papszTok[0], OFTInteger);
                    if( numTok > 2 && atoi(papszTok[2]) > 0 )
                        poFieldDefn->SetWidth( atoi(papszTok[2]) );
                }
                else if (numTok >= 4 && EQUAL(papszTok[1], "decimal"))
                {
                    /*-------------------------------------------------
                     * DECIMAL type
                     *------------------------------------------------*/
                    nStatus = m_poDATFile->ValidateFieldInfoFromTAB(iField, 
                                                               papszTok[0],
                                                               TABFDecimal,
                                                           atoi(papszTok[2]),
                                                           atoi(papszTok[3]));
                    poFieldDefn = new OGRFieldDefn(papszTok[0], OFTReal);
                    poFieldDefn->SetWidth(atoi(papszTok[2]));
                    poFieldDefn->SetPrecision(atoi(papszTok[3]));
                }
                else if (numTok >= 2 && EQUAL(papszTok[1], "float"))
                {
                    /*-------------------------------------------------
                     * FLOAT type
                     *------------------------------------------------*/
                    nStatus = m_poDATFile->ValidateFieldInfoFromTAB(iField, 
                                                               papszTok[0],
                                                               TABFFloat,
                                                               0, 0);
                    poFieldDefn = new OGRFieldDefn(papszTok[0], OFTReal);
                }
                else if (numTok >= 2 && EQUAL(papszTok[1], "date"))
                {
                    /*-------------------------------------------------
                     * DATE type (returned as a string: "DD/MM/YYYY")
                     *------------------------------------------------*/
                    nStatus = m_poDATFile->ValidateFieldInfoFromTAB(iField, 
                                                               papszTok[0],
                                                               TABFDate,
                                                               0,
                                                               0);
                    poFieldDefn = new OGRFieldDefn(papszTok[0], 
#ifdef MITAB_USE_OFTDATETIME
                                                   OFTDate);
#else
                                                   OFTString);
#endif
                    poFieldDefn->SetWidth(10);
                }
                else if (numTok >= 2 && EQUAL(papszTok[1], "time"))
                {
                    /*-------------------------------------------------
                     * TIME type (returned as a string: "HH:MM:SS")
                     *------------------------------------------------*/
                    nStatus = m_poDATFile->ValidateFieldInfoFromTAB(iField, 
                                                               papszTok[0],
                                                               TABFTime,
                                                               0,
                                                               0);
                    poFieldDefn = new OGRFieldDefn(papszTok[0], 
#ifdef MITAB_USE_OFTDATETIME
                                                   OFTTime);
#else
                                                   OFTString);
#endif
                    poFieldDefn->SetWidth(9);
                }
                else if (numTok >= 2 && EQUAL(papszTok[1], "datetime"))
                {
                    /*-------------------------------------------------
                     * DATETIME type (returned as a string: "DD/MM/YYYY HH:MM:SS")
                     *------------------------------------------------*/
                    nStatus = m_poDATFile->ValidateFieldInfoFromTAB(iField, 
                                                               papszTok[0],
                                                               TABFDateTime,
                                                               0,
                                                               0);
                    poFieldDefn = new OGRFieldDefn(papszTok[0], 
#ifdef MITAB_USE_OFTDATETIME
                                                   OFTDateTime);
#else
                                                   OFTString);
#endif
                    poFieldDefn->SetWidth(19);
                }
                else if (numTok >= 2 && EQUAL(papszTok[1], "logical"))
                {
                    /*-------------------------------------------------
                     * LOGICAL type (value "T" or "F")
                     *------------------------------------------------*/
                    nStatus = m_poDATFile->ValidateFieldInfoFromTAB(iField, 
                                                               papszTok[0],
                                                               TABFLogical,
                                                               0,
                                                               0);
                    poFieldDefn = new OGRFieldDefn(papszTok[0], OFTString);
                    poFieldDefn->SetWidth(1);
                }
                else 
                    nStatus = -1; // Unrecognized field type or line corrupt

                if (nStatus != 0)
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                     "Failed to parse field definition at line %d in file %s", 
                             iLine+1, m_pszFname);
                    CSLDestroy(papszTok);
                    delete poFieldDefn;
                    return -1;
                }
                /*-----------------------------------------------------
                 * Keep track of index number if present
                 *----------------------------------------------------*/
                if (numTok >= 4 && EQUAL(papszTok[numTok-2], "index"))
                {
                    m_panIndexNo[iField] = atoi(papszTok[numTok-1]);
                }
                else
                {
                    m_panIndexNo[iField] = 0;
                }

                /*-----------------------------------------------------
                 * Add the FieldDefn to the FeatureDefn and continue with
                 * the next one.
                 *----------------------------------------------------*/
                m_poDefn->AddFieldDefn(poFieldDefn);
                // AddFieldDenf() takes a copy, so we delete the original
                if (poFieldDefn) delete poFieldDefn;
                poFieldDefn = NULL;
            }

            /*---------------------------------------------------------
             * OK, we're done... end the loop now.
             *--------------------------------------------------------*/
            break;
        }/* end of fields section*/
        else
        {
            // Simply Ignore unrecognized lines
        }

    }

    CSLDestroy(papszTok);

    if (m_poDefn->GetFieldCount() == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "%s contains no table field definition.  "
                 "This type of .TAB file cannot be read by this library.",
                 m_pszFname);
        return -1;
    }

    return 0;
}

/**********************************************************************
 *                   TABFile::WriteTABFile()
 *
 * Generate the .TAB file using mainly the attribute fields definition.
 *
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABFile::WriteTABFile()
{
    VSILFILE *fp;

    if (m_poMAPFile == NULL || m_eAccessMode == TABRead)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "WriteTABFile() can be used only with Write access.");
        return -1;
    }
    if (!m_bNeedTABRewrite )
    {
        return 0;
    }

    // First update file version number...
    int nMapObjVersion = m_poMAPFile->GetMinTABFileVersion();
    m_nVersion = MAX(m_nVersion, nMapObjVersion);

    if ( (fp = VSIFOpenL(m_pszFname, "wt")) != NULL)
    {
        VSIFPrintfL(fp, "!table\n");
        VSIFPrintfL(fp, "!version %d\n", m_nVersion);
        VSIFPrintfL(fp, "!charset %s\n", m_pszCharset);
        VSIFPrintfL(fp, "\n");

        if (m_poDefn && m_poDefn->GetFieldCount() > 0)
        {
            int iField;
            OGRFieldDefn *poFieldDefn;
            const char *pszFieldType;

            VSIFPrintfL(fp, "Definition Table\n");
            VSIFPrintfL(fp, "  Type NATIVE Charset \"%s\"\n", m_pszCharset);
            VSIFPrintfL(fp, "  Fields %d\n", m_poDefn->GetFieldCount());

            for(iField=0; iField<m_poDefn->GetFieldCount(); iField++)
            {
                poFieldDefn = m_poDefn->GetFieldDefn(iField);
                switch(GetNativeFieldType(iField))
                {
                  case TABFChar:
                    pszFieldType = CPLSPrintf("Char (%d)", 
                                              poFieldDefn->GetWidth());
                    break;
                  case TABFDecimal:
                    pszFieldType = CPLSPrintf("Decimal (%d,%d)",
                                              poFieldDefn->GetWidth(),
                                              poFieldDefn->GetPrecision());
                    break;
                  case TABFInteger:
                    if( poFieldDefn->GetWidth() == 0 )
                        pszFieldType = "Integer";
                    else
                        pszFieldType = CPLSPrintf("Integer (%d)", 
                                                  poFieldDefn->GetWidth());
                    break;
                  case TABFSmallInt:
                    if( poFieldDefn->GetWidth() == 0 )
                        pszFieldType = "SmallInt";
                    else
                        pszFieldType = CPLSPrintf("SmallInt (%d)", 
                                                  poFieldDefn->GetWidth());
                    break;
                  case TABFFloat:
                    pszFieldType = "Float";
                    break;
                  case TABFLogical:
                    pszFieldType = "Logical";
                    break;
                  case TABFDate:
                    pszFieldType = "Date";
                    break;
                  case TABFTime:
                    pszFieldType = "Time";
                    break;
                  case TABFDateTime:
                    pszFieldType = "DateTime";
                    break;
                  default:
                    // Unsupported field type!!!  This should never happen.
                    CPLError(CE_Failure, CPLE_AssertionFailed,
                             "WriteTABFile(): Unsupported field type");
                    VSIFCloseL(fp);
                    return -1;
                }

                if (GetFieldIndexNumber(iField) == 0)
                {
                    VSIFPrintfL(fp, "    %s %s ;\n", poFieldDefn->GetNameRef(), 
                            pszFieldType );
                }
                else
                {
                    VSIFPrintfL(fp, "    %s %s Index %d ;\n", 
                            poFieldDefn->GetNameRef(), pszFieldType,
                            GetFieldIndexNumber(iField) );
                }
                
            }
        }
        else
        {
            VSIFPrintfL(fp, "Definition Table\n");
            VSIFPrintfL(fp, "  Type NATIVE Charset \"%s\"\n", m_pszCharset);
            VSIFPrintfL(fp, "  Fields 1\n");
            VSIFPrintfL(fp, "    FID Integer ;\n" );
        }

        VSIFCloseL(fp);

        m_bNeedTABRewrite = FALSE;
    }
    else
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed to create file `%s'", m_pszFname);
        return -1;
    }

    return 0;
}

/**********************************************************************
 *                   TABFile::Close()
 *
 * Close current file, and release all memory used.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABFile::Close()
{
    CPLErrorReset();

    // Commit the latest changes to the file...
    
    // In Write access, it's time to write the .TAB file.
    if (m_eAccessMode != TABRead)
    {
        WriteTABFile();
    }

    if (m_poMAPFile)
    {
        m_poMAPFile->Close();
        delete m_poMAPFile;
        m_poMAPFile = NULL;
    }

    if (m_poDATFile)
    {
        m_poDATFile->Close();
        delete m_poDATFile;
        m_poDATFile = NULL;
    }

    if (m_poINDFile)
    {
        m_poINDFile->Close();
        delete m_poINDFile;
        m_poINDFile = NULL;
    }

    if (m_poCurFeature)
    {
        delete m_poCurFeature;
        m_poCurFeature = NULL;
    }

    if (m_poDefn )
        m_poDefn->Release();
    m_poDefn = NULL;
    
    if (m_poSpatialRef)
        m_poSpatialRef->Release();
    m_poSpatialRef = NULL;
    
    CSLDestroy(m_papszTABFile);
    m_papszTABFile = NULL;

    CPLFree(m_pszFname);
    m_pszFname = NULL;

    CPLFree(m_pszCharset);
    m_pszCharset = NULL;

    CPLFree(m_panIndexNo);
    m_panIndexNo = NULL;

    CPLFree(m_panMatchingFIDs);
    m_panMatchingFIDs = NULL;

    return 0;
}

/**********************************************************************
 *                   TABFile::SetQuickSpatialIndexMode()
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
int TABFile::SetQuickSpatialIndexMode(GBool bQuickSpatialIndexMode/*=TRUE*/)
{
    if (m_eAccessMode != TABWrite || m_poMAPFile == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "SetQuickSpatialIndexMode() failed: file not opened for write access.");
        return -1;
    }


    return m_poMAPFile->SetQuickSpatialIndexMode(bQuickSpatialIndexMode);
}



/**********************************************************************
 *                   TABFile::GetNextFeatureId()
 *
 * Returns feature id that follows nPrevId, or -1 if it is the
 * last feature id.  Pass nPrevId=-1 to fetch the first valid feature id.
 **********************************************************************/
GIntBig TABFile::GetNextFeatureId(GIntBig nPrevId)
{
    if( m_bLastOpWasWrite )
        ResetReading();
    m_bLastOpWasRead = TRUE;

    /*-----------------------------------------------------------------
     * Are we using spatial rather than .ID based traversal?
     *----------------------------------------------------------------*/
    if( bUseSpatialTraversal )
        return m_poMAPFile->GetNextFeatureId( nPrevId );

    /*-----------------------------------------------------------------
     * Should we use an attribute index traversal?
     *----------------------------------------------------------------*/
    if( m_poAttrQuery != NULL)
    {
        if( m_panMatchingFIDs == NULL )
        {
            m_iMatchingFID = 0;
            m_panMatchingFIDs = m_poAttrQuery->EvaluateAgainstIndices( this,
                                                                 NULL );
        }
        if( m_panMatchingFIDs != NULL )
        {
            if( m_panMatchingFIDs[m_iMatchingFID] == OGRNullFID )
                return OGRNullFID;

            return m_panMatchingFIDs[m_iMatchingFID++] + 1;
        }
    }

    /*-----------------------------------------------------------------
     * Establish what the next logical feature ID should be
     *----------------------------------------------------------------*/
    int nFeatureId = -1;

    if (nPrevId <= 0 && m_nLastFeatureId > 0)
        nFeatureId = 1;       // Feature Ids start at 1
    else if (nPrevId > 0 && nPrevId < m_nLastFeatureId)
        nFeatureId = nPrevId + 1;
    else
    {
        // This was the last feature
        return OGRNullFID;
    }

    /*-----------------------------------------------------------------
     * Skip any feature with NONE geometry and a deleted attribute record
     *----------------------------------------------------------------*/
    while(nFeatureId <= m_nLastFeatureId)
    {
        if ( m_poMAPFile->MoveToObjId(nFeatureId) != 0 ||
             m_poDATFile->GetRecordBlock(nFeatureId) == NULL )
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "GetNextFeatureId() failed: unable to set read pointer "
                     "to feature id %d",  nFeatureId);
            return -1;
        }

// __TODO__ Add a test here to check if object is deleted, 
// i.e. 0x40 set on object_id in object block
        if (m_poMAPFile->GetCurObjType() != TAB_GEOM_NONE ||
            m_poDATFile->IsCurrentRecordDeleted() == FALSE)
        {
            // This feature contains at least a geometry or some attributes...
            // return its id.
            return nFeatureId;
        }

        nFeatureId++;
    }

    // If we reached this point, then we kept skipping deleted features
    // and stopped when EOF was reached.
    return -1;
}

/**********************************************************************
 *                   TABFile::GetNextFeatureId_Spatial()
 *
 * Returns feature id that follows nPrevId, or -1 if it is the
 * last feature id, but by traversing the spatial tree instead of the
 * direct object index.  Generally speaking the feature id's will be
 * returned in an unordered fashion.  
 **********************************************************************/
int TABFile::GetNextFeatureId_Spatial(int nPrevId)
{
    if (m_eAccessMode != TABRead)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
            "GetNextFeatureId_Spatial() can be used only with Read access.");
        return -1;
    }

    if( m_poMAPFile == NULL )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
            "GetNextFeatureId_Spatial() requires availability of .MAP file." );
        return -1;
    }

    return m_poMAPFile->GetNextFeatureId( nPrevId );
}

/**********************************************************************
 *                   TABFile::GetFeatureRef()
 *
 * Fill and return a TABFeature object for the specified feature id.
 *
 * The retruned pointer is a reference to an object owned and maintained
 * by this TABFile object.  It should not be altered or freed by the 
 * caller and its contents is guaranteed to be valid only until the next
 * call to GetFeatureRef() or Close().
 *
 * Returns NULL if the specified feature id does not exist of if an
 * error happened.  In any case, CPLError() will have been called to
 * report the reason of the failure.
 *
 * If an unsupported object type is encountered (likely from a newer version
 * of MapInfo) then a valid feature will be returned with a NONE geometry,
 * and a warning will be produced with code TAB_WarningFeatureTypeNotSupported
 * CPLGetLastErrorNo() should be used to detect that case.
 **********************************************************************/
TABFeature *TABFile::GetFeatureRef(GIntBig nFeatureId)
{
    CPLErrorReset();

    /*-----------------------------------------------------------------
     * Make sure file is opened and Validate feature id by positioning
     * the read pointers for the .MAP and .DAT files to this feature id.
     *----------------------------------------------------------------*/
    if (m_poMAPFile == NULL)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "GetFeatureRef() failed: file is not opened!");
        return NULL;
    }

    if( m_bLastOpWasWrite )
        ResetReading();
    m_bLastOpWasRead = TRUE;

    if (nFeatureId <= 0 || nFeatureId > m_nLastFeatureId ||
        m_poMAPFile->MoveToObjId(nFeatureId) != 0 ||
        m_poDATFile->GetRecordBlock(nFeatureId) == NULL )
    {
        //     CPLError(CE_Failure, CPLE_IllegalArg,
        //    "GetFeatureRef() failed: invalid feature id %d", 
        //    nFeatureId);
        return NULL;
    }
    
    if( m_poDATFile->IsCurrentRecordDeleted() )
    {
        if( m_poMAPFile->GetCurObjType() != TAB_GEOM_NONE )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Valid .MAP record " CPL_FRMT_GIB " found, but .DAT is marked as deleted. File likely corrupt",
                    nFeatureId);
        }
        return NULL;
    }
    
    /*-----------------------------------------------------------------
     * Flush current feature object
     * __TODO__ try to reuse if it is already of the right type
     *----------------------------------------------------------------*/
    if (m_poCurFeature)
    {
        delete m_poCurFeature;
        m_poCurFeature = NULL;
    }

    /*-----------------------------------------------------------------
     * Create new feature object of the right type
     * Unsupported object types are returned as raw TABFeature (i.e. NONE
     * geometry)
     *----------------------------------------------------------------*/
    m_poCurFeature = TABFeature::CreateFromMapInfoType(m_poMAPFile->GetCurObjType(), 
                                                       m_poDefn);

    /*-----------------------------------------------------------------
     * Read fields from the .DAT file
     * GetRecordBlock() has already been called above...
     *----------------------------------------------------------------*/
    if (m_poCurFeature->ReadRecordFromDATFile(m_poDATFile) != 0)
    {
        delete m_poCurFeature;
        m_poCurFeature = NULL;
        return NULL;
    }

    /*-----------------------------------------------------------------
     * Read geometry from the .MAP file
     * MoveToObjId() has already been called above...
     *----------------------------------------------------------------*/
    TABMAPObjHdr *poObjHdr = 
        TABMAPObjHdr::NewObj(m_poMAPFile->GetCurObjType(), 
                             m_poMAPFile->GetCurObjId());
    // Note that poObjHdr==NULL is a valid case if geometry type is NONE

    if ((poObjHdr && poObjHdr->ReadObj(m_poMAPFile->GetCurObjBlock()) != 0) ||
        m_poCurFeature->ReadGeometryFromMAPFile(m_poMAPFile, poObjHdr) != 0)
    {
        delete m_poCurFeature;
        m_poCurFeature = NULL;
        if (poObjHdr) 
            delete poObjHdr;
        return NULL;
    }
    if (poObjHdr)       // May be NULL if feature geometry type is NONE
        delete poObjHdr; 

    m_nCurFeatureId = nFeatureId;
    m_poCurFeature->SetFID(m_nCurFeatureId);

    m_poCurFeature->SetRecordDeleted(m_poDATFile->IsCurrentRecordDeleted());

    return m_poCurFeature;
}

/**********************************************************************
 *                   TABFile::DeleteFeature()
 *
 * Standard OGR DeleteFeature implementation.
 **********************************************************************/
OGRErr TABFile::DeleteFeature(GIntBig nFeatureId)
{
    CPLErrorReset();

    if (m_eAccessMode == TABRead)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "DeleteFeature() cannot be used in read-only access.");
        return OGRERR_FAILURE;
    }

    /*-----------------------------------------------------------------
     * Make sure file is opened and establish new feature id.
     *----------------------------------------------------------------*/
    if (m_poMAPFile == NULL)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "DeleteFeature() failed: file is not opened!");
        return OGRERR_FAILURE;
    }

    if( m_bLastOpWasWrite )
        ResetReading();

    if (nFeatureId <= 0 || nFeatureId > m_nLastFeatureId ||
        m_poMAPFile->MoveToObjId(nFeatureId) != 0 ||
        m_poDATFile->GetRecordBlock(nFeatureId) == NULL )
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "DeleteFeature() failed: invalid feature id " CPL_FRMT_GIB, 
                 nFeatureId);
        return OGRERR_FAILURE;
    }
    
    if( m_poDATFile->IsCurrentRecordDeleted() )
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "DeleteFeature() failed: record is already deleted!");
        return OGRERR_FAILURE;
    }

    if (m_poCurFeature)
    {
        delete m_poCurFeature;
        m_poCurFeature = NULL;
    }
    
    if( m_poMAPFile->MarkAsDeleted() != 0 ||
        m_poDATFile->MarkAsDeleted() != 0 )
    {
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/**********************************************************************
 *                   TABFile::WriteFeature()
 *
 * Write a feature to this dataset.  
 *
 * Returns 0 on success, or -1 if an error happened in which case,
 * CPLError() will have been called to
 * report the reason of the failure.
 **********************************************************************/
int TABFile::WriteFeature(TABFeature *poFeature)
{

    m_bLastOpWasWrite = TRUE;

    /*-----------------------------------------------------------------
     * Make sure file is opened and establish new feature id.
     *----------------------------------------------------------------*/
    if (m_poMAPFile == NULL)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "WriteFeature() failed: file is not opened!");
        return -1;
    }

    int nFeatureId;
    if ( poFeature->GetFID() >= 0 )
    {
        nFeatureId = poFeature->GetFID();
    }
    else if (m_nLastFeatureId < 1)
    {
        /*-------------------------------------------------------------
         * Special hack to write out at least one field if none are in 
         * OGRFeatureDefn.
         *------------------------------------------------------------*/
        if( m_poDATFile->GetNumFields() == 0 )
        {
            CPLError(CE_Warning, CPLE_IllegalArg,
                     "MapInfo tables must contain at least 1 column, adding dummy FID column.");
            CPLErrorReset();
            m_poDATFile->AddField("FID", TABFInteger, 10, 0 );
        }

        nFeatureId = 1;
    }
    else
    {
        nFeatureId = m_nLastFeatureId + 1;
    }

    poFeature->SetFID(nFeatureId);


    /*-----------------------------------------------------------------
     * Write fields to the .DAT file and update .IND if necessary
     *----------------------------------------------------------------*/
    if (m_poDATFile->GetRecordBlock(nFeatureId) == NULL ||
        poFeature->WriteRecordToDATFile(m_poDATFile, m_poINDFile,
                                        m_panIndexNo) != 0 )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed writing attributes for feature id %d in %s",
                 nFeatureId, m_pszFname);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Write geometry to the .MAP file
     * The call to PrepareNewObj() takes care of the .ID file.
     *----------------------------------------------------------------*/
    TABMAPObjHdr *poObjHdr = 
        TABMAPObjHdr::NewObj(poFeature->ValidateMapInfoType(m_poMAPFile),
                             nFeatureId);

    if ( poObjHdr == NULL || m_poMAPFile == NULL )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed writing geometry for feature id %d in %s",
                 nFeatureId, m_pszFname);
        if (poObjHdr)
            delete poObjHdr;
        return -1;
    }

    /*-----------------------------------------------------------------
     * ValidateMapInfoType() may have returned TAB_GEOM_NONE if feature
     * contained an invalid geometry for its class. Need to catch that
     * case and return the error.
     *----------------------------------------------------------------*/
    if (poObjHdr->m_nType == TAB_GEOM_NONE &&
        poFeature->GetFeatureClass() != TABFCNoGeomFeature )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Invalid geometry for feature id %d in %s",
                 nFeatureId, m_pszFname);
        return -1;
    }

    /*-----------------------------------------------------------------
     * The ValidateMapInfoType() call above has forced calculation of the
     * feature's IntMBR. Store that value in the ObjHdr for use by
     * PrepareNewObj() to search the best node to insert the feature.
     *----------------------------------------------------------------*/
    if ( poObjHdr && poObjHdr->m_nType != TAB_GEOM_NONE)
    {
        poFeature->GetIntMBR(poObjHdr->m_nMinX, poObjHdr->m_nMinY,
                             poObjHdr->m_nMaxX, poObjHdr->m_nMaxY);
    }

/*
    if( m_nCurFeatureId < m_nLastFeatureId )
    {
        delete GetFeatureRef(m_nLastFeatureId);
        m_poCurFeature = NULL;
    }*/

    if ( m_poMAPFile->PrepareNewObj(poObjHdr) != 0 ||
         poFeature->WriteGeometryToMAPFile(m_poMAPFile, poObjHdr) != 0 ||
         m_poMAPFile->CommitNewObj(poObjHdr) != 0 )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed writing geometry for feature id %d in %s",
                 nFeatureId, m_pszFname);
        if (poObjHdr)
            delete poObjHdr;
        return -1;
    }
    
    m_nLastFeatureId = MAX(m_nLastFeatureId, nFeatureId);
    m_nCurFeatureId = nFeatureId;

    delete poObjHdr;

    return 0;
}


/**********************************************************************
 *                   TABFile::CreateFeature()
 *
 * Write a new feature to this dataset. The passed in feature is updated 
 * with the new feature id.
 *
 * Returns OGRERR_NONE on success, or an appropriate OGRERR_ code if an
 * error happened in which case, CPLError() will have been called to
 * report the reason of the failure.
 **********************************************************************/
OGRErr TABFile::CreateFeature(TABFeature *poFeature)
{
    CPLErrorReset();

    if (m_eAccessMode == TABRead)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "CreateFeature() cannot be used in read-only access.");
        return OGRERR_FAILURE;
    }

    long nFeatureId = poFeature->GetFID();
    if (nFeatureId != OGRNullFID )
    {
        if (nFeatureId <= 0 || nFeatureId > m_nLastFeatureId )
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "CreateFeature() failed: invalid feature id %ld", 
                      nFeatureId);
            return OGRERR_FAILURE;
        }

        if( m_poDATFile->GetRecordBlock(nFeatureId) == NULL ||
            !m_poDATFile->IsCurrentRecordDeleted() )
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                    "CreateFeature() failed: cannot re-write already existing feature %ld", 
                    nFeatureId);
            return OGRERR_FAILURE;
        }
    }

    if (WriteFeature(poFeature) < 0)
        return OGRERR_FAILURE;

    return OGRERR_NONE;
}

/**********************************************************************
 *                   TABFile::ISetFeature()
 *
 * Implementation of OGRLayer's SetFeature()
 **********************************************************************/
OGRErr TABFile::ISetFeature( OGRFeature *poFeature )

{
    CPLErrorReset();

    if (m_eAccessMode == TABRead)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetFeature() cannot be used in read-only access.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Make sure file is opened.
     *----------------------------------------------------------------*/
    if (m_poMAPFile == NULL)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "SetFeature() failed: file is not opened!");
        return OGRERR_FAILURE;
    }
    
    long nFeatureId = poFeature->GetFID();
    if (nFeatureId == OGRNullFID )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetFeature() must be used on a feature with a FID.");
        return OGRERR_FAILURE;
    }
    if (nFeatureId <= 0 || nFeatureId > m_nLastFeatureId )
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                    "SetFeature() failed: invalid feature id %ld", 
                    nFeatureId);
        return OGRERR_FAILURE;
    }

    OGRGeometry* poGeom = poFeature->GetGeometryRef();
    if( poGeom != NULL &&
        ((wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint) ||
         (wkbFlatten(poGeom->getGeometryType()) == wkbGeometryCollection)) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetFeature() failed: setting MultiPoint or GeometryCollection not supported");
        return OGRERR_FAILURE;
    }

    TABFeature* poTABFeature = CreateTABFeature(poFeature);
    if( poTABFeature == NULL )
        return OGRERR_FAILURE;

    if( m_bLastOpWasWrite )
        ResetReading();

    if (m_poDATFile->GetRecordBlock(nFeatureId) == NULL )
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "SetFeature() failed: invalid feature id %ld", 
                 nFeatureId);
        delete poTABFeature;
        return OGRERR_FAILURE;
    }

    /* If the object is not already deleted, delete it */
    if( !(m_poDATFile->IsCurrentRecordDeleted()) )
    {
        OGRFeature* poOldFeature = GetFeature(nFeatureId);
        if( poOldFeature != NULL )
        {
            /* Optimization: if old and new features are the same, do nothing */
            if( poOldFeature->Equal(poFeature) )
            {
                CPLDebug("MITAB", "Un-modified object %ld", nFeatureId);
                delete poTABFeature;
                delete poOldFeature;
                return OGRERR_NONE;
            }

            /* Optimization: if old and new geometries are the same, just */
            /* rewrite the attributes */
            OGRGeometry* poOldGeom = poOldFeature->GetGeometryRef();
            OGRGeometry* poNewGeom = poFeature->GetGeometryRef();
            if( (poOldGeom == NULL && poNewGeom == NULL ) ||
                (poOldGeom != NULL && poNewGeom != NULL && poOldGeom->Equals(poNewGeom)) )
            {
                const char* pszOldStyle = poOldFeature->GetStyleString();
                const char* pszNewStyle = poFeature->GetStyleString();
                if( (pszOldStyle == NULL && pszNewStyle == NULL) ||
                    (pszOldStyle != NULL && pszNewStyle != NULL && EQUAL(pszOldStyle, pszNewStyle)) )
                {
                    CPLDebug("MITAB", "Rewrite only attributes for object %ld", nFeatureId);
                    if (poTABFeature->WriteRecordToDATFile(m_poDATFile, m_poINDFile,
                                                        m_panIndexNo) != 0 )
                    {
                        CPLError(CE_Failure, CPLE_FileIO,
                                "Failed writing attributes for feature id %ld in %s",
                                nFeatureId, m_pszFname);
                        delete poTABFeature;
                        delete poOldFeature;
                        return OGRERR_FAILURE;
                    }

                    delete poTABFeature;
                    delete poOldFeature;
                    return OGRERR_NONE;
                }
            }

            delete poOldFeature;
        }

        if (DeleteFeature(nFeatureId) != OGRERR_NONE)
        {
            delete poTABFeature;
            return OGRERR_FAILURE;
        }
    }

    int nStatus = WriteFeature(poTABFeature);
    
    delete poTABFeature;
    
    if (nStatus < 0)
        return OGRERR_FAILURE;

    return OGRERR_NONE;
}


/**********************************************************************
 *                   TABFile::GetLayerDefn()
 *
 * Returns a reference to the OGRFeatureDefn that will be used to create
 * features in this dataset.
 *
 * Returns a reference to an object that is maintained by this TABFile
 * object (and thus should not be modified or freed by the caller) or
 * NULL if the OGRFeatureDefn has not been initialized yet (i.e. no file
 * opened yet)
 **********************************************************************/
OGRFeatureDefn *TABFile::GetLayerDefn()
{
    return m_poDefn;
}

/**********************************************************************
 *                   TABFile::SetFeatureDefn()
 *
 * Pass a reference to the OGRFeatureDefn that will be used to create
 * features in this dataset.  This function should be called after
 * creating a new dataset, but before writing the first feature.
 * All features that will be written to this dataset must share this same
 * OGRFeatureDefn.
 *
 * A reference to the OGRFeatureDefn will be kept and will be used to
 * build the .DAT file, etc.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABFile::SetFeatureDefn(OGRFeatureDefn *poFeatureDefn,
                         TABFieldType *paeMapInfoNativeFieldTypes /* =NULL */)
{
    int           iField, numFields;
    OGRFieldDefn *poFieldDefn;
    TABFieldType eMapInfoType = TABFUnknown;
    int nStatus = 0;

    if (m_eAccessMode != TABWrite)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetFeatureDefn() can be used only with Write access.");
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

    /*-----------------------------------------------------------------
     * Pass field information to the .DAT file, after making sure that
     * it has been created and that it does not contain any field
     * definition yet.
     *----------------------------------------------------------------*/
    if (m_poDATFile== NULL || m_poDATFile->GetNumFields() > 0 )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "SetFeatureDefn() can be called only once in a newly "
                 "created dataset.");
        return -1;
    }

    numFields = poFeatureDefn->GetFieldCount();
    for(iField=0; nStatus==0 && iField < numFields; iField++)
    {
        poFieldDefn = m_poDefn->GetFieldDefn(iField);

        /*-------------------------------------------------------------
         * Make sure field name is valid... check for special chars, etc.
         *------------------------------------------------------------*/
        char *pszCleanName = TABCleanFieldName(poFieldDefn->GetNameRef());
        if (!EQUAL(pszCleanName, poFieldDefn->GetNameRef()))
            poFieldDefn->SetName(pszCleanName);
        CPLFree(pszCleanName);
        pszCleanName = NULL;

        if (paeMapInfoNativeFieldTypes)
        {
            eMapInfoType = paeMapInfoNativeFieldTypes[iField];
        }
        else
        {
            /*---------------------------------------------------------
             * Map OGRFieldTypes to MapInfo native types
             *--------------------------------------------------------*/
            switch(poFieldDefn->GetType())
            {
              case OFTInteger:
                eMapInfoType = TABFInteger;
                break;
              case OFTReal:
                if( poFieldDefn->GetWidth()>0 || poFieldDefn->GetPrecision()>0 )
                    eMapInfoType = TABFDecimal;
                else
                    eMapInfoType = TABFFloat;
                break;
              case OFTDateTime:
                eMapInfoType = TABFDateTime;
                break;
              case OFTDate:
                eMapInfoType = TABFDate;
                break;
              case OFTTime:
                eMapInfoType = TABFTime;
                break;
              case OFTString:
              default:
                eMapInfoType = TABFChar;
            }
        }

        nStatus = m_poDATFile->AddField(poFieldDefn->GetNameRef(),
                                            eMapInfoType,
                                            poFieldDefn->GetWidth(),
                                            poFieldDefn->GetPrecision());
    }

    /*-----------------------------------------------------------------
     * Alloc the array to keep track of indexed fields (default=NOT indexed)
     *----------------------------------------------------------------*/
    m_panIndexNo = (int *)CPLCalloc(numFields, sizeof(int));

    return nStatus;
}

/**********************************************************************
 *                   TABFile::AddFieldNative()
 *
 * Create a new field using a native mapinfo data type... this is an 
 * alternative to defining fields through the OGR interface.
 * This function should be called after creating a new dataset.
 *
 * This function will build/update the OGRFeatureDefn that will have to be
 * used when writing features to this dataset.
 *
 * A reference to the OGRFeatureDefn can be obtained using GetLayerDefn().
 *
 * Note: The bUnique flag has no effect on TABFiles.  See the TABView class.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABFile::AddFieldNative(const char *pszName, TABFieldType eMapInfoType,
                            int nWidth /*=0*/, int nPrecision /*=0*/,
                            GBool bIndexed /*=FALSE*/, GBool /*bUnique=FALSE*/, int bApproxOK)
{
    OGRFieldDefn *poFieldDefn;
    int nStatus = 0;
    char *pszCleanName = NULL;
    char szNewFieldName[31+1]; /* 31 is the max characters for a field name*/
    int nRenameNum = 1;

    if (m_eAccessMode == TABRead || m_poDATFile == NULL)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "AddFieldNative() cannot be used only with Read access.");
        return -1;
    }

    m_bNeedTABRewrite = TRUE;

    /*-----------------------------------------------------------------
     * Validate field width... must be <= 254
     *----------------------------------------------------------------*/
    if (nWidth > 254)
    {
        CPLError(CE_Warning, CPLE_IllegalArg,
                 "Invalid size (%d) for field '%s'.  "
                 "Size must be 254 or less.", nWidth, pszName);
        nWidth=254;
    }

    /*-----------------------------------------------------------------
     * Map fields with width=0 (variable length in OGR) to a valid default
     *----------------------------------------------------------------*/
    if (eMapInfoType == TABFDecimal && nWidth == 0)
        nWidth=20;
    else if (nWidth == 0)
        nWidth=254; /* char fields */

    /*-----------------------------------------------------------------
     * Make sure field name is valid... check for special chars, etc.
     * (pszCleanName will have to be freed.)
     *----------------------------------------------------------------*/
    pszCleanName = TABCleanFieldName(pszName);

    if( !bApproxOK &&
        ( m_poDefn->GetFieldIndex(pszCleanName) >= 0 ||
          !EQUAL(pszName, pszCleanName) ) )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Failed to add field named '%s'",
                  pszName );
    }

    strncpy(szNewFieldName, pszCleanName, 31);
    szNewFieldName[31] = '\0';

    while (m_poDefn->GetFieldIndex(szNewFieldName) >= 0 && nRenameNum < 10) 
      sprintf( szNewFieldName, "%.29s_%.1d", pszCleanName, nRenameNum++ );

    while (m_poDefn->GetFieldIndex(szNewFieldName) >= 0 && nRenameNum < 100) 
      sprintf( szNewFieldName, "%.29s%.2d", pszCleanName, nRenameNum++ );

    if (m_poDefn->GetFieldIndex(szNewFieldName) >= 0)
    {
      CPLError( CE_Failure, CPLE_NotSupported, 
                "Too many field names like '%s' when truncated to 31 letters " 
                "for MapInfo format.", pszCleanName );
    }

    if( !EQUAL(pszCleanName,szNewFieldName) ) 
    {
      CPLError( CE_Warning, CPLE_NotSupported,
                "Normalized/laundered field name: '%s' to '%s'",
                pszCleanName,
                szNewFieldName );
    }

    /*-----------------------------------------------------------------
     * Map MapInfo native types to OGR types
     *----------------------------------------------------------------*/
    poFieldDefn = NULL;

    switch(eMapInfoType)
    {
      case TABFChar:
        /*-------------------------------------------------
         * CHAR type
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, OFTString);
        poFieldDefn->SetWidth(nWidth);
        break;
      case TABFInteger:
        /*-------------------------------------------------
         * INTEGER type
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, OFTInteger);
        if (nWidth <= 10)
            poFieldDefn->SetWidth(nWidth);
        break;
      case TABFSmallInt:
        /*-------------------------------------------------
         * SMALLINT type
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, OFTInteger);
        if (nWidth <= 5)
            poFieldDefn->SetWidth(nWidth);
        break;
      case TABFDecimal:
        /*-------------------------------------------------
         * DECIMAL type
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, OFTReal);
        poFieldDefn->SetWidth(nWidth);
        poFieldDefn->SetPrecision(nPrecision);
        break;
      case TABFFloat:
        /*-------------------------------------------------
         * FLOAT type
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, OFTReal);
        break;
      case TABFDate:
        /*-------------------------------------------------
         * DATE type (V450, returned as a string: "DD/MM/YYYY")
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, 
#ifdef MITAB_USE_OFTDATETIME
                                                   OFTDate);
#else
                                                   OFTString);
#endif
        poFieldDefn->SetWidth(10);
        m_nVersion = MAX(m_nVersion, 450);
        break;
      case TABFTime:
        /*-------------------------------------------------
         * TIME type (V900, returned as a string: "HH:MM:SS")
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, 
#ifdef MITAB_USE_OFTDATETIME
                                                   OFTTime);
#else
                                                   OFTString);
#endif
        poFieldDefn->SetWidth(8);
        m_nVersion = MAX(m_nVersion, 900);
        break;
      case TABFDateTime:
        /*-------------------------------------------------
         * DATETIME type (V900, returned as a string: "DD/MM/YYYY HH:MM:SS")
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, 
#ifdef MITAB_USE_OFTDATETIME
                                                   OFTDateTime);
#else
                                                   OFTString);
#endif
        poFieldDefn->SetWidth(19);
        m_nVersion = MAX(m_nVersion, 900);
        break;
      case TABFLogical:
        /*-------------------------------------------------
         * LOGICAL type (value "T" or "F")
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(szNewFieldName, OFTString);
        poFieldDefn->SetWidth(1);
        break;
      default:
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported type for field %s", szNewFieldName);
        CPLFree(pszCleanName);
        return -1;
    }

    /*-----------------------------------------------------
     * Add the FieldDefn to the FeatureDefn 
     *----------------------------------------------------*/
    m_poDefn->AddFieldDefn(poFieldDefn);
    delete poFieldDefn;

    /*-----------------------------------------------------
     * ... and pass field info to the .DAT file.
     *----------------------------------------------------*/
    nStatus = m_poDATFile->AddField(szNewFieldName, eMapInfoType, 
                                    nWidth, nPrecision);

    /*-----------------------------------------------------------------
     * Extend the array to keep track of indexed fields (default=NOT indexed)
     *----------------------------------------------------------------*/
    m_panIndexNo = (int *)CPLRealloc(m_panIndexNo,
                                     m_poDefn->GetFieldCount()*sizeof(int));
    m_panIndexNo[m_poDefn->GetFieldCount()-1] = 0;

     /*-----------------------------------------------------------------
     * Index the field if requested
     *----------------------------------------------------------------*/
    if (nStatus == 0 && bIndexed)
        nStatus = SetFieldIndexed(m_poDefn->GetFieldCount()-1);

    if (nStatus == 0 && m_eAccessMode == TABReadWrite)
        nStatus = WriteTABFile();

    CPLFree(pszCleanName);
    return nStatus;
}


/**********************************************************************
 *                   TABFile::GetNativeFieldType()
 *
 * Returns the native MapInfo field type for the specified field.
 *
 * Returns TABFUnknown if file is not opened, or if specified field index is
 * invalid.
 *
 * Note that field ids are positive and start at 0.
 **********************************************************************/
TABFieldType TABFile::GetNativeFieldType(int nFieldId)
{
    if (m_poDATFile)
    {
        return m_poDATFile->GetFieldType(nFieldId);
    }
    return TABFUnknown;
}



/**********************************************************************
 *                   TABFile::GetFieldIndexNumber()
 *
 * Returns the field's index number that was specified in the .TAB header
 * or 0 if the specified field is not indexed.
 *
 * Note that field ids are positive and start at 0
 * and valid index ids are positive and start at 1.
 **********************************************************************/
int  TABFile::GetFieldIndexNumber(int nFieldId)
{
    if (m_panIndexNo == NULL || nFieldId < 0 || 
        m_poDATFile== NULL || nFieldId >= m_poDATFile->GetNumFields())
        return 0;  // no index

    return m_panIndexNo[nFieldId];
}

/************************************************************************
 *                       TABFile::SetFieldIndexed()
 *
 * Request that a field be indexed.  This will create the .IND file if
 * necessary, etc.
 *
 * Note that field ids are positive and start at 0.
 *
 * Returns 0 on success, -1 on error.
 ************************************************************************/
int TABFile::SetFieldIndexed( int nFieldId )
{
    /*-----------------------------------------------------------------
     * Make sure things are OK
     *----------------------------------------------------------------*/
    if (m_pszFname == NULL || m_eAccessMode != TABWrite || m_poDefn == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "SetFieldIndexed() must be called after opening a new "
                 "dataset, but before writing the first feature to it.");
        return -1;
    }

    if (m_panIndexNo == NULL || nFieldId < 0 || 
        m_poDATFile== NULL || nFieldId >= m_poDATFile->GetNumFields())
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Invalid field number in SetFieldIndexed().");
        return -1;
    }

    /*-----------------------------------------------------------------
     * If field is already indexed then just return
     *----------------------------------------------------------------*/
    if (m_panIndexNo[nFieldId] != 0)
        return 0;  // Nothing to do


    /*-----------------------------------------------------------------
     * Create .IND file if it's not done yet.
     *
     * Note: We can pass the .TAB's filename directly and the
     * TABINDFile class will automagically adjust the extension.
     *----------------------------------------------------------------*/
    if (m_poINDFile == NULL)
    {
        m_poINDFile = new TABINDFile;
   
        if ( m_poINDFile->Open(m_pszFname, "w", TRUE) != 0)
        {
            // File could not be opened... 
            delete m_poINDFile;
            m_poINDFile = NULL;
            return -1;
        }
    }

    /*-----------------------------------------------------------------
     * Init new index.
     *----------------------------------------------------------------*/
    int nNewIndexNo;
    OGRFieldDefn *poFieldDefn = m_poDefn->GetFieldDefn(nFieldId);

    if (poFieldDefn == NULL ||
        (nNewIndexNo = m_poINDFile->CreateIndex(GetNativeFieldType(nFieldId),
                                                poFieldDefn->GetWidth()) ) < 1)
    {
        // Failed... an error has already been reported.
        return -1;
    }

    m_panIndexNo[nFieldId] = nNewIndexNo;

    return 0;
}

/************************************************************************
 *                       TABFile::IsFieldIndexed()
 *
 * Returns TRUE if field is indexed, or FALSE otherwise.
 ************************************************************************/
GBool TABFile::IsFieldIndexed( int nFieldId )
{
    return (GetFieldIndexNumber(nFieldId) > 0 ? TRUE:FALSE);
}



/**********************************************************************
 *                   TABFile::GetINDFileRef()
 *
 * Opens the .IND file for this dataset and returns a reference to
 * the handle.  
 * If the .IND file has already been opened then the same handle is 
 * returned directly.
 * If the .IND file does not exist then the function silently returns NULL.
 *
 * Note that the returned TABINDFile handle is only a reference to an
 * object that is owned by this class.  Callers can use it but cannot
 * destroy the object.  The object will remain valid for as long as 
 * the TABFile will remain open.
 **********************************************************************/
TABINDFile  *TABFile::GetINDFileRef()
{
    if (m_pszFname == NULL)
        return NULL;

    if (m_eAccessMode == TABRead && m_poINDFile == NULL)
    {
        /*-------------------------------------------------------------
         * File is not opened yet... do it now.
         *
         * Note: We can pass the .TAB's filename directly and the
         * TABINDFile class will automagically adjust the extension.
         *------------------------------------------------------------*/
        m_poINDFile = new TABINDFile;
   
        if ( m_poINDFile->Open(m_pszFname, "r", TRUE) != 0)
        {
            // File could not be opened... probably does not exist
            delete m_poINDFile;
            m_poINDFile = NULL;
        }
        else if (m_panIndexNo && m_poDATFile)
        {
            /*---------------------------------------------------------
             * Pass type information for each indexed field.
             *--------------------------------------------------------*/
            for(int i=0; i<m_poDATFile->GetNumFields(); i++)
            {
                if (m_panIndexNo[i] > 0)
                {
                    m_poINDFile->SetIndexFieldType(m_panIndexNo[i],
                                                   GetNativeFieldType(i));
                }
            }
        }
    }

    return m_poINDFile;
}


/**********************************************************************
 *                   TABFile::SetBounds()
 *
 * Set projection coordinates bounds of the newly created dataset.
 *
 * This function must be called after creating a new dataset and before any
 * feature can be written to it.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABFile::SetBounds(double dXMin, double dYMin, 
                       double dXMax, double dYMax)
{
    if (m_eAccessMode != TABWrite)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetBounds() can be used only with Write access.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Check that dataset has been created but no feature set yet.
     *----------------------------------------------------------------*/
    if (m_poMAPFile && m_nLastFeatureId < 1)
    {
        m_poMAPFile->SetCoordsysBounds(dXMin, dYMin, dXMax, dYMax);

        m_bBoundsSet = TRUE;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "SetBounds() can be called only after dataset has been "
                 "created and before any feature is set.");
        return -1;
    }

    return 0;
}


/**********************************************************************
 *                   TABFile::GetBounds()
 *
 * Fetch projection coordinates bounds of a dataset.
 *
 * The bForce flag has no effect on TAB files since the bounds are
 * always in the header.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABFile::GetBounds(double &dXMin, double &dYMin, 
                       double &dXMax, double &dYMax,
                       GBool /*bForce = TRUE*/)
{
    TABMAPHeaderBlock *poHeader;

    if (m_poMAPFile && (poHeader=m_poMAPFile->GetHeaderBlock()) != NULL)
    {
        /*-------------------------------------------------------------
         * Projection bounds correspond to the +/- 1e9 integer coord. limits
         *------------------------------------------------------------*/
        double dX0, dX1, dY0, dY1;
        m_poMAPFile->Int2Coordsys(-1000000000, -1000000000,  
                                  dX0, dY0);
        m_poMAPFile->Int2Coordsys(1000000000, 1000000000, 
                                  dX1, dY1);
        /*-------------------------------------------------------------
         * ... and make sure that Min < Max
         *------------------------------------------------------------*/
        dXMin = MIN(dX0, dX1);
        dXMax = MAX(dX0, dX1);
        dYMin = MIN(dY0, dY1);
        dYMax = MAX(dY0, dY1);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
             "GetBounds() can be called only after dataset has been opened.");
        return -1;
    }

    return 0;
}


/**********************************************************************
 *                   TABFile::GetExtent()
 *
 * Fetch extent of the data currently stored in the dataset.
 *
 * The bForce flag has no effect on TAB files since that value is
 * always in the header.
 *
 * Returns OGRERR_NONE/OGRRERR_FAILURE.
 **********************************************************************/
OGRErr TABFile::GetExtent (OGREnvelope *psExtent,
                           CPL_UNUSED int bForce)
{
    TABMAPHeaderBlock *poHeader;

    if (m_poMAPFile && (poHeader=m_poMAPFile->GetHeaderBlock()) != NULL)
    {
        double dX0, dX1, dY0, dY1;
        /*-------------------------------------------------------------
         * Fetch extent of the data from the .map header block
         * this value is different from the projection bounds.
         *------------------------------------------------------------*/
        m_poMAPFile->Int2Coordsys(poHeader->m_nXMin, poHeader->m_nYMin,  
                                  dX0, dY0);
        m_poMAPFile->Int2Coordsys(poHeader->m_nXMax, poHeader->m_nYMax, 
                                  dX1, dY1);

       /*-------------------------------------------------------------
         * ... and make sure that Min < Max
         *------------------------------------------------------------*/
        psExtent->MinX = MIN(dX0, dX1);
        psExtent->MaxX = MAX(dX0, dX1);
        psExtent->MinY = MIN(dY0, dY1);
        psExtent->MaxY = MAX(dY0, dY1);

        return OGRERR_NONE;
    }

    return OGRERR_FAILURE;
}

/**********************************************************************
 *                   TABFile::GetFeatureCountByType()
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
int TABFile::GetFeatureCountByType(int &numPoints, int &numLines,
                                   int &numRegions, int &numTexts,
                                   GBool /* bForce = TRUE*/ )
{
    TABMAPHeaderBlock *poHeader;

    if (m_poMAPFile && (poHeader=m_poMAPFile->GetHeaderBlock()) != NULL)
    {
        numPoints  = poHeader->m_numPointObjects;
        numLines   = poHeader->m_numLineObjects;
        numRegions = poHeader->m_numRegionObjects;
        numTexts   = poHeader->m_numTextObjects;
    }
    else
    {
        numPoints = numLines = numRegions = numTexts = 0;
        return -1;
    }

    return 0;
}


/**********************************************************************
 *                   TABFile::SetMIFCoordSys()
 *
 * Set projection for a new file using a MIF coordsys string.
 *
 * This function must be called after creating a new dataset and before any
 * feature can be written to it.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABFile::SetMIFCoordSys(const char *pszMIFCoordSys)
{
    if (m_eAccessMode != TABWrite)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetMIFCoordSys() can be used only with Write access.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Check that dataset has been created but no feature set yet.
     *----------------------------------------------------------------*/
    if (m_poMAPFile && m_nLastFeatureId < 1)
    {
        OGRSpatialReference *poSpatialRef;

        poSpatialRef = MITABCoordSys2SpatialRef( pszMIFCoordSys );

        if (poSpatialRef)
        {
            double dXMin, dYMin, dXMax, dYMax;
            if (SetSpatialRef(poSpatialRef) == 0)
            {
                if (MITABExtractCoordSysBounds(pszMIFCoordSys,
                                               dXMin, dYMin, 
                                               dXMax, dYMax) == TRUE)
                {
                    // If the coordsys string contains bounds, then use them
                    if (SetBounds(dXMin, dYMin, dXMax, dYMax) != 0)
                    {
                        // Failed Setting Bounds... an error should have
                        // been already reported.
                        return -1;
                    }
                }
            }
            else
            {
                // Failed setting poSpatialRef... and error should have 
                // been reported.
                return -1;
            }

            // Release our handle on poSpatialRef
            if( poSpatialRef->Dereference() == 0 )
                delete poSpatialRef;
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "SetMIFCoordSys() can be called only after dataset has been "
                 "created and before any feature is set.");
        return -1;
    }

    return 0;
}

/**********************************************************************
 *                   TABFile::SetProjInfo()
 *
 * Set projection for a new file using a TABProjInfo structure.
 *
 * This function must be called after creating a new dataset and before any
 * feature can be written to it.
 *
 * This call will also trigger a lookup of default bounds for the specified
 * projection (except nonearth), and reset the m_bBoundsValid flag.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABFile::SetProjInfo(TABProjInfo *poPI)
{
    if (m_eAccessMode != TABWrite)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetProjInfo() can be used only with Write access.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Lookup default bounds and reset m_bBoundsSet flag
     *----------------------------------------------------------------*/
    double dXMin, dYMin, dXMax, dYMax;

    m_bBoundsSet = FALSE;
    if (MITABLookupCoordSysBounds(poPI, dXMin, dYMin, dXMax, dYMax) == TRUE)
    {
        SetBounds(dXMin, dYMin, dXMax, dYMax);
    }

    /*-----------------------------------------------------------------
     * Check that dataset has been created but no feature set yet.
     *----------------------------------------------------------------*/
    if (m_poMAPFile && m_nLastFeatureId < 1)
    {
        if (m_poMAPFile->GetHeaderBlock()->SetProjInfo( poPI ) != 0)
            return -1;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "SetProjInfo() can be called only after dataset has been "
                 "created and before any feature is set.");
        return -1;
    }

    return 0;
}


/************************************************************************/
/*                            DeleteField()                             */
/************************************************************************/

OGRErr TABFile::DeleteField( int iField )
{
    if( m_poDATFile == NULL || !TestCapability(OLCDeleteField) )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "DeleteField");
        return OGRERR_FAILURE;
    }

    if (iField < 0 || iField >= m_poDefn->GetFieldCount())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid field index");
        return OGRERR_FAILURE;
    }

    if ( m_poDATFile->DeleteField( iField ) == 0 )
    {
        m_bNeedTABRewrite = TRUE;

        /* Delete from the array of indexed fields */
        if( iField < m_poDefn->GetFieldCount() - 1 )
        {
            memmove(m_panIndexNo + iField, m_panIndexNo + iField + 1,
                    (m_poDefn->GetFieldCount() - 1 - iField) * sizeof(int));
        }

        m_poDefn->DeleteFieldDefn( iField );

        if (m_eAccessMode == TABReadWrite)
            WriteTABFile();

        return OGRERR_NONE;
    }
    else
        return OGRERR_FAILURE;
}

/************************************************************************/
/*                           ReorderFields()                            */
/************************************************************************/

OGRErr TABFile::ReorderFields( int* panMap )
{
    if( m_poDATFile == NULL || !TestCapability(OLCDeleteField) )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "ReorderFields");
        return OGRERR_FAILURE;
    }
    if (m_poDefn->GetFieldCount() == 0)
        return OGRERR_NONE;

    OGRErr eErr = OGRCheckPermutation(panMap, m_poDefn->GetFieldCount());
    if (eErr != OGRERR_NONE)
        return eErr;

    if ( m_poDATFile->ReorderFields( panMap ) == 0 )
    {
        m_bNeedTABRewrite = TRUE;

        int* panNewIndexedField = (int*) CPLMalloc(sizeof(int)*m_poDefn->GetFieldCount());
        for(int i=0;i<m_poDefn->GetFieldCount();i++)
        {
            panNewIndexedField[i] = m_panIndexNo[panMap[i]];
        }
        CPLFree(m_panIndexNo);
        m_panIndexNo = panNewIndexedField;

        m_poDefn->ReorderFieldDefns( panMap );

        if (m_eAccessMode == TABReadWrite)
            WriteTABFile();

        return OGRERR_NONE;
    }
    else
        return OGRERR_FAILURE;
}

/************************************************************************/
/*                           AlterFieldDefn()                           */
/************************************************************************/

OGRErr TABFile::AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags )
{
    if( m_poDATFile == NULL || !TestCapability(OLCDeleteField) )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "AlterFieldDefn");
        return OGRERR_FAILURE;
    }

    if (iField < 0 || iField >= m_poDefn->GetFieldCount())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid field index");
        return OGRERR_FAILURE;
    }

    if ( m_poDATFile->AlterFieldDefn( iField, poNewFieldDefn, nFlags ) == 0 )
    {
        m_bNeedTABRewrite = TRUE;

        OGRFieldDefn* poFieldDefn = m_poDefn->GetFieldDefn(iField);
        if ((nFlags & ALTER_TYPE_FLAG) &&
            poNewFieldDefn->GetType() != poFieldDefn->GetType())
        {
            poFieldDefn->SetType(poNewFieldDefn->GetType());
            if( (nFlags & ALTER_WIDTH_PRECISION_FLAG) == 0 )
                poFieldDefn->SetWidth(254);
        }
        if (nFlags & ALTER_NAME_FLAG)
            poFieldDefn->SetName(poNewFieldDefn->GetNameRef());
        if ((nFlags & ALTER_WIDTH_PRECISION_FLAG) &&
            poFieldDefn->GetType() == OFTString)
        {
            poFieldDefn->SetWidth(m_poDATFile->GetFieldWidth(iField));
        }

        if (m_eAccessMode == TABReadWrite)
            WriteTABFile();

        return OGRERR_NONE;
    }
    else
        return OGRERR_FAILURE;
}

/************************************************************************/
/*                            SyncToDisk()                             */
/************************************************************************/

OGRErr TABFile::SyncToDisk()
{
    /* Silently return */
    if( m_eAccessMode == TABRead )
        return OGRERR_NONE;

    if( WriteTABFile() != 0 )
        return OGRERR_FAILURE;

    if( m_poMAPFile->SyncToDisk() != 0 )
        return OGRERR_FAILURE;

    if( m_poDATFile->SyncToDisk() != 0 )
        return OGRERR_FAILURE;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int TABFile::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCSequentialWrite) )
        return m_eAccessMode != TABRead;

    else if( EQUAL(pszCap,OLCRandomWrite) )
        return m_eAccessMode != TABRead;

    else if( EQUAL(pszCap,OLCDeleteFeature) )
        return m_eAccessMode != TABRead;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return m_poFilterGeom == NULL
            && m_poAttrQuery == NULL;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastGetExtent) )
        return TRUE;

    else if( EQUAL(pszCap,OLCCreateField) )
        return m_eAccessMode != TABRead;

    else if( EQUAL(pszCap,OLCDeleteField) )
        return m_eAccessMode != TABRead;

    else if( EQUAL(pszCap,OLCReorderFields) )
        return m_eAccessMode != TABRead;

    else if( EQUAL(pszCap,OLCAlterFieldDefn) )
        return m_eAccessMode != TABRead;

    else 
        return FALSE;
}

/**********************************************************************
 *                   TABFile::Dump()
 *
 * Dump block contents... available only in DEBUG mode.
 **********************************************************************/
#ifdef DEBUG

void TABFile::Dump(FILE *fpOut /*=NULL*/)
{
    if (fpOut == NULL)
        fpOut = stdout;

    fprintf(fpOut, "----- TABFile::Dump() -----\n");

    if (m_poMAPFile == NULL)
    {
        fprintf(fpOut, "File is not opened.\n");
    }
    else
    {
        fprintf(fpOut, "File is opened: %s\n", m_pszFname);
        fprintf(fpOut, "Associated TABLE file ...\n\n");
        m_poDATFile->Dump(fpOut);
        fprintf(fpOut, "... end of TABLE file dump.\n\n");
        if( GetSpatialRef() != NULL )
        {
            char        *pszWKT;

            GetSpatialRef()->exportToWkt( &pszWKT );
            fprintf( fpOut, "SRS = %s\n", pszWKT );
            OGRFree( pszWKT );                                          
        }
        fprintf(fpOut, "Associated .MAP file ...\n\n");
        m_poMAPFile->Dump(fpOut);
        fprintf(fpOut, "... end of .MAP file dump.\n\n");

    }

    fflush(fpOut);
}

#endif // DEBUG
