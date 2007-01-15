/**********************************************************************
 * $Id: mitab_tabfile.cpp,v 1.59 2006/02/08 05:02:58 dmorissette Exp $
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
 * Revision 1.59  2006/02/08 05:02:58  dmorissette
 * Fixed crash when attempting to write TABPolyline object with an invalid
 * geometry (GDAL bug 1059)
 *
 * Revision 1.58  2005/10/13 20:12:03  fwarmerdam
 * layers with just regions can't be set as type wkbPolygon because they may
 * have multipolygons (bug GDAL:958)
 *     http://bugzilla.remotesensing.org/show_bug.cgi?id=958
 *
 * Revision 1.57  2005/10/04 15:44:31  dmorissette
 * First round of support for Collection objects. Currently supports reading
 * from .TAB/.MAP and writing to .MIF. Still lacks symbol support and write
 * support. (Based in part on patch and docs from Jim Hope, bug 1126)
 *
 * Revision 1.56  2004/09/17 19:26:23  fwarmerdam
 * Several fixes in error/failure cases in TABFile::Open() care of
 * Stephen Cheesman of GeoSoft.
 *
 * Revision 1.55  2004/06/30 20:29:04  dmorissette
 * Fixed refs to old address danmo@videotron.ca
 *
 * Revision 1.54  2004/05/20 13:49:46  fwarmerdam
 * Removed special bailout for m_poMAPFile == NULL in TABFile::Close().
 * It is important to cleanup other used memory.
 *
 * Revision 1.53  2003/01/18 20:22:39  daniel
 * Fixed leak of TABMAPObjHdr when writing NONE geometries in SetFeature()
 *
 * Revision 1.52  2002/09/23 13:15:35  warmerda
 * fixed memory leak in SetMIFCoordSys
 *
 * Revision 1.51  2002/09/23 13:06:21  warmerda
 * ensure pre-created m_poDefn is referenced properly
 *
 * Revision 1.50  2002/08/27 17:19:22  warmerda
 * auto-add FID column if there aren't any columns
 *
 * Revision 1.49  2002/03/26 01:48:40  daniel
 * Added Multipoint object type (V650)
 *
 * Revision 1.48  2001/11/19 15:08:50  daniel
 * Handle the case of TAB_GEOM_NONE with the new TABMAPObjHdr classes.
 *
 * Revision 1.47  2001/11/17 21:54:06  daniel
 * Made several changes in order to support writing objects in 16 bits 
 * coordinate format. New TABMAPObjHdr-derived classes are used to hold 
 * object info in mem until block is full.
 *
 * Revision 1.46  2001/09/19 21:39:31  warmerda
 * improved capabilities logic
 *
 * Revision 1.45  2001/09/14 19:14:43  warmerda
 * added attribute query support
 *
 * Revision 1.44  2001/09/14 03:23:55  warmerda
 * Substantial upgrade to support spatial queries using spatial indexes
 *
 * Revision 1.43  2001/03/15 03:57:51  daniel
 * Added implementation for new OGRLayer::GetExtent(), returning data MBR.
 *
 * Revision 1.42  2001/03/09 04:14:19  daniel
 * Fixed problem creating new files with mixed case extensions (e.g. ".Tab")
 *
 * Revision 1.41  2001/01/23 21:23:42  daniel
 * Added projection bounds lookup table, called from TABFile::SetProjInfo()
 *
 * Revision 1.40  2001/01/22 16:03:58  warmerda
 * expanded tabs
 *
 * Revision 1.39  2000/12/15 05:32:31  daniel
 * Handle table type LINKED the same way as table type NATIVE.
 * Make sure max char field width is 254 in AddFieldNative().
 *
 * Revision 1.38  2000/10/18 03:53:16  daniel
 * GetBounds() now calculates bounds based on +/- 1e9 integer coordinates limit
 *
 * Revision 1.37  2000/09/20 18:32:02  daniel
 * Accept "FORMAT: DBF" in version 100 .tab headers
 *
 * Revision 1.36  2000/09/07 23:32:13  daniel
 * Added RecordDeletedFlag to TABFeature with get/set methods
 *
 * Revision 1.35  2000/07/04 01:50:02  warmerda
 * removed unprotected debugging printf.
 *
 * Revision 1.34  2000/06/28 00:31:05  warmerda
 * Try to set geometry type on OGRFeatureDefn by feature count
 *
 * Revision 1.33  2000/03/19 23:22:43  daniel
 * Fixed test on return value of SetSpatialRef() in SetMIFCoordSys()
 *
 * Revision 1.32  2000/02/28 17:11:14  daniel
 * Support indexed fields and new V450 object types
 *
 * Revision 1.31  2000/02/18 20:45:56  daniel
 * Validate field names on write
 *
 * Revision 1.30  2000/01/26 18:18:01  warmerda
 * fixed capabilities test
 *
 * Revision 1.29  2000/01/18 23:13:05  daniel
 * Added missing m_poDefn->Reference()
 *
 * Revision 1.28  2000/01/18 22:14:36  daniel
 * Fixed compile warnings
 *
 * Revision 1.27  2000/01/16 19:08:49  daniel
 * Added support for reading 'Table Type DBF' tables
 *
 * ...
 *
 * Revision 1.1  1999/07/12 04:18:25  daniel
 * Initial checkin
 *
 **********************************************************************/

#include "mitab.h"
#include "mitab_utils.h"

#include <ctype.h>      /* isspace() */

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
    m_pszCharset = NULL;
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
/*                          GetFeatureCount()                           */
/************************************************************************/
int TABFile::GetFeatureCount (int bForce)
{
    
    if( m_poFilterGeom != NULL || m_poAttrQuery != NULL )
        return OGRLayer::GetFeatureCount( bForce );
    else
        return m_nLastFeatureId;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/
void TABFile::ResetReading()
{
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
            TABMAPHeaderBlock *poHeader;
    
            poHeader = m_poMAPFile->GetHeaderBlock();

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
}

/**********************************************************************
 *                   TABFile::Open()
 *
 * Open a .TAB dataset and the associated files, and initialize the 
 * structures to be ready to read features from (or write to) it.
 *
 * Supported access modes are "r" (read-only) and "w" (create new dataset).
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
int TABFile::Open(const char *pszFname, const char *pszAccess,
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

    /*-----------------------------------------------------------------
     * Validate access mode
     *----------------------------------------------------------------*/
    if (EQUALN(pszAccess, "r", 1))
    {
        m_eAccessMode = TABRead;
        pszAccess = "rb";
    }
    else if (EQUALN(pszAccess, "w", 1))
    {
        m_eAccessMode = TABWrite;
        pszAccess = "wb";
    }
    else
    {
        if (!bTestOpenNoError)
            CPLError(CE_Failure, CPLE_FileIO,
                 "Open() failed: access mode \"%s\" not supported", pszAccess);
        else
            CPLErrorReset();

        return -1;
    }

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
    if (m_eAccessMode == TABRead)
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
        m_pszCharset = CPLStrdup("Neutral");
        m_eTableType = TABTableNative;

        /*-------------------------------------------------------------
         * Do initial setup of feature definition.
         *------------------------------------------------------------*/
        char *pszFeatureClassName = TABGetBasename(m_pszFname);
        m_poDefn = new OGRFeatureDefn(pszFeatureClassName);
        m_poDefn->Reference();
        CPLFree(pszFeatureClassName);
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
   
    if ( m_poDATFile->Open(pszTmpFname, pszAccess, m_eTableType) != 0)
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
    if (m_eAccessMode == TABRead && ParseTABFileFields() != 0)
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
    if (m_eAccessMode == TABRead)
    {
        /*-------------------------------------------------------------
         * Read access: .MAP/.ID are optional... try to open but return
         * no error if files do not exist.
         *------------------------------------------------------------*/
        if (m_poMAPFile->Open(pszTmpFname, pszAccess, TRUE) < 0)
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
        else
            /* we leave it unknown indicating a mixture */;
    }
    else if (m_poMAPFile->Open(pszTmpFname, pszAccess) != 0)
    {
        // Open Failed for write... 
        // an error has already been reported, just return.
        CPLFree(pszTmpFname);
        Close();
        if (bTestOpenNoError)
            CPLErrorReset();

        return -1;
    }


    CPLFree(pszTmpFname);
    pszTmpFname = NULL;

    /*-----------------------------------------------------------------
     * __TODO__ we could probably call GetSpatialRef() here to force
     * parsing the projection information... this would allow us to 
     * assignSpatialReference() on the geometries that we return.
     *----------------------------------------------------------------*/

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

    if (m_eAccessMode != TABRead)
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

    if (m_eAccessMode != TABRead)
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
        while(*pszStr != '\0' && isspace(*pszStr))
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
                    poFieldDefn = new OGRFieldDefn(papszTok[0], OFTString);
                    poFieldDefn->SetWidth(10);
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
 * This private method should be used only during the Close() call with
 * write access mode.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABFile::WriteTABFile()
{
    FILE *fp;

    if (m_eAccessMode != TABWrite)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "WriteTABFile() can be used only with Write access.");
        return -1;
    }

    if ( (fp = VSIFOpen(m_pszFname, "wt")) != NULL)
    {
        fprintf(fp, "!table\n");
        fprintf(fp, "!version %d\n", m_nVersion);
        fprintf(fp, "!charset %s\n", m_pszCharset);
        fprintf(fp, "\n");

        if (m_poDefn && m_poDefn->GetFieldCount() > 0)
        {
            int iField;
            OGRFieldDefn *poFieldDefn;
            const char *pszFieldType;

            fprintf(fp, "Definition Table\n");
            fprintf(fp, "  Type NATIVE Charset \"%s\"\n", m_pszCharset);
            fprintf(fp, "  Fields %d\n", m_poDefn->GetFieldCount());

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
                    pszFieldType = "Integer";
                    break;
                  case TABFSmallInt:
                    pszFieldType = "SmallInt";
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
                  default:
                    // Unsupported field type!!!  This should never happen.
                    CPLError(CE_Failure, CPLE_AssertionFailed,
                             "WriteTABFile(): Unsupported field type");
                    VSIFClose(fp);
                    return -1;
                }

                if (GetFieldIndexNumber(iField) == 0)
                {
                    fprintf(fp, "    %s %s ;\n", poFieldDefn->GetNameRef(), 
                            pszFieldType );
                }
                else
                {
                    fprintf(fp, "    %s %s Index %d ;\n", 
                            poFieldDefn->GetNameRef(), pszFieldType,
                            GetFieldIndexNumber(iField) );
                }
                
            }
        }
        else
        {
            fprintf(fp, "Definition Table\n");
            fprintf(fp, "  Type NATIVE Charset \"%s\"\n", m_pszCharset);
            fprintf(fp, "  Fields 1\n");
            fprintf(fp, "    FID Integer ;\n" );
        }

        VSIFClose(fp);
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
    // Commit the latest changes to the file...
    
    // In Write access, it's time to write the .TAB file.
    if (m_eAccessMode == TABWrite && m_poMAPFile)
    {
        // First update file version number...
        int nMapObjVersion = m_poMAPFile->GetMinTABFileVersion();
        m_nVersion = MAX(m_nVersion, nMapObjVersion);

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

    /*-----------------------------------------------------------------
     * Note: we have to check the reference count before deleting 
     * m_poSpatialRef and m_poDefn
     *----------------------------------------------------------------*/
    if (m_poDefn )
    {
        int nRefCount = m_poDefn->Dereference();

        CPLAssert( nRefCount >= 0 );

        if( nRefCount == 0 )
            delete m_poDefn;
        m_poDefn = NULL;
    }
    
    if (m_poSpatialRef && m_poSpatialRef->Dereference() == 0)
        delete m_poSpatialRef;
    m_poSpatialRef = NULL;
    
    CSLDestroy(m_papszTABFile);
    m_papszTABFile = NULL;

    CPLFree(m_pszFname);
    m_pszFname = NULL;

    CPLFree(m_pszCharset);
    m_pszCharset = NULL;

    CPLFree(m_panIndexNo);
    m_panIndexNo = NULL;

    return 0;
}

/**********************************************************************
 *                   TABFile::GetNextFeatureId()
 *
 * Returns feature id that follows nPrevId, or -1 if it is the
 * last feature id.  Pass nPrevId=-1 to fetch the first valid feature id.
 **********************************************************************/
int TABFile::GetNextFeatureId(int nPrevId)
{
    if (m_eAccessMode != TABRead)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GetNextFeatureId() can be used only with Read access.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Are we using spatial rather than .ID based traversal?
     *----------------------------------------------------------------*/
    if( bUseSpatialTraversal )
        return m_poMAPFile->GetNextFeatureId( nPrevId );

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
TABFeature *TABFile::GetFeatureRef(int nFeatureId)
{
    CPLErrorReset();

    if (m_eAccessMode != TABRead)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GetFeatureRef() can be used only with Read access.");
        return NULL;
    }

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


    if (nFeatureId <= 0 || nFeatureId > m_nLastFeatureId ||
        m_poMAPFile->MoveToObjId(nFeatureId) != 0 ||
        m_poDATFile->GetRecordBlock(nFeatureId) == NULL )
    {
        //     CPLError(CE_Failure, CPLE_IllegalArg,
        //    "GetFeatureRef() failed: invalid feature id %d", 
        //    nFeatureId);
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
     *----------------------------------------------------------------*/
    switch(m_poMAPFile->GetCurObjType())
    {
      case TAB_GEOM_NONE:
        m_poCurFeature = new TABFeature(m_poDefn);
        break;
      case TAB_GEOM_SYMBOL_C:
      case TAB_GEOM_SYMBOL:
        m_poCurFeature = new TABPoint(m_poDefn);
        break;
      case TAB_GEOM_FONTSYMBOL_C:
      case TAB_GEOM_FONTSYMBOL:
        m_poCurFeature = new TABFontPoint(m_poDefn);
        break;
      case TAB_GEOM_CUSTOMSYMBOL_C:
      case TAB_GEOM_CUSTOMSYMBOL:
        m_poCurFeature = new TABCustomPoint(m_poDefn);
        break;
      case TAB_GEOM_LINE_C:
      case TAB_GEOM_LINE:
      case TAB_GEOM_PLINE_C:
      case TAB_GEOM_PLINE:
      case TAB_GEOM_MULTIPLINE_C:
      case TAB_GEOM_MULTIPLINE:
      case TAB_GEOM_V450_MULTIPLINE_C:
      case TAB_GEOM_V450_MULTIPLINE:
       m_poCurFeature = new TABPolyline(m_poDefn);
        break;
      case TAB_GEOM_ARC_C:
      case TAB_GEOM_ARC:
        m_poCurFeature = new TABArc(m_poDefn);
        break;

      case TAB_GEOM_REGION_C:
      case TAB_GEOM_REGION:
      case TAB_GEOM_V450_REGION_C:
      case TAB_GEOM_V450_REGION:
        m_poCurFeature = new TABRegion(m_poDefn);
        break;
      case TAB_GEOM_RECT_C:
      case TAB_GEOM_RECT:
      case TAB_GEOM_ROUNDRECT_C:
      case TAB_GEOM_ROUNDRECT:
        m_poCurFeature = new TABRectangle(m_poDefn);
        break;
      case TAB_GEOM_ELLIPSE_C:
      case TAB_GEOM_ELLIPSE:
        m_poCurFeature = new TABEllipse(m_poDefn);
        break;
      case TAB_GEOM_TEXT_C:
      case TAB_GEOM_TEXT:
        m_poCurFeature = new TABText(m_poDefn);
        break;
      case TAB_GEOM_MULTIPOINT_C:
      case TAB_GEOM_MULTIPOINT:
        m_poCurFeature = new TABMultiPoint(m_poDefn);
        break;
      case TAB_GEOM_COLLECTION_C:
      case TAB_GEOM_COLLECTION:
        m_poCurFeature = new TABCollection(m_poDefn);  
        break;
      default:
        /*-------------------------------------------------------------
         * Unsupported feature type... we still return a valid feature
         * with NONE geometry after producing a Warning.
         * Callers can trap that case by checking CPLGetLastErrorNo() 
         * against TAB_WarningFeatureTypeNotSupported
         *------------------------------------------------------------*/
//        m_poCurFeature = new TABDebugFeature(m_poDefn);
        m_poCurFeature = new TABFeature(m_poDefn);

        CPLError(CE_Warning, TAB_WarningFeatureTypeNotSupported,
                 "Unsupported object type %d (0x%2.2x).  Feature will be "
                 "returned with NONE geometry.", 
                 m_poMAPFile->GetCurObjType(), m_poMAPFile->GetCurObjType() );
    }

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
 *                   TABFile::SetFeature()
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
int TABFile::SetFeature(TABFeature *poFeature, int nFeatureId /*=-1*/)
{
    if (m_eAccessMode != TABWrite)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetFeature() can be used only with Write access.");
        return -1;
    }

    if (nFeatureId != -1)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SetFeature(): random access not implemented yet.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Make sure file is opened and establish new feature id.
     *----------------------------------------------------------------*/
    if (m_poMAPFile == NULL)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "SetFeature() failed: file is not opened!");
        return -1;
    }

    if (m_nLastFeatureId < 1)
    {
        /*-------------------------------------------------------------
         * OK, this is the first feature in the dataset... make sure the
         * .DAT schema has been initialized.
         *------------------------------------------------------------*/
        if (m_poDefn == NULL)
            SetFeatureDefn(poFeature->GetDefnRef(), NULL);

        /*-------------------------------------------------------------
         * Special hack to write out at least one field if none are in 
         * OGRFeatureDefn.
         *------------------------------------------------------------*/
        if( m_poDATFile->GetNumFields() == 0 )
        {
            CPLError(CE_Warning, CPLE_IllegalArg,
                     "MapInfo tables must contain at least 1 column, adding dummy FID column.");
            m_poDATFile->AddField("FID", TABFInteger, 10, 0 );
        }

        nFeatureId = m_nLastFeatureId = 1;
    }
    else
    {
        nFeatureId = ++ m_nLastFeatureId;
    }


    /*-----------------------------------------------------------------
     * Write fields to the .DAT file and update .IND if necessary
     *----------------------------------------------------------------*/
    if (m_poDATFile == NULL ||
        m_poDATFile->GetRecordBlock(nFeatureId) == NULL ||
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
    TABMAPObjectBlock *poObjBlock = NULL;

    
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

    if ( poObjHdr == NULL || m_poMAPFile == NULL ||
        m_poMAPFile->PrepareNewObj(nFeatureId, poObjHdr->m_nType) != 0 ||
         poFeature->WriteGeometryToMAPFile(m_poMAPFile, poObjHdr) != 0 )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed writing geometry for feature id %d in %s",
                 nFeatureId, m_pszFname);
        return -1;
    }

    if (poObjHdr->m_nType == TAB_GEOM_NONE)
    {
        // NONE objects have no reference in the ObjectBlocks.  Just flush it.
        delete poObjHdr;
    }
    else if ( (poObjBlock = m_poMAPFile->GetCurObjBlock()) == NULL ||
              poObjBlock->AddObject(poObjHdr) != 0 )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed writing object header for feature id %d in %s",
                 nFeatureId, m_pszFname);
        return -1;
    }

    return nFeatureId;
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
                eMapInfoType = TABFFloat;
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
 * This function should be called after creating a new dataset, but before 
 * writing the first feature.
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
                            GBool bIndexed /*=FALSE*/, GBool /*bUnique=FALSE*/)
{
    OGRFieldDefn *poFieldDefn;
    int nStatus = 0;
    char *pszCleanName = NULL;

    if (m_eAccessMode != TABWrite)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "AddFieldNative() can be used only with Write access.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Check that call happens at the right time in dataset's life.
     *----------------------------------------------------------------*/
    if (m_eAccessMode != TABWrite || 
        m_nLastFeatureId > 0 || m_poDATFile == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "AddFieldNative() must be called after opening a new "
                 "dataset, but before writing the first feature to it.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Create new OGRFeatureDefn if not done yet...
     *----------------------------------------------------------------*/
    if (m_poDefn== NULL)
    {
        char *pszFeatureClassName = TABGetBasename(m_pszFname);
        m_poDefn = new OGRFeatureDefn(pszFeatureClassName);
        CPLFree(pszFeatureClassName);
        // Ref count defaults to 0... set it to 1
        m_poDefn->Reference();
    }

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
        poFieldDefn = new OGRFieldDefn(pszCleanName, OFTString);
        poFieldDefn->SetWidth(nWidth);
        break;
      case TABFInteger:
        /*-------------------------------------------------
         * INTEGER type
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(pszCleanName, OFTInteger);
        break;
      case TABFSmallInt:
        /*-------------------------------------------------
         * SMALLINT type
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(pszCleanName, OFTInteger);
        break;
      case TABFDecimal:
        /*-------------------------------------------------
         * DECIMAL type
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(pszCleanName, OFTReal);
        poFieldDefn->SetWidth(nWidth);
        poFieldDefn->SetPrecision(nPrecision);
        break;
      case TABFFloat:
        /*-------------------------------------------------
         * FLOAT type
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(pszCleanName, OFTReal);
        break;
      case TABFDate:
        /*-------------------------------------------------
         * DATE type (returned as a string: "DD/MM/YYYY")
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(pszCleanName, OFTString);
        poFieldDefn->SetWidth(10);
        break;
      case TABFLogical:
        /*-------------------------------------------------
         * LOGICAL type (value "T" or "F")
         *------------------------------------------------*/
        poFieldDefn = new OGRFieldDefn(pszCleanName, OFTString);
        poFieldDefn->SetWidth(1);
        break;
      default:
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported type for field %s", pszCleanName);
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
    nStatus = m_poDATFile->AddField(pszCleanName, eMapInfoType, 
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
OGRErr TABFile::GetExtent (OGREnvelope *psExtent, int bForce)
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

    /*-----------------------------------------------------------------
     * Lookup default bounds and reset m_bBoundsSet flag
     *----------------------------------------------------------------*/
    double dXMin, dYMin, dXMax, dYMax;

    m_bBoundsSet = FALSE;
    if (MITABLookupCoordSysBounds(poPI, dXMin, dYMin, dXMax, dYMax) == TRUE)
    {
        SetBounds(dXMin, dYMin, dXMax, dYMax);
    }

    return 0;
}



/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int TABFile::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCSequentialWrite) )
        return m_eAccessMode == TABWrite;

    else if( EQUAL(pszCap,OLCRandomWrite) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return m_poFilterGeom == NULL
            && m_poAttrQuery == NULL;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastGetExtent) )
        return TRUE;

    else if( EQUAL(pszCap,OLCCreateField) )
        return TRUE;

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
