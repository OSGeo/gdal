/******************************************************************************
 * $Id$
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2003/04/04 06:17:47  warmerda
 * New
 *
 */

#include "ogr_oci.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGROCILoaderLayer()                          */
/************************************************************************/

OGROCILoaderLayer::OGROCILoaderLayer( OGROCIDataSource *poDSIn, 
                                      const char * pszTableName,
                                      const char * pszGeomColIn,
                                      int nSRIDIn, 
                                      const char *pszLoaderFile )

{
    poDS = poDSIn;

    iNextFIDToWrite = 1;

    bTruncationReported = FALSE;
    bHeaderWritten = FALSE;

    poFeatureDefn = new OGRFeatureDefn( pszTableName );
    poFeatureDefn->Reference();
    
    pszGeomName = CPLStrdup( pszGeomColIn );
    pszFIDName = CPLStrdup( "OGR_FID" );

    nSRID = nSRIDIn;
    poSRS = poDSIn->FetchSRS( nSRID );

    if( poSRS != NULL )
        poSRS->Reference();

/* -------------------------------------------------------------------- */
/*      Open the loader file.                                           */
/* -------------------------------------------------------------------- */
    fpLoader = VSIFOpen( pszLoaderFile, "wt" );
    if( fpLoader == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to open SQL*Loader control file:%s", 
                  pszLoaderFile );
        return;
    }

}

/************************************************************************/
/*                         ~OGROCILoaderLayer()                         */
/************************************************************************/

OGROCILoaderLayer::~OGROCILoaderLayer()

{
    if( fpLoader != NULL )
    {
        VSIFClose( fpLoader );
        FinalizeNewLayer();
    }

    if( poSRS != NULL && poSRS->Dereference() == 0 )
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
/*      Write loader header info.                                       */
/* -------------------------------------------------------------------- */
    VSIFPrintf( fpLoader, "LOAD DATA\n" );
    VSIFPrintf( fpLoader, "INFILE *\n" );
    VSIFPrintf( fpLoader, "TRUNCATE\n" );
    VSIFPrintf( fpLoader, "CONTINUEIF NEXT(1:1) = '#'\n" );
    VSIFPrintf( fpLoader, "INTO TABLE \"%s\"\n", 
             poFeatureDefn->GetName() );
    VSIFPrintf( fpLoader, "FIELDS TERMINATED BY '|'\n" );
    VSIFPrintf( fpLoader, "TRAILING NULLCOLS (\n" );
    VSIFPrintf( fpLoader, "    ogr_fid INTEGER EXTERNAL,\n" );
    VSIFPrintf( fpLoader, "    ora_geometry COLUMN OBJECT (\n" );
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
        else if( poFldDefn->GetType() == OFTReal )
        {
            VSIFPrintf( fpLoader, "    \"%s\" FLOAT EXTERNAL", 
                        poFldDefn->GetNameRef() );
        }
        else if( poFldDefn->GetType() == OFTString )
        {
            VSIFPrintf( fpLoader, "    \"%s\" CHAR", 
                        poFldDefn->GetNameRef() );
        }
        else
        {
            VSIFPrintf( fpLoader, "    \"%s\" CHAR", 
                        poFldDefn->GetNameRef() );
        }

        if( iField < poFeatureDefn->GetFieldCount() - 1 )
            VSIFPrintf( fpLoader, "," );
        VSIFPrintf( fpLoader, "\n" );
    }

    VSIFPrintf( fpLoader, ")\n" );
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
    return NULL;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGROCILoaderLayer::ResetReading()

{
    OGROCILayer::ResetReading();
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGROCILoaderLayer::CreateFeature( OGRFeature *poFeature )

{
    WriteLoaderHeader();

/* -------------------------------------------------------------------- */
/*      Add extents of this geometry to the existing layer extents.     */
/* -------------------------------------------------------------------- */
    if( poFeature->GetGeometryRef() != NULL )
    {
        OGREnvelope  sThisExtent;
        
        poFeature->GetGeometryRef()->getEnvelope( &sThisExtent );
        sExtent.Merge( sThisExtent );
    }

/* -------------------------------------------------------------------- */
/*      Set the FID.                                                    */
/* -------------------------------------------------------------------- */
    int nFID = poFeature->GetFID();

    if( nFID == -1 )
        nFID = iNextFIDToWrite++;

    VSIFPrintf( fpLoader, "%d|", nFID );

/* -------------------------------------------------------------------- */
/*      Set the geometry                                                */
/* -------------------------------------------------------------------- */
    if( poFeature->GetGeometryRef() != NULL)
    {
        char szSRID[128];
        int  nGType;
        int  i;

        if( nSRID == -1 )
            strcpy( szSRID, "NULL" );
        else
            sprintf( szSRID, "%d", nSRID );

        if( TranslateToSDOGeometry( poFeature->GetGeometryRef(), &nGType )
            == OGRERR_NONE )
        {
            VSIFPrintf( fpLoader, "%d|", nGType );
            for( i = 0; i < nElemInfoCount; i++ )
                VSIFPrintf( fpLoader, "%d|", panElemInfo[i] );
            VSIFPrintf( fpLoader, "/" );

            for( i = 0; i < nOrdinalCount; i++ )
            {
                VSIFPrintf( fpLoader, "%.16g|", padfOrdinals[i] );
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

    for( i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( i > 0 )
            VSIFPrintf( fpLoader, "|" );

        if( !poFeature->IsFieldSet( i ) )
        {
            continue;
        }

        OGRFieldDefn *poFldDefn = poFeatureDefn->GetFieldDefn(i);
        const char *pszStrValue = poFeature->GetFieldAsString(i);

        if( poFldDefn->GetType() == OFTInteger 
            || poFldDefn->GetType() == OFTReal )
        {
            if( poFldDefn->GetWidth() > 0 && bPreservePrecision
                && (int) strlen(pszStrValue) > poFldDefn->GetWidth() )
            {
                ReportTruncation( poFldDefn );
            }
            else
                VSIFPrintf( fpLoader, "%s", pszStrValue );
        }
        else 
        {
            int		iChar;

            if( strstr(pszStrValue,"'") == NULL 
                && (poFldDefn->GetWidth() <= 0 
                    || (int) strlen(pszStrValue) < poFldDefn->GetWidth()) )
                VSIFPrintf( fpLoader, "'%s'", pszStrValue );
            else
            {
                VSIFPrintf( fpLoader, "'" );
                for( iChar = 0; pszStrValue[iChar] != '\0'; iChar++ )
                {
                    if( poFldDefn->GetWidth() != 0 && bPreservePrecision
                    && iChar >= poFldDefn->GetWidth() )
                    {
                        ReportTruncation( poFldDefn );
                        break;
                    }
                    
                    if( pszStrValue[iChar] == '\'' )
                    {
                        VSIFPrintf( fpLoader, "\\\'" );
                    }
                    else
                        VSIFPrintf( fpLoader, "%c", pszStrValue[iChar] );
                }
                VSIFPrintf( fpLoader, "'" );
            }
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

int OGROCILoaderLayer::GetFeatureCount( int bForce )

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

    sDimUpdate.Appendf( strlen(poFeatureDefn->GetName()) + 100,
                        " WHERE table_name = '%s'", 
                        poFeatureDefn->GetName() );

/* -------------------------------------------------------------------- */
/*      Execute the metadata update.                                    */
/* -------------------------------------------------------------------- */
    OGROCIStatement oExecStatement( poDS->GetSession() );

    if( oExecStatement.Execute( sDimUpdate.GetString() ) != CE_None )
        return;
}
