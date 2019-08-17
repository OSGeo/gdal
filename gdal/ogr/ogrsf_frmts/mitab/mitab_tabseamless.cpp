/**********************************************************************
 *
 * Name:     mitab_tabseamless.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the TABSeamless class, used to handle seamless
 *           .TAB datasets.
 * Author:   Daniel Morissette, dmorissette@dmsolutions.ca
 *
 **********************************************************************
 * Copyright (c) 1999-2004, Daniel Morissette
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
 **********************************************************************/

#include "cpl_port.h"
#include "mitab.h"

#include <cctype>
#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "mitab_priv.h"
#include "mitab_utils.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$")

/*=====================================================================
 *                      class TABSeamless
 *
 * Support for seamless vector datasets.
 *
 * The current implementation has some limitations (base assumptions):
 *  - Read-only
 *  - Base tables can only be of type TABFile
 *  - Feature Ids are build using the id of the base table in the main
 *    index table (upper 32 bits) and the actual feature id of each object
 *    inside the base tables (lower 32 bits).
 *  - Only relative paths are supported for base tables names.
 *
 *====================================================================*/

/**********************************************************************
 *                   TABSeamless::TABSeamless()
 *
 * Constructor.
 **********************************************************************/
TABSeamless::TABSeamless() :
    m_pszFname(nullptr),
    m_pszPath(nullptr),
    m_eAccessMode(TABRead),
    m_poFeatureDefnRef(nullptr),
    m_poIndexTable(nullptr),
    m_nTableNameField(-1),
    m_nCurBaseTableId(-1),
    m_poCurBaseTable(nullptr),
    m_bEOF(FALSE)
{
    m_poCurFeature = nullptr;
    m_nCurFeatureId = -1;
}

/**********************************************************************
 *                   TABSeamless::~TABSeamless()
 *
 * Destructor.
 **********************************************************************/
TABSeamless::~TABSeamless()
{
    TABSeamless::Close();
}

void TABSeamless::ResetReading()
{
    if (m_poIndexTable)
        OpenBaseTable(-1);  // Asking for first table resets everything

    // Reset m_nCurFeatureId so that next pass via GetNextFeatureId()
    // will start from the beginning
    m_nCurFeatureId = -1;
}

/**********************************************************************
 *                   TABSeamless::Open()
 *
 * Open a seamless .TAB dataset and initialize the structures to be ready
 * to read features from it.
 *
 * Seamless .TAB files are composed of a main .TAB file in which each
 * feature is the MBR of a base table.
 *
 * Set bTestOpenNoError=TRUE to silently return -1 with no error message
 * if the file cannot be opened.  This is intended to be used in the
 * context of a TestOpen() function.  The default value is FALSE which
 * means that an error is reported if the file cannot be opened.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABSeamless::Open(const char *pszFname, TABAccess eAccess,
                      GBool bTestOpenNoError /*= FALSE*/,
                      const char* /*pszCharset = NULL */ )
{
    char nStatus = 0;

    if (m_poIndexTable)
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
        nStatus = static_cast<char>(OpenForRead(pszFname, bTestOpenNoError));
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
 *                   TABSeamless::OpenForRead()
 *
 * Open for reading
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABSeamless::OpenForRead(const char *pszFname,
                             GBool bTestOpenNoError /*= FALSE*/ )
{
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
     * Open .TAB file... since it is a small text file, we will just load
     * it as a stringlist in memory.
     *----------------------------------------------------------------*/
    char **papszTABFile = TAB_CSLLoad(m_pszFname);
    if (papszTABFile == nullptr)
    {
        if (!bTestOpenNoError)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                     "Failed opening %s.", m_pszFname);
        }

        CPLFree(m_pszFname);
        CSLDestroy(papszTABFile);
        return -1;
    }

    /*-------------------------------------------------------------
     * Look for a metadata line with "\IsSeamless" = "TRUE".
     * If there is no such line, then we may have a valid .TAB file,
     * but we do not support it in this class.
     *------------------------------------------------------------*/
    GBool bSeamlessFound = FALSE;
    for (int i=0; !bSeamlessFound && papszTABFile && papszTABFile[i]; i++)
    {
        const char *pszStr = papszTABFile[i];
        while(*pszStr != '\0' && isspace(static_cast<unsigned char>(*pszStr)))
            pszStr++;
        if (STARTS_WITH_CI(pszStr, "\"\\IsSeamless\" = \"TRUE\""))
            bSeamlessFound = TRUE;
    }
    CSLDestroy(papszTABFile);

    if ( !bSeamlessFound )
    {
        if (!bTestOpenNoError)
            CPLError(CE_Failure, CPLE_NotSupported,
                     "%s does not appear to be a Seamless TAB File.  "
                     "This type of .TAB file cannot be read by this library.",
                     m_pszFname);
        else
            CPLErrorReset();

        CPLFree(m_pszFname);

        return -1;
    }

    /*-----------------------------------------------------------------
     * OK, this appears to be a valid seamless TAB dataset...
     * Extract the path component from the main .TAB filename
     * to build the filename of the base tables
     *----------------------------------------------------------------*/
    m_pszPath = CPLStrdup(m_pszFname);
    nFnameLen = static_cast<int>(strlen(m_pszPath));
    for( ; nFnameLen > 0; nFnameLen--)
    {
        if (m_pszPath[nFnameLen-1] == '/' ||
            m_pszPath[nFnameLen-1] == '\\' )
        {
            break;
        }
        m_pszPath[nFnameLen-1] = '\0';
    }

    /*-----------------------------------------------------------------
     * Open the main Index table and look for the "Table" field that
     * should contain the path to the base table for each rectangle MBR
     *----------------------------------------------------------------*/
    m_poIndexTable = new TABFile;
    if (m_poIndexTable->Open(m_pszFname, m_eAccessMode, bTestOpenNoError) != 0)
    {
        // Open Failed... an error has already been reported, just return.
        if (bTestOpenNoError)
            CPLErrorReset();
        Close();
        return -1;
    }

    OGRFeatureDefn *poDefn = m_poIndexTable->GetLayerDefn();
    if (poDefn == nullptr ||
        (m_nTableNameField = poDefn->GetFieldIndex("Table")) == -1)
    {
        if (!bTestOpenNoError)
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Open Failed: Field 'Table' not found in Seamless "
                     "Dataset '%s'.  This is type of file not currently "
                     "supported.",
                     m_pszFname);
        Close();
        return -1;
    }

    /*-----------------------------------------------------------------
     * We need to open the first table to get its FeatureDefn
     *----------------------------------------------------------------*/
    if (OpenBaseTable(-1, bTestOpenNoError) != 0 )
    {
        // Open Failed... an error has already been reported, just return.
        if (bTestOpenNoError)
            CPLErrorReset();
        Close();
        return -1;
    }

    CPLAssert(m_poCurBaseTable);
    m_poFeatureDefnRef = m_poCurBaseTable->GetLayerDefn();
    m_poFeatureDefnRef->Reference();

    return 0;
}

/**********************************************************************
 *                   TABSeamless::Close()
 *
 * Close current file, and release all memory used.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABSeamless::Close()
{
    if (m_poIndexTable)
        delete m_poIndexTable;  // Automatically closes.
    m_poIndexTable = nullptr;

    if (m_poFeatureDefnRef )
        m_poFeatureDefnRef->Release();
    m_poFeatureDefnRef = nullptr;

    if (m_poCurFeature)
        delete m_poCurFeature;
    m_poCurFeature = nullptr;
    m_nCurFeatureId = -1;

    CPLFree(m_pszFname);
    m_pszFname = nullptr;

    CPLFree(m_pszPath);
    m_pszPath = nullptr;

    m_nTableNameField = -1;
    m_nCurBaseTableId = -1;

    if (m_poCurBaseTable)
        delete m_poCurBaseTable;
    m_poCurBaseTable = nullptr;

    return 0;
}

/**********************************************************************
 *                   TABSeamless::OpenBaseTable()
 *
 * Open the base table for specified IndexFeature.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABSeamless::OpenBaseTable(TABFeature *poIndexFeature,
                               GBool bTestOpenNoError /*=FALSE*/)
{
    CPLAssert(poIndexFeature);

    /*-----------------------------------------------------------------
     * Fetch table id.  We actually use the index feature's ids as the
     * base table ids.
     *----------------------------------------------------------------*/
    GIntBig nTableId64 = poIndexFeature->GetFID();
    int nTableId = static_cast<int>(nTableId64);
    CPLAssert(static_cast<GIntBig>(nTableId) == nTableId64);

    if (m_nCurBaseTableId == nTableId && m_poCurBaseTable != nullptr)
    {
        // The right table is already opened.  Not much to do!
        m_poCurBaseTable->ResetReading();
        return 0;
    }

    // Close current base table
    if (m_poCurBaseTable)
        delete m_poCurBaseTable;
    m_nCurBaseTableId = -1;

    m_bEOF = FALSE;

    /*-----------------------------------------------------------------
     * Build full path to the table and open it.
     * __TODO__ For now we assume that all table filename paths are relative
     *          but we may have to deal with absolute filenames as well.
     *----------------------------------------------------------------*/
    const char *pszName = poIndexFeature->GetFieldAsString(m_nTableNameField);
    char *pszFname = CPLStrdup(CPLSPrintf("%s%s", m_pszPath, pszName));

#ifndef _WIN32
    // On Unix, replace any '\\' in path with '/'
    char *pszPtr = pszFname;
    while((pszPtr = strchr(pszPtr, '\\')) != nullptr)
    {
        *pszPtr = '/';
        pszPtr++;
    }
#endif

    m_poCurBaseTable = new TABFile;
    if (m_poCurBaseTable->Open(pszFname, m_eAccessMode, bTestOpenNoError) != 0)
    {
        // Open Failed... an error has already been reported, just return.
        if (bTestOpenNoError)
            CPLErrorReset();
        delete m_poCurBaseTable;
        m_poCurBaseTable = nullptr;
        CPLFree(pszFname);
        return -1;
    }

    // Set the spatial filter to the new table
    if( m_poFilterGeom != nullptr )
    {
        m_poCurBaseTable->SetSpatialFilter( m_poFilterGeom );
    }

    m_nCurBaseTableId = nTableId;
    CPLFree(pszFname);

    return 0;
}

/**********************************************************************
 *                   TABSeamless::OpenBaseTable()
 *
 * Open the base table for specified IndexFeature.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABSeamless::OpenBaseTable(int nTableId, GBool bTestOpenNoError /*=FALSE*/)
{

    if (nTableId == -1)
    {
        // Open first table from dataset
        m_poIndexTable->ResetReading();
        if (OpenNextBaseTable(bTestOpenNoError) != 0)
        {
            // Open Failed... an error has already been reported.
            if (bTestOpenNoError)
                CPLErrorReset();
            return -1;
        }
    }
    else if (nTableId == m_nCurBaseTableId && m_poCurBaseTable != nullptr)
    {
        // The right table is already opened.  Not much to do!
        m_poCurBaseTable->ResetReading();
        return 0;
    }
    else
    {
        TABFeature *poIndexFeature = m_poIndexTable->GetFeatureRef(nTableId);

        if (poIndexFeature)
        {
            if (OpenBaseTable(poIndexFeature, bTestOpenNoError) != 0)
            {
                // Open Failed... an error has already been reported.
                if (bTestOpenNoError)
                    CPLErrorReset();
                return -1;
            }
        }
    }

    return 0;
}

/**********************************************************************
 *                   TABSeamless::OpenNextBaseTable()
 *
 * Open the next base table in the dataset, using GetNextFeature() so that
 * the spatial filter is respected.
 *
 * m_bEOF will be set if there are no more base tables to read.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABSeamless::OpenNextBaseTable(GBool bTestOpenNoError /*=FALSE*/)
{
    CPLAssert(m_poIndexTable);

    TABFeature *poIndexFeature = cpl::down_cast<TABFeature*>(m_poIndexTable->GetNextFeature());

    if (poIndexFeature)
    {
        if (OpenBaseTable(poIndexFeature, bTestOpenNoError) != 0)
        {
            // Open Failed... an error has already been reported.
            if (bTestOpenNoError)
                CPLErrorReset();
            delete poIndexFeature;
            return -1;
        }
        delete poIndexFeature;
        m_bEOF = FALSE;
    }
    else
    {
        // Reached EOF
        m_bEOF = TRUE;
    }

    return 0;
}

/**********************************************************************
 *                   TABSeamless::EncodeFeatureId()
 *
 * Combine the table id + feature id into a single feature id that should
 * be unique amongst all base tables in this seamless dataset.
 **********************************************************************/
GIntBig TABSeamless::EncodeFeatureId(int nTableId, int nBaseFeatureId)
{
    if (nTableId == -1 || nBaseFeatureId == -1)
        return -1;

    /* Feature encoding is now based on the numbers of bits on the number
       of features in the index table. */

    return (static_cast<GIntBig>(nTableId)<<32) + nBaseFeatureId;
}

int TABSeamless::ExtractBaseTableId(GIntBig nEncodedFeatureId)
{
    if (nEncodedFeatureId == -1)
        return -1;

    return static_cast<int>(nEncodedFeatureId>>32);
}

int TABSeamless::ExtractBaseFeatureId(GIntBig nEncodedFeatureId)
{
    if (nEncodedFeatureId == -1)
        return -1;

    return static_cast<int>(nEncodedFeatureId & 0xffffffff);
}

/**********************************************************************
 *                   TABSeamless::GetNextFeatureId()
 *
 * Returns feature id that follows nPrevId, or -1 if it is the
 * last feature id.  Pass nPrevId=-1 to fetch the first valid feature id.
 **********************************************************************/
GIntBig TABSeamless::GetNextFeatureId(GIntBig nPrevId)
{
    if (m_poIndexTable == nullptr || m_poCurBaseTable == nullptr)
        return -1; // File is not opened yet

    if (nPrevId == -1 || m_nCurBaseTableId != ExtractBaseTableId(nPrevId))
    {
        if (OpenBaseTable(ExtractBaseTableId(nPrevId)) != 0)
            return -1;
    }

    int nId = ExtractBaseFeatureId(nPrevId);
    do
    {
        nId = static_cast<int>(m_poCurBaseTable->GetNextFeatureId(nId));
        if (nId != -1)
            return EncodeFeatureId(m_nCurBaseTableId, nId);  // Found one!
        else
            OpenNextBaseTable();  // Skip to next tile and loop again
    } while (nId == -1 && !m_bEOF && m_poCurBaseTable);

    return -1;
}

/**********************************************************************
 *                   TABSeamless::GetFeatureRef()
 *
 * Fill and return a TABFeature object for the specified feature id.
 *
 * The returned pointer is a reference to an object owned and maintained
 * by this TABSeamless object.  It should not be altered or freed by the
 * caller and its contents is guaranteed to be valid only until the next
 * call to GetFeatureRef() or Close().
 *
 * Returns NULL if the specified feature id does not exist of if an
 * error happened.  In any case, CPLError() will have been called to
 * report the reason of the failure.
 **********************************************************************/
TABFeature *TABSeamless::GetFeatureRef(GIntBig nFeatureId)
{
    if (m_poIndexTable == nullptr)
        return nullptr; // File is not opened yet

    if (nFeatureId == m_nCurFeatureId && m_poCurFeature)
        return m_poCurFeature;

    if (m_nCurBaseTableId != ExtractBaseTableId(nFeatureId))
    {
        if (OpenBaseTable(ExtractBaseTableId(nFeatureId)) != 0)
            return nullptr;
    }

    if (m_poCurBaseTable)
    {
        if (m_poCurFeature)
            delete m_poCurFeature;
        m_poCurFeature = nullptr;

        TABFeature* poCurFeature = static_cast<TABFeature*>(m_poCurBaseTable->GetFeature(ExtractBaseFeatureId(nFeatureId)));
        if( poCurFeature == nullptr )
            return nullptr;
        m_poCurFeature = new TABFeature(m_poFeatureDefnRef);
        m_poCurFeature->SetFrom(poCurFeature);
        delete poCurFeature;

        m_nCurFeatureId = nFeatureId;

        m_poCurFeature->SetFID(nFeatureId);

        return m_poCurFeature;
    }

    return nullptr;
}

/**********************************************************************
 *                   TABSeamless::GetLayerDefn()
 *
 * Returns a reference to the OGRFeatureDefn that will be used to create
 * features in this dataset.
 *
 * Returns a reference to an object that is maintained by this TABSeamless
 * object (and thus should not be modified or freed by the caller) or
 * NULL if the OGRFeatureDefn has not been initialized yet (i.e. no file
 * opened yet)
 **********************************************************************/
OGRFeatureDefn *TABSeamless::GetLayerDefn()
{
    return m_poFeatureDefnRef;
}

/**********************************************************************
 *                   TABSeamless::GetNativeFieldType()
 *
 * Returns the native MapInfo field type for the specified field.
 *
 * Returns TABFUnknown if file is not opened, or if specified field index is
 * invalid.
 *
 * Note that field ids are positive and start at 0.
 **********************************************************************/
TABFieldType TABSeamless::GetNativeFieldType(int nFieldId)
{
    if (m_poCurBaseTable)
        return m_poCurBaseTable->GetNativeFieldType(nFieldId);

    return TABFUnknown;
}

/**********************************************************************
 *                   TABSeamless::IsFieldIndexed()
 *
 * Returns TRUE if field is indexed, or FALSE otherwise.
 **********************************************************************/
GBool TABSeamless::IsFieldIndexed(int nFieldId)
{
    if (m_poCurBaseTable)
        return m_poCurBaseTable->IsFieldIndexed(nFieldId);

    return FALSE;
}

/**********************************************************************
 *                   TABSeamless::IsFieldUnique()
 *
 * Returns TRUE if field is in the Unique table, or FALSE otherwise.
 **********************************************************************/
GBool TABSeamless::IsFieldUnique(int nFieldId)
{
    if (m_poCurBaseTable)
        return m_poCurBaseTable->IsFieldUnique(nFieldId);

    return FALSE;
}

/**********************************************************************
 *                   TABSeamless::GetBounds()
 *
 * Fetch projection coordinates bounds of a dataset.
 *
 * The bForce flag has no effect on TAB files since the bounds are
 * always in the header.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABSeamless::GetBounds( double &dXMin, double &dYMin,
                            double &dXMax, double &dYMax,
                            GBool bForce /*= TRUE*/ )
{
    if (m_poIndexTable == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
             "GetBounds() can be called only after dataset has been opened.");
        return -1;
    }

    return m_poIndexTable->GetBounds(dXMin, dYMin, dXMax, dYMax, bForce);
}

/**********************************************************************
 *                   TABSeamless::GetExtent()
 *
 * Fetch extent of the data currently stored in the dataset.
 *
 * The bForce flag has no effect on TAB files since that value is
 * always in the header.
 *
 * Returns OGRERR_NONE/OGRRERR_FAILURE.
 **********************************************************************/
OGRErr TABSeamless::GetExtent (OGREnvelope *psExtent, int bForce)
{
    if (m_poIndexTable == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
             "GetExtent() can be called only after dataset has been opened.");
        return OGRERR_FAILURE;
    }

    return m_poIndexTable->GetExtent(psExtent, bForce);
}

/**********************************************************************
 *                   TABSeamless::GetFeatureCountByType()
 *
 * Return number of features of each type.
 *
 * Note that the sum of the 4 returned values may be different from
 * the total number of features since features with NONE geometry
 * are not taken into account here.
 *
 * Returns 0 on success, or silently returns -1 (with no error) if this
 * information is not available.
 **********************************************************************/
int TABSeamless::GetFeatureCountByType(CPL_UNUSED int &numPoints,
                                       CPL_UNUSED int &numLines,
                                       CPL_UNUSED int &numRegions,
                                       CPL_UNUSED int &numTexts,
                                       CPL_UNUSED GBool bForce /*= TRUE*/)
{
    /*-----------------------------------------------------------------
     * __TODO__  This should be implemented to return -1 if force=false,
     * or scan all the base tables if force=true
     *----------------------------------------------------------------*/

    return -1;
}

GIntBig TABSeamless::GetFeatureCount(int bForce)
{
    /*-----------------------------------------------------------------
     * __TODO__  This should be implemented to return -1 if force=false,
     * or scan all the base tables if force=true
     *----------------------------------------------------------------*/

    return OGRLayer::GetFeatureCount(bForce);
}

/**********************************************************************
 *                   TABSeamless::GetSpatialRef()
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
OGRSpatialReference *TABSeamless::GetSpatialRef()
{
    if (m_poIndexTable == nullptr)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "GetSpatialRef() failed: file has not been opened yet.");
        return nullptr;
    }

    return m_poIndexTable->GetSpatialRef();
}

/**********************************************************************
 *                   IMapInfoFile::SetSpatialFilter()
 *
 * Standard OGR SetSpatialFiltere implementation.  This method is used
 * to set a SpatialFilter for this OGRLayer.
 **********************************************************************/
void TABSeamless::SetSpatialFilter (OGRGeometry * poGeomIn )

{
    IMapInfoFile::SetSpatialFilter( poGeomIn );

    if( m_poIndexTable )
        m_poIndexTable->SetSpatialFilter( poGeomIn );

    if( m_poCurBaseTable )
        m_poCurBaseTable->SetSpatialFilter( poGeomIn );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int TABSeamless::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCSequentialWrite)
             || EQUAL(pszCap,OLCRandomWrite) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastGetExtent) )
        return TRUE;

    else if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return TestUtf8Capability();

    else
        return FALSE;
}

/**********************************************************************
 *                   TABSeamless::Dump()
 *
 * Dump block contents... available only in DEBUG mode.
 **********************************************************************/
#ifdef DEBUG

void TABSeamless::Dump(FILE *fpOut /*=NULL*/)
{
    if (fpOut == nullptr)
        fpOut = stdout;

    fprintf(fpOut, "----- TABSeamless::Dump() -----\n");

    if (m_poIndexTable == nullptr)
    {
        fprintf(fpOut, "File is not opened.\n");
    }
    else
    {
        fprintf(fpOut, "File is opened: %s\n", m_pszFname);
    }

    fflush(fpOut);
}

#endif // DEBUG
