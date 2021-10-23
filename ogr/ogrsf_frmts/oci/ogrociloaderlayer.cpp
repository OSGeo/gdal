/******************************************************************************
 *
 * Project:  Oracle Spatial Driver
 * Purpose:  Implementation of the OGROCILoaderLayer class.  This implements
 *           an output only OGRLayer for writing an SQL*Loader file.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "ogr_oci.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGROCILoaderLayer()                          */
/************************************************************************/

OGROCILoaderLayer::OGROCILoaderLayer( OGROCIDataSource *poDSIn,
                                      const char * pszTableName,
                                      const char * pszGeomColIn,
                                      int nSRIDIn,
                                      const char *pszLoaderFilenameIn )

{
    poDS = poDSIn;

    iNextFIDToWrite = 1;

    bTruncationReported = FALSE;
    bHeaderWritten = FALSE;
    nLDRMode = LDRM_UNKNOWN;

    poFeatureDefn = new OGRFeatureDefn( pszTableName );
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();

    pszGeomName = CPLStrdup( pszGeomColIn );
    pszFIDName = (char*)CPLGetConfigOption( "OCI_FID", "OGR_FID" );

    nSRID = nSRIDIn;
    poSRS = poDSIn->FetchSRS( nSRID );

    if( poSRS != nullptr )
        poSRS->Reference();

/* -------------------------------------------------------------------- */
/*      Open the loader file.                                           */
/* -------------------------------------------------------------------- */
    pszLoaderFilename = CPLStrdup( pszLoaderFilenameIn );

    fpData = nullptr;
    fpLoader = VSIFOpen( pszLoaderFilename, "wt" );
    if( fpLoader == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to open SQL*Loader control file:%s",
                  pszLoaderFilename );
        return;
    }
}

/************************************************************************/
/*                         ~OGROCILoaderLayer()                         */
/************************************************************************/

OGROCILoaderLayer::~OGROCILoaderLayer()

{
    if( fpData != nullptr )
        VSIFClose( fpData );

    if( fpLoader != nullptr )
    {
        VSIFClose( fpLoader );
        FinalizeNewLayer();
    }

    CPLFree( pszLoaderFilename );

    if( poSRS != nullptr && poSRS->Dereference() == 0 )
        delete poSRS;
}

/************************************************************************/
/*                         WriteLoaderHeader()                          */
/************************************************************************/

void OGROCILoaderLayer::WriteLoaderHeader()

{
    if( bHeaderWritten )
        return;

/* -------------------------------------------------------------------- */
/*      Determine name of geometry column to use.                       */
/* -------------------------------------------------------------------- */
    const char *pszGeometryName =
        CSLFetchNameValue( papszOptions, "GEOMETRY_NAME" );
    if( pszGeometryName == nullptr )
        pszGeometryName = "ORA_GEOMETRY";

/* -------------------------------------------------------------------- */
/*      Determine our operation mode.                                   */
/* -------------------------------------------------------------------- */
    const char *pszLDRMode = CSLFetchNameValue( papszOptions, "LOADER_MODE" );

    if( pszLDRMode != nullptr && EQUAL(pszLDRMode,"VARIABLE") )
        nLDRMode = LDRM_VARIABLE;
    else if( pszLDRMode != nullptr && EQUAL(pszLDRMode,"BINARY") )
        nLDRMode = LDRM_BINARY;
    else
        nLDRMode = LDRM_STREAM;

/* -------------------------------------------------------------------- */
/*      Write loader header info.                                       */
/* -------------------------------------------------------------------- */
    VSIFPrintf( fpLoader, "LOAD DATA\n" );
    if( nLDRMode == LDRM_STREAM )
    {
        VSIFPrintf( fpLoader, "INFILE *\n" );
        VSIFPrintf( fpLoader, "CONTINUEIF NEXT(1:1) = '#'\n" );
    }
    else if( nLDRMode == LDRM_VARIABLE )
    {
        const char *pszDataFilename = CPLResetExtension( pszLoaderFilename,
                                                         "dat" );
        fpData = VSIFOpen( pszDataFilename, "wb" );
        if( fpData == nullptr )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Unable to open data output file `%s'.",
                      pszDataFilename );
            return;
        }

        VSIFPrintf( fpLoader, "INFILE %s \"var 8\"\n", pszDataFilename );
    }
    const char *pszExpectedFIDName =
        CPLGetConfigOption( "OCI_FID", "OGR_FID" );

    VSIFPrintf( fpLoader, "INTO TABLE \"%s\" REPLACE\n",
                poFeatureDefn->GetName() );
    VSIFPrintf( fpLoader, "FIELDS TERMINATED BY '|'\n" );
    VSIFPrintf( fpLoader, "TRAILING NULLCOLS (\n" );
    VSIFPrintf( fpLoader, "    %s INTEGER EXTERNAL,\n", pszExpectedFIDName );
    VSIFPrintf( fpLoader, "    %s COLUMN OBJECT (\n",
                pszGeometryName );
    VSIFPrintf( fpLoader, "      SDO_GTYPE INTEGER EXTERNAL,\n" );
    VSIFPrintf( fpLoader, "      SDO_ELEM_INFO VARRAY TERMINATED BY '|/'\n" );
    VSIFPrintf( fpLoader, "        (elements INTEGER EXTERNAL),\n" );
    VSIFPrintf( fpLoader, "      SDO_ORDINATES VARRAY TERMINATED BY '|/'\n" );
    VSIFPrintf( fpLoader, "        (ordinates FLOAT EXTERNAL)\n" );
    VSIFPrintf( fpLoader, "    ),\n" );

/* -------------------------------------------------------------------- */
/*      Write user field schema.                                        */
/* -------------------------------------------------------------------- */
    int iField;

    for( iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        OGRFieldDefn *poFldDefn = poFeatureDefn->GetFieldDefn(iField);

        if( poFldDefn->GetType() == OFTInteger )
        {
            VSIFPrintf( fpLoader, "    \"%s\" INTEGER EXTERNAL",
                        poFldDefn->GetNameRef() );
        }
        else if( poFldDefn->GetType() == OFTInteger64 )
        {
            VSIFPrintf( fpLoader, "    \"%s\" LONGINTEGER EXTERNAL",
                        poFldDefn->GetNameRef() );
        }
        else if( poFldDefn->GetType() == OFTReal )
        {
            VSIFPrintf( fpLoader, "    \"%s\" FLOAT EXTERNAL",
                        poFldDefn->GetNameRef() );
        }
        else /* if( poFldDefn->GetType() == OFTString ) or default case */
        {
            VSIFPrintf( fpLoader, "    \"%s\" VARCHARC(4)",
                        poFldDefn->GetNameRef() );
        }

        if( iField < poFeatureDefn->GetFieldCount() - 1 )
            VSIFPrintf( fpLoader, "," );
        VSIFPrintf( fpLoader, "\n" );
    }

    VSIFPrintf( fpLoader, ")\n" );

    if( nLDRMode == LDRM_STREAM )
        VSIFPrintf( fpLoader, "begindata\n" );

    bHeaderWritten = TRUE;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/*                                                                      */
/*      We override the next feature method because we know that we     */
/*      implement the attribute query within the statement and so we    */
/*      don't have to test here.   Eventually the spatial query will    */
/*      be fully tested within the statement as well.                   */
/************************************************************************/

OGRFeature *OGROCILoaderLayer::GetNextFeature()

{
    CPLError( CE_Failure, CPLE_NotSupported,
              "GetNextFeature() not supported for an OGROCILoaderLayer." );
    return nullptr;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGROCILoaderLayer::ResetReading()

{
    OGROCILayer::ResetReading();
}

/************************************************************************/
/*                       WriteFeatureStreamMode()                       */
/************************************************************************/

OGRErr OGROCILoaderLayer::WriteFeatureStreamMode( OGRFeature *poFeature )

{
/* -------------------------------------------------------------------- */
/*      Write the FID.                                                  */
/* -------------------------------------------------------------------- */
    VSIFPrintf( fpLoader, " " CPL_FRMT_GIB "|", poFeature->GetFID() );

/* -------------------------------------------------------------------- */
/*      Set the geometry                                                */
/* -------------------------------------------------------------------- */
    int  nLineLen = 0;
    if( poFeature->GetGeometryRef() != nullptr)
    {
        char szSRID[128];
        int  nGType;
        int  i;

        if( nSRID == -1 )
            strcpy( szSRID, "NULL" );
        else
            snprintf( szSRID, sizeof(szSRID), "%d", nSRID );

        if( TranslateToSDOGeometry( poFeature->GetGeometryRef(), &nGType )
            == OGRERR_NONE )
        {
            VSIFPrintf( fpLoader, "%d|", nGType );
            for( i = 0; i < nElemInfoCount; i++ )
            {
                VSIFPrintf( fpLoader, "%d|", panElemInfo[i] );
                if( ++nLineLen > 18 && i < nElemInfoCount-1 )
                {
                    VSIFPrintf( fpLoader, "\n#" );
                    nLineLen = 0;
                }
            }
            VSIFPrintf( fpLoader, "/" );

            for( i = 0; i < nOrdinalCount; i++ )
            {
                VSIFPrintf( fpLoader, "%.16g|", padfOrdinals[i] );
                if( ++nLineLen > 6 && i < nOrdinalCount-1 )
                {
                    VSIFPrintf( fpLoader, "\n#" );
                    nLineLen = 0;
                }
            }
            VSIFPrintf( fpLoader, "/" );
        }
        else
        {
            VSIFPrintf( fpLoader, "0|/|/" );
        }
    }
    else
    {
        VSIFPrintf( fpLoader, "0|/|/" );
    }

/* -------------------------------------------------------------------- */
/*      Set the other fields.                                           */
/* -------------------------------------------------------------------- */
    int i;

    nLineLen = 0;
    VSIFPrintf( fpLoader, "\n#" );

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        OGRFieldDefn *poFldDefn = poFeatureDefn->GetFieldDefn(i);

        if( !poFeature->IsFieldSetAndNotNull( i ) )
        {
            if( poFldDefn->GetType() != OFTInteger
                && poFldDefn->GetType() != OFTInteger64
                && poFldDefn->GetType() != OFTReal )
                VSIFPrintf( fpLoader, "%04d", 0 );
            continue;
        }

        const char *pszStrValue = poFeature->GetFieldAsString(i);

        if( nLineLen > 70 )
        {
            VSIFPrintf( fpLoader, "\n#" );
            nLineLen = 0;
        }

        nLineLen += static_cast<int>(strlen(pszStrValue));

        if( poFldDefn->GetType() == OFTInteger
            || poFldDefn->GetType() == OFTInteger64
            || poFldDefn->GetType() == OFTReal )
        {
            if( poFldDefn->GetWidth() > 0 && bPreservePrecision
                && (int) strlen(pszStrValue) > poFldDefn->GetWidth() )
            {
                ReportTruncation( poFldDefn );
                VSIFPrintf( fpLoader, "|" );
            }
            else
                VSIFPrintf( fpLoader, "%s|", pszStrValue );
        }
        else
        {
            int nLength = static_cast<int>(strlen(pszStrValue));

            if( poFldDefn->GetWidth() > 0 && nLength > poFldDefn->GetWidth() )
            {
                ReportTruncation( poFldDefn );
                nLength = poFldDefn->GetWidth();
            }

            VSIFPrintf( fpLoader, "%04d", nLength );
            VSIFWrite( (void *) pszStrValue, 1, nLength, fpLoader );
        }
    }

    if( VSIFPrintf( fpLoader, "\n" ) == 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Write to loader file failed, likely out of disk space." );
        return OGRERR_FAILURE;
    }
    else
        return OGRERR_NONE;
}

/************************************************************************/
/*                      WriteFeatureVariableMode()                      */
/************************************************************************/

OGRErr OGROCILoaderLayer::WriteFeatureVariableMode( OGRFeature *poFeature )

{
    OGROCIStringBuf oLine;

    if( fpData == nullptr )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Write the FID.                                                  */
/* -------------------------------------------------------------------- */
    oLine.Append( "00000000" );
    oLine.Appendf( 32, " " CPL_FRMT_GIB "|", poFeature->GetFID() );

/* -------------------------------------------------------------------- */
/*      Set the geometry                                                */
/* -------------------------------------------------------------------- */
    if( poFeature->GetGeometryRef() != nullptr)
    {
        char szSRID[128];
        int  nGType;
        int  i;

        if( nSRID == -1 )
            strcpy( szSRID, "NULL" );
        else
            snprintf( szSRID, sizeof(szSRID), "%d", nSRID );

        if( TranslateToSDOGeometry( poFeature->GetGeometryRef(), &nGType )
            == OGRERR_NONE )
        {
            oLine.Appendf( 32, "%d|", nGType );
            for( i = 0; i < nElemInfoCount; i++ )
            {
                oLine.Appendf( 32, "%d|", panElemInfo[i] );
            }
            oLine.Append( "/" );

            for( i = 0; i < nOrdinalCount; i++ )
            {
                oLine.Appendf( 32, "%.16g|", padfOrdinals[i] );
            }
            oLine.Append( "/" );
        }
        else
        {
            oLine.Append( "0|/|/" );
        }
    }
    else
    {
        oLine.Append( "0|/|/" );
    }

/* -------------------------------------------------------------------- */
/*      Set the other fields.                                           */
/* -------------------------------------------------------------------- */
    int i;

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        OGRFieldDefn *poFldDefn = poFeatureDefn->GetFieldDefn(i);

        if( !poFeature->IsFieldSetAndNotNull( i ) )
        {
            if( poFldDefn->GetType() != OFTInteger
                && poFldDefn->GetType() != OFTInteger64
                && poFldDefn->GetType() != OFTReal )
                oLine.Append( "0000" );
            else
                oLine.Append( "|" );
            continue;
        }

        const char *pszStrValue = poFeature->GetFieldAsString(i);

        if( poFldDefn->GetType() == OFTInteger
            || poFldDefn->GetType() == OFTInteger64
            || poFldDefn->GetType() == OFTReal )
        {
            if( poFldDefn->GetWidth() > 0 && bPreservePrecision
                && (int) strlen(pszStrValue) > poFldDefn->GetWidth() )
            {
                ReportTruncation( poFldDefn );
                oLine.Append( "|" );
            }
            else
            {
                oLine.Append( pszStrValue );
                oLine.Append( "|" );
            }
        }
        else
        {
            int nLength = static_cast<int>(strlen(pszStrValue));

            if( poFldDefn->GetWidth() > 0 && nLength > poFldDefn->GetWidth() )
            {
                ReportTruncation( poFldDefn );
                nLength = poFldDefn->GetWidth();
                ((char *) pszStrValue)[nLength] = '\0';
            }

            oLine.Appendf( 5, "%04d", nLength );
            oLine.Append( pszStrValue );
        }
    }

    oLine.Appendf( 3, "\n" );

/* -------------------------------------------------------------------- */
/*      Update the line's length, and write to disk.                    */
/* -------------------------------------------------------------------- */
    char szLength[9] = {};
    size_t  nStringLen = strlen(oLine.GetString());

    snprintf( szLength, sizeof(szLength), "%08d", (int) (nStringLen-8) );
    memcpy( oLine.GetString(), szLength, 8 );

    if( VSIFWrite( oLine.GetString(), 1, nStringLen, fpData ) != nStringLen )
    {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Write to loader file failed, likely out of disk space." );
        return OGRERR_FAILURE;
    }
    else
        return OGRERR_NONE;
}

/************************************************************************/
/*                       WriteFeatureBinaryMode()                       */
/************************************************************************/

OGRErr OGROCILoaderLayer::WriteFeatureBinaryMode( OGRFeature * /*poFeature*/ )

{
    return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGROCILoaderLayer::ICreateFeature( OGRFeature *poFeature )

{
    WriteLoaderHeader();

/* -------------------------------------------------------------------- */
/*      Set the FID.                                                    */
/* -------------------------------------------------------------------- */
    if( poFeature->GetFID() == OGRNullFID )
        poFeature->SetFID( iNextFIDToWrite++ );

/* -------------------------------------------------------------------- */
/*      Add extents of this geometry to the existing layer extents.     */
/* -------------------------------------------------------------------- */
    if( poFeature->GetGeometryRef() != nullptr )
    {
        OGREnvelope  sThisExtent;

        poFeature->GetGeometryRef()->getEnvelope( &sThisExtent );
        sExtent.Merge( sThisExtent );
    }

/* -------------------------------------------------------------------- */
/*      Call the mode specific write function.                          */
/* -------------------------------------------------------------------- */
    if( nLDRMode == LDRM_STREAM )
        return WriteFeatureStreamMode( poFeature );
    else if( nLDRMode == LDRM_VARIABLE )
        return WriteFeatureVariableMode( poFeature );
    else if( nLDRMode == LDRM_BINARY )
        return WriteFeatureBinaryMode( poFeature );
    else
        return OGRERR_UNSUPPORTED_OPERATION;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGROCILoaderLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCSequentialWrite) )
        return TRUE;

    else if( EQUAL(pszCap,OLCCreateField) )
        return TRUE;

    else
        return OGROCILayer::TestCapability( pszCap );
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

GIntBig OGROCILoaderLayer::GetFeatureCount( int /* bForce */ )

{
    return iNextFIDToWrite - 1;
}

/************************************************************************/
/*                          FinalizeNewLayer()                          */
/*                                                                      */
/*      Our main job here is to update the USER_SDO_GEOM_METADATA       */
/*      table to include the correct array of dimension object with     */
/*      the appropriate extents for this layer.  We may also do         */
/*      spatial indexing at this point.                                 */
/************************************************************************/

void OGROCILoaderLayer::FinalizeNewLayer()

{
    OGROCIStringBuf  sDimUpdate;

/* -------------------------------------------------------------------- */
/*      If the dimensions are degenerate (all zeros) then we assume     */
/*      there were no geometries, and we don't bother setting the       */
/*      dimensions.                                                     */
/* -------------------------------------------------------------------- */
    if( sExtent.MaxX == 0 && sExtent.MinX == 0
        && sExtent.MaxY == 0 && sExtent.MinY == 0 )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Layer %s appears to have no geometry ... not setting SDO DIMINFO metadata.",
                  poFeatureDefn->GetName() );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Establish the extents and resolution to use.                    */
/* -------------------------------------------------------------------- */
    double           dfResSize;
    double           dfXMin, dfXMax, dfXRes;
    double           dfYMin, dfYMax, dfYRes;
    double           dfZMin, dfZMax, dfZRes;

    if( sExtent.MaxX - sExtent.MinX > 400 )
        dfResSize = 0.001;
    else
        dfResSize = 0.0000001;

    dfXMin = sExtent.MinX - dfResSize * 3;
    dfXMax = sExtent.MaxX + dfResSize * 3;
    dfXRes = dfResSize;
    ParseDIMINFO( "DIMINFO_X", &dfXMin, &dfXMax, &dfXRes );

    dfYMin = sExtent.MinY - dfResSize * 3;
    dfYMax = sExtent.MaxY + dfResSize * 3;
    dfYRes = dfResSize;
    ParseDIMINFO( "DIMINFO_Y", &dfYMin, &dfYMax, &dfYRes );

    dfZMin = -100000.0;
    dfZMax = 100000.0;
    dfZRes = 0.002;
    ParseDIMINFO( "DIMINFO_Z", &dfZMin, &dfZMax, &dfZRes );

/* -------------------------------------------------------------------- */
/*      Prepare dimension update statement.                             */
/* -------------------------------------------------------------------- */
    sDimUpdate.Append( "UPDATE USER_SDO_GEOM_METADATA SET DIMINFO = " );
    sDimUpdate.Append( "MDSYS.SDO_DIM_ARRAY(" );

    sDimUpdate.Appendf(200,
                       "MDSYS.SDO_DIM_ELEMENT('X',%.16g,%.16g,%.12g)",
                       dfXMin, dfXMax, dfXRes );
    sDimUpdate.Appendf(200,
                       ",MDSYS.SDO_DIM_ELEMENT('Y',%.16g,%.16g,%.12g)",
                       dfYMin, dfYMax, dfYRes );

    if( nDimension == 3 )
    {
        sDimUpdate.Appendf(200,
                           ",MDSYS.SDO_DIM_ELEMENT('Z',%.16g,%.16g,%.12g)",
                           dfZMin, dfZMax, dfZRes );
    }

    sDimUpdate.Append( ")" );

    sDimUpdate.Appendf( static_cast<int>(strlen(poFeatureDefn->GetName()) + 100),
                        " WHERE table_name = UPPER('%s')",
                        poFeatureDefn->GetName() );

/* -------------------------------------------------------------------- */
/*      Execute the metadata update.                                    */
/* -------------------------------------------------------------------- */
    OGROCIStatement oExecStatement( poDS->GetSession() );

    if( oExecStatement.Execute( sDimUpdate.GetString() ) != CE_None )
        return;
}
