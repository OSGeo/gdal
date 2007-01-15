/******************************************************************************
 * $Id$
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.14  2005/09/21 00:59:55  fwarmerdam
 * fixup OGRFeatureDefn and OGRSpatialReference refcount handling
 *
 * Revision 1.13  2003/07/08 15:23:58  warmerda
 * avoid casting warning
 *
 * Revision 1.12  2003/04/14 14:14:40  warmerda
 * fixed problem with Y origin for Landranger DTM
 *
 * Revision 1.11  2003/02/27 21:09:47  warmerda
 * Apply ZMult to LANDFORM_PROFILE_DTM data to scale to meters.
 * adfGeoTransform[5] is left positive to correct coordinates ... the first
 * entry in the first profile column is the *south* west corner, not the
 * north west corner.  See also: FME bug RT3887.
 * Note: adfGeoTransform[] change applied to LANDRANGER and LANDFORM products
 * but only check on LANDFORM products.
 *
 * Revision 1.10  2001/07/18 04:55:16  warmerda
 * added CPL_CSVID
 *
 * Revision 1.9  2001/01/19 20:31:12  warmerda
 * expand tabs
 *
 * Revision 1.8  1999/11/03 19:27:21  warmerda
 * Pass column in SetFPPos() as the feature id to ensure that the seek
 * isn't arbitrarily ignored.
 *
 * Revision 1.7  1999/11/03 19:07:58  warmerda
 * open raster file if not open when trying to read
 *
 * Revision 1.6  1999/10/04 18:19:22  warmerda
 * Avoid type conversion warnings.
 *
 * Revision 1.5  1999/10/04 13:38:39  warmerda
 * Fixed handling for filling holes in the column offset array.
 *
 * Revision 1.4  1999/10/04 13:28:43  warmerda
 * added DEM_SAMPLE support
 *
 * Revision 1.3  1999/10/04 12:52:22  warmerda
 * Added DTM_ as a prefix to raster layer names.
 *
 * Revision 1.2  1999/10/04 11:36:53  warmerda
 * Added HEIGHT attribute.
 *
 * Revision 1.1  1999/10/04 03:07:49  warmerda
 * New
 *
 */

#include "ntf.h"

CPL_CVSID("$Id$");

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
    NTFRecord   *poRecord;

    while( (poRecord = ReadRecord()) != NULL
           && poRecord->GetType() != NRT_GRIDHREC
           && poRecord->GetType() != NRT_VTR )
    {
        delete poRecord;
    }

    if( poRecord->GetType() != NRT_GRIDHREC )
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

    panColumnOffset = (long *) CPLCalloc(sizeof(long),nRasterXSize);

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
        int     iPrev;
        
        for( iPrev = 0; iPrev < iColumn-1; iPrev++ )
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
    NTFRecord   *poRecord;
    
    SetFPPos( panColumnOffset[iColumn], iColumn );
    poRecord = ReadRecord();

    if( iColumn < nRasterXSize-1 )
    {
        GetFPPos( panColumnOffset+iColumn+1, NULL );
    }
    
/* -------------------------------------------------------------------- */
/*      Handle LANDRANGER DTM columns.                                  */
/* -------------------------------------------------------------------- */
    if( pafElev != NULL && GetProductId() == NPC_LANDRANGER_DTM )
    {
        double  dfVScale, dfVOffset;

        dfVOffset = atoi(poRecord->GetField(56,65));
        dfVScale = atoi(poRecord->GetField(66,75)) * 0.001;

        for( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            pafElev[iPixel] = (float) (dfVOffset + dfVScale *
                atoi(poRecord->GetField(84+iPixel*4,87+iPixel*4)));
        }
    }
            
/* -------------------------------------------------------------------- */
/*      Handle PROFILE                                                  */
/* -------------------------------------------------------------------- */
    else if( pafElev != NULL && GetProductId() == NPC_LANDFORM_PROFILE_DTM )
    {
        for( int iPixel = 0; iPixel < nRasterXSize; iPixel++ )
        {
            pafElev[iPixel] = (float) 
           (atoi(poRecord->GetField(19+iPixel*5,23+iPixel*5)) * GetZMult());
        }
    }
    
    delete poRecord;

    return CE_None;
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
                                      NTFFileReader * poReaderIn )

{
    char        szLayerName[128];

    sprintf( szLayerName, "DTM_%s", poReaderIn->GetTileName() );
    poFeatureDefn = new OGRFeatureDefn( szLayerName );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbPoint25D );

    OGRFieldDefn      oHeight( "HEIGHT", OFTInteger );
    oHeight.SetWidth(5);
    poFeatureDefn->AddFieldDefn( &oHeight );

    poReader = poReaderIn;
    poDS = poDSIn;
    poFilterGeom = NULL;

    pafColumn = (float *) CPLCalloc(sizeof(float),
                                    poReader->GetRasterYSize());
    iColumnOffset = -1;
    iCurrentFC = 0;

/* -------------------------------------------------------------------- */
/*      Check for DEM subsampling, and compute total feature count      */
/*      accordingly.                                                    */
/* -------------------------------------------------------------------- */
    if( poDS->GetOption( "DEM_SAMPLE" ) == NULL )
        nDEMSample = 1;
    else
        nDEMSample = MAX(1,atoi(poDS->GetOption("DEM_SAMPLE")));
    
    nFeatureCount = (poReader->GetRasterXSize() / nDEMSample)
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
    iCurrentFC = 0;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRNTFRasterLayer::GetNextFeature()

{
    if( iCurrentFC == 0 )
        iCurrentFC = 1;
    else
    {
        int     iReqColumn, iReqRow;
        
        iReqColumn = (iCurrentFC - 1) / poReader->GetRasterYSize();
        iReqRow = iCurrentFC - iReqColumn * poReader->GetRasterXSize() - 1;

        if( iReqRow + nDEMSample > poReader->GetRasterYSize() )
        {
            iReqRow = 0;
            iReqColumn += nDEMSample;
        }
        else
        {
            iReqRow += nDEMSample;
        }

        iCurrentFC = iReqColumn * poReader->GetRasterYSize()
            + iReqRow + 1;
    }

    return GetFeature( (long) iCurrentFC );
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRNTFRasterLayer::GetFeature( long nFeatureId )

{
    int         iReqColumn, iReqRow;
    
/* -------------------------------------------------------------------- */
/*      Is this in the range of legal feature ids (pixels)?             */
/* -------------------------------------------------------------------- */
    if( nFeatureId < 1
        || nFeatureId > poReader->GetRasterXSize()*poReader->GetRasterYSize() )
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Do we need to load a different column.                          */
/* -------------------------------------------------------------------- */
    iReqColumn = (nFeatureId - 1) / poReader->GetRasterYSize();
    iReqRow = nFeatureId - iReqColumn * poReader->GetRasterXSize() - 1;
    
    if( iReqColumn != iColumnOffset )
    {
        iColumnOffset = iReqColumn;
        if( poReader->ReadRasterColumn( iReqColumn, pafColumn ) != CE_None )
            return NULL;
    }
    
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
    poFeature->SetField( 0, (int) pafColumn[iReqRow] );
    
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

int OGRNTFRasterLayer::GetFeatureCount( int bForce )

{
    return nFeatureCount;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRNTFRasterLayer::GetSpatialRef()

{
    return poDS->GetSpatialRef();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRNTFRasterLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCSequentialWrite) 
             || EQUAL(pszCap,OLCRandomWrite) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return FALSE;

    else 
        return FALSE;
}


