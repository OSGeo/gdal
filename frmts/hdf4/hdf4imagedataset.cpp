/******************************************************************************
 * $Id$
 *
 * Project:  Hierarchical Data Format Release 4 (HDF4)
 * Purpose:  Read subdatasets of HDF4 file.
 *           This driver initially based on code supplied by Markus Neteler
 * Author:   Andrey Kiselev, dron@remotesensing.org
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <dron@remotesensing.org>
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
 * Revision 1.30  2003/09/05 18:15:43  dron
 * Fixes in ASTER DEM zone detection.
 *
 * Revision 1.29  2003/06/30 14:44:28  dron
 * Fixed problem introduced with Hyperion support.
 *
 * Revision 1.28  2003/06/26 20:42:31  dron
 * Support for Hyperion Level 1 data product.
 *
 * Revision 1.27  2003/06/25 08:26:18  dron
 * Support for Aster Level 1A/1B/2 products.
 *
 * Revision 1.26  2003/06/12 15:07:56  dron
 * Added support for SeaWiFS Level 3 Standard Mapped Image Products.
 *
 * Revision 1.25  2003/06/10 09:32:31  dron
 * Added support for MODIS Level 3 products.
 *
 * Revision 1.24  2003/05/21 14:11:43  dron
 * MODIS Level 1B earth-view (EV) product now supported.
 *
 * Revision 1.23  2003/03/31 12:51:35  dron
 * GetNoData()/SetNoData() functions added.
 *
 * Revision 1.22  2003/02/28 17:07:37  dron
 * Global metadata now combined with local ones.
 *
 * Revision 1.21  2003/01/28 21:19:04  dron
 * DFNT_CHAR8 type was used instead of DFNT_INT8
 *
 * Revision 1.20  2003/01/27 16:45:16  dron
 * Fixed problems with wrong count in SDsetattr() calls.
 *
 * Revision 1.19  2002/12/30 15:07:45  dron
 * SetProjCS() call removed.
 *
 * Revision 1.18  2002/12/19 19:20:20  dron
 * Wrong comments removed.
 *
 * Revision 1.17  2002/12/02 19:07:55  dron
 * Added SetMetadata()/SetMetadataItem().
 *
 * Revision 1.16  2002/11/23 18:54:17  warmerda
 * added CREATIONDATATYPES metadata for drivers
 *
 * Revision 1.15  2002/11/13 06:43:36  warmerda
 * honour quoted strings when tokenizing filename
 *
 * Revision 1.14  2002/11/12 09:16:52  dron
 * Added rank choosing in Create() method.
 *
 * Revision 1.13  2002/11/11 16:43:50  dron
 * Create() method now really works.
 *
 * Revision 1.12  2002/11/08 18:29:04  dron
 * Added Create() method.
 *
 * Revision 1.11  2002/11/07 13:23:44  dron
 * Support for projection information writing.
 *
 * Revision 1.10  2002/11/06 15:47:14  dron
 * Added support for 3D datasets creation
 *
 * Revision 1.9  2002/10/25 14:28:54  dron
 * Initial support for HDF4 creation.
 *
 * Revision 1.8  2002/09/06 11:47:23  dron
 * Fixes in pixel sizes determining for ASTER L1B
 *
 * Revision 1.7  2002/09/06 10:42:23  dron
 * Georeferencing for ASTER Level 1b datasets and ASTER DEMs.
 *
 * Revision 1.6  2002/07/23 12:27:58  dron
 * General Raster Interface support added.
 *
 * Revision 1.5  2002/07/18 08:27:05  dron
 * Improved multidimesional SDS arrays handling.
 *
 * Revision 1.4  2002/07/17 16:24:31  dron
 * MODIS support improved a bit.
 *
 * Revision 1.3  2002/07/17 13:36:18  dron
 * <hdf.h> and <mfhdf.h> changed to "hdf.h" and "mfhdf.h".
 *
 * Revision 1.2  2002/07/16 17:51:10  warmerda
 * removed hdf/ from include statements
 *
 * Revision 1.1  2002/07/16 11:04:11  dron
 * New driver: HDF4 datasets. Initial version.
 *
 *
 */

#include <string.h>
#include <math.h>

#include "hdf.h"
#include "mfhdf.h"

#include "gdal_priv.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"

#include "hdf4dataset.h"

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_HDF4(void);
CPL_C_END

// Signature to recognize files written by GDAL
const char      *pszGDALSignature =
        "Created with GDAL (http://www.remotesensing.org/gdal/)";

const double    PI = 3.14159265358979323846;

/************************************************************************/
/* ==================================================================== */
/*                              HDF4ImageDataset                        */
/* ==================================================================== */
/************************************************************************/

class HDF4ImageDataset : public HDF4Dataset
{
    friend class HDF4ImageRasterBand;

    char        *pszFilename;
    HDF4SubdatasetType iSubdatasetType;
    int32       iSDS, iGR, iPal, iDataset;
    int32       iRank, iNumType, nAttrs, iInterlaceMode, iPalInterlaceMode, iPalDataType;
    int32       nComps, nPalEntries;
    int32       aiDimSizes[MAX_VAR_DIMS];
    int         iXDim, iYDim, iBandDim;
    char        **papszLocalMetadata;
#define    N_COLOR_ENTRIES    256
    uint8       aiPaletteData[N_COLOR_ENTRIES][3];      // XXX: Static array currently
    char        szName[65];

    GDALColorTable *poColorTable;

    int         bHasGeoTransfom;
    double      adfGeoTransform[6];
    char        *pszProjection;
    char        *pszGCPProjection;
    GDAL_GCP    *pasGCPList;
    int         nGCPCount;

    void                ReadCoordinates( const char*, double*, double* );
    void                ToUTM( OGRSpatialReference *, double *, double * );

  public:
                HDF4ImageDataset();
                ~HDF4ImageDataset();
    
    static GDALDataset  *Open( GDALOpenInfo * );
    static GDALDataset  *Create( const char * pszFilename,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType, char ** papszParmList );
    virtual void        FlushCache( void );
    CPLErr              GetGeoTransform( double * padfTransform );
    virtual CPLErr      SetGeoTransform( double * );
    const char          *GetProjectionRef();
    virtual CPLErr      SetProjection( const char * );
    virtual int         GetGCPCount();
    virtual const char  *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();
    GDALDataType        GetDataType( int32 );
};

/************************************************************************/
/* ==================================================================== */
/*                            HDF4ImageRasterBand                       */
/* ==================================================================== */
/************************************************************************/

class HDF4ImageRasterBand : public GDALRasterBand
{
    friend class HDF4ImageDataset;

    int         bNoDataSet;
    double      dfNoDataValue;

  public:

                HDF4ImageRasterBand( HDF4ImageDataset *, int, GDALDataType );
    
    virtual CPLErr          IReadBlock( int, int, void * );
    virtual CPLErr          IWriteBlock( int, int, void * );
    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
    virtual double	    GetNoDataValue( int * );
    virtual CPLErr	    SetNoDataValue( double );
};

/************************************************************************/
/*                           HDF4ImageRasterBand()                      */
/************************************************************************/

HDF4ImageRasterBand::HDF4ImageRasterBand( HDF4ImageDataset *poDS, int nBand,
                                          GDALDataType eType )

{
    this->poDS = poDS;
    this->nBand = nBand;
    eDataType = eType;
    bNoDataSet = FALSE;
    dfNoDataValue = -9999.0;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr HDF4ImageRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                        void * pImage )
{
    HDF4ImageDataset    *poGDS = (HDF4ImageDataset *) poDS;
    int32               iStart[MAX_NC_DIMS], iEdges[MAX_NC_DIMS];
    CPLErr              eErr = CE_None;

    if( poGDS->eAccess == GA_Update )
    {
        memset( pImage, 0,
                nBlockXSize * nBlockYSize * GDALGetDataTypeSize(eDataType) / 8 );
        return CE_None;
    }

    switch ( poGDS->iSubdatasetType )
    {
        case HDF4_SDS:
        poGDS->iSDS = SDselect( poGDS->hSD, poGDS->iDataset );
        /* HDF rank:
        A rank 2 dataset is an image read in scan-line order (2D). 
        A rank 3 dataset is a series of images which are read in an image
        at a time to form a volume.
        A rank 4 dataset may be thought of as a series of volumes.

        The "iStart" array specifies the multi-dimensional index of the
        starting corner of the hyperslab to read. The values are zero based.

        The "edge" array specifies the number of values to read along each
        dimension of the hyperslab.

        The "iStride" array allows for sub-sampling along each dimension. If a
        iStride value is specified for a dimension, that many values will be
        skipped over when reading along that dimension. Specifying
        iStride = NULL in the C interface or iStride = 1 in either
        interface specifies contiguous reading of data. If the iStride
        values are set to 0, SDreaddata returns FAIL (or -1). No matter
        what iStride value is provided, data is always placed contiguously
        in buffer.
     
        See also:
        http://www.dur.ac.uk/~dcs0elb/au-case-study/code/hdf-browse.c.html
        http://dao.gsfc.nasa.gov/DAO_people/yin/quads.code.html
        */
        switch ( poGDS->iRank )
        {
            case 4:     // 4Dim: volume-time
            // FIXME: needs sample file. Does not works currently.
            iStart[3] = 0/* range: 0--aiDimSizes[3]-1 */;       iEdges[3] = 1;
            iStart[2] = 0/* range: 0--aiDimSizes[2]-1 */;       iEdges[2] = 1;
            iStart[1] = nBlockYOff; iEdges[1] = nBlockYSize;
            iStart[0] = nBlockXOff; iEdges[0] = nBlockXSize;
            break;
            case 3: // 3Dim: volume
            iStart[poGDS->iBandDim] = nBand - 1;
            iEdges[poGDS->iBandDim] = 1;
        
            iStart[poGDS->iYDim] = nBlockYOff;
            iEdges[poGDS->iYDim] = nBlockYSize;
        
            iStart[poGDS->iXDim] = nBlockXOff;
            iEdges[poGDS->iXDim] = nBlockXSize;
            break;
            case 2: // 2Dim: rows/cols
            iStart[poGDS->iYDim] = nBlockYOff;
            iEdges[poGDS->iYDim] = nBlockYSize;
        
            iStart[poGDS->iXDim] = nBlockXOff;
            iEdges[poGDS->iXDim] = nBlockXSize;
            break;
        }
        // Read HDF SDS arrays
        switch ( poGDS->iNumType )
        {
            case DFNT_FLOAT32:
            SDreaddata( poGDS->iSDS, iStart, NULL, iEdges, (float32 *)pImage );
            break;
            case DFNT_FLOAT64:
            SDreaddata( poGDS->iSDS, iStart, NULL, iEdges, (float64 *)pImage );
            break;
            case DFNT_INT8:
            SDreaddata( poGDS->iSDS, iStart, NULL, iEdges, (int8 *)pImage );
            break;
            case DFNT_UINT8:
            SDreaddata( poGDS->iSDS, iStart, NULL, iEdges, (uint8 *)pImage );
            break;
            case DFNT_INT16:
            SDreaddata( poGDS->iSDS, iStart, NULL, iEdges, (int16 *)pImage );
            break;
            case DFNT_UINT16:
            SDreaddata( poGDS->iSDS, iStart, NULL, iEdges, (uint16 *)pImage );
            break;
            case DFNT_INT32:
            SDreaddata( poGDS->iSDS, iStart, NULL, iEdges, (int32 *)pImage );
            break;
            case DFNT_UINT32:
            SDreaddata( poGDS->iSDS, iStart, NULL, iEdges, (uint32 *)pImage );
            break;
            case DFNT_CHAR8:
            SDreaddata(poGDS->iSDS, iStart, NULL, iEdges, (char8 *)pImage);
            break;
            case DFNT_UCHAR8:
            SDreaddata(poGDS->iSDS, iStart, NULL, iEdges, (uchar8 *)pImage);
            break;
            default:
            eErr = CE_Failure;
            break;
        }
        poGDS->iSDS = SDendaccess( poGDS->iSDS );
        break;
        case HDF4_GR:
        iStart[poGDS->iYDim] = nBlockYOff;
        iEdges[poGDS->iYDim] = nBlockYSize;
        
        iStart[poGDS->iXDim] = nBlockXOff;
        iEdges[poGDS->iXDim] = nBlockXSize;
        switch ( poGDS->iNumType )
        {
            case DFNT_FLOAT32:
            GRreadimage( poGDS->iGR, iStart, NULL, iEdges, (float32 *)pImage );
            break;
            case DFNT_FLOAT64:
            GRreadimage( poGDS->iGR, iStart, NULL, iEdges, (float64 *)pImage );
            break;
            case DFNT_INT8:
            GRreadimage( poGDS->iGR, iStart, NULL, iEdges, (int8 *)pImage );
            break;
            case DFNT_UINT8:
            GRreadimage( poGDS->iGR, iStart, NULL, iEdges, (uint8 *)pImage );
            break;
            case DFNT_INT16:
            GRreadimage( poGDS->iGR, iStart, NULL, iEdges, (int16 *)pImage );
            break;
            case DFNT_UINT16:
            GRreadimage( poGDS->iGR, iStart, NULL, iEdges, (uint16 *)pImage );
            break;
            case DFNT_INT32:
            GRreadimage( poGDS->iGR, iStart, NULL, iEdges, (int32 *)pImage );
            break;
            case DFNT_UINT32:
            GRreadimage( poGDS->iGR, iStart, NULL, iEdges, (uint32 *)pImage );
            break;
            case DFNT_CHAR8:
            GRreadimage(poGDS->iGR, iStart, NULL, iEdges, (char8 *)pImage);
            break;
            case DFNT_UCHAR8:
            GRreadimage(poGDS->iGR, iStart, NULL, iEdges, (uchar8 *)pImage);
            break;
            default:
            eErr = CE_Failure;
            break;
        }
        break;
        default:
        eErr = CE_Failure;
        break;
    }

    return eErr;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr HDF4ImageRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                         void * pImage )
{
    HDF4ImageDataset    *poGDS = (HDF4ImageDataset *)poDS;
    int32               iStart[MAX_NC_DIMS], iEdges[MAX_NC_DIMS];
    CPLErr              eErr = CE_None;

    CPLAssert( poGDS != NULL
               && nBlockXOff >= 0
               && nBlockYOff >= 0
               && pImage != NULL );

    switch ( poGDS->iRank )
    {
        case 3:
        iStart[poGDS->iBandDim] = nBand - 1;
        iEdges[poGDS->iBandDim] = 1;
    
        iStart[poGDS->iYDim] = nBlockYOff;
        iEdges[poGDS->iYDim] = nBlockYSize;
    
        iStart[poGDS->iXDim] = nBlockXOff;
        iEdges[poGDS->iXDim] = nBlockXSize;
        if ( (SDwritedata( poGDS->iSDS, iStart, NULL,
                           iEdges, (VOIDP)pImage )) < 0 )
            eErr = CE_Failure;
        break;
        case 2:
        poGDS->iSDS = SDselect( poGDS->hSD, nBand - 1 );
        iStart[poGDS->iYDim] = nBlockYOff;
        iEdges[poGDS->iYDim] = nBlockYSize;
    
        iStart[poGDS->iXDim] = nBlockXOff;
        iEdges[poGDS->iXDim] = nBlockXSize;
        if ( (SDwritedata( poGDS->iSDS, iStart, NULL,
                           iEdges, (VOIDP)pImage )) < 0 )
            eErr = CE_Failure;
        poGDS->iSDS = SDendaccess( poGDS->iSDS );
        break;
        default:
        eErr = CE_Failure;
        break;
    }

    return eErr;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *HDF4ImageRasterBand::GetColorTable()
{
    HDF4ImageDataset    *poGDS = (HDF4ImageDataset *) poDS;

    return poGDS->poColorTable;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp HDF4ImageRasterBand::GetColorInterpretation()
{
    HDF4ImageDataset    *poGDS = (HDF4ImageDataset *) poDS;

    if ( poGDS->iSubdatasetType == HDF4_SDS )
        return GCI_GrayIndex;
    else if ( poGDS->iSubdatasetType == HDF4_GR )
    {
        if ( poGDS->poColorTable != NULL )
            return GCI_PaletteIndex;
        else if ( poGDS->nBands != 1 )
        {
            if ( nBand == 1 )
                return GCI_RedBand;
            else if ( nBand == 2 )
                return GCI_GreenBand;
            else if ( nBand == 3 )
                return GCI_BlueBand;
            else if ( nBand == 4 )
                return GCI_AlphaBand;
            else
                return GCI_Undefined;
        }
        else
            return GCI_GrayIndex;
    }
    else
        return GCI_GrayIndex;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double HDF4ImageRasterBand::GetNoDataValue( int * pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = bNoDataSet;

    return dfNoDataValue;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr HDF4ImageRasterBand::SetNoDataValue( double dfNoData )

{
    bNoDataSet = TRUE;
    dfNoDataValue = dfNoData;

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                              HDF4ImageDataset                        */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           HDF4ImageDataset()                         */
/************************************************************************/

HDF4ImageDataset::HDF4ImageDataset()
{
    pszFilename = NULL;
    hSD = 0;
    hGR = 0;
    iSDS = 0;
    iGR = 0;
    iSubdatasetType = HDF4_UNKNOWN;
    papszLocalMetadata = NULL;
    poColorTable = NULL;
    pszProjection = CPLStrdup( "" );
    pszGCPProjection = CPLStrdup( "" );
    bHasGeoTransfom = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    nGCPCount = 0;
    pasGCPList = NULL;

}

/************************************************************************/
/*                            ~HDF4ImageDataset()                       */
/************************************************************************/

HDF4ImageDataset::~HDF4ImageDataset()
{
    FlushCache();
    
    if ( pszFilename )
        CPLFree( pszFilename );
    if ( iSDS > 0 )
        SDendaccess( iSDS );
    if ( hSD > 0 )
        SDend( hSD );
    if ( iGR > 0 )
        GRendaccess( iGR );
    if ( hGR > 0 )
        GRend( hGR );
    if ( papszLocalMetadata )
        CSLDestroy( papszLocalMetadata );
    if ( poColorTable != NULL )
        delete poColorTable;
    if ( pszProjection )
        CPLFree( pszProjection );
    if ( pszGCPProjection )
        CPLFree( pszGCPProjection );
    if( nGCPCount > 0 )
    {
        for( int i = 0; i < nGCPCount; i++ )
        {
            if ( pasGCPList[i].pszId )
                CPLFree( pasGCPList[i].pszId );
            if ( pasGCPList[i].pszInfo )
                CPLFree( pasGCPList[i].pszInfo );
        }

        CPLFree( pasGCPList );
    }
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr HDF4ImageDataset::GetGeoTransform( double * padfTransform )
{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );

    if ( !bHasGeoTransfom )
        return CE_Failure;

    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr HDF4ImageDataset::SetGeoTransform( double * padfTransform )
{
    bHasGeoTransfom = TRUE;
    memcpy( adfGeoTransform, padfTransform, sizeof(double) * 6 );

    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *HDF4ImageDataset::GetProjectionRef()

{
    return pszProjection;
}

/************************************************************************/
/*                          SetProjection()                             */
/************************************************************************/

CPLErr HDF4ImageDataset::SetProjection( const char *pszNewProjection )

{
    if ( pszProjection )
        CPLFree( pszProjection );
    pszProjection = CPLStrdup( pszNewProjection );

    return CE_None;
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int HDF4ImageDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *HDF4ImageDataset::GetGCPProjection()

{
    if( nGCPCount > 0 )
        return pszGCPProjection;
    else
        return "";
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *HDF4ImageDataset::GetGCPs()
{
    return pasGCPList;
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void HDF4ImageDataset::FlushCache()

{
    int         iBand;
    char        *pszName;
    const char  *pszValue;
    
    GDALDataset::FlushCache();

    if( eAccess == GA_ReadOnly )
        return;

    // Write out transformation matrix
    pszValue = CPLSPrintf( "%f, %f, %f, %f, %f, %f",
                                   adfGeoTransform[0], adfGeoTransform[1],
                                   adfGeoTransform[2], adfGeoTransform[3],
                                   adfGeoTransform[4], adfGeoTransform[5] );
    if ( (SDsetattr( hSD, "TransformationMatrix", DFNT_CHAR8,
                     strlen(pszValue) + 1, pszValue )) < 0 )
    {
        CPLDebug( "HDF4Image",
                  "Cannot write transformation matrix to output file" );
    }

    // Write out projection
    if ( pszProjection != NULL && !EQUAL( pszProjection, "" ) )
    {
        if ( (SDsetattr( hSD, "Projection", DFNT_CHAR8,
                         strlen(pszProjection) + 1, pszProjection )) < 0 )
            {
                CPLDebug( "HDF4Image",
                          "Cannot write projection information to output file");
            }
    }

    // Store all metadata from source dataset as HDF attributes
    if ( papszMetadata )
    {
        char    **papszMeta = papszMetadata;

        while ( *papszMeta )
        {
            pszValue = CPLParseNameValue( *papszMeta++, &pszName );
            if ( (SDsetattr( hSD, pszName, DFNT_CHAR8,
                             strlen(pszValue) + 1, pszValue )) < 0 );
            {
                CPLDebug( "HDF4Image",
                          "Cannot write metadata information to output file");
            }

            CPLFree( pszName );
        }
    }

    // Write out NoData values
    for ( iBand = 1; iBand <= nBands; iBand++ )
    {
        HDF4ImageRasterBand *poBand =
            (HDF4ImageRasterBand *)GetRasterBand(iBand);

        if ( poBand->bNoDataSet )
        {
            pszName = CPLStrdup( CPLSPrintf( "NoDataValue%d", iBand ) );
            pszValue = CPLSPrintf( "%lf", poBand->dfNoDataValue );
            if ( (SDsetattr( hSD, pszName, DFNT_CHAR8,
                             strlen(pszValue) + 1, pszValue )) < 0 )
                {
                    CPLDebug( "HDF4Image",
                              "Cannot write NoData value for band %d "
                              "to output file", iBand);
                }

            CPLFree( pszName );
       }
    }

    // Write out band descriptions
    for ( iBand = 1; iBand <= nBands; iBand++ )
    {
        HDF4ImageRasterBand *poBand =
            (HDF4ImageRasterBand *)GetRasterBand(iBand);

        pszName = CPLStrdup( CPLSPrintf( "BandDesc%d", iBand ) );
        pszValue = poBand->GetDescription();
        if ( pszValue != NULL && !EQUAL( pszValue, "" ) )
        {
            if ( (SDsetattr( hSD, pszName, DFNT_CHAR8,
                             strlen(pszValue) + 1, pszValue )) < 0 )
            {
                CPLDebug( "HDF4Image",
                          "Cannot write band's %d description to output file",
                          iBand);
            }
        }

        CPLFree( pszName );
    }
}

/************************************************************************/
/*              Translate HDF4 data type into GDAL data type            */
/************************************************************************/
GDALDataType HDF4ImageDataset::GetDataType( int32 iNumType )
{
    switch (iNumType)
    {
        case DFNT_CHAR8: // The same as DFNT_CHAR
        case DFNT_UCHAR8: // The same as DFNT_UCHAR
        case DFNT_INT8:
        case DFNT_UINT8:
        return GDT_Byte;
        break;
        case DFNT_INT16:
        return GDT_Int16;
        break;
        case DFNT_UINT16:
        return GDT_UInt16;
        break;
        case DFNT_INT32:
        return GDT_Int32;
        break;
        case DFNT_UINT32:
        return GDT_UInt32;
        break;
        case DFNT_INT64:
        return GDT_Unknown;
        break;
        case DFNT_UINT64:
        return GDT_Unknown;
        break;
        case DFNT_FLOAT32:
        return GDT_Float32;
        break;
        case DFNT_FLOAT64:
        return GDT_Float64;
        break;
        default:
        return GDT_Unknown;
        break;
    }
}

/************************************************************************/
/*                                ToUTM()                               */
/************************************************************************/

void HDF4ImageDataset::ToUTM( OGRSpatialReference *poProj,
                              double *pdfGeoX, double *pdfGeoY )
{
    OGRCoordinateTransformation *poTransform = NULL;
    OGRSpatialReference *poLatLong = NULL;
    poLatLong = poProj->CloneGeogCS();
    poTransform = OGRCreateCoordinateTransformation( poLatLong, poProj );
    
    if( poTransform != NULL )
        poTransform->Transform( 1, pdfGeoX, pdfGeoY, NULL );
        
    if( poTransform != NULL )
        delete poTransform;

    if( poLatLong != NULL )
        delete poLatLong;
}

/************************************************************************/
/*                            ReadCoordinates()                         */
/************************************************************************/

void HDF4ImageDataset::ReadCoordinates( const char *pszString,
                                        double *pdfX, double *pdfY )
{
    char **papszStrList;
    papszStrList = CSLTokenizeString2( pszString, ", ", 0 );
    *pdfX = atof(papszStrList[0]);
    *pdfY = atof(papszStrList[1]);
    CSLDestroy( papszStrList );
}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *HDF4ImageDataset::Open( GDALOpenInfo * poOpenInfo )
{
    int         i, j;
    
    if( !EQUALN( poOpenInfo->pszFilename, "HDF4_SDS:", 9 ) &&
        !EQUALN( poOpenInfo->pszFilename, "HDF4_GR:", 8 ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    char                **papszSubdatasetName;
    HDF4ImageDataset    *poDS;

    poDS = new HDF4ImageDataset( );

    poDS->fp = poOpenInfo->fp;
    poOpenInfo->fp = NULL;

    papszSubdatasetName = CSLTokenizeString2( poOpenInfo->pszFilename,
                                              ":", CSLT_HONOURSTRINGS );
    poDS->pszFilename = CPLStrdup( papszSubdatasetName[2] );

    if( EQUAL( papszSubdatasetName[0], "HDF4_SDS" ) )
        poDS->iSubdatasetType = HDF4_SDS;
    else if ( EQUAL( papszSubdatasetName[0], "HDF4_GR" ) )
        poDS->iSubdatasetType = HDF4_GR;
    else
        poDS->iSubdatasetType = HDF4_UNKNOWN;
    
    if( EQUAL( papszSubdatasetName[1], "GDAL_HDF4" ) )
        poDS->iDataType = GDAL_HDF4;
    else if( EQUAL( papszSubdatasetName[1], "ASTER_L1A" ) )
        poDS->iDataType = ASTER_L1A;
    else if( EQUAL( papszSubdatasetName[1], "ASTER_L1B" ) )
        poDS->iDataType = ASTER_L1B;
    else if( EQUAL( papszSubdatasetName[1], "ASTER_L2" ) )
        poDS->iDataType = ASTER_L2;
    else if( EQUAL( papszSubdatasetName[1], "AST14DEM" ) )
        poDS->iDataType = AST14DEM;
    else if( EQUAL( papszSubdatasetName[1], "MODIS_L1B" ) )
        poDS->iDataType = MODIS_L1B;
    else if( EQUAL( papszSubdatasetName[1], "MODIS_L3" ) )
        poDS->iDataType = MODIS_L3;
    else if( EQUAL( papszSubdatasetName[1], "MODIS_UNK" ) )
        poDS->iDataType = MODIS_UNK;
    else if( EQUAL( papszSubdatasetName[1], "SEAWIFS_L3" ) )
        poDS->iDataType = SEAWIFS_L3;
    else if( EQUAL( papszSubdatasetName[1], "HYPERION_L1" ) )
        poDS->iDataType = HYPERION_L1;
    else
        poDS->iDataType = UNKNOWN;

    // Does our file still here?
    if ( !Hishdf( poDS->pszFilename ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    int32       iAttribute, nValues, iAttrNumType;
    char        szAttrName[MAX_NC_NAME];
    
    if( poOpenInfo->eAccess == GA_ReadOnly )
        poDS->hHDF4 = Hopen( poDS->pszFilename, DFACC_READ, 0 );
    else
        poDS->hHDF4 = Hopen( poDS->pszFilename, DFACC_WRITE, 0 );
    
    if( poDS->hHDF4 <= 0 )
        return( NULL );

/* -------------------------------------------------------------------- */
/*      Select SDS or GR to read from.                                  */
/* -------------------------------------------------------------------- */
    poDS->iDataset = atoi( papszSubdatasetName[3] );
    CSLDestroy( papszSubdatasetName );

    switch ( poDS->iSubdatasetType )
    {
        case HDF4_SDS:
        poDS->hSD = SDstart( poDS->pszFilename, DFACC_READ );
        if ( poDS->hSD == -1 )
            return NULL;
        
        if ( poDS->ReadGlobalAttributes( poDS->hSD ) != CE_None )
            return NULL;
    
        poDS->iSDS = SDselect( poDS->hSD, poDS->iDataset );
        SDgetinfo( poDS->iSDS, poDS->szName, &poDS->iRank, poDS->aiDimSizes,
                   &poDS->iNumType, &poDS->nAttrs);

        // We will duplicate global metadata for every subdataset
        poDS->papszLocalMetadata = CSLDuplicate( poDS->papszGlobalMetadata );

        for ( iAttribute = 0; iAttribute < poDS->nAttrs; iAttribute++ )
        {
            SDattrinfo( poDS->iSDS, iAttribute, szAttrName,
                        &iAttrNumType, &nValues );
            poDS->papszLocalMetadata =
                poDS->TranslateHDF4Attributes( poDS->iSDS, iAttribute,
                    szAttrName, iAttrNumType, nValues, poDS->papszLocalMetadata );
        }
        poDS->SetMetadata( poDS->papszLocalMetadata, "" );
        poDS->iSDS = SDendaccess( poDS->iSDS );

        CPLDebug( "HDF4Image",
                  "aiDimSizes[0]=%d, aiDimSizes[1]=%d, "
                  "aiDimSizes[2]=%d, aiDimSizes[3]=%d",
                  poDS->aiDimSizes[0], poDS->aiDimSizes[1],
                  poDS->aiDimSizes[2], poDS->aiDimSizes[3] );

        switch( poDS->iRank )
        {
            case 2:
            poDS->nBands = 1;
            poDS->iXDim = 1;
            poDS->iYDim = 0;
            break;
            case 3:
            if( poDS->aiDimSizes[0] < poDS->aiDimSizes[2] )
            {
                poDS->iBandDim = 0;
                poDS->iXDim = 2;
                poDS->iYDim = 1;
            }
            else
            {
                poDS->iBandDim = 2;
                poDS->iXDim = 1;
                poDS->iYDim = 0;
            }
            poDS->nBands = poDS->aiDimSizes[poDS->iBandDim];
            break;
            case 4: // FIXME
            poDS->nBands = poDS->aiDimSizes[2] * poDS->aiDimSizes[3];
            break;
            default:
            break;
        }
        break;
        case HDF4_GR:
        poDS->hGR = GRstart( poDS->hHDF4 );
        if ( poDS->hGR == -1 )
           return NULL;
        
        poDS->iGR = GRselect( poDS->hGR, poDS->iDataset );
        if ( GRgetiminfo( poDS->iGR, poDS->szName,
                          &poDS->iRank, &poDS->iNumType,
                          &poDS->iInterlaceMode, poDS->aiDimSizes,
                          &poDS->nAttrs ) != 0 )
            return NULL;

        // We will duplicate global metadata for every subdataset
        poDS->papszLocalMetadata = CSLDuplicate( poDS->papszGlobalMetadata );

        for ( iAttribute = 0; iAttribute < poDS->nAttrs; iAttribute++ )
        {
            GRattrinfo( poDS->iGR, iAttribute, szAttrName,
                        &iAttrNumType, &nValues );
            poDS->papszLocalMetadata = 
                poDS->TranslateHDF4Attributes( poDS->iGR, iAttribute,
                    szAttrName, iAttrNumType, nValues, poDS->papszLocalMetadata );
        }
        poDS->SetMetadata( poDS->papszLocalMetadata, "" );
        // Read colour table
        GDALColorEntry oEntry;
         
        poDS->iPal = GRgetlutid ( poDS->iGR, poDS->iDataset );
        if ( poDS->iPal != -1 )
        {
            GRgetlutinfo( poDS->iPal, &poDS->nComps, &poDS->iPalDataType,
                          &poDS->iPalInterlaceMode, &poDS->nPalEntries );
            GRreadlut( poDS->iPal, poDS->aiPaletteData );
            poDS->poColorTable = new GDALColorTable();
            for( i = 0; i < N_COLOR_ENTRIES; i++ )
            {
                oEntry.c1 = poDS->aiPaletteData[i][0];
                oEntry.c2 = poDS->aiPaletteData[i][1];
                oEntry.c3 = poDS->aiPaletteData[i][2];
                oEntry.c4 = 255;
                
                poDS->poColorTable->SetColorEntry( i, &oEntry );
            }
        }

        poDS->iXDim = 0;
        poDS->iYDim = 1;
        poDS->nBands = poDS->iRank;
        break;
        default:
        return NULL;
        break;
    }
    
    poDS->nRasterXSize = poDS->aiDimSizes[poDS->iXDim];
    poDS->nRasterYSize = poDS->aiDimSizes[poDS->iYDim];

    if ( poDS->iDataType == HYPERION_L1 )
    {
        // XXX: Hyperion SDSs has Height x Bands x Width dimensions scheme
        poDS->nBands = poDS->aiDimSizes[1];
        poDS->nRasterXSize = poDS->aiDimSizes[2];
        poDS->nRasterYSize = poDS->aiDimSizes[0];
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( i = 1; i <= poDS->nBands; i++ )
        poDS->SetBand( i, new HDF4ImageRasterBand( poDS, i,
                                    poDS->GetDataType( poDS->iNumType ) ) );

/* -------------------------------------------------------------------- */
/*      Now we will handle particular types of HDF products. Every      */
/*      HDF product has its own structure.                              */
/* -------------------------------------------------------------------- */

    // Variables for reading georeferencing
    int             iUTMZone;
    double          dfULX, dfULY, dfURX, dfURY, dfLLX, dfLLY, dfLRX, dfLRY;
    double          dfCenterX, dfCenterY;
    OGRSpatialReference oSRS;

    // We will need this auxiliary variables to read geolocation SDSs
    int32           iSDS, iRank, iNumType, nAttrs, nXPoints, nYPoints;
    char            szName[VSNAMELENMAX + 1];
    int32	    aiDimSizes[MAX_VAR_DIMS];
    int32           iStart[MAX_NC_DIMS], iEdges[MAX_NC_DIMS];
    GInt16          iHeightNoData;
    int             bLatNoDataSet = FALSE, bLongNoDataSet = FALSE;
    int             bHeightNoDataSet = FALSE;

    const char      *pszValue;

    switch ( poDS->iDataType )
    {

/* -------------------------------------------------------------------- */
/*      HDF created by GDAL.                                            */
/* -------------------------------------------------------------------- */
        case GDAL_HDF4:
            CPLDebug( "HDF4Image", "Input dataset interpreted as GDAL_HDF4" );

            if ( (pszValue =
                  CSLFetchNameValue(poDS->papszGlobalMetadata, "Projection")) )
            {
                if ( poDS->pszProjection )
                    CPLFree( poDS->pszProjection );
                poDS->pszProjection = CPLStrdup( pszValue );
            }
            if ( (pszValue = CSLFetchNameValue(poDS->papszGlobalMetadata,
                                               "TransformationMatrix")) )
            {
                int i = 0;
                char *pszString = (char *) pszValue; 
                while ( *pszValue && i < 6 )
                {
                    poDS->adfGeoTransform[i++] = strtod(pszString, &pszString);
                    pszString++;
                }
                poDS->bHasGeoTransfom = TRUE;
            }
            for( i = 1; i <= poDS->nBands; i++ )
            {
                if ( (pszValue =
                      CSLFetchNameValue(poDS->papszGlobalMetadata,
                                        CPLSPrintf("BandDesc%d", i))) )
                    poDS->GetRasterBand( i )->SetDescription( pszValue );
            }
            for( i = 1; i <= poDS->nBands; i++ )
            {
                if ( (pszValue =
                      CSLFetchNameValue(poDS->papszGlobalMetadata,
                                        CPLSPrintf("NoDataValue%d", i))) )
                    poDS->GetRasterBand( i )->SetNoDataValue( atof(pszValue) );
            }
        break;

/* -------------------------------------------------------------------- */
/*      ASTER Level 1A.                                                 */
/* -------------------------------------------------------------------- */
        case ASTER_L1A:
        {
            CPLDebug( "HDF4Image", "Input dataset interpreted as ASTER_L1A" );

/* -------------------------------------------------------------------- */
/*      Read geolocation points.                                        */
/* -------------------------------------------------------------------- */
            double          *pdfLat = NULL, *pdfLong = NULL;
            GInt32          *piLatticeX = NULL, *piLatticeY = NULL;

            nXPoints = nYPoints = 0;
            for ( i = poDS->iDataset - 1; i >= 0; i-- )
            {
                iSDS = SDselect( poDS->hSD, i );

                if ( SDgetinfo( iSDS, szName, &iRank, aiDimSizes,
                                &iNumType, &nAttrs) == 0 )
                {
                    if ( EQUALN( szName, "Latitude", 8 )
                         && iNumType == DFNT_FLOAT64 )
                    {
                        if ( (nXPoints && nXPoints != aiDimSizes[1])
                             || (nYPoints && nYPoints != aiDimSizes[0]) )
                        {
                            // Error: geolocation arrays has different sizes
                            break;
                        }
                        else
                        {
                            nXPoints = aiDimSizes[1];
                            nYPoints = aiDimSizes[0];
                        }

                        iStart[1] = 0;
                        iEdges[1] = nXPoints;

                        iStart[0] = 0;
                        iEdges[0] = nYPoints;
                        pdfLat = (double *)CPLMalloc( nXPoints * nYPoints *
                                                      sizeof( double ) );
                        SDreaddata( iSDS, iStart, NULL, iEdges, pdfLat );
                    }
                    else if ( EQUALN( szName, "Longitude", 9 )
                              && iNumType == DFNT_FLOAT64 )
                    {
                        if ( (nXPoints && nXPoints != aiDimSizes[1])
                             || (nYPoints && nYPoints != aiDimSizes[0]) )
                        {
                            // Error: geolocation arrays has different sizes
                            break;
                        }
                        else
                        {
                            nXPoints = aiDimSizes[1];
                            nYPoints = aiDimSizes[0];
                        }

                        iStart[1] = 0;
                        iEdges[1] = nXPoints;

                        iStart[0] = 0;
                        iEdges[0] = nYPoints;
                        pdfLong = (double *)CPLMalloc( nXPoints * nYPoints *
                                                       sizeof( double ) );
                        SDreaddata( iSDS, iStart, NULL, iEdges, pdfLong );
                    }
                    else if ( EQUALN( szName, "LatticePoint", 12 )
                              && iNumType == DFNT_INT32 )
                    {
                        if ( (nXPoints && nXPoints != aiDimSizes[1])
                             || (nYPoints && nYPoints != aiDimSizes[0])
                             || aiDimSizes[2] != 2 )
                        {
                            // Error: geolocation arrays has different sizes
                            break;
                        }
                        else
                        {
                            nXPoints = aiDimSizes[1];
                            nYPoints = aiDimSizes[0];
                        }

                        iStart[1] = 0;
                        iEdges[1] = nXPoints;

                        iStart[0] = 0;
                        iEdges[0] = nYPoints;

                        iStart[2] = 0;
                        iEdges[2] = 1;
                        piLatticeX = (GInt32 *)CPLMalloc( nXPoints * nYPoints *
                                                          sizeof( GInt32 ) );
                        SDreaddata( iSDS, iStart, NULL, iEdges, piLatticeX );

                        iStart[2] = 1;
                        iEdges[2] = 1;
                        piLatticeY = (GInt32 *)CPLMalloc( nXPoints * nYPoints *
                                                          sizeof( GInt32 ) );
                        SDreaddata( iSDS, iStart, NULL, iEdges, piLatticeY );
                    }
                }

                poDS->iSDS = SDendaccess( iSDS );
            }

            if ( pdfLat && pdfLong && piLatticeX && piLatticeY )
            {
                if ( poDS->pszGCPProjection )
                    CPLFree( poDS->pszGCPProjection );
                poDS->pszGCPProjection = CPLStrdup( "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9108\"]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",\"4326\"]]" );

                CPLDebug( "HDF4Image",
                          "Reading geolocation points: nXPoints=%d, nYPoints=%d",
                          nXPoints, nYPoints );
                
                poDS->nGCPCount = 0;
                poDS->pasGCPList =
                    (GDAL_GCP *) CPLCalloc( nXPoints * nYPoints,
                                            sizeof( GDAL_GCP ) );
                GDALInitGCPs( nXPoints * nYPoints, poDS->pasGCPList );

                for ( i = 0; i < nYPoints; i++ )
                {
                    for ( j = 0; j < nXPoints; j++ )
                    {
                        int index = i * nXPoints + j;

                        // GCPs in Level 1A dataset are in geocentric
                        // coordinates. Convert them in geodetic (we will
                        // convert latitudes only, longitudes does not need to
                        // be converted, because they are the same).
                        // This calculation valid for WGS84 datum only.
                        poDS->pasGCPList[index].dfGCPY = 
                            atan(tan(pdfLat[index]*PI/180)/0.99330562)*180/PI;
                        poDS->pasGCPList[index].dfGCPX = pdfLong[index];
                        poDS->pasGCPList[index].dfGCPZ = 0.0;

                        poDS->pasGCPList[index].dfGCPPixel =
                            piLatticeX[index] + 0.5;
                        poDS->pasGCPList[index].dfGCPLine =
                            piLatticeY[index] + 0.5;
                        poDS->nGCPCount++;
                    }
                }
            }

            if ( pdfLat )
                CPLFree( pdfLat );
            if ( pdfLong )
                CPLFree( pdfLong );
            if ( piLatticeX )
                CPLFree( piLatticeX );
            if ( piLatticeY )
                CPLFree( piLatticeY );
        }
        break;

/* -------------------------------------------------------------------- */
/*      ASTER Level 1B.                                                 */
/* -------------------------------------------------------------------- */
        case ASTER_L1B:
        {
            CPLDebug( "HDF4Image", "Input dataset interpreted as ASTER_L1B" );

            // XXX: Band 3B should be always georeferenced using GCPs,
            // not the geotransformation matrix, because corner coordinates
            // valid only for other bands, not 3B.
            if ( !EQUAL( poDS->szName, "ImageData3B" ) &&
                 strlen( poDS->szName ) >= 10 &&
                 EQUAL( CSLFetchNameValue( poDS->papszGlobalMetadata,
                    CPLSPrintf("MPMETHOD%s", &poDS->szName[9]) ), "UTM" ) )
            {
                oSRS.SetWellKnownGeogCS( "WGS84" );
                
                poDS->ReadCoordinates( CSLFetchNameValue( 
                    poDS->papszGlobalMetadata, "UPPERLEFT" ), &dfULY, &dfULX );
                poDS->ReadCoordinates( CSLFetchNameValue( 
                    poDS->papszGlobalMetadata, "UPPERRIGHT" ), &dfURY, &dfURX );
                poDS->ReadCoordinates( CSLFetchNameValue( 
                    poDS->papszGlobalMetadata, "LOWERLEFT" ), &dfLLY, &dfLLX );
                poDS->ReadCoordinates( CSLFetchNameValue( 
                    poDS->papszGlobalMetadata, "LOWERRIGHT" ), &dfLRY, &dfLRX );
                poDS->ReadCoordinates( CSLFetchNameValue( 
                    poDS->papszGlobalMetadata, "SCENECENTER" ),
                                    &dfCenterY, &dfCenterX );
                
                iUTMZone = atoi( CSLFetchNameValue( poDS->papszGlobalMetadata,
                                CPLSPrintf("UTMZONECODE%s", &poDS->szName[9]) ) );
                if( iUTMZone > 0 )
                    oSRS.SetUTM( iUTMZone, TRUE );
                else
                    oSRS.SetUTM( - iUTMZone, FALSE );

                oSRS.SetLinearUnits( SRS_UL_METER, 1.0 );
                if ( poDS->pszProjection )
                    CPLFree( poDS->pszProjection );
                oSRS.exportToWkt( &poDS->pszProjection );

                poDS->ToUTM( &oSRS, &dfULX, &dfULY );
                poDS->ToUTM( &oSRS, &dfURX, &dfURY );
                poDS->ToUTM( &oSRS, &dfLLX, &dfLLY );
                poDS->ToUTM( &oSRS, &dfLRX, &dfLRY );
                
                // Calculate geotransfom matrix from corner coordinates
                GDAL_GCP asGCPs[3];
                GDALInitGCPs( 3, asGCPs );

                asGCPs[0].dfGCPPixel = 0.5;
                asGCPs[0].dfGCPLine = 0.5;
                asGCPs[0].dfGCPX = dfULX;
                asGCPs[0].dfGCPY = dfULY;
                asGCPs[0].dfGCPZ = 0.0;

                asGCPs[1].dfGCPPixel = poDS->nRasterXSize + 0.5;
                asGCPs[1].dfGCPLine = 0.5;
                asGCPs[1].dfGCPX = dfURX;
                asGCPs[1].dfGCPY = dfURY;
                asGCPs[1].dfGCPZ = 0.0;

                asGCPs[2].dfGCPPixel = poDS->nRasterXSize + 0.5;
                asGCPs[2].dfGCPLine = poDS->nRasterYSize + 0.5;
                asGCPs[2].dfGCPX = dfLRX;
                asGCPs[2].dfGCPY = dfLRY;
                asGCPs[2].dfGCPZ = 0.0;

                GDALGCPsToGeoTransform( 3, asGCPs, poDS->adfGeoTransform, TRUE );
                GDALDeinitGCPs( 3, asGCPs );
                poDS->bHasGeoTransfom = TRUE;
            }

/* -------------------------------------------------------------------- */
/*      Read geolocation points.                                        */
/* -------------------------------------------------------------------- */
            double          *pdfLat = NULL, *pdfLong = NULL;

            nXPoints = nYPoints = 0;
            for ( i = poDS->iDataset - 1; i >= 0; i-- )
            {
                iSDS = SDselect( poDS->hSD, i );

                if ( SDgetinfo( iSDS, szName, &iRank, aiDimSizes,
                                &iNumType, &nAttrs) == 0 )
                {
                    if ( EQUALN( szName, "Latitude", 8 )
                         && iNumType == DFNT_FLOAT64 )
                    {
                        if ( (nXPoints && nXPoints != aiDimSizes[1])
                             || (nYPoints && nYPoints != aiDimSizes[0]) )
                        {
                            CPLDebug( "HDF4Image",
                                      "Size of latitude array differs from "
                                      "struct metadata, skip reading" );
                            goto CleanupAndBreakAsterL1B;
                        }
                        else
                        {
                            nXPoints = aiDimSizes[1];
                            nYPoints = aiDimSizes[0];
                        }

                        iStart[1] = 0;
                        iEdges[1] = nXPoints;

                        iStart[0] = 0;
                        iEdges[0] = nYPoints;
                        pdfLat = (double *)CPLCalloc( nXPoints * nYPoints,
                                                      sizeof( double ) );
                        SDreaddata( iSDS, iStart, NULL, iEdges, pdfLat );
                    }
                    else if ( EQUALN( szName, "Longitude", 9 )
                              && iNumType == DFNT_FLOAT64 )
                    {
                        if ( (nXPoints && nXPoints != aiDimSizes[1])
                             || (nYPoints && nYPoints != aiDimSizes[0]) )
                        {
                            CPLDebug( "HDF4Image",
                                      "Size of longitude array differs from "
                                      "struct metadata, skip reading" );
                            goto CleanupAndBreakAsterL1B;
                        }
                        else
                        {
                            nXPoints = aiDimSizes[1];
                            nYPoints = aiDimSizes[0];
                        }

                        iStart[1] = 0;
                        iEdges[1] = nXPoints;

                        iStart[0] = 0;
                        iEdges[0] = nYPoints;
                        pdfLong = (double *)CPLCalloc( nXPoints * nYPoints,
                                                       sizeof( double ) );
                        SDreaddata( iSDS, iStart, NULL, iEdges, pdfLong );
                    }
                }

                poDS->iSDS = SDendaccess( iSDS );
            }

            if ( pdfLat && pdfLong )
            {
                if ( poDS->pszGCPProjection )
                    CPLFree( poDS->pszGCPProjection );
                poDS->pszGCPProjection = CPLStrdup( "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9108\"]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",\"4326\"]]" );

                HDF4EOSDimensionMap *psDMX = NULL, *psDMY = NULL;
                int                 nCount = CPLListCount( poDS->psDataField );

                for ( i = 0; i < nCount; i++ )
                {
                    HDF4EOSDataField *psTemp = (HDF4EOSDataField *)
                       CPLListGetData( CPLListGet( poDS->psDataField, i ) );

                    if ( EQUAL( psTemp->pszDataFieldName, poDS->szName ) )
                    {
                        psDMX = (HDF4EOSDimensionMap *)
                            CPLListGetData(CPLListGet( psTemp->psDimList, 1 ));
                        psDMY = (HDF4EOSDimensionMap *)
                            CPLListGetData(CPLListGet( psTemp->psDimList, 0 ));
                    }
                }

                if ( !psDMX || !psDMY )
                    goto CleanupAndBreakAsterL1B;

                CPLDebug( "HDF4Image",
                          "Reading geolocation points: nXPoints=%d, nYPoints=%d, "
                          "OffsetX=%f, IncrementX=%f, OffsetY=%f, IncrementY=%f",
                          nXPoints, nYPoints,
                          psDMX->dfOffset, psDMX->dfIncrement,
                          psDMY->dfOffset, psDMY->dfIncrement );

                poDS->nGCPCount = 0;
                poDS->pasGCPList =
                    (GDAL_GCP *) CPLCalloc( nXPoints * nYPoints,
                                            sizeof( GDAL_GCP ) );
                GDALInitGCPs( nXPoints * nYPoints, poDS->pasGCPList );

                for ( i = 0; i < nYPoints; i++ )
                {
                    for ( j = 0; j < nXPoints; j++ )
                    {
                        int index = i * nXPoints + j;

                        // GCPs in Level 1B dataset are in geocentric
                        // coordinates. Convert them in geodetic (we will
                        // convert latitudes only, longitudes does not need to
                        // be converted, because they are the same).
                        // This calculation valid for WGS84 datum only.
                        poDS->pasGCPList[index].dfGCPY = 
                            atan(tan(pdfLat[index]*PI/180)/0.99330562)*180/PI;
                        poDS->pasGCPList[index].dfGCPX = pdfLong[index];
                        poDS->pasGCPList[index].dfGCPZ = 0.0;

                        poDS->pasGCPList[index].dfGCPPixel =
                            j * psDMX->dfIncrement + psDMX->dfOffset + 0.5;
                        poDS->pasGCPList[index].dfGCPLine =
                            i * psDMY->dfIncrement + psDMY->dfOffset + 0.5;
                        poDS->nGCPCount++;
                    }
                }
            }

CleanupAndBreakAsterL1B:
            if ( pdfLat )
                CPLFree( pdfLat );
            if ( pdfLong )
                CPLFree( pdfLong );
        }
        break;

/* -------------------------------------------------------------------- */
/*      ASTER Level 2.                                                 */
/* -------------------------------------------------------------------- */
        case ASTER_L2:
        {
            CPLDebug( "HDF4Image", "Input dataset interpreted as ASTER_L2" );

            // XXX: We will rely on the first available MPMETHODn
            // and UTMZONECODEn records.
            if ( EQUAL( CSLFetchNameValue( poDS->papszGlobalMetadata,
                                           "MPMETHOD1" ),
                        "UTM" ) )
            {
                oSRS.SetWellKnownGeogCS( "WGS84" );
                
                poDS->ReadCoordinates( CSLFetchNameValue( 
                    poDS->papszGlobalMetadata, "UPPERLEFT" ), &dfULY, &dfULX );
                poDS->ReadCoordinates( CSLFetchNameValue( 
                    poDS->papszGlobalMetadata, "UPPERRIGHT" ), &dfURY, &dfURX );
                poDS->ReadCoordinates( CSLFetchNameValue( 
                    poDS->papszGlobalMetadata, "LOWERLEFT" ), &dfLLY, &dfLLX );
                poDS->ReadCoordinates( CSLFetchNameValue( 
                    poDS->papszGlobalMetadata, "LOWERRIGHT" ), &dfLRY, &dfLRX );
                poDS->ReadCoordinates( CSLFetchNameValue( 
                    poDS->papszGlobalMetadata, "SCENECENTER" ),
                                    &dfCenterY, &dfCenterX );
                
                iUTMZone = atoi( CSLFetchNameValue( poDS->papszGlobalMetadata,
                                                    "UTMZONECODE1" ) );
                if( iUTMZone > 0 )
                    oSRS.SetUTM( iUTMZone, TRUE );
                else
                    oSRS.SetUTM( - iUTMZone, FALSE );

                oSRS.SetLinearUnits( SRS_UL_METER, 1.0 );
                if ( poDS->pszProjection )
                    CPLFree( poDS->pszProjection );
                oSRS.exportToWkt( &poDS->pszProjection );

                poDS->ToUTM( &oSRS, &dfULX, &dfULY );
                poDS->ToUTM( &oSRS, &dfURX, &dfURY );
                poDS->ToUTM( &oSRS, &dfLLX, &dfLLY );
                poDS->ToUTM( &oSRS, &dfLRX, &dfLRY );
                
                // Calculate geotransfom matrix from corner coordinates
                GDAL_GCP asGCPs[3];
                GDALInitGCPs( 3, asGCPs );

                asGCPs[0].dfGCPPixel = 0.5;
                asGCPs[0].dfGCPLine = 0.5;
                asGCPs[0].dfGCPX = dfULX;
                asGCPs[0].dfGCPY = dfULY;
                asGCPs[0].dfGCPZ = 0.0;

                asGCPs[1].dfGCPPixel = poDS->nRasterXSize + 0.5;
                asGCPs[1].dfGCPLine = 0.5;
                asGCPs[1].dfGCPX = dfURX;
                asGCPs[1].dfGCPY = dfURY;
                asGCPs[1].dfGCPZ = 0.0;

                asGCPs[2].dfGCPPixel = poDS->nRasterXSize + 0.5;
                asGCPs[2].dfGCPLine = poDS->nRasterYSize + 0.5;
                asGCPs[2].dfGCPX = dfLRX;
                asGCPs[2].dfGCPY = dfLRY;
                asGCPs[2].dfGCPZ = 0.0;

                GDALGCPsToGeoTransform( 3, asGCPs, poDS->adfGeoTransform, TRUE );
                GDALDeinitGCPs( 3, asGCPs );
                poDS->bHasGeoTransfom = TRUE;
            }

/* -------------------------------------------------------------------- */
/*      Read geolocation points.                                        */
/* -------------------------------------------------------------------- */
            double          *pdfLat = NULL, *pdfLong = NULL;

            nXPoints = nYPoints = 0;
            for ( i = 0; i < poDS->nDatasets; i++ )
            {
                iSDS = SDselect( poDS->hSD, i );

                if ( SDgetinfo( iSDS, szName, &iRank, aiDimSizes,
                                &iNumType, &nAttrs) == 0 )
                {
                    if ( EQUALN( szName, "GeodeticLatitude", 16 )
                         && iNumType == DFNT_FLOAT64 )
                    {
                        if ( (nXPoints && nXPoints != aiDimSizes[1])
                             || (nYPoints && nYPoints != aiDimSizes[0]) )
                        {
                            CPLDebug( "HDF4Image",
                                      "Size of latitude array differs from "
                                      "struct metadata, skip reading" );
                            goto CleanupAndBreakAsterL2;
                        }
                        else
                        {
                            nXPoints = aiDimSizes[1];
                            nYPoints = aiDimSizes[0];
                        }

                        iStart[1] = 0;
                        iEdges[1] = nXPoints;

                        iStart[0] = 0;
                        iEdges[0] = nYPoints;
                        pdfLat = (double *)CPLCalloc( nXPoints * nYPoints,
                                                      sizeof( double ) );
                        SDreaddata( iSDS, iStart, NULL, iEdges, pdfLat );
                    }
                    else if ( EQUALN( szName, "Longitude", 9 )
                              && iNumType == DFNT_FLOAT64 )
                    {
                        if ( (nXPoints && nXPoints != aiDimSizes[1])
                             || (nYPoints && nYPoints != aiDimSizes[0]) )
                        {
                            CPLDebug( "HDF4Image",
                                      "Size of longitude array differs from "
                                      "struct metadata, skip reading" );
                            goto CleanupAndBreakAsterL2;
                        }
                        else
                        {
                            nXPoints = aiDimSizes[1];
                            nYPoints = aiDimSizes[0];
                        }

                        iStart[1] = 0;
                        iEdges[1] = nXPoints;

                        iStart[0] = 0;
                        iEdges[0] = nYPoints;
                        pdfLong = (double *)CPLCalloc( nXPoints * nYPoints,
                                                       sizeof( double ) );
                        SDreaddata( iSDS, iStart, NULL, iEdges, pdfLong );
                    }
                }

                poDS->iSDS = SDendaccess( iSDS );
            }

            if ( pdfLat && pdfLong )
            {
                if ( poDS->pszGCPProjection )
                    CPLFree( poDS->pszGCPProjection );
                poDS->pszGCPProjection = CPLStrdup( "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9108\"]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",\"4326\"]]" );

                HDF4EOSDimensionMap *psDMX = NULL, *psDMY = NULL;
                int                 nCount = CPLListCount( poDS->psDataField );

                for ( i = 0; i < nCount; i++ )
                {
                    HDF4EOSDataField *psTemp = (HDF4EOSDataField *)
                        CPLListGetData( CPLListGet( poDS->psDataField, i ) );

                    if ( EQUAL( psTemp->pszDataFieldName, poDS->szName ) )
                    {
                        psDMX = (HDF4EOSDimensionMap *)
                            CPLListGetData(CPLListGet( psTemp->psDimList, 1 ));
                        psDMY = (HDF4EOSDimensionMap *)
                            CPLListGetData(CPLListGet( psTemp->psDimList, 0 ));
                    }
                }

                if ( !psDMX || !psDMY )
                    goto CleanupAndBreakAsterL2;

                CPLDebug( "HDF4Image",
                          "Reading geolocation points: nXPoints=%d, nYPoints=%d, "
                          "OffsetX=%f, IncrementX=%f, OffsetY=%f, IncrementY=%f",
                          nXPoints, nYPoints,
                          psDMX->dfOffset, psDMX->dfIncrement,
                          psDMY->dfOffset, psDMY->dfIncrement );

                poDS->nGCPCount = 0;
                poDS->pasGCPList =
                    (GDAL_GCP *) CPLCalloc( nXPoints * nYPoints,
                                            sizeof( GDAL_GCP ) );
                GDALInitGCPs( nXPoints * nYPoints, poDS->pasGCPList );

                for ( i = 0; i < nYPoints; i++ )
                {
                    for ( j = 0; j < nXPoints; j++ )
                    {
                        int index = i * nXPoints + j;

                        poDS->pasGCPList[index].dfGCPY = pdfLat[index];
                        poDS->pasGCPList[index].dfGCPX = pdfLong[index];
                        poDS->pasGCPList[index].dfGCPZ = 0.0;

                        poDS->pasGCPList[index].dfGCPPixel =
                            j * psDMX->dfIncrement + psDMX->dfOffset + 0.5;
                        poDS->pasGCPList[index].dfGCPLine =
                            i * psDMY->dfIncrement + psDMY->dfOffset + 0.5;
                        poDS->nGCPCount++;
                    }
                }
            }

CleanupAndBreakAsterL2:
            if ( pdfLat )
                CPLFree( pdfLat );
            if ( pdfLong )
                CPLFree( pdfLong );
        }
        break;

/* -------------------------------------------------------------------- */
/*      ASTER DEM product.                                              */
/* -------------------------------------------------------------------- */
        case AST14DEM:
            CPLDebug( "HDF4Image", "Input dataset interpreted as AST14DEM" );

            oSRS.SetWellKnownGeogCS( "WGS84" );
            
            poDS->ReadCoordinates( CSLFetchNameValue( 
                poDS->papszGlobalMetadata, "UPPERLEFT" ), &dfULY, &dfULX );
            poDS->ReadCoordinates( CSLFetchNameValue( 
                poDS->papszGlobalMetadata, "UPPERRIGHT" ), &dfURY, &dfURX );
            poDS->ReadCoordinates( CSLFetchNameValue( 
                poDS->papszGlobalMetadata, "LOWERLEFT" ), &dfLLY, &dfLLX );
            poDS->ReadCoordinates( CSLFetchNameValue( 
                poDS->papszGlobalMetadata, "LOWERRIGHT" ), &dfLRY, &dfLRX );
            poDS->ReadCoordinates( CSLFetchNameValue( 
                poDS->papszGlobalMetadata, "SCENECENTER" ),
                                &dfCenterY, &dfCenterX );
            
            // Calculate UTM zone from scene center coordinates
            iUTMZone = 30 + (int) ((dfCenterX + 6.0) / 6.0);
            if( dfCenterY > 0 )
                oSRS.SetUTM( iUTMZone, TRUE );
            else
                oSRS.SetUTM( - iUTMZone, FALSE );

            oSRS.SetLinearUnits( SRS_UL_METER, 1.0 );
            if ( poDS->pszProjection )
                    CPLFree( poDS->pszProjection );
            oSRS.exportToWkt( &poDS->pszProjection );
            
            poDS->ToUTM( &oSRS, &dfULX, &dfULY );
            poDS->ToUTM( &oSRS, &dfURX, &dfURY );
            poDS->ToUTM( &oSRS, &dfLLX, &dfLLY );
            poDS->ToUTM( &oSRS, &dfLRX, &dfLRY );
            
            // Pixel size always 30 m for ASTER DEMs
            poDS->adfGeoTransform[1] = 30;
            if( dfCenterY > 0 )
                poDS->adfGeoTransform[5] = -30;
            else            
                poDS->adfGeoTransform[5] = 30;

            poDS->adfGeoTransform[0] = dfLLX + poDS->adfGeoTransform[1] / 2;
            poDS->adfGeoTransform[3] = dfULY - poDS->adfGeoTransform[5] / 2;
            poDS->adfGeoTransform[2] = 0.0;
            poDS->adfGeoTransform[4] = 0.0;
            poDS->bHasGeoTransfom = TRUE;
        break;

/* -------------------------------------------------------------------- */
/*      MODIS Level 1B.                                                 */
/* -------------------------------------------------------------------- */
        case MODIS_L1B:
        {
            CPLDebug( "HDF4Image", "Input dataset interpreted as MODIS_L1B" );

            // Read band descriptions and NoData value
            char    **papszBandDesc =
                CSLTokenizeString2( CSLFetchNameValue( poDS->papszLocalMetadata,
                                                       "band_names" ),
                                    ",", CSLT_HONOURSTRINGS );
            i = 0;
            while ( i < poDS->nBands && papszBandDesc[i] )
            {
                GDALRasterBand  *poBand = poDS->GetRasterBand( i + 1 );

                poBand->SetNoDataValue(
                    atof(CSLFetchNameValue( poDS->papszLocalMetadata,
                                            "_FillValue" )) );
                poBand->SetDescription( CPLSPrintf( "%s, band %s",
                    CSLFetchNameValue( poDS->papszLocalMetadata, "long_name" ),
                    papszBandDesc[i++] ) );
            }

            CSLDestroy( papszBandDesc );

/* -------------------------------------------------------------------- */
/*      Read geolocation points.                                        */
/* -------------------------------------------------------------------- */
            float           *pfLat = NULL, *pfLong = NULL;
            GInt16          *piHeight = NULL;
            float           fLatNoData, fLongNoData;

            nXPoints = nYPoints = 0;
            for ( i = 0; i < poDS->nDatasets; i++ )
            {
                int32   iAttribute, nValues;
                char	szAttrName[MAX_NC_NAME];

                iSDS = SDselect( poDS->hSD, i );

                if ( SDgetinfo( iSDS, szName, &iRank, aiDimSizes,
                                &iNumType, &nAttrs) == 0 )
                {
                    if ( EQUALN( szName, "Latitude", 8 )
                         && iNumType == DFNT_FLOAT32 )
                    {
                        if ( (nXPoints && nXPoints != aiDimSizes[1])
                             || (nYPoints && nYPoints != aiDimSizes[0]) )
                        {
                            // Error: geolocation arrays has different sizes
                            break;
                        }
                        else
                        {
                            nXPoints = aiDimSizes[1];
                            nYPoints = aiDimSizes[0];
                        }

                        iStart[1] = 0;
                        iEdges[1] = nXPoints;

                        iStart[0] = 0;
                        iEdges[0] = nYPoints;
                        pfLat = (float *)CPLMalloc( nXPoints * nYPoints *
                                                    sizeof( float ) );
                        SDreaddata( iSDS, iStart, NULL, iEdges, pfLat );

                        iAttribute = SDfindattr( iSDS, "_FillValue" );
                        SDattrinfo( iSDS, iAttribute, szAttrName,
                                    &iNumType, &nValues );
                        if ( iNumType == DFNT_FLOAT32 && nValues == 1 )
                        {
                            SDreadattr( iSDS, iAttribute, &fLatNoData );
                            bLatNoDataSet = TRUE;
                        }
                    }
                    else if ( EQUALN( szName, "Longitude", 9 )
                              && iNumType == DFNT_FLOAT32 )
                    {
                        if ( (nXPoints && nXPoints != aiDimSizes[1])
                             || (nYPoints && nYPoints != aiDimSizes[0]) )
                        {
                            // Error: geolocation arrays has different sizes
                            break;
                        }
                        else
                        {
                            nXPoints = aiDimSizes[1];
                            nYPoints = aiDimSizes[0];
                        }

                        iStart[1] = 0;
                        iEdges[1] = nXPoints;

                        iStart[0] = 0;
                        iEdges[0] = nYPoints;
                        pfLong = (float *)CPLMalloc( nXPoints * nYPoints *
                                                     sizeof( float ) );
                        SDreaddata( iSDS, iStart, NULL, iEdges, pfLong );

                        iAttribute = SDfindattr( iSDS, "_FillValue" );
                        SDattrinfo( iSDS, iAttribute, szAttrName,
                                    &iNumType, &nValues );
                        if ( iNumType == DFNT_FLOAT32 && nValues == 1 )
                        {
                            SDreadattr( iSDS, iAttribute, &fLongNoData );
                            bLongNoDataSet = TRUE;
                        }
                    }
                    else if ( EQUALN( szName, "Height", 6 )
                              && iNumType == DFNT_INT16 )
                    {
                        if ( (nXPoints && nXPoints != aiDimSizes[1])
                             || (nYPoints && nYPoints != aiDimSizes[0]) )
                        {
                            // Error: geolocation arrays has different sizes
                            break;
                        }
                        else
                        {
                            nXPoints = aiDimSizes[1];
                            nYPoints = aiDimSizes[0];
                        }

                        iStart[1] = 0;
                        iEdges[1] = nXPoints;

                        iStart[0] = 0;
                        iEdges[0] = nYPoints;
                        piHeight = (GInt16 *)CPLMalloc( nXPoints * nYPoints *
                                                       sizeof( GInt16 ) );
                        SDreaddata( iSDS, iStart, NULL, iEdges, piHeight );

                        iAttribute = SDfindattr( iSDS, "_FillValue" );
                        SDattrinfo( iSDS, iAttribute, szAttrName,
                                    &iNumType, &nValues );
                        if ( iNumType == DFNT_INT16 && nValues == 1 )
                        {
                            SDreadattr( iSDS, iAttribute, &iHeightNoData );
                            bHeightNoDataSet = TRUE;
                        }
                    }
                }

                poDS->iSDS = SDendaccess( iSDS );
            }

            if ( pfLat && pfLong )
            {
                if ( poDS->pszGCPProjection )
                    CPLFree( poDS->pszGCPProjection );
                poDS->pszGCPProjection = CPLStrdup( "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9108\"]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",\"4326\"]]" );

                HDF4EOSDimensionMap *psDMX = NULL, *psDMY = NULL;
                int                 nCount = CPLListCount( poDS->psDataField );

                for ( i = 0; i < nCount; i++ )
                {
                    HDF4EOSDataField *psTemp = (HDF4EOSDataField *)
                        CPLListGetData( CPLListGet( poDS->psDataField, i ) );

                    if ( EQUAL( psTemp->pszDataFieldName, poDS->szName ) )
                    {
                        psDMX = (HDF4EOSDimensionMap *)
                            CPLListGetData(CPLListGet( psTemp->psDimList, 1 ));
                        psDMY = (HDF4EOSDimensionMap *)
                            CPLListGetData(CPLListGet( psTemp->psDimList, 0 ));
                    }
                }

                if ( !psDMX || !psDMY )
                    break;

                CPLDebug( "HDF4Image",
                          "Reading geolocation points: nXPoints=%d, nYPoints=%d, "
                          "OffsetX=%f, IncrementX=%f, OffsetY=%f, IncrementY=%f",
                          nXPoints, nYPoints,
                          psDMX->dfOffset, psDMX->dfIncrement,
                          psDMY->dfOffset, psDMY->dfIncrement );
                
                poDS->nGCPCount = 0;
                poDS->pasGCPList =
                    (GDAL_GCP *) CPLCalloc( nXPoints * nYPoints,
                                            sizeof(GDAL_GCP) );
                GDALInitGCPs( nXPoints * nYPoints, poDS->pasGCPList );

                for ( i = 0; i < nYPoints; i++ )
                {
                    for ( j = 0; j < nXPoints; j++ )
                    {
                        int index = i * nXPoints + j;

                        if ( (bLatNoDataSet && pfLat[index] == fLatNoData) ||
                             (bLongNoDataSet && pfLong[index] == fLongNoData) )
                            continue;

                        poDS->pasGCPList[index].dfGCPY = pfLat[index];
                        poDS->pasGCPList[index].dfGCPX = pfLong[index];
                        if ( piHeight && (bHeightNoDataSet &&
                                          piHeight[index] != iHeightNoData) )
                            poDS->pasGCPList[index].dfGCPZ = piHeight[index];
                        else 
                            poDS->pasGCPList[index].dfGCPZ = 0.0;
                        poDS->pasGCPList[index].dfGCPPixel =
                            j * psDMX->dfIncrement + psDMX->dfOffset + 0.5;
                        poDS->pasGCPList[index].dfGCPLine =
                            i * psDMY->dfIncrement + psDMY->dfOffset + 0.5;
                        poDS->nGCPCount++;
                    }
                }
            }

            if ( pfLat )
                CPLFree( pfLat );
            if ( pfLong )
                CPLFree( pfLong );
            if ( piHeight )
                CPLFree( piHeight );
        }
        break;

/* -------------------------------------------------------------------- */
/*      MODIS Level 3.                                                  */
/* -------------------------------------------------------------------- */
        case MODIS_L3:
        {
            CPLDebug( "HDF4Image", "Input dataset interpreted as MODIS_L3" );

            // Read band descriptions and NoData value
            pszValue = CSLFetchNameValue( poDS->papszGlobalMetadata,
                                          "SHORTNAME" );
            for ( i = 1; i <= poDS->nBands; i++ )
            {
                GDALRasterBand  *poBand = poDS->GetRasterBand( i );

                // In case of mean, standard deviation or number of
                // observations map types NoData value indicated by the 0.0
                if ( *(pszValue + 4) == 'M' || *(pszValue + 4) == 'S'
                      || *(pszValue + 4) == 'N' )
                    poBand->SetNoDataValue( 0.0 );
                // In other case NoData filled with value 255
                else
                    poBand->SetNoDataValue( 255.0 );
                poBand->SetDescription(
                    CSLFetchNameValue( poDS->papszLocalMetadata, "Long_name" ) );
            }

            // Read coordinate system and geotransform matrix
            oSRS.SetWellKnownGeogCS( "WGS84" );
            
            if ( EQUAL(CSLFetchNameValue(poDS->papszGlobalMetadata,
                                         "Map Projection"),
                       "Equidistant Cylindrical") )
            {
                oSRS.SetEquirectangular( 0.0, 0.0, 0.0, 0.0 );
                oSRS.SetLinearUnits( SRS_UL_METER, 1 );
                if ( poDS->pszProjection )
                    CPLFree( poDS->pszProjection );
                oSRS.exportToWkt( &poDS->pszProjection );
            }

            dfULX = atof( CSLFetchNameValue(poDS->papszGlobalMetadata,
                                            "Westernmost Longitude") );
            dfULY = atof( CSLFetchNameValue(poDS->papszGlobalMetadata,
                                            "Northernmost Latitude") );
            dfLRX = atof( CSLFetchNameValue(poDS->papszGlobalMetadata,
                                            "Easternmost Longitude") );
            dfLRY = atof( CSLFetchNameValue(poDS->papszGlobalMetadata,
                                            "Southernmost Latitude") );
            poDS->ToUTM( &oSRS, &dfULX, &dfULY );
            poDS->ToUTM( &oSRS, &dfLRX, &dfLRY );
            poDS->adfGeoTransform[0] = dfULX;
            poDS->adfGeoTransform[3] = dfULY;
            poDS->adfGeoTransform[1] = (dfLRX - dfULX) / poDS->nRasterXSize;
            poDS->adfGeoTransform[5] = (dfULY - dfLRY) / poDS->nRasterYSize;
            if ( dfULY > 0)     // Northern hemisphere
                poDS->adfGeoTransform[5] = - poDS->adfGeoTransform[5];
            poDS->adfGeoTransform[2] = 0.0;
            poDS->adfGeoTransform[4] = 0.0;
            poDS->bHasGeoTransfom = TRUE;
        }
        break;

/* -------------------------------------------------------------------- */
/*      SeaWiFS Level 3 Standard Mapped Image Products.                 */
/*      Organized similar to MODIS Level 3 products.                    */
/* -------------------------------------------------------------------- */
        case SEAWIFS_L3:
        {
            CPLDebug( "HDF4Image", "Input dataset interpreted as SEAWIFS_L3" );

            // Read band description
            for ( i = 1; i <= poDS->nBands; i++ )
            {
                poDS->GetRasterBand( i )->SetDescription(
                    CSLFetchNameValue( poDS->papszGlobalMetadata, "Parameter" ) );
            }

            // Read coordinate system and geotransform matrix
            oSRS.SetWellKnownGeogCS( "WGS84" );
            
            if ( EQUAL(CSLFetchNameValue(poDS->papszGlobalMetadata,
                                         "Map Projection"),
                       "Equidistant Cylindrical") )
            {
                oSRS.SetEquirectangular( 0.0, 0.0, 0.0, 0.0 );
                oSRS.SetLinearUnits( SRS_UL_METER, 1 );
                if ( poDS->pszProjection )
                    CPLFree( poDS->pszProjection );
                oSRS.exportToWkt( &poDS->pszProjection );
            }

            dfULX = atof( CSLFetchNameValue(poDS->papszGlobalMetadata,
                                            "Westernmost Longitude") );
            dfULY = atof( CSLFetchNameValue(poDS->papszGlobalMetadata,
                                            "Northernmost Latitude") );
            dfLRX = atof( CSLFetchNameValue(poDS->papszGlobalMetadata,
                                            "Easternmost Longitude") );
            dfLRY = atof( CSLFetchNameValue(poDS->papszGlobalMetadata,
                                            "Southernmost Latitude") );
            poDS->ToUTM( &oSRS, &dfULX, &dfULY );
            poDS->ToUTM( &oSRS, &dfLRX, &dfLRY );
            poDS->adfGeoTransform[0] = dfULX;
            poDS->adfGeoTransform[3] = dfULY;
            poDS->adfGeoTransform[1] = (dfLRX - dfULX) / poDS->nRasterXSize;
            poDS->adfGeoTransform[5] = (dfULY - dfLRY) / poDS->nRasterYSize;
            if ( dfULY > 0)     // Northern hemisphere
                poDS->adfGeoTransform[5] = - poDS->adfGeoTransform[5];
            poDS->adfGeoTransform[2] = 0.0;
            poDS->adfGeoTransform[4] = 0.0;
            poDS->bHasGeoTransfom = TRUE;
        }
        break;

/* -------------------------------------------------------------------- */
/*      Hyperion Level 1.                                               */
/* -------------------------------------------------------------------- */
        case HYPERION_L1:
        {
            CPLDebug( "HDF4Image", "Input dataset interpreted as HYPERION_L1" );
        }
        break;

        default:
        break;
    }

    return( poDS );
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *HDF4ImageDataset::Create( const char * pszFilename,
                                       int nXSize, int nYSize, int nBands,
                                       GDALDataType eType,
                                       char **papszOptions )

{
/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    HDF4ImageDataset    *poDS;
    const char          *pszSDSName;
    int                 iBand;
    int32               aiDimSizes[MAX_VAR_DIMS];

    poDS = new HDF4ImageDataset();

/* -------------------------------------------------------------------- */
/*      Choose rank for the created dataset.                            */
/* -------------------------------------------------------------------- */
    poDS->iRank = 3;
    if ( CSLFetchNameValue( papszOptions, "RANK" ) != NULL &&
         EQUAL( CSLFetchNameValue( papszOptions, "RANK" ), "2" ) )
        poDS->iRank = 2;
    
    poDS->hSD = SDstart( pszFilename, DFACC_CREATE );
    if ( poDS->hSD == -1 )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Can't create HDF4 file %s", pszFilename );
        return NULL;
    }
    poDS->iXDim = 1;
    poDS->iYDim = 0;
    poDS->iBandDim = 2;
    aiDimSizes[poDS->iXDim] = nXSize;
    aiDimSizes[poDS->iYDim] = nYSize;
    aiDimSizes[poDS->iBandDim] = nBands;

    if ( poDS->iRank == 2 )
    {
        for ( iBand = 0; iBand < nBands; iBand++ )
        {
            pszSDSName = CPLSPrintf( "Band%d", iBand );
            switch ( eType )
            {
                case GDT_Float64:
                poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_FLOAT64,
                                       poDS->iRank, aiDimSizes );
                break;
                case GDT_Float32:
                poDS->iSDS = SDcreate(poDS-> hSD, pszSDSName, DFNT_FLOAT32,
                                      poDS->iRank, aiDimSizes );
                break;
                case GDT_UInt32:
                poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_UINT32,
                                       poDS->iRank, aiDimSizes );
                break;
                case GDT_UInt16:
                poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_UINT16,
                                       poDS->iRank, aiDimSizes );
                break;
                case GDT_Int32:
                poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_INT32,
                                       poDS->iRank, aiDimSizes );
                break;
                case GDT_Int16:
                poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_INT16,
                                       poDS->iRank, aiDimSizes );
                break;
                case GDT_Byte:
                default:
                poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_UINT8,
                                       poDS->iRank, aiDimSizes );
                break;
            }
            poDS->iSDS = SDendaccess( poDS->iSDS );
        }
    }
    else if ( poDS->iRank == 3 )
    {
        pszSDSName = "3-dimensional Scientific Dataset";
        switch ( eType )
        {
            case GDT_Float64:
            poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_FLOAT64,
                                   poDS->iRank, aiDimSizes );
            break;
            case GDT_Float32:
            poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_FLOAT32,
                                   poDS->iRank, aiDimSizes );
            break;
            case GDT_UInt32:
            poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_UINT32,
                                   poDS->iRank, aiDimSizes );
            break;
            case GDT_UInt16:
            poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_UINT16,
                                   poDS->iRank, aiDimSizes );
            break;
            case GDT_Int32:
            poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_INT32,
                                   poDS->iRank, aiDimSizes );
            break;
            case GDT_Int16:
            poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_INT16,
                                   poDS->iRank, aiDimSizes );
            break;
            case GDT_Byte:
            default:
            poDS->iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_UINT8,
                                   poDS->iRank, aiDimSizes );
            break;
        }
    }
    else                                            // Should never happen
        return NULL;

    if ( poDS->iSDS < 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Can't create SDS with rank %d for file %s",
                  poDS->iRank, pszFilename );
        return NULL;
    }

    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->eAccess = GA_Update;
    poDS->iSubdatasetType = HDF4_SDS;
    poDS->iDataType = GDAL_HDF4;
    poDS->nBands = nBands;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( iBand = 1; iBand <= nBands; iBand++ )
        poDS->SetBand( iBand, new HDF4ImageRasterBand( poDS, iBand, eType ) );

    SDsetattr( poDS->hSD, "Signature", DFNT_CHAR8, strlen(pszGDALSignature) + 1,
               pszGDALSignature );
    
    return (GDALDataset *) poDS;
}

/************************************************************************/
/*                        GDALRegister_HDF4Image()                      */
/************************************************************************/

void GDALRegister_HDF4Image()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "HDF4Image" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "HDF4Image" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "HDF4 Dataset" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_hdf4.html" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16 UInt16 Int32 UInt32 Float32 Float64" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, 
"<CreationOptionList>"
"   <Option name='RANK' type='int' description='Rank of output SDS'/>"
"</CreationOptionList>" );

        poDriver->pfnOpen = HDF4ImageDataset::Open;
        poDriver->pfnCreate = HDF4ImageDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

