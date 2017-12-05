/******************************************************************************
 *
 * Project:  NTF Translator
 * Purpose:  Handle UK Ordnance Survey Raster DTM products.  Includes some
 *           raster related methods from NTFFileReader and the implementation
 *           of OGRNTFRasterLayer.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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

#include "ntf.h"

#include <algorithm>

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                     NTFFileReader Raster Methods                     */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          IsRasterProduct()                           */
/************************************************************************/

int NTFFileReader::IsRasterProduct()

{
    return GetProductId() == NPC_LANDRANGER_DTM
        || GetProductId() == NPC_LANDFORM_PROFILE_DTM;
}

/************************************************************************/
/*                       EstablishRasterAccess()                        */
/************************************************************************/

void NTFFileReader::EstablishRasterAccess()

{
/* -------------------------------------------------------------------- */
/*      Read the type 50 record.                                        */
/* -------------------------------------------------------------------- */
    NTFRecord *poRecord = NULL;

    while( (poRecord = ReadRecord()) != NULL
           && poRecord->GetType() != NRT_GRIDHREC
           && poRecord->GetType() != NRT_VTR )
    {
        delete poRecord;
    }

    if( poRecord == NULL ||
        poRecord->GetType() != NRT_GRIDHREC )
    {
        delete poRecord;
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unable to find GRIDHREC (type 50) record in what appears\n"
                  "to be an NTF Raster DTM product." );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Parse if LANDRANGER_DTM                                         */
/* -------------------------------------------------------------------- */
    if( GetProductId() == NPC_LANDRANGER_DTM )
    {
        nRasterXSize = atoi(poRecord->GetField(13,16));
        nRasterYSize = atoi(poRecord->GetField(17,20));

        // NOTE: unusual use of GeoTransform - the pixel origin is the
        // bottom left corner!
        adfGeoTransform[0] = atoi(poRecord->GetField(25,34));
        adfGeoTransform[1] = 50;
        adfGeoTransform[2] = 0;
        adfGeoTransform[3] = atoi(poRecord->GetField(35,44));
        adfGeoTransform[4] = 0;
        adfGeoTransform[5] = 50;

        nRasterDataType = 3; /* GDT_Int16 */
    }

/* -------------------------------------------------------------------- */
/*      Parse if LANDFORM_PROFILE_DTM                                   */
/* -------------------------------------------------------------------- */
    else if( GetProductId() == NPC_LANDFORM_PROFILE_DTM )
    {
        nRasterXSize = atoi(poRecord->GetField(23,30));
        nRasterYSize = atoi(poRecord->GetField(31,38));

        // NOTE: unusual use of GeoTransform - the pixel origin is the
        // bottom left corner!
        adfGeoTransform[0] = atoi(poRecord->GetField(13,17))
                           + GetXOrigin();
        adfGeoTransform[1] = atoi(poRecord->GetField(39,42));
        adfGeoTransform[2] = 0;
        adfGeoTransform[3] = atoi(poRecord->GetField(18,22))
                           + GetYOrigin();
        adfGeoTransform[4] = 0;
        adfGeoTransform[5] = atoi(poRecord->GetField(43,46));

        nRasterDataType = 3; /* GDT_Int16 */
    }

/* -------------------------------------------------------------------- */
/*      Initialize column offsets table.                                */
/* -------------------------------------------------------------------- */
    delete poRecord;

    if( !GDALCheckDatasetDimensions(nRasterXSize, nRasterYSize) )
        return;

    panColumnOffset = static_cast<vsi_l_offset *>(
        CPLCalloc(sizeof(vsi_l_offset), nRasterXSize));

    GetFPPos( panColumnOffset+0, NULL );

/* -------------------------------------------------------------------- */
/*      Create an OGRSFLayer for this file readers raster points.       */
/* -------------------------------------------------------------------- */
    if( poDS != NULL )
    {
        poRasterLayer = new OGRNTFRasterLayer( poDS, this );
        poDS->AddLayer( poRasterLayer );
    }
}

/************************************************************************/
/*                          ReadRasterColumn()                          */
/************************************************************************/

CPLErr NTFFileReader::ReadRasterColumn( int iColumn, float *pafElev )

{
/* -------------------------------------------------------------------- */
/*      If we don't already have the scanline offset of the previous    */
/*      line, force reading of previous records to establish it.        */
/* -------------------------------------------------------------------- */
    if( panColumnOffset[iColumn] == 0 )
    {
        for( int iPrev = 0; iPrev < iColumn-1; iPrev++ )
        {
            if( panColumnOffset[iPrev+1] == 0 )
            {
                CPLErr  eErr;

                eErr = ReadRasterColumn( iPrev, NULL );
                if( eErr != CE_None )
                    return eErr;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      If the dataset isn't open, open it now.                         */
/* -------------------------------------------------------------------- */
    if( GetFP() == NULL )
        Open();

/* -------------------------------------------------------------------- */
/*      Read requested record.                                          */
/* -------------------------------------------------------------------- */
    SetFPPos( panColumnOffset[iColumn], iColumn );
    NTFRecord *poRecord = ReadRecord();
    if( poRecord == NULL )
        return CE_Failure;

    CPLErr eErr = CE_None;
    if( iColumn < nRasterXSize-1 )
    {
        GetFPPos( panColumnOffset+iColumn+1, NULL );
    }

/* -------------------------------------------------------------------- */
/*      Handle LANDRANGER DTM columns.                                  */
/* -------------------------------------------------------------------- */
    if( pafElev != NULL && GetProductId() == NPC_LANDRANGER_DTM )
    {
        const double dfVOffset = atoi(poRecord->GetField(56,65));
        const double dfVScale = atoi(poRecord->GetField(66,75)) * 0.001;

        for( int iPixel = 0; iPixel < nRasterYSize; iPixel++ )
        {
            const char* pszValue = poRecord->GetField(84+iPixel*4,87+iPixel*4);
            if( pszValue[0] == '\0' || pszValue[0] == ' ' )
            {
                eErr = CE_Failure;
                break;
            }
            pafElev[iPixel] = (float) (dfVOffset + dfVScale *
                atoi(pszValue));
        }
    }

/* -------------------------------------------------------------------- */
/*      Handle PROFILE                                                  */
/* -------------------------------------------------------------------- */
    else if( pafElev != NULL && GetProductId() == NPC_LANDFORM_PROFILE_DTM )
    {
        for( int iPixel = 0; iPixel < nRasterYSize; iPixel++ )
        {
            const char* pszValue = poRecord->GetField(19+iPixel*5,23+iPixel*5);
            if( pszValue[0] == '\0' || pszValue[0] == ' ' )
            {
                eErr = CE_Failure;
                break;
            }
            pafElev[iPixel] = (float)(atoi(pszValue) * GetZMult());
        }
    }

    delete poRecord;

    return eErr;
}

/************************************************************************/
/* ==================================================================== */
/*                        OGRNTFRasterLayer                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          OGRNTFRasterLayer                           */
/************************************************************************/

OGRNTFRasterLayer::OGRNTFRasterLayer( OGRNTFDataSource *poDSIn,
                                      NTFFileReader * poReaderIn ) :
    poFeatureDefn(NULL),
    poFilterGeom(NULL),
    poReader(poReaderIn),
    pafColumn(static_cast<float *>(
        CPLCalloc(sizeof(float), poReaderIn->GetRasterYSize()))),
    iColumnOffset(-1),
    iCurrentFC(1),
    // Check for DEM subsampling.
    nDEMSample(poDSIn->GetOption( "DEM_SAMPLE" ) == NULL ?
               1 : std::max(1, atoi(poDSIn->GetOption("DEM_SAMPLE")))),
    nFeatureCount(0)
{
    char szLayerName[128];
    snprintf( szLayerName, sizeof(szLayerName),
              "DTM_%s", poReaderIn->GetTileName() );
    poFeatureDefn = new OGRFeatureDefn( szLayerName );

    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbPoint25D );
    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poDSIn->GetSpatialRef());

    OGRFieldDefn oHeight( "HEIGHT", OFTReal );
    poFeatureDefn->AddFieldDefn( &oHeight );

    nFeatureCount = static_cast<GIntBig>(poReader->GetRasterXSize() / nDEMSample)
                  * (poReader->GetRasterYSize() / nDEMSample);
}

/************************************************************************/
/*                         ~OGRNTFRasterLayer()                         */
/************************************************************************/

OGRNTFRasterLayer::~OGRNTFRasterLayer()

{
    CPLFree( pafColumn );
    if( poFeatureDefn )
        poFeatureDefn->Release();

    if( poFilterGeom != NULL )
        delete poFilterGeom;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRNTFRasterLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( poFilterGeom != NULL )
    {
        delete poFilterGeom;
        poFilterGeom = NULL;
    }

    if( poGeomIn != NULL )
        poFilterGeom = poGeomIn->clone();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRNTFRasterLayer::ResetReading()

{
    iCurrentFC = 1;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRNTFRasterLayer::GetNextFeature()

{
    if( iCurrentFC > static_cast<GIntBig>(poReader->GetRasterXSize())*
                                          poReader->GetRasterYSize() )
    {
        return NULL;
    }

    OGRFeature* poFeature = GetFeature( iCurrentFC );

    int     iReqColumn, iReqRow;

    iReqColumn = static_cast<int>((iCurrentFC - 1) / poReader->GetRasterYSize());
    iReqRow = static_cast<int>(iCurrentFC - iReqColumn * poReader->GetRasterYSize() - 1);

    if( iReqRow + nDEMSample > poReader->GetRasterYSize() )
    {
        iReqRow = 0;
        iReqColumn += nDEMSample;
    }
    else
    {
        iReqRow += nDEMSample;
    }

    iCurrentFC = static_cast<GIntBig>(iReqColumn) * poReader->GetRasterYSize()
        + iReqRow + 1;

    return poFeature;
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRNTFRasterLayer::GetFeature( GIntBig nFeatureId )

{
    int         iReqColumn, iReqRow;

/* -------------------------------------------------------------------- */
/*      Is this in the range of legal feature ids (pixels)?             */
/* -------------------------------------------------------------------- */
    if( nFeatureId < 1
        || nFeatureId > static_cast<GIntBig>(poReader->GetRasterXSize())*poReader->GetRasterYSize() )
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Do we need to load a different column.                          */
/* -------------------------------------------------------------------- */
    iReqColumn = static_cast<int>((nFeatureId - 1) / poReader->GetRasterYSize());
    iReqRow = static_cast<int>(nFeatureId - iReqColumn * poReader->GetRasterYSize() - 1);

    if( iReqColumn != iColumnOffset )
    {
        iColumnOffset = iReqColumn;
        if( poReader->ReadRasterColumn( iReqColumn, pafColumn ) != CE_None )
            return NULL;
    }
    if( iReqRow < 0 || iReqRow >= poReader->GetRasterYSize() )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding feature.                                 */
/* -------------------------------------------------------------------- */
    OGRFeature  *poFeature = new OGRFeature( poFeatureDefn );
    double      *padfGeoTransform = poReader->GetGeoTransform();

    poFeature->SetFID( nFeatureId );

    // NOTE: unusual use of GeoTransform - the pixel origin is the
    // bottom left corner!
    poFeature->SetGeometryDirectly(
        new OGRPoint( padfGeoTransform[0] + padfGeoTransform[1] * iReqColumn,
                      padfGeoTransform[3] + padfGeoTransform[5] * iReqRow,
                      pafColumn[iReqRow] ) );
    poFeature->SetField( 0, pafColumn[iReqRow] );

    return poFeature;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/*                                                                      */
/*      If a spatial filter is in effect, we turn control over to       */
/*      the generic counter.  Otherwise we return the total count.      */
/*      Eventually we should consider implementing a more efficient     */
/*      way of counting features matching a spatial query.              */
/************************************************************************/

GIntBig OGRNTFRasterLayer::GetFeatureCount( CPL_UNUSED int bForce )
{
    return nFeatureCount;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRNTFRasterLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return TRUE;

    return FALSE;
}
