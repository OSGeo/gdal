/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 4 (HDF4)
 * Purpose:  Read subdatasets of HDF4 file.
 *           This driver initially based on code supplied by Markus Neteler
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <dron@ak4719.spb.edu>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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

#if defined(_WIN32)
// min/max are defined here on Windows, so block them.
// TODO: Move this to someplace more appropriate.
#  define NOMINMAX
#endif

#include <string.h>
#include <math.h>

#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_priv.h"
#include "ogr_spatialref.h"

#include "hdf.h"
#include "mfhdf.h"

#include "HdfEosDef.h"

#include "hdf4compat.h"
#include "hdf4dataset.h"

#include "nasakeywordhandler.h"

#include <algorithm>

CPL_CVSID("$Id$")

constexpr int HDF4_SDS_MAXNAMELEN = 65;

extern const char * const pszGDALSignature;

// Signature to recognize files written by GDAL.
const char * const pszGDALSignature =
    "Created with GDAL (http://www.remotesensing.org/gdal/)";

extern CPLMutex *hHDF4Mutex;

constexpr int N_BUF_SIZE = 8192;

/************************************************************************/
/* ==================================================================== */
/*  List of HDF-EOS Swath product types.                                */
/* ==================================================================== */
/************************************************************************/

enum HDF4EOSProduct
{
    PROD_UNKNOWN,
    PROD_ASTER_L1A,
    PROD_ASTER_L1B,
    PROD_ASTER_L2,
    PROD_ASTER_L3,
    PROD_AST14DEM,
    PROD_MODIS_L1B,
    PROD_MODIS_L2
};

/************************************************************************/
/* ==================================================================== */
/*                              HDF4ImageDataset                        */
/* ==================================================================== */
/************************************************************************/

constexpr int N_COLOR_ENTRIES = 256;

class HDF4ImageDataset final: public HDF4Dataset
{
    friend class HDF4ImageRasterBand;

    char        *pszFilename;
    int32       hHDF4;
    int32       iGR;
    int32       iPal;
    int32       iDataset;
    int32       iRank;
    int32       iNumType;
    int32       nAttrs;
    int32       iInterlaceMode;
    int32       iPalInterlaceMode;
    int32       iPalDataType;
    int32       nComps;
    int32       nPalEntries;
    int32       aiDimSizes[H4_MAX_VAR_DIMS];
    int         iXDim;
    int         iYDim;
    int         iBandDim;
    int         i4Dim;
    int         nBandCount;
    char        **papszLocalMetadata;
    uint8       aiPaletteData[N_COLOR_ENTRIES][3]; // XXX: Static array for now
    char        szName[HDF4_SDS_MAXNAMELEN];
    char        *pszSubdatasetName;
    char        *pszFieldName;

    GDALColorTable *poColorTable;

    OGRSpatialReference oSRS;
    bool        bHasGeoTransform;
    double      adfGeoTransform[6];
    char        *pszProjection;
    char        *pszGCPProjection;
    GDAL_GCP    *pasGCPList;
    int         nGCPCount;

    HDF4DatasetType iDatasetType;

    int32       iSDS;

    int         nBlockPreferredXSize;
    int         nBlockPreferredYSize;
    bool        bReadTile;

    void                ToGeoref( double *, double * );
    void                GetImageDimensions( char * );
    void                GetSwatAttrs( int32 );
    void                GetGridAttrs( int32 hGD );
    void                CaptureNRLGeoTransform(void);
    void                CaptureL1GMTLInfo(void);
    void                CaptureCoastwatchGCTPInfo(void);
    void                ProcessModisSDSGeolocation(void);
    int                 ProcessSwathGeolocation( int32, char ** );

    static long         USGSMnemonicToCode( const char* );
    static void         ReadCoordinates( const char*, double*, double* );

  public:
                HDF4ImageDataset();
    virtual ~HDF4ImageDataset();

    static GDALDataset  *Open( GDALOpenInfo * );
    static GDALDataset  *Create( const char * pszFilename,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType, char ** papszParamList );
    virtual void        FlushCache( bool bAtClosing ) override;
    CPLErr              GetGeoTransform( double * padfTransform ) override;
    virtual CPLErr      SetGeoTransform( double * ) override;
    const char          *_GetProjectionRef() override;
    const OGRSpatialReference* GetSpatialRef() const override {
        return GetSpatialRefFromOldGetProjectionRef();
    }
    virtual CPLErr      _SetProjection( const char * ) override;
    CPLErr SetSpatialRef(const OGRSpatialReference* poSRS) override {
        return OldSetProjectionFromSetSpatialRef(poSRS);
    }
    virtual int         GetGCPCount() override;
    virtual const char  *_GetGCPProjection() override;
    const OGRSpatialReference* GetGCPSpatialRef() const override {
        return GetGCPSpatialRefFromOldGetGCPProjection();
    }
    virtual const GDAL_GCP *GetGCPs() override;
};

/************************************************************************/
/* ==================================================================== */
/*                            HDF4ImageRasterBand                       */
/* ==================================================================== */
/************************************************************************/

class HDF4ImageRasterBand final: public GDALPamRasterBand
{
    friend class HDF4ImageDataset;

    bool        bNoDataSet;
    double      dfNoDataValue;

    bool        bHaveScale;
    bool        bHaveOffset;
    double      dfScale;
    double      dfOffset;

    CPLString   osUnitType;

  public:
                HDF4ImageRasterBand( HDF4ImageDataset *, int, GDALDataType );
    virtual ~HDF4ImageRasterBand() {}

    virtual CPLErr          IReadBlock( int, int, void * ) override;
    virtual CPLErr          IWriteBlock( int, int, void * ) override;
    virtual GDALColorInterp GetColorInterpretation() override;
    virtual GDALColorTable *GetColorTable() override;
    virtual double          GetNoDataValue( int * ) override;
    virtual CPLErr          SetNoDataValue( double ) override;
    virtual double          GetOffset( int *pbSuccess ) override;
    virtual double          GetScale( int *pbSuccess ) override;
    virtual const char     *GetUnitType() override;
};

/************************************************************************/
/*                           HDF4ImageRasterBand()                      */
/************************************************************************/

HDF4ImageRasterBand::HDF4ImageRasterBand( HDF4ImageDataset *poDSIn, int nBandIn,
                                          GDALDataType eType ) :
    bNoDataSet(false),
    dfNoDataValue(-9999.0),
    bHaveScale(false),
    bHaveOffset(false),
    dfScale(1.0),
    dfOffset(0.0)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = eType;

    nBlockXSize = poDSIn->GetRasterXSize();

    // Aim for a block of about 1000000 pixels.  Chunking up substantially
    // improves performance in some situations.  For now we only chunk up for
    // SDS and EOS based datasets since other variations haven't been
    // tested. #2208
    if( poDSIn->iDatasetType == HDF4_SDS ||
        poDSIn->iDatasetType == HDF4_EOS)
    {
        const int nChunkSize =
            atoi( CPLGetConfigOption("HDF4_BLOCK_PIXELS", "1000000") );

        nBlockYSize = nChunkSize / poDSIn->GetRasterXSize();
        nBlockYSize =
            std::max(1, std::min(nBlockYSize, poDSIn->GetRasterYSize()));
    }
    else
    {
        nBlockYSize = 1;
    }

    // HDF4_EOS:EOS_GRID case. We ensure that the block size matches
    // the raster width, as the IReadBlock() code can only handle multiple
    // blocks per row.
    if( poDSIn->nBlockPreferredXSize == nBlockXSize &&
         poDSIn->nBlockPreferredYSize > 0 )
    {
        if( poDSIn->nBlockPreferredYSize == 1 )
        {
            // Avoid defaulting to tile reading when the preferred height is 1
            // as it leads to very poor performance with:
            // ftp://e4ftl01u.ecs.nasa.gov/MODIS_Composites/MOLT/MOD13Q1.005/2006.06.10/MOD13Q1.A2006161.h21v13.005.2008234103220.hd
            poDSIn->bReadTile = false;
        }
        else
        {
            nBlockYSize = poDSIn->nBlockPreferredYSize;
        }
    }

/* -------------------------------------------------------------------- */
/*      We need to avoid using the tile based api if we aren't          */
/*      matching the tile size. (#4672)                                 */
/* -------------------------------------------------------------------- */
    if( nBlockXSize != poDSIn->nBlockPreferredXSize
        || nBlockYSize != poDSIn->nBlockPreferredYSize )
    {
        poDSIn->bReadTile = false;
    }
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr HDF4ImageRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                        void * pImage )
{
    CPLAssert( nBlockXOff == 0 );
    HDF4ImageDataset *poGDS = reinterpret_cast<HDF4ImageDataset *>( poDS );

    CPLMutexHolderD(&hHDF4Mutex);

    if( poGDS->eAccess == GA_Update )
    {
        memset( pImage, 0,
                nBlockXSize * nBlockYSize
                * GDALGetDataTypeSizeBytes(eDataType) );
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Work out some block oriented details.                           */
/* -------------------------------------------------------------------- */
    const int nYOff = nBlockYOff * nBlockYSize;
    const int nYSize =
        std::min(nYOff + nBlockYSize, poDS->GetRasterYSize()) - nYOff;

/* -------------------------------------------------------------------- */
/*      HDF files with external data files, such as some landsat        */
/*      products (eg. data/hdf/L1G) need to be told what directory      */
/*      to look in to find the external files.  Normally this is the    */
/*      directory holding the hdf file.                                 */
/* -------------------------------------------------------------------- */
    HXsetdir(CPLGetPath(poGDS->pszFilename));

/* -------------------------------------------------------------------- */
/*      Handle different configurations.                                */
/* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;
    int32 aiStart[H4_MAX_NC_DIMS] = {};
    int32 aiEdges[H4_MAX_NC_DIMS] = {};

    switch ( poGDS->iDatasetType )
    {
      case HDF4_SDS:
      {
          // We avoid doing SDselect() / SDendaccess() for each block access
          // as this is very slow when zlib compression is used.

          if( poGDS->iSDS == FAIL )
              poGDS->iSDS = SDselect( poGDS->hSD, poGDS->iDataset );

          /* HDF rank:
             A rank 2 dataset is an image read in scan-line order (2D).
             A rank 3 dataset is a series of images which are read in
             an image at a time to form a volume.
             A rank 4 dataset may be thought of as a series of volumes.

             The "aiStart" array specifies the multi-dimensional index of the
             starting corner of the hyperslab to read. The values are zero
             based.

             The "edge" array specifies the number of values to read along
             each dimension of the hyperslab.

             The "iStride" array allows for sub-sampling along each
             dimension. If a iStride value is specified for a dimension,
             that many values will be skipped over when reading along that
             dimension. Specifying iStride = NULL in the C interface or
             iStride = 1 in either interface specifies contiguous reading
             of data. If the iStride values are set to 0, SDreaddata
             returns FAIL (or -1). No matter what iStride value is
             provided, data is always placed contiguously in buffer.
          */
          switch ( poGDS->iRank )
          {
            case 4:     // 4Dim: volume-time
                        // FIXME: needs sample file. Does not work currently.
              aiStart[3] = 0;  // range: 0--aiDimSizes[3]-1
              aiEdges[3] = 1;
              aiStart[2] = 0;  // range: 0--aiDimSizes[2]-1
              aiEdges[2] = 1;
              aiStart[1] = nYOff;
              aiEdges[1] = nYSize;
              aiStart[0] = nBlockXOff;
              aiEdges[0] = nBlockXSize;
              break;
            case 3: // 3Dim: volume
              aiStart[poGDS->iBandDim] = nBand - 1;
              aiEdges[poGDS->iBandDim] = 1;

              aiStart[poGDS->iYDim] = nYOff;
              aiEdges[poGDS->iYDim] = nYSize;

              aiStart[poGDS->iXDim] = nBlockXOff;
              aiEdges[poGDS->iXDim] = nBlockXSize;
              break;
            case 2: // 2Dim: rows/cols
              aiStart[poGDS->iYDim] = nYOff;
              aiEdges[poGDS->iYDim] = nYSize;

              aiStart[poGDS->iXDim] = nBlockXOff;
              aiEdges[poGDS->iXDim] = nBlockXSize;
              break;
            case 1: //1Dim:
              aiStart[poGDS->iXDim] = nBlockXOff;
              aiEdges[poGDS->iXDim] = nBlockXSize;
              break;
          }

          // Read HDF SDS array
          if( SDreaddata( poGDS->iSDS, aiStart, nullptr, aiEdges, pImage ) < 0 )
          {
              CPLError( CE_Failure, CPLE_AppDefined,
                        "SDreaddata() failed for block." );
              eErr = CE_Failure;
          }

          //SDendaccess( l_iSDS );
      }
      break;

      case HDF4_GR:
      {
          const int nDataTypeSize =
              GDALGetDataTypeSizeBytes(poGDS->GetDataType(poGDS->iNumType));
          GByte *pbBuffer = reinterpret_cast<GByte *>(
              CPLMalloc(nBlockXSize*nBlockYSize*poGDS->iRank*nDataTypeSize) );

          aiStart[poGDS->iYDim] = nYOff;
          aiEdges[poGDS->iYDim] = nYSize;

          aiStart[poGDS->iXDim] = nBlockXOff;
          aiEdges[poGDS->iXDim] = nBlockXSize;

          if( GRreadimage(poGDS->iGR, aiStart, nullptr, aiEdges, pbBuffer) < 0 )
          {
              CPLError( CE_Failure, CPLE_AppDefined,
                        "GRreaddata() failed for block." );
              eErr = CE_Failure;
          }
          else
          {
              for ( int i = 0, j = (nBand - 1) * nDataTypeSize;
                    i < nBlockXSize * nDataTypeSize;
                    i += nDataTypeSize, j += poGDS->nBands * nDataTypeSize )
                  memcpy( reinterpret_cast<GByte *>( pImage ) + i,
                          pbBuffer + j, nDataTypeSize );
          }

          CPLFree( pbBuffer );
      }
      break;

      case HDF4_EOS:
      {
          switch ( poGDS->iSubdatasetType )
          {
            case H4ST_EOS_GRID:
            {
                const int32 hGD = GDattach( poGDS->hHDF4,
                                      poGDS->pszSubdatasetName );
                switch ( poGDS->iRank )
                {
                  case 4: // 4Dim: volume
                    aiStart[poGDS->i4Dim] =
                        (nBand - 1)
                        / poGDS->aiDimSizes[poGDS->iBandDim];
                    aiEdges[poGDS->i4Dim] = 1;

                    aiStart[poGDS->iBandDim] =
                        (nBand - 1)
                        % poGDS->aiDimSizes[poGDS->iBandDim];
                    aiEdges[poGDS->iBandDim] = 1;

                    aiStart[poGDS->iYDim] = nYOff;
                    aiEdges[poGDS->iYDim] = nYSize;

                    aiStart[poGDS->iXDim] = nBlockXOff;
                    aiEdges[poGDS->iXDim] = nBlockXSize;
                    break;
                  case 3: // 3Dim: volume
                    aiStart[poGDS->iBandDim] = nBand - 1;
                    aiEdges[poGDS->iBandDim] = 1;

                    aiStart[poGDS->iYDim] = nYOff;
                    aiEdges[poGDS->iYDim] = nYSize;

                    aiStart[poGDS->iXDim] = nBlockXOff;
                    aiEdges[poGDS->iXDim] = nBlockXSize;
                    break;
                  case 2: // 2Dim: rows/cols
                    aiStart[poGDS->iYDim] = nYOff;
                    aiEdges[poGDS->iYDim] = nYSize;

                    aiStart[poGDS->iXDim] = nBlockXOff;
                    aiEdges[poGDS->iXDim] = nBlockXSize;
                    break;
                }

                /* Ensure that we don't overlap the bottom or right edges */
                /* of the dataset in order to use the GDreadtile() API */
                if( poGDS->bReadTile &&
                    (nBlockXOff + 1) * nBlockXSize <= nRasterXSize &&
                    (nBlockYOff + 1) * nBlockYSize <= nRasterYSize )
                {
                    int32 tilecoords[] = { nBlockYOff , nBlockXOff };
                    if( GDreadtile( hGD, poGDS->pszFieldName,
                                    tilecoords, pImage ) != 0 )
                    {
                        CPLError( CE_Failure, CPLE_AppDefined,
                                  "GDreadtile() failed for block." );
                        eErr = CE_Failure;
                    }
                }
                else if( GDreadfield( hGD, poGDS->pszFieldName,
                                aiStart, nullptr, aiEdges, pImage ) < 0 )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "GDreadfield() failed for block." );
                    eErr = CE_Failure;
                }
                GDdetach( hGD );
            }
            break;

            case H4ST_EOS_SWATH:
            case H4ST_EOS_SWATH_GEOL:
            {
                const int32 hSW = SWattach( poGDS->hHDF4,
                                            poGDS->pszSubdatasetName );
                switch ( poGDS->iRank )
                {
                  case 3: // 3Dim: volume
                    aiStart[poGDS->iBandDim] = nBand - 1;
                    aiEdges[poGDS->iBandDim] = 1;

                    aiStart[poGDS->iYDim] = nYOff;
                    aiEdges[poGDS->iYDim] = nYSize;

                    aiStart[poGDS->iXDim] = nBlockXOff;
                    aiEdges[poGDS->iXDim] = nBlockXSize;
                    break;
                  case 2: // 2Dim: rows/cols
                    aiStart[poGDS->iYDim] = nYOff;
                    aiEdges[poGDS->iYDim] = nYSize;

                    aiStart[poGDS->iXDim] = nBlockXOff;
                    aiEdges[poGDS->iXDim] = nBlockXSize;
                    break;
                }
                if( SWreadfield( hSW, poGDS->pszFieldName,
                                 aiStart, nullptr, aiEdges, pImage ) < 0 )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "SWreadfield() failed for block." );
                    eErr = CE_Failure;
                }
                SWdetach( hSW );
            }
            break;
            default:
              break;
          }
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
    CPLAssert( nBlockXOff == 0 );
    CPLAssert( nBlockYOff >= 0 );
    CPLAssert( pImage != nullptr );

    HDF4ImageDataset *poGDS = reinterpret_cast<HDF4ImageDataset *>( poDS );
    CPLAssert( poGDS != nullptr );

    int32 aiStart[H4_MAX_NC_DIMS] = {};
    int32 aiEdges[H4_MAX_NC_DIMS] = {};
    CPLErr eErr = CE_None;

    CPLMutexHolderD(&hHDF4Mutex);

/* -------------------------------------------------------------------- */
/*      Work out some block oriented details.                           */
/* -------------------------------------------------------------------- */
    const int nYOff = nBlockYOff * nBlockYSize;
    const int nYSize =
        std::min(nYOff + nBlockYSize, poDS->GetRasterYSize()) - nYOff;

/* -------------------------------------------------------------------- */
/*      Process based on rank.                                          */
/* -------------------------------------------------------------------- */
    switch ( poGDS->iRank )
    {
        case 3:
            {
                const int32 l_iSDS = SDselect( poGDS->hSD, poGDS->iDataset );

                aiStart[poGDS->iBandDim] = nBand - 1;
                aiEdges[poGDS->iBandDim] = 1;

                aiStart[poGDS->iYDim] = nYOff;
                aiEdges[poGDS->iYDim] = nYSize;

                aiStart[poGDS->iXDim] = nBlockXOff;
                aiEdges[poGDS->iXDim] = nBlockXSize;

                if( (SDwritedata( l_iSDS, aiStart, nullptr,
                                  aiEdges, (VOIDP)pImage )) < 0 )
                    eErr = CE_Failure;

                SDendaccess( l_iSDS );
            }
            break;

        case 2:
            {
                const int32 l_iSDS = SDselect( poGDS->hSD, nBand - 1 );
                aiStart[poGDS->iYDim] = nYOff;
                aiEdges[poGDS->iYDim] = nYSize;

                aiStart[poGDS->iXDim] = nBlockXOff;
                aiEdges[poGDS->iXDim] = nBlockXSize;

                if( (SDwritedata( l_iSDS, aiStart, nullptr,
                                  aiEdges, (VOIDP)pImage )) < 0 )
                    eErr = CE_Failure;

                SDendaccess( l_iSDS );
            }
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
    HDF4ImageDataset *poGDS = reinterpret_cast<HDF4ImageDataset *>( poDS );

    return poGDS->poColorTable;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp HDF4ImageRasterBand::GetColorInterpretation()
{
    HDF4ImageDataset *poGDS = reinterpret_cast<HDF4ImageDataset *>( poDS );

    if( poGDS->iDatasetType == HDF4_SDS )
    {
        return GCI_GrayIndex;
    }
    else if( poGDS->iDatasetType == HDF4_GR )
    {
        if( poGDS->poColorTable != nullptr )
        {
            return GCI_PaletteIndex;
        }
        else if( poGDS->nBands != 1 )
        {
            if( nBand == 1 )
                return GCI_RedBand;
            else if( nBand == 2 )
                return GCI_GreenBand;
            else if( nBand == 3 )
                return GCI_BlueBand;
            else if( nBand == 4 )
                return GCI_AlphaBand;
            else
                return GCI_Undefined;
        }
        else
        {
            return GCI_GrayIndex;
        }
    }

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
    bNoDataSet = true;
    dfNoDataValue = dfNoData;

    return CE_None;
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

const char *HDF4ImageRasterBand::GetUnitType()

{
    if( !osUnitType.empty() )
        return osUnitType;

    return GDALRasterBand::GetUnitType();
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double HDF4ImageRasterBand::GetOffset( int *pbSuccess )

{
    if( bHaveOffset )
    {
        if( pbSuccess != nullptr )
            *pbSuccess = TRUE;
        return dfOffset;
    }

    return GDALRasterBand::GetOffset( pbSuccess );
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double HDF4ImageRasterBand::GetScale( int *pbSuccess )

{
    if( bHaveScale )
    {
        if( pbSuccess != nullptr )
            *pbSuccess = TRUE;
        return dfScale;
    }

    return GDALRasterBand::GetScale( pbSuccess );
}

/************************************************************************/
/* ==================================================================== */
/*                              HDF4ImageDataset                        */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           HDF4ImageDataset()                         */
/************************************************************************/

HDF4ImageDataset::HDF4ImageDataset() :
    pszFilename(nullptr),
    hHDF4(0),
    iGR(0),
    iPal(0),
    iDataset(0),
    iRank(0),
    iNumType(0),
    nAttrs(0),
    iInterlaceMode(0),
    iPalInterlaceMode(0),
    iPalDataType(0),
    nComps(0),
    nPalEntries(0),
    iXDim(0),
    iYDim(0),
    iBandDim(-1),
    i4Dim(0),
    nBandCount(0),
    pszSubdatasetName(nullptr),
    pszFieldName(nullptr),
    poColorTable(nullptr),
    oSRS( OGRSpatialReference() ),
    bHasGeoTransform(false),
    pszProjection(CPLStrdup( "" )),
    pszGCPProjection(CPLStrdup( "" )),
    pasGCPList(nullptr),
    nGCPCount(0),
    iDatasetType(HDF4_UNKNOWN),
    iSDS(FAIL),
    nBlockPreferredXSize(-1),
    nBlockPreferredYSize(-1),
    bReadTile(false)
{
    oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    memset(aiDimSizes, 0, sizeof(aiDimSizes));
    papszLocalMetadata = nullptr;
    memset(aiPaletteData, 0, sizeof(aiPaletteData));
    memset(szName, 0, sizeof(szName));
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~HDF4ImageDataset()                       */
/************************************************************************/

HDF4ImageDataset::~HDF4ImageDataset()
{
    CPLMutexHolderD(&hHDF4Mutex);

    HDF4ImageDataset::FlushCache(true);

    CPLFree( pszFilename );
    if( iSDS != FAIL )
        SDendaccess( iSDS );
    if( hSD > 0 )
        SDend( hSD );
    hSD = 0;
    if( iGR > 0 )
        GRendaccess( iGR );
    if( hGR > 0 )
        GRend( hGR );
    hGR = 0;
    CPLFree( pszSubdatasetName );
    CPLFree( pszFieldName );
    if( papszLocalMetadata )
        CSLDestroy( papszLocalMetadata );
    if( poColorTable != nullptr )
        delete poColorTable;

    CPLFree( pszProjection );
    CPLFree( pszGCPProjection );
    if( nGCPCount > 0 )
    {
        for( int i = 0; i < nGCPCount; i++ )
        {
            CPLFree( pasGCPList[i].pszId );
            CPLFree( pasGCPList[i].pszInfo );
        }

        CPLFree( pasGCPList );
    }
    if( hHDF4 > 0 )
    {
        switch ( iDatasetType )
        {
            case HDF4_EOS:
                switch ( iSubdatasetType )
                {
                    case H4ST_EOS_SWATH:
                    case H4ST_EOS_SWATH_GEOL:
                        SWclose( hHDF4 );
                        break;
                    case H4ST_EOS_GRID:
                        GDclose( hHDF4 );
                        break;
                    default:
                        break;
                }
                break;
            case HDF4_SDS:
            case HDF4_GR:
                hHDF4 = Hclose( hHDF4 );
                break;
            default:
                break;
        }
    }
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr HDF4ImageDataset::GetGeoTransform( double * padfTransform )
{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );

    if( !bHasGeoTransform )
        return CE_Failure;

    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr HDF4ImageDataset::SetGeoTransform( double * padfTransform )
{
    bHasGeoTransform = true;
    memcpy( adfGeoTransform, padfTransform, sizeof(double) * 6 );

    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *HDF4ImageDataset::_GetProjectionRef()

{
    return pszProjection;
}

/************************************************************************/
/*                          SetProjection()                             */
/************************************************************************/

CPLErr HDF4ImageDataset::_SetProjection( const char *pszNewProjection )

{
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

const char *HDF4ImageDataset::_GetGCPProjection()

{
    if( nGCPCount > 0 )
        return pszGCPProjection;

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

void HDF4ImageDataset::FlushCache(bool bAtClosing)

{
    CPLMutexHolderD(&hHDF4Mutex);

    GDALDataset::FlushCache(bAtClosing);

    if( eAccess == GA_ReadOnly )
        return;

    // Write out transformation matrix.
    const char *pszValue =
        CPLSPrintf( "%f, %f, %f, %f, %f, %f",
                    adfGeoTransform[0], adfGeoTransform[1],
                    adfGeoTransform[2], adfGeoTransform[3],
                    adfGeoTransform[4], adfGeoTransform[5] );
    if( (SDsetattr( hSD, "TransformationMatrix", DFNT_CHAR8,
                     static_cast<int>(strlen(pszValue)) + 1, pszValue )) < 0 )
    {
        CPLDebug( "HDF4Image",
                  "Cannot write transformation matrix to output file" );
    }

    // Write out projection
    if( pszProjection != nullptr && !EQUAL( pszProjection, "" ) )
    {
        if( (SDsetattr( hSD, "Projection", DFNT_CHAR8,
                        static_cast<int>(strlen(pszProjection)) + 1,
                        pszProjection )) < 0 )
            {
                CPLDebug("HDF4Image",
                         "Cannot write projection information to output file");
            }
    }

    // Store all metadata from source dataset as HDF attributes
    if( GetMetadata() )
    {
        char **papszMeta = GetMetadata();

        while( *papszMeta )
        {
            char *pszName = nullptr;
            pszValue = CPLParseNameValue( *papszMeta++, &pszName );
            if( pszName != nullptr &&
                (SDsetattr( hSD, pszName, DFNT_CHAR8,
                            static_cast<int>(strlen(pszValue)) + 1,
                            pszValue )) < 0 )
            {
                CPLDebug( "HDF4Image",
                          "Cannot write metadata information to output file" );
            }

            CPLFree( pszName );
        }
    }

    // Write out NoData values
    for ( int iBand = 1; iBand <= nBands; iBand++ )
    {
        HDF4ImageRasterBand *poBand =
            reinterpret_cast<HDF4ImageRasterBand *>( GetRasterBand(iBand) );

        if( poBand->bNoDataSet )
        {
            char *pszName = CPLStrdup( CPLSPrintf( "NoDataValue%d", iBand ) );
            pszValue = CPLSPrintf( "%f", poBand->dfNoDataValue );
            if( (SDsetattr( hSD, pszName, DFNT_CHAR8,
                            static_cast<int>(strlen(pszValue)) + 1,
                            pszValue )) < 0 )
                {
                    CPLDebug( "HDF4Image",
                              "Cannot write NoData value for band %d "
                              "to output file", iBand);
                }

            CPLFree( pszName );
       }
    }

    // Write out band descriptions
    for ( int iBand = 1; iBand <= nBands; iBand++ )
    {
        HDF4ImageRasterBand *poBand =
            reinterpret_cast<HDF4ImageRasterBand *>( GetRasterBand(iBand) );

        char *pszName = CPLStrdup( CPLSPrintf( "BandDesc%d", iBand ) );
        pszValue = poBand->GetDescription();
        if( pszValue != nullptr && !EQUAL( pszValue, "" ) )
        {
            if( SDsetattr( hSD, pszName, DFNT_CHAR8,
                           static_cast<int>(strlen(pszValue)) + 1,
                           pszValue ) < 0 )
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
/*                        USGSMnemonicToCode()                          */
/************************************************************************/

long HDF4ImageDataset::USGSMnemonicToCode( const char* pszMnemonic )
{
    if( EQUAL(pszMnemonic, "UTM") )
        return 1L;
    else if( EQUAL(pszMnemonic, "LAMCC") )
        return 4L;
    else if( EQUAL(pszMnemonic, "PS") )
        return 6L;
    else if( EQUAL(pszMnemonic, "PC") )
        return 7L;
    else if( EQUAL(pszMnemonic, "TM") )
        return 9L;
    else if( EQUAL(pszMnemonic, "EQRECT") )
        return 17L;
    else if( EQUAL(pszMnemonic, "OM") )
        return 20L;
    else if( EQUAL(pszMnemonic, "SOM") )
        return 22L;
    else
        return 1L;  // UTM by default
}

/************************************************************************/
/*                              ToGeoref()                              */
/************************************************************************/

void HDF4ImageDataset::ToGeoref( double *pdfGeoX, double *pdfGeoY )
{
    OGRSpatialReference* poLatLong = oSRS.CloneGeogCS();
    OGRCoordinateTransformation* poTransform = nullptr;
    if( poLatLong )
    {
        poLatLong->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        poTransform =
            OGRCreateCoordinateTransformation( poLatLong, &oSRS );
    }

    if( poTransform != nullptr )
        poTransform->Transform( 1, pdfGeoX, pdfGeoY, nullptr );

    if( poTransform != nullptr )
        delete poTransform;

    if( poLatLong != nullptr )
        delete poLatLong;
}

/************************************************************************/
/*                            ReadCoordinates()                         */
/************************************************************************/

void HDF4ImageDataset::ReadCoordinates( const char *pszString,
                                        double *pdfCenterY, double *pdfCenterX )
{
    char **papszStrList = CSLTokenizeString2( pszString, ", ", 0 );
    *pdfCenterY = CPLAtof( papszStrList[0] ); /* lat */
    *pdfCenterX = CPLAtof( papszStrList[1] ); /* lon */
    CSLDestroy( papszStrList );
}

/************************************************************************/
/*                         CaptureL1GMTLInfo()                          */
/************************************************************************/

/*  FILE L71002025_02520010722_M_MTL.L1G

GROUP = L1_METADATA_FILE
  ...
  GROUP = PRODUCT_METADATA
    PRODUCT_TYPE = "L1G"
    PROCESSING_SOFTWARE = "IAS_5.1"
    EPHEMERIS_TYPE = "DEFINITIVE"
    SPACECRAFT_ID = "Landsat7"
    SENSOR_ID = "ETM+"
    ACQUISITION_DATE = 2001-07-22
    WRS_PATH = 002
    STARTING_ROW = 025
    ENDING_ROW = 025
    BAND_COMBINATION = "12345--7-"
    PRODUCT_UL_CORNER_LAT = 51.2704805
    PRODUCT_UL_CORNER_LON = -53.8914311
    PRODUCT_UR_CORNER_LAT = 50.8458100
    PRODUCT_UR_CORNER_LON = -50.9869091
    PRODUCT_LL_CORNER_LAT = 49.6960897
    PRODUCT_LL_CORNER_LON = -54.4047933
    PRODUCT_LR_CORNER_LAT = 49.2841436
    PRODUCT_LR_CORNER_LON = -51.5900428
    PRODUCT_UL_CORNER_MAPX = 298309.894
    PRODUCT_UL_CORNER_MAPY = 5683875.631
    PRODUCT_UR_CORNER_MAPX = 500921.624
    PRODUCT_UR_CORNER_MAPY = 5632678.683
    PRODUCT_LL_CORNER_MAPX = 254477.193
    PRODUCT_LL_CORNER_MAPY = 5510407.880
    PRODUCT_LR_CORNER_MAPX = 457088.923
    PRODUCT_LR_CORNER_MAPY = 5459210.932
    PRODUCT_SAMPLES_REF = 6967
    PRODUCT_LINES_REF = 5965
    BAND1_FILE_NAME = "L71002025_02520010722_B10.L1G"
    BAND2_FILE_NAME = "L71002025_02520010722_B20.L1G"
    BAND3_FILE_NAME = "L71002025_02520010722_B30.L1G"
    BAND4_FILE_NAME = "L71002025_02520010722_B40.L1G"
    BAND5_FILE_NAME = "L71002025_02520010722_B50.L1G"
    BAND7_FILE_NAME = "L72002025_02520010722_B70.L1G"
    METADATA_L1_FILE_NAME = "L71002025_02520010722_MTL.L1G"
    CPF_FILE_NAME = "L7CPF20010701_20010930_06"
    HDF_DIR_FILE_NAME = "L71002025_02520010722_HDF.L1G"
  END_GROUP = PRODUCT_METADATA
  ...
  GROUP = PROJECTION_PARAMETERS
    REFERENCE_DATUM = "NAD83"
    REFERENCE_ELLIPSOID = "GRS80"
    GRID_CELL_SIZE_PAN = 15.000000
    GRID_CELL_SIZE_THM = 60.000000
    GRID_CELL_SIZE_REF = 30.000000
    ORIENTATION = "NOM"
    RESAMPLING_OPTION = "CC"
    MAP_PROJECTION = "UTM"
  END_GROUP = PROJECTION_PARAMETERS
  GROUP = UTM_PARAMETERS
    ZONE_NUMBER = 22
  END_GROUP = UTM_PARAMETERS
END_GROUP = L1_METADATA_FILE
END
*/

void HDF4ImageDataset::CaptureL1GMTLInfo()

{
/* -------------------------------------------------------------------- */
/*      Does the physical file look like it matches our expected        */
/*      name pattern?                                                   */
/* -------------------------------------------------------------------- */
    if( strlen(pszFilename) < 8
        || !EQUAL(pszFilename+strlen(pszFilename)-8,"_HDF.L1G") )
        return;

/* -------------------------------------------------------------------- */
/*      Construct the name of the corresponding MTL file.  We should    */
/*      likely be able to extract that from the HDF itself but I'm      */
/*      not sure where to find it.                                      */
/* -------------------------------------------------------------------- */
    CPLString osMTLFilename = pszFilename;
    osMTLFilename.resize(osMTLFilename.length() - 8);
    osMTLFilename += "_MTL.L1G";

/* -------------------------------------------------------------------- */
/*      Ingest the MTL using the NASAKeywordHandler written for the     */
/*      PDS driver.                                                     */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL( osMTLFilename, "r" );
    if( fp == nullptr )
        return;

    NASAKeywordHandler oMTL;
    if( !oMTL.Ingest( fp, 0 ) )
    {
        VSIFCloseL( fp );
        return;
    }

    VSIFCloseL( fp );

/* -------------------------------------------------------------------- */
/*  Note: Different variation of MTL files use different group names.   */
/*            Check for LPGS_METADATA_FILE and L1_METADATA_FILE.        */
/* -------------------------------------------------------------------- */
    CPLString osPrefix;

    if( oMTL.GetKeyword( "LPGS_METADATA_FILE.PRODUCT_METADATA"
                         ".PRODUCT_UL_CORNER_LON", nullptr ) )
        osPrefix = "LPGS_METADATA_FILE.PRODUCT_METADATA.PRODUCT_";
    else if( oMTL.GetKeyword( "L1_METADATA_FILE.PRODUCT_METADATA"
                              ".PRODUCT_UL_CORNER_LON", nullptr ) )
        osPrefix = "L1_METADATA_FILE.PRODUCT_METADATA.PRODUCT_";
    else
        return;

    const double dfULX
        = CPLAtof( oMTL.GetKeyword( (osPrefix+"UL_CORNER_LON").c_str(), "0" ) );
    const double dfULY
        = CPLAtof( oMTL.GetKeyword( (osPrefix+"UL_CORNER_LAT").c_str(), "0" ) );
    const double dfLRX
        = CPLAtof( oMTL.GetKeyword( (osPrefix+"LR_CORNER_LON").c_str(), "0" ) );
    const double dfLRY
        = CPLAtof( oMTL.GetKeyword( (osPrefix+"LR_CORNER_LAT").c_str(), "0" ) );
    const double dfLLX
        = CPLAtof( oMTL.GetKeyword( (osPrefix+"LL_CORNER_LON").c_str(), "0" ) );
    const double dfLLY
        = CPLAtof( oMTL.GetKeyword( (osPrefix+"LL_CORNER_LAT").c_str(), "0" ) );
    const double dfURX
        = CPLAtof( oMTL.GetKeyword( (osPrefix+"UR_CORNER_LON").c_str(), "0" ) );
    const double dfURY
        = CPLAtof( oMTL.GetKeyword( (osPrefix+"UR_CORNER_LAT").c_str(), "0" ) );

    CPLFree( pszGCPProjection );
    pszGCPProjection = CPLStrdup(
        "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,"
        "298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],TOWGS84[0,0,0,0,0,0,0],"
        "AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,"
        "AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,"
        "AUTHORITY[\"EPSG\",\"9108\"]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST]"
        ",AUTHORITY[\"EPSG\",\"4326\"]]" );

    nGCPCount = 4;
    pasGCPList = (GDAL_GCP *) CPLCalloc( nGCPCount, sizeof( GDAL_GCP ) );
    GDALInitGCPs( nGCPCount, pasGCPList );

    pasGCPList[0].dfGCPX = dfULX;
    pasGCPList[0].dfGCPY = dfULY;
    pasGCPList[0].dfGCPPixel = 0.0;
    pasGCPList[0].dfGCPLine = 0.0;

    pasGCPList[1].dfGCPX = dfURX;
    pasGCPList[1].dfGCPY = dfURY;
    pasGCPList[1].dfGCPPixel = GetRasterXSize();
    pasGCPList[1].dfGCPLine = 0.0;

    pasGCPList[2].dfGCPX = dfLLX;
    pasGCPList[2].dfGCPY = dfLLY;
    pasGCPList[2].dfGCPPixel = 0.0;
    pasGCPList[2].dfGCPLine = GetRasterYSize();

    pasGCPList[3].dfGCPX = dfLRX;
    pasGCPList[3].dfGCPY = dfLRY;
    pasGCPList[3].dfGCPPixel = GetRasterXSize();
    pasGCPList[3].dfGCPLine = GetRasterYSize();
}

/************************************************************************/
/*                       CaptureNRLGeoTransform()                       */
/*                                                                      */
/*      Capture geotransform and coordinate system from NRL (Naval      */
/*      Research Laboratory, Stennis Space Center) metadata.            */
/************************************************************************/

/* Example metadata:
Metadata:
  createTime=Fri Oct  1 18:00:07 2004
  createSoftware=APS v2.8.4
  createPlatform=i686-pc-linux-gnu
  createAgency=Naval Research Laboratory, Stennis Space Center
  sensor=MODIS
  sensorPlatform=TERRA-AM
  sensorAgency=NASA
  sensorType=whiskbroom
  sensorSpectrum=Visible/Thermal
  sensorNumberOfBands=36
  sensorBandUnits=nano meters
  sensorBands=645, 858.5, 469, 555, 1240, 1640, 2130, 412.5, 443, 488, 531, 551,
 667, 678, 748, 869.5, 905, 936, 940, 3750, 3959, 3959, 4050, 4465.5, 4515.5, 13
75, 6715, 7325, 8550, 9730, 11130, 12020, 13335, 13635, 13935, 14235
  sensorBandWidths=50, 35, 20, 20, 20, 24, 50, 15, 10, 10, 10, 10, 10, 10, 10, 1
5, 30, 10, 50, 90, 60, 60, 60, 65, 67, 30, 360, 300, 300, 300, 500, 500, 300, 30
0, 300, 300
  sensorNominalAltitudeInKM=705
  sensorScanWidthInKM=2330
  sensorResolutionInKM=1
  sensorPlatformType=Polar-orbiting Satellite
  timeStartYear=2004
  timeStartDay=275
  timeStartTime=56400000
  timeStart=Fri Oct  1 15:40:00 2004
  timeDayNight=Day
  timeEndYear=2004
  timeEndDay=275
  timeEndTime=56700000
  timeEnd=Fri Oct  1 15:45:00 2004
  inputMasks=HIGLINT,CLDICE,LAND,ATMFAIL
  inputMasksInt=523
  processedVersion=1.2
  file=MODAM2004275.L3_Mosaic_NOAA_GMX
  fileTitle=NRL Level-3 Mosaic
  fileVersion=3.0
  fileClassification=UNCLASSIFIED
  fileStatus=EXPERIMENTAL
  navType=mapped
  mapProjectionSystem=NRL(USGS)
  mapProjection=Gomex
  mapUpperLeft=31, -99
  mapUpperRight=31, -79
  mapLowerLeft=14.9844128048645, -99
  mapLowerRight=14.9844128048645, -79
  inputFiles=MODAM2004275154000.L3_NOAA_GMX
  ...
 */

void HDF4ImageDataset::CaptureNRLGeoTransform()

{
/* -------------------------------------------------------------------- */
/*      Collect the four corners.                                       */
/* -------------------------------------------------------------------- */
    double adfXY[8] = {};
    static const char * const apszItems[] = {
        "mapUpperLeft", "mapUpperRight", "mapLowerLeft", "mapLowerRight" };
    bool bLLPossible = true;

    for( int iCorner = 0; iCorner < 4; iCorner++ )
    {
        const char *pszCornerLoc =
            CSLFetchNameValue( papszGlobalMetadata, apszItems[iCorner] );

        if( pszCornerLoc == nullptr )
            return;

        char **papszTokens = CSLTokenizeStringComplex( pszCornerLoc, ",",
                                                       FALSE, FALSE );
        if( CSLCount( papszTokens ) != 2 )
        {
            CSLDestroy( papszTokens );
            return;
        }

        adfXY[iCorner*2+0] = CPLAtof( papszTokens[1] );
        adfXY[iCorner*2+1] = CPLAtof( papszTokens[0] );

        if( adfXY[iCorner*2+0] < -360 || adfXY[iCorner*2+0] > 360
            || adfXY[iCorner*2+1] < -90 || adfXY[iCorner*2+1] > 90 )
            bLLPossible = false;

        CSLDestroy( papszTokens );
    }

/* -------------------------------------------------------------------- */
/*      Does this look like nice clean "northup" lat/long data?         */
/* -------------------------------------------------------------------- */
    if( adfXY[0*2+0] == adfXY[2*2+0] && adfXY[0*2+1] == adfXY[1*2+1]
        && bLLPossible )
    {
        bHasGeoTransform = true;
        adfGeoTransform[0] = adfXY[0*2+0];
        adfGeoTransform[1] = (adfXY[1*2+0] - adfXY[0*2+0]) / nRasterXSize;
        adfGeoTransform[2] = 0.0;
        adfGeoTransform[3] = adfXY[0*2+1];
        adfGeoTransform[4] = 0.0;
        adfGeoTransform[5] = (adfXY[2*2+1] - adfXY[0*2+1]) / nRasterYSize;

        oSRS.SetWellKnownGeogCS( "WGS84" );
        CPLFree( pszProjection );
        oSRS.exportToWkt( &pszProjection );
    }

/* -------------------------------------------------------------------- */
/*      Can we find the USGS Projection Parameters?                     */
/* -------------------------------------------------------------------- */
    bool bGotGCTPProjection = false;
    int l_iSDSIndex = FAIL;
    int l_iSDS = FAIL;
    const char *mapProjection = CSLFetchNameValue( papszGlobalMetadata,
                                                   "mapProjection" );

    if( mapProjection )
        l_iSDSIndex = SDnametoindex( hSD, mapProjection );

    if( l_iSDSIndex != FAIL )
        l_iSDS = SDselect( hSD, l_iSDSIndex );

    if( l_iSDS != FAIL )
    {
        char l_szName[HDF4_SDS_MAXNAMELEN] = {};
        int32 l_iRank = 0;
        int32 l_iNumType = 0;
        int32 l_nAttrs = 0;
        int32 l_aiDimSizes[H4_MAX_VAR_DIMS] = {};

        double adfGCTP[29] = {};
        int32 aiStart[H4_MAX_NC_DIMS] = {};
        int32 aiEdges[H4_MAX_NC_DIMS] = {};

        aiStart[0] = 0;
        aiEdges[0] = 29;

        if( SDgetinfo( l_iSDS, l_szName, &l_iRank, l_aiDimSizes, &l_iNumType,
                       &l_nAttrs) == 0
            && l_iNumType == DFNT_FLOAT64
            && l_iRank == 1
            && l_aiDimSizes[0] >= 29
            && SDreaddata( l_iSDS, aiStart, nullptr, aiEdges, adfGCTP ) == 0
            && oSRS.importFromUSGS( static_cast<long>( adfGCTP[1] ),
                                    static_cast<long>( adfGCTP[2] ),
                                    adfGCTP+4,
                                    static_cast<long>( adfGCTP[3] ) )
            == OGRERR_NONE )
        {
            CPLDebug( "HDF4Image",
                      "GCTP Params = %g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,"
                      "%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g",
                      adfGCTP[0],
                      adfGCTP[1],
                      adfGCTP[2],
                      adfGCTP[3],
                      adfGCTP[4],
                      adfGCTP[5],
                      adfGCTP[6],
                      adfGCTP[7],
                      adfGCTP[8],
                      adfGCTP[9],
                      adfGCTP[10],
                      adfGCTP[11],
                      adfGCTP[12],
                      adfGCTP[13],
                      adfGCTP[14],
                      adfGCTP[15],
                      adfGCTP[16],
                      adfGCTP[17],
                      adfGCTP[18],
                      adfGCTP[19],
                      adfGCTP[20],
                      adfGCTP[21],
                      adfGCTP[22],
                      adfGCTP[23],
                      adfGCTP[24],
                      adfGCTP[25],
                      adfGCTP[26],
                      adfGCTP[27],
                      adfGCTP[28] );

            CPLFree( pszProjection );
            oSRS.exportToWkt( &pszProjection );
            bGotGCTPProjection = true;
        }

        SDendaccess(l_iSDS);
    }

/* -------------------------------------------------------------------- */
/*      If we derived a GCTP based projection, then we need to          */
/*      transform the lat/long corners into this projection and use     */
/*      them to establish the geotransform.                             */
/* -------------------------------------------------------------------- */
    if( bLLPossible && bGotGCTPProjection )
    {
        OGRSpatialReference oWGS84;

        oWGS84.SetWellKnownGeogCS( "WGS84" );
        oWGS84.SetAxisMappingStrategy( OAMS_TRADITIONAL_GIS_ORDER );

        OGRCoordinateTransformation *poCT =
            OGRCreateCoordinateTransformation( &oWGS84, &oSRS );

        double dfULX = adfXY[0*2+0];
        double dfULY = adfXY[0*2+1];

        double dfLRX = adfXY[3*2+0];
        double dfLRY = adfXY[3*2+1];

        if( poCT->Transform( 1, &dfULX, &dfULY )
            && poCT->Transform( 1, &dfLRX, &dfLRY ) )
        {
            bHasGeoTransform = true;
            adfGeoTransform[0] = dfULX;
            adfGeoTransform[1] = (dfLRX - dfULX) / nRasterXSize;
            adfGeoTransform[2] = 0.0;
            adfGeoTransform[3] = dfULY;
            adfGeoTransform[4] = 0.0;
            adfGeoTransform[5] = (dfLRY - dfULY) / nRasterYSize;
        }

        delete poCT;
    }
}

/************************************************************************/
/*                     CaptureCoastwatchGCTPInfo()                      */
/************************************************************************/

/* Example Metadata from:

  http://coastwatch.noaa.gov/interface/most_recent.php?sensor=MODIS&product=chlorNASA

Definitions at:
  http://coastwatch.noaa.gov/cw_form_hdf.html

Metadata:
  satellite=Aqua
  sensor=MODIS
  origin=USDOC/NOAA/NESDIS CoastWatch
  history=PGE01:4.1.12;PGE02:4.3.1.12;SeaDAS Version ?.?, MSl12 4.0.2, Linux 2.4.21-27.0.1.EL
cwregister GulfOfMexicoSinusoidal.hdf MODSCW.P2005023.1835.swath09.hdf MODSCW.P2005023.1835.GM16.mapped09.hdf
cwgraphics MODSCW.P2005023.1835.GM16.closest.hdf
cwmath --template chlor_a --expr chlor_a=select(and(l2_flags,514)!=0,nan,chlor_a) /data/aps/browse/lvl3/seadas/coastwatch/hdf/MODSCW_P2005023_1835_GM16_closest.hdf /data/aps/browse/lvl3/seadas/coastwatch/maskhdf/MODSCW_P2005023_1835_GM16_closest_chlora.hdf
cwmath --template latitude --expr latitude=latitude /data/aps/browse/lvl3/seadas/coastwatch/hdf/MODSCW_P2005023_1835_GM16_closest.hdf /data/aps/browse/lvl3/seadas/coastwatch/maskhdf/MODSCW_P2005023_1835_GM16_closest_chlora.hdf
cwmath --template longitude --expr longitude=longitude /data/aps/browse/lvl3/seadas/coastwatch/hdf/MODSCW_P2005023_1835_GM16_closest.hdf /data/aps/browse/lvl3/seadas/coastwatch/maskhdf/MODSCW_P2005023_1835_GM16_closest_chlora.hdf
  cwhdf_version=3.2
  pass_type=day
  pass_date=12806
  start_time=66906
  temporal_extent=298
  projection_type=mapped
  projection=Sinusoidal
  gctp_sys=16
  gctp_zone=62
  gctp_parm=6378137, 0, 0, 0, -89000000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  gctp_datum=12
  et_affine=0, -1008.74836097881, 1008.74836097881, 0, -953126.102425113, 3447041.10282512
  rows=1540
  cols=2000
  polygon_latitude=31, 31, 31, 31, 31, 27.5095879249529, 24.0191758499058, 20.5287637748587, 17.0383516998116, 17.0383516998116, 17.0383516998116, 17.0383516998116, 17.0383516998116, 20.5287637748587, 24.0191758499058, 27.5095879249529, 31
  polygon_longitude=-99, -93.7108573344442, -88.4217146688883, -83.1325720033325, -77.8434293377767, -78.217853417453, -78.5303805448579, -78.7884829057512, -78.9979508907244, -83.7397542896832, -88.481557688642, -93.2233610876007, -97.9651644865595, -98.1529175079091, -98.3842631146439, -98.664391423662, -99
  orbit_type=ascending
  raster_type=RasterPixelIsArea
  swath_sync_lines=1

 */

void HDF4ImageDataset::CaptureCoastwatchGCTPInfo()

{
    if( CSLFetchNameValue( papszGlobalMetadata, "gctp_sys" ) == nullptr
        || CSLFetchNameValue( papszGlobalMetadata, "gctp_zone" ) == nullptr
        || CSLFetchNameValue( papszGlobalMetadata, "gctp_parm" ) == nullptr
        || CSLFetchNameValue( papszGlobalMetadata, "gctp_datum" ) == nullptr
        || CSLFetchNameValue( papszGlobalMetadata, "et_affine" ) == nullptr )
        return;

/* -------------------------------------------------------------------- */
/*      Grab USGS/GCTP Parameters.                                      */
/* -------------------------------------------------------------------- */
    const int nSys
        = atoi( CSLFetchNameValue( papszGlobalMetadata, "gctp_sys" ) );
    const int nZone
        = atoi( CSLFetchNameValue( papszGlobalMetadata, "gctp_zone" ) );
    const int nDatum
        = atoi( CSLFetchNameValue( papszGlobalMetadata, "gctp_datum" ) );

    char **papszTokens = CSLTokenizeStringComplex(
        CSLFetchNameValue( papszGlobalMetadata, "gctp_parm" ), ",",
        FALSE, FALSE );
    if( CSLCount(papszTokens) < 15 )
    {
        CSLDestroy(papszTokens);
        return;
    }

    double adfParams[15];
    for( int iParam = 0; iParam < 15; iParam++ )
        adfParams[iParam] = CPLAtof( papszTokens[iParam] );
    CSLDestroy( papszTokens );

/* -------------------------------------------------------------------- */
/*      Convert into an SRS.                                            */
/* -------------------------------------------------------------------- */

    if( oSRS.importFromUSGS( nSys, nZone, adfParams, nDatum ) != OGRERR_NONE )
        return;

    CPLFree( pszProjection );
    oSRS.exportToWkt( &pszProjection );

/* -------------------------------------------------------------------- */
/*      Capture the affine transform info.                              */
/* -------------------------------------------------------------------- */

    papszTokens = CSLTokenizeStringComplex(
        CSLFetchNameValue( papszGlobalMetadata, "et_affine" ), ",",
        FALSE, FALSE );
    if( CSLCount(papszTokens) != 6 )
    {
        CSLDestroy(papszTokens);
        return;
    }

    // We don't seem to have proper ef_affine docs so I don't
    // know which of these two coefficients goes where.
    if( CPLAtof(papszTokens[0]) != 0.0 || CPLAtof(papszTokens[3]) != 0.0 )
    {
        CSLDestroy(papszTokens);
        return;
    }

    bHasGeoTransform = true;
    adfGeoTransform[0] = CPLAtof( papszTokens[4] );
    adfGeoTransform[1] = CPLAtof( papszTokens[2] );
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = CPLAtof( papszTokens[5] );
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = CPLAtof( papszTokens[1] );

    // Middle of pixel adjustment.
    adfGeoTransform[0] -= adfGeoTransform[1] * 0.5;
    adfGeoTransform[3] -= adfGeoTransform[5] * 0.5;

    CSLDestroy(papszTokens);
}

/************************************************************************/
/*                          GetImageDimensions()                        */
/************************************************************************/

void HDF4ImageDataset::GetImageDimensions( char *pszDimList )
{
    char    **papszDimList = CSLTokenizeString2( pszDimList, ",",
                                                 CSLT_HONOURSTRINGS );
    const int nDimCount = CSLCount( papszDimList );

    // TODO: check whether nDimCount is > 1 and do something if it isn't.

    // Search for the "Band" word in the name of dimension
    // or take the first one as a number of bands
    if( iRank == 2 )
    {
        nBandCount = 1;
    }
    else
    {
        for( int i = 0; i < nDimCount; i++ )
        {
            if( strstr( papszDimList[i], "band" ) )
            {
                iBandDim = i;
                nBandCount = aiDimSizes[i];
                // Handle 4D datasets
                if( iRank > 3 && i < nDimCount - 1 )
                {
                    // FIXME: is there a better way to search for
                    // the 4th dimension?
                    i4Dim = i + 1;
                    nBandCount *= aiDimSizes[i4Dim];
                }
                break;
            }
        }
    }

    // Search for the starting "X" and "Y" in the names or take
    // the last two dimensions as X and Y sizes.
    iXDim = nDimCount - 1;
    iYDim = nDimCount - 2;

    for( int i = 0; i < nDimCount; i++ )
    {
        if( STARTS_WITH_CI(papszDimList[i], "X") && iBandDim != i )
            iXDim = i;
        else if( STARTS_WITH_CI(papszDimList[i], "Y") && iBandDim != i )
            iYDim = i;
    }

    // If didn't get a band dimension yet, but have an extra
    // dimension, use it as the band dimension.
    if( iRank > 2 && iBandDim == -1 )
    {
        if( iXDim != 0 && iYDim != 0 )
            iBandDim = 0;
        else if( iXDim != 1 && iYDim != 1 )
            iBandDim = 1;
        else if( iXDim != 2 && iYDim != 2 )
            iBandDim = 2;

        nBandCount = aiDimSizes[iBandDim];
    }

    CSLDestroy( papszDimList );
}

/************************************************************************/
/*                            GetSwatAttrs()                            */
/************************************************************************/

void HDF4ImageDataset::GetSwatAttrs( int32 hSW )
{
/* -------------------------------------------------------------------- */
/*      At the start we will fetch the global HDF attributes.           */
/* -------------------------------------------------------------------- */
    int32 hDummy = 0;

    EHidinfo( hHDF4, &hDummy, &hSD );
    ReadGlobalAttributes( hSD );
    papszLocalMetadata = CSLDuplicate( papszGlobalMetadata );

/* -------------------------------------------------------------------- */
/*      Fetch the esoteric HDF-EOS attributes then.                     */
/* -------------------------------------------------------------------- */
    int32 nStrBufSize = 0;

    if( SWinqattrs( hSW, nullptr, &nStrBufSize ) > 0 && nStrBufSize > 0 )
    {
        char *pszAttrList
            = reinterpret_cast<char *>( CPLMalloc( nStrBufSize + 1 ) );
        SWinqattrs( hSW, pszAttrList, &nStrBufSize );

#ifdef DEBUG
        CPLDebug( "HDF4Image", "List of attributes in swath \"%s\": %s",
                  pszFieldName, pszAttrList );
#endif

        char **papszAttributes = CSLTokenizeString2( pszAttrList, ",",
                                                     CSLT_HONOURSTRINGS );
        const int l_nAttrs = CSLCount( papszAttributes );
        for( int i = 0; i < l_nAttrs; i++ )
        {
            int32 l_iNumType = 0;
            int32 nValues = 0;

            if( SWattrinfo( hSW, papszAttributes[i],
                            &l_iNumType, &nValues ) < 0 )
                continue;
            const int nDataTypeSize = GetDataTypeSize(l_iNumType);
            if( nDataTypeSize == 0 )
                continue;
            CPLAssert( (nValues % nDataTypeSize) == 0);

            void *pData = CPLMalloc( nValues + 1);
            SWreadattr( hSW, papszAttributes[i], pData );

            if( l_iNumType == DFNT_CHAR8 || l_iNumType == DFNT_UCHAR8 )
            {
                reinterpret_cast<char *>( pData )[nValues] = '\0';
                papszLocalMetadata = CSLAddNameValue(
                    papszLocalMetadata,
                    papszAttributes[i],
                    const_cast<const char *>(
                        reinterpret_cast<char *>( pData ) ) );
            }
            else
            {
                char *pszTemp = SPrintArray( GetDataType(l_iNumType), pData,
                                             nValues / nDataTypeSize, ", " );
                papszLocalMetadata = CSLAddNameValue( papszLocalMetadata,
                                                      papszAttributes[i],
                                                      pszTemp );
                CPLFree( pszTemp );
            }

            CPLFree( pData );
        }

        CSLDestroy( papszAttributes );
        CPLFree( pszAttrList );
    }

/* -------------------------------------------------------------------- */
/*      After fetching HDF-EOS specific stuff we will read the generic  */
/*      HDF attributes and append them to the list of metadata.         */
/* -------------------------------------------------------------------- */
    int32 l_iSDS = 0;
    if( SWsdid(hSW, pszFieldName, &l_iSDS) != -1 )
    {
        int32 l_iRank = 0;
        int32 l_iNumType = 0;
        int32 l_nAttrs = 0;
        char l_szName[HDF4_SDS_MAXNAMELEN] = {};
        int32 l_aiDimSizes[H4_MAX_VAR_DIMS] = {};

        if( SDgetinfo( l_iSDS, l_szName, &l_iRank, l_aiDimSizes, &l_iNumType,
                       &l_nAttrs) == 0 )
        {
            for( int32 iAttribute = 0; iAttribute < l_nAttrs; iAttribute++ )
            {
                char szAttrName[H4_MAX_NC_NAME] = {};
                int32 nValues = 0;
                SDattrinfo( l_iSDS, iAttribute, szAttrName,
                            &l_iNumType, &nValues );
                papszLocalMetadata =
                    TranslateHDF4Attributes( l_iSDS, iAttribute,
                                             szAttrName, l_iNumType,
                                             nValues, papszLocalMetadata );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Finally make the whole list visible.                            */
/* -------------------------------------------------------------------- */
    SetMetadata( papszLocalMetadata );
}

/************************************************************************/
/*                            GetGridAttrs()                            */
/************************************************************************/

void HDF4ImageDataset::GetGridAttrs( int32 hGD )
{
/* -------------------------------------------------------------------- */
/*      At the start we will fetch the global HDF attributes.           */
/* -------------------------------------------------------------------- */
    int32 hDummy = 0;

    EHidinfo( hHDF4, &hDummy, &hSD );
    ReadGlobalAttributes( hSD );
    papszLocalMetadata = CSLDuplicate( papszGlobalMetadata );

/* -------------------------------------------------------------------- */
/*      Fetch the esoteric HDF-EOS attributes then.                     */
/* -------------------------------------------------------------------- */
    int32 nStrBufSize = 0;

    if( GDinqattrs( hGD, nullptr, &nStrBufSize ) > 0 && nStrBufSize > 0 )
    {
        char *pszAttrList
            = reinterpret_cast<char *>( CPLMalloc( nStrBufSize + 1 ) );
        GDinqattrs( hGD, pszAttrList, &nStrBufSize );

#ifdef DEBUG
        CPLDebug( "HDF4Image", "List of attributes in grid %s: %s",
                  pszFieldName, pszAttrList );
#endif

        char **papszAttributes = CSLTokenizeString2( pszAttrList, ",",
                                                     CSLT_HONOURSTRINGS );
        const int l_nAttrs = CSLCount( papszAttributes );
        for ( int i = 0; i < l_nAttrs; i++ )
        {
            int32 l_iNumType = 0;
            int32 nValues = 0;

            GDattrinfo( hGD, papszAttributes[i], &l_iNumType, &nValues );
            const int nDataTypeSize = GetDataTypeSize(l_iNumType);
            if( nDataTypeSize == 0 )
                continue;
            CPLAssert( (nValues % nDataTypeSize) == 0);

            void *pData = CPLMalloc( nValues + 1);
            GDreadattr( hGD, papszAttributes[i], pData );

            if( l_iNumType == DFNT_CHAR8 || l_iNumType == DFNT_UCHAR8 )
            {
                reinterpret_cast<char *>( pData )[nValues] = '\0';
                papszLocalMetadata = CSLAddNameValue( papszLocalMetadata,
                                                      papszAttributes[i],
                                                      (const char *) pData );
            }
            else
            {
                char *pszTemp = SPrintArray( GetDataType(l_iNumType), pData,
                                             nValues / nDataTypeSize, ", " );
                papszLocalMetadata = CSLAddNameValue( papszLocalMetadata,
                                                      papszAttributes[i],
                                                      pszTemp );
                CPLFree( pszTemp );
            }

            CPLFree( pData );
        }

        CSLDestroy( papszAttributes );
        CPLFree( pszAttrList );
    }

/* -------------------------------------------------------------------- */
/*      After fetching HDF-EOS specific stuff we will read the generic  */
/*      HDF attributes and append them to the list of metadata.         */
/* -------------------------------------------------------------------- */
    int32 l_iSDS = 0;
    if( GDsdid(hGD, pszFieldName, &l_iSDS) != -1 )
    {
        int32 l_iRank = 0;
        int32 l_iNumType = 0;
        int32 l_nAttrs = 0;
        int32 nValues = 0;
        char l_szName[HDF4_SDS_MAXNAMELEN] = {};
        int32 l_aiDimSizes[H4_MAX_VAR_DIMS] = {};

        if( SDgetinfo( l_iSDS, l_szName, &l_iRank, l_aiDimSizes, &l_iNumType,
                       &l_nAttrs) == 0 )
        {
            for( int32 iAttribute = 0; iAttribute < l_nAttrs; iAttribute++ )
            {
                char szAttrName[H4_MAX_NC_NAME] = {};
                SDattrinfo( l_iSDS, iAttribute, szAttrName,
                            &l_iNumType, &nValues );
                papszLocalMetadata =
                    TranslateHDF4Attributes( l_iSDS, iAttribute,
                                             szAttrName, l_iNumType,
                                             nValues, papszLocalMetadata );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Finally make the whole list visible.                            */
/* -------------------------------------------------------------------- */
    SetMetadata( papszLocalMetadata );
}

/************************************************************************/
/*                     ProcessModisSDSGeolocation()                     */
/*                                                                      */
/*      Recognise latitude and longitude geolocation arrays in          */
/*      simple SDS datasets like:                                       */
/*                                                                      */
/*      download.osgeo.org/gdal/data/hdf4/A2006005182000.L2_LAC_SST.x.hdf */
/*                                                                      */
/*      As reported in ticket #1895.                                    */
/************************************************************************/

void HDF4ImageDataset::ProcessModisSDSGeolocation(void)

{
    // No point in assigning geolocation to the geolocation SDSes themselves.
    if( EQUAL(szName,"longitude") || EQUAL(szName,"latitude") )
        return;

    if( nRasterYSize == 1 )
        return;

/* -------------------------------------------------------------------- */
/*      Scan for latitude and longitude sections.                       */
/* -------------------------------------------------------------------- */
    int32 nDatasets = 0;
    int32 nAttributes = 0;

    if( SDfileinfo( hSD, &nDatasets, &nAttributes ) != 0 )
        return;

    int nLongitudeWidth = 0;
    int nLongitudeHeight = 0;
    int nLatitudeWidth = 0;
    int nLatitudeHeight = 0;
    int iXIndex=-1;
    int iYIndex=-1;
    for( int iDSIndex = 0; iDSIndex < nDatasets; iDSIndex++ )
    {
        int32 l_iRank = 0;
        int32 l_iNumType = 0;
        int32 l_nAttrs = 0;
        char l_szName[HDF4_SDS_MAXNAMELEN] = {};
        int32 l_aiDimSizes[H4_MAX_VAR_DIMS] = {};

        const int32 l_iSDS = SDselect( hSD, iDSIndex );

        if( SDgetinfo( l_iSDS, l_szName, &l_iRank, l_aiDimSizes, &l_iNumType,
                       &l_nAttrs) == 0 )
        {
            if( EQUAL(l_szName,"latitude") )
            {
                iYIndex = iDSIndex;
                if( l_iRank == 2 )
                {
                    nLatitudeWidth = l_aiDimSizes[1];
                    nLatitudeHeight = l_aiDimSizes[0];
                }
            }

            if( EQUAL(l_szName,"longitude") )
            {
                iXIndex = iDSIndex;
                if( l_iRank == 2 )
                {
                    nLongitudeWidth = l_aiDimSizes[1];
                    nLongitudeHeight = l_aiDimSizes[0];
                }
            }
        }

        SDendaccess(l_iSDS);
    }

    if( iXIndex == -1 || iYIndex == -1 )
        return;

    int nPixelOffset = 0;
    int nLineOffset = 0;
    int nPixelStep = 1;
    int nLineStep = 1;
    if( nLongitudeWidth != nLatitudeWidth ||
        nLongitudeHeight != nLatitudeHeight )
    {
        CPLDebug("HDF4", "Longitude and latitude subdatasets don't have same "
                 "dimensions...");
    }
    else if( nLongitudeWidth > 0 && nLongitudeHeight > 0 )
    {
        nPixelStep
            = static_cast<int>( 0.5 + 1.0 * nRasterXSize / nLongitudeWidth );
        nLineStep
            = static_cast<int>( 0.5 + 1.0 * nRasterYSize / nLongitudeHeight );
        nPixelOffset = (nPixelStep-1) / 2;
        nLineOffset = (nLineStep-1) / 2;
    }

/* -------------------------------------------------------------------- */
/*      We found geolocation information.  Record it as metadata.       */
/* -------------------------------------------------------------------- */

    SetMetadataItem( "SRS", SRS_WKT_WGS84_LAT_LONG, "GEOLOCATION" );

    CPLString  osWrk;
    osWrk.Printf( "HDF4_SDS:UNKNOWN:\"%s\":%d",
                  pszFilename, iXIndex );
    SetMetadataItem( "X_DATASET", osWrk, "GEOLOCATION" );
    SetMetadataItem( "X_BAND", "1" , "GEOLOCATION" );

    osWrk.Printf( "HDF4_SDS:UNKNOWN:\"%s\":%d",
                  pszFilename, iYIndex );
    SetMetadataItem( "Y_DATASET", osWrk, "GEOLOCATION" );
    SetMetadataItem( "Y_BAND", "1" , "GEOLOCATION" );

    SetMetadataItem( "PIXEL_OFFSET", CPLSPrintf("%d", nPixelOffset),
                     "GEOLOCATION" );
    SetMetadataItem( "PIXEL_STEP", CPLSPrintf("%d", nPixelStep),
                     "GEOLOCATION" );

    SetMetadataItem( "LINE_OFFSET", CPLSPrintf("%d", nLineOffset),
                     "GEOLOCATION" );
    SetMetadataItem( "LINE_STEP", CPLSPrintf("%d", nLineStep), "GEOLOCATION" );
}

/************************************************************************/
/*                      ProcessSwathGeolocation()                       */
/*                                                                      */
/*      Handle the swath geolocation data for a swath.  Attach          */
/*      geolocation metadata corresponding to it (if there is no        */
/*      lattice), and also attach it as GCPs.  This is only invoked     */
/*      for EOS_SWATH, not EOS_SWATH_GEOL datasets.                     */
/************************************************************************/

int HDF4ImageDataset::ProcessSwathGeolocation( int32 hSW, char **papszDimList )
{

/* -------------------------------------------------------------------- */
/*  Determine a product name.                                           */
/* -------------------------------------------------------------------- */
    const char *pszProduct =
        CSLFetchNameValue( papszLocalMetadata, "SHORTNAME" );

    HDF4EOSProduct eProduct = PROD_UNKNOWN;
    if( pszProduct )
    {
        if( STARTS_WITH_CI(pszProduct, "ASTL1A") )
            eProduct = PROD_ASTER_L1A;
        else if( STARTS_WITH_CI(pszProduct, "ASTL1B") )
            eProduct = PROD_ASTER_L1B;
        else if( STARTS_WITH_CI(pszProduct, "AST_04")
                 || STARTS_WITH_CI(pszProduct, "AST_05")
                 || STARTS_WITH_CI(pszProduct, "AST_06")
                 || STARTS_WITH_CI(pszProduct, "AST_07")
                 || STARTS_WITH_CI(pszProduct, "AST_08")
                 || STARTS_WITH_CI(pszProduct, "AST_09")
                 || STARTS_WITH_CI(pszProduct, "AST13")
                 || STARTS_WITH_CI(pszProduct, "AST3") )
            eProduct = PROD_ASTER_L2;
        else if( STARTS_WITH_CI(pszProduct, "AST14") )
            eProduct = PROD_ASTER_L3;
        else if( STARTS_WITH_CI(pszProduct, "MOD02")
                 || STARTS_WITH_CI(pszProduct, "MYD02") )
            eProduct = PROD_MODIS_L1B;
        else if( STARTS_WITH_CI(pszProduct, "MOD07_L2") )
            eProduct = PROD_MODIS_L2;
    }

/* -------------------------------------------------------------------- */
/*      Read names of geolocation fields and corresponding              */
/*      geolocation maps.                                               */
/* -------------------------------------------------------------------- */
    int32 nStrBufSize = 0;
    const int32 nDataFields = SWnentries( hSW, HDFE_NENTGFLD, &nStrBufSize );
    if( nDataFields < 0 || nDataFields > 1024 * 1024 )
        return FALSE;
    char *pszGeoList = reinterpret_cast<char *>( CPLMalloc( nStrBufSize + 1 ) );
    int32 *paiRank
        = reinterpret_cast<int32 *>( CPLMalloc( nDataFields * sizeof(int32) ) );
    int32 *paiNumType
        = reinterpret_cast<int32 *>( CPLMalloc( nDataFields * sizeof(int32) ) );

    if( nDataFields != SWinqgeofields(hSW, pszGeoList, paiRank, paiNumType) )
    {
        CPLDebug( "HDF4Image",
                  "Can't get the list of geolocation fields in swath \"%s\"",
                  pszSubdatasetName );
    }

#ifdef DEBUG
    else
    {
        CPLDebug( "HDF4Image",
                  "Number of geolocation fields in swath \"%s\": %ld",
                  pszSubdatasetName, static_cast<long>( nDataFields ) );
        CPLDebug( "HDF4Image",
                  "List of geolocation fields in swath \"%s\": %s",
                  pszSubdatasetName, pszGeoList );
        char *pszTmp = SPrintArray( GDT_UInt32, paiRank,
                              nDataFields, "," );
        CPLDebug( "HDF4Image",
                  "Geolocation fields ranks: %s", pszTmp );
        CPLFree( pszTmp );
    }
#endif

    CPLFree( paiRank );
    CPLFree( paiNumType );

/* -------------------------------------------------------------------- */
/*      Read geolocation data.                                          */
/* -------------------------------------------------------------------- */
    char    szXGeo[N_BUF_SIZE] = "";
    char    szYGeo[N_BUF_SIZE] = "";
    char    szPixel[N_BUF_SIZE]= "";
    char    szLine[N_BUF_SIZE] = "";
    int32 *paiOffset = nullptr;
    int32 *paiIncrement = nullptr;

    int32   nDimMaps = SWnentries( hSW, HDFE_NENTMAP, &nStrBufSize );
    if( nDimMaps <= 0 )
    {

#ifdef DEBUG
        CPLDebug( "HDF4Image", "No geolocation maps in swath \"%s\"",
                  pszSubdatasetName );
        CPLDebug( "HDF4Image",
                  "Suppose one-to-one mapping. X field is \"%s\", "
                  "Y field is \"%s\"",
                  papszDimList[iXDim], papszDimList[iYDim] );
#endif

        snprintf( szPixel, sizeof(szPixel), "%s", papszDimList[iXDim] );

        snprintf( szLine, sizeof(szLine), "%s", papszDimList[iYDim]);

        snprintf( szXGeo, sizeof(szXGeo), "%s", papszDimList[iXDim]);

        snprintf( szYGeo, sizeof(szYGeo), "%s", papszDimList[iYDim]);

        paiOffset = reinterpret_cast<int32 *>( CPLCalloc( 2, sizeof(int32) ) );
        paiIncrement
            = reinterpret_cast<int32 *>( CPLCalloc( 2, sizeof(int32) ) );
        paiOffset[0] = 0;
        paiOffset[1] = 0;
        paiIncrement[0] = 1;
        paiIncrement[1] = 1;
    }
    else
    {
        char *pszDimMaps
            = reinterpret_cast<char *>( CPLMalloc( nStrBufSize + 1 ) );
        paiOffset
            = reinterpret_cast<int32 *>( CPLCalloc( nDimMaps, sizeof(int32) ) );
        paiIncrement
            = reinterpret_cast<int32 *>( CPLCalloc( nDimMaps, sizeof(int32) ) );

        *pszDimMaps = '\0';
        if( nDimMaps != SWinqmaps(hSW, pszDimMaps, paiOffset, paiIncrement) )
        {
            CPLDebug( "HDF4Image",
                      "Can't get the list of geolocation maps in swath \"%s\"",
                      pszSubdatasetName );
        }

#ifdef DEBUG
        else
        {

            CPLDebug( "HDF4Image",
                      "List of geolocation maps in swath \"%s\": %s",
                      pszSubdatasetName, pszDimMaps );
            char *pszTmp = SPrintArray( GDT_Int32, paiOffset,
                                  nDimMaps, "," );
            CPLDebug( "HDF4Image",
                      "Geolocation map offsets: %s", pszTmp );
            CPLFree( pszTmp );
            pszTmp = SPrintArray( GDT_Int32, paiIncrement,
                                  nDimMaps, "," );
            CPLDebug( "HDF4Image",
                      "Geolocation map increments: %s", pszTmp );
            CPLFree( pszTmp );
        }
#endif

        char **papszDimMap =
            CSLTokenizeString2( pszDimMaps, ",", CSLT_HONOURSTRINGS );
        const int nDimMapCount = CSLCount(papszDimMap);

        for( int i = 0; i < nDimMapCount; i++ )
        {
            if( strstr(papszDimMap[i], papszDimList[iXDim]) )
            {
                snprintf( szPixel, sizeof(szPixel), "%s", papszDimList[iXDim] );

                snprintf( szXGeo, sizeof(szXGeo), "%s", papszDimMap[i] );

                char *pszTemp = strchr( szXGeo, '/' );
                if( pszTemp )
                    *pszTemp = '\0';
            }
            else if( strstr(papszDimMap[i], papszDimList[iYDim]) )
            {
                snprintf( szLine, sizeof(szLine), "%s", papszDimList[iYDim] );

                snprintf( szYGeo, sizeof(szYGeo), "%s", papszDimMap[i] );

                char *pszTemp = strchr( szYGeo, '/' );
                if( pszTemp )
                    *pszTemp = '\0';
            }
        }

        CSLDestroy( papszDimMap );
        CPLFree( pszDimMaps );
    }

    if( *szXGeo == 0 || *szYGeo == 0 )
    {
        CPLFree( paiOffset );
        CPLFree( paiIncrement );
        CPLFree( pszGeoList );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Read geolocation fields.                                        */
/* -------------------------------------------------------------------- */
    char szGeoDimList[N_BUF_SIZE] = "";
    char **papszGeolocations = CSLTokenizeString2( pszGeoList, ",",
                                                   CSLT_HONOURSTRINGS );
    const int nGeolocationsCount = CSLCount( papszGeolocations );
    int32 l_aiDimSizes[H4_MAX_VAR_DIMS] = {};

    int32 iWrkNumType = 0;
    void *pLat = nullptr;
    void *pLong = nullptr;

    int32 l_iRank = 0;
    int32 nLatCount = 0;
    int32 nLongCount = 0;
    int32 nXPoints = 0;
    int32 nYPoints = 0;
    int iDataSize = 0;

    int iPixelDim = -1;
    int iLineDim = -1;
    int iLongDim = -1;
    int iLatDim = -1;

    for( int i = 0; i < nGeolocationsCount; i++ )
    {
        // Skip "SceneLineNumber" table if present in the list of geolocation
        // fields. It is not needed to fetch geocoding data.
        if( EQUAL(papszGeolocations[i], "SceneLineNumber") )
            continue;

        if( SWfieldinfo( hSW, papszGeolocations[i], &l_iRank,
                         l_aiDimSizes, &iWrkNumType, szGeoDimList ) < 0 )
        {

            CPLDebug( "HDF4Image",
                      "Can't read attributes of geolocation field \"%s\"",
                      papszGeolocations[i] );
            CSLDestroy(papszGeolocations);
            CPLFree( paiOffset );
            CPLFree( paiIncrement );
            CPLFree( pszGeoList );
            return FALSE;
        }

        CPLDebug( "HDF4Image",
                  "List of dimensions in geolocation field \"%s\": %s",
                  papszGeolocations[i], szGeoDimList );

        char **papszGeoDimList = CSLTokenizeString2( szGeoDimList,
                                                     ",", CSLT_HONOURSTRINGS );

        const int iXGeo = CSLFindString( papszGeoDimList, szXGeo );
        const int iYGeo = CSLFindString( papszGeoDimList, szYGeo );
        if( CSLCount(papszGeoDimList) > H4_MAX_VAR_DIMS
            || iXGeo < 0
            || iYGeo < 0 )
        {
            CSLDestroy( papszGeoDimList );
            CSLDestroy(papszGeolocations);
            CPLFree( paiOffset );
            CPLFree( paiIncrement );
            CPLFree( pszGeoList );
            return FALSE;
        }

        nXPoints = l_aiDimSizes[iXGeo];
        nYPoints = l_aiDimSizes[iYGeo];

        if( EQUAL(szPixel, papszDimList[iXDim]) )
        {
            iPixelDim = 1;
            iLineDim = 0;
        }
        else
        {
            iPixelDim = 0;
            iLineDim = 1;
        }

        iDataSize = GetDataTypeSize( iWrkNumType );
        if( strstr( papszGeolocations[i], "Latitude" ) )
        {
            iLatDim = i;
            nLatCount = nXPoints * nYPoints;
            pLat = CPLMalloc( nLatCount * iDataSize );
            if( SWreadfield( hSW, papszGeolocations[i], nullptr,
                             nullptr, nullptr, (VOIDP)pLat ) < 0 )
            {
                CPLDebug( "HDF4Image",
                          "Can't read geolocation field %s",
                          papszGeolocations[i]);
                CPLFree( pLat );
                pLat = nullptr;
            }
        }
        else if( strstr( papszGeolocations[i], "Longitude" ) )
        {
            iLongDim = i;
            nLongCount = nXPoints * nYPoints;
            pLong = CPLMalloc( nLongCount * iDataSize );
            if( SWreadfield( hSW, papszGeolocations[i], nullptr,
                             nullptr, nullptr, (VOIDP)pLong ) < 0 )
            {
                CPLDebug( "HDF4Image",
                          "Can't read geolocation field %s",
                          papszGeolocations[i]);
                CPLFree( pLong );
                pLong = nullptr;
            }
        }

        CSLDestroy( papszGeoDimList );
    }

/* -------------------------------------------------------------------- */
/*      Do we have a lattice table?                                     */
/* -------------------------------------------------------------------- */
    void *pLatticeX = nullptr;
    void *pLatticeY = nullptr;
    int32 iLatticeType = 0;
    int32 iLatticeDataSize = 0;
    char pszLatticePoint[] = "LatticePoint";
    if( SWfieldinfo(hSW, pszLatticePoint, &l_iRank, l_aiDimSizes,
                    &iLatticeType, szGeoDimList) == 0
        && l_iRank == 3
        && nXPoints == l_aiDimSizes[1]
        && nYPoints == l_aiDimSizes[0]
        && l_aiDimSizes[2] == 2 )
    {
        iLatticeDataSize = GetDataTypeSize( iLatticeType );

        int32 iStart[H4_MAX_NC_DIMS] = {};
        int32 iEdges[H4_MAX_NC_DIMS] = {};
        iStart[1] = 0;
        iEdges[1] = nXPoints;

        iStart[0] = 0;
        iEdges[0] = nYPoints;

        iStart[2] = 0;
        iEdges[2] = 1;

        pLatticeX = CPLMalloc( nLatCount * iLatticeDataSize );
        if( SWreadfield( hSW, pszLatticePoint, iStart, nullptr,
                         iEdges, (VOIDP)pLatticeX ) < 0 )
        {
            CPLDebug( "HDF4Image", "Can't read lattice field" );
            CPLFree( pLatticeX );
            pLatticeX = nullptr;
        }

        iStart[2] = 1;
        iEdges[2] = 1;

        pLatticeY = CPLMalloc( nLatCount * iLatticeDataSize );
        if( SWreadfield( hSW, pszLatticePoint, iStart, nullptr,
                         iEdges, (VOIDP)pLatticeY ) < 0 )
        {
            CPLDebug( "HDF4Image", "Can't read lattice field" );
            CPLFree( pLatticeY );
            pLatticeY = nullptr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Determine whether to use no, partial or full GCPs.              */
/* -------------------------------------------------------------------- */
    const char *pszGEOL_AS_GCPS = CPLGetConfigOption( "GEOL_AS_GCPS",
                                                      "PARTIAL" );
    int iGCPStepX = 0;
    int iGCPStepY = 0;

    if( EQUAL(pszGEOL_AS_GCPS,"NONE") )
    {
        // Leave as is: iGCPStepX = iGCPStepY = 0;
    }
    else if( EQUAL(pszGEOL_AS_GCPS,"FULL") )
    {
        iGCPStepX = 1;
        iGCPStepY = 1;
    }
    else
    {
        // Aim for 10x10 grid or so.
        iGCPStepX = std::max(static_cast<int32>(1), ((nXPoints - 1) / 11));
        iGCPStepY = std::max(static_cast<int32>(1), ((nYPoints - 1) / 11));
    }

/* -------------------------------------------------------------------- */
/*  Fetch projection information for various datasets.                  */
/* -------------------------------------------------------------------- */
    if( nLatCount && nLongCount && nLatCount == nLongCount
        && pLat && pLong )
    {
        CPLFree( pszGCPProjection );
        pszGCPProjection = nullptr;

        // ASTER Level 1A
        if( eProduct == PROD_ASTER_L1A )
        {
            pszGCPProjection = CPLStrdup(
                "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\","
                "6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],"
                "TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",\"6326\"]],"
                "PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],"
                "UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\","
                "\"9108\"]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],"
                "AUTHORITY[\"EPSG\",\"4326\"]]" );
        }

        // ASTER Level 1B, Level 2
        else if( eProduct == PROD_ASTER_L1B
                 || eProduct == PROD_ASTER_L2 )
        {
            // Construct the metadata keys.
            // A band number is taken from the field name.
            const char *pszBand = strpbrk( pszFieldName, "0123456789" );

            if( !pszBand )
                pszBand = "";

            char *pszProjLine =
                CPLStrdup(CPLSPrintf("MPMETHOD%s", pszBand));
            char *pszParamsLine =
                CPLStrdup(CPLSPrintf("PROJECTIONPARAMETERS%s",
                                     pszBand));
            char *pszZoneLine =
                CPLStrdup(CPLSPrintf("UTMZONECODE%s",
                                     pszBand));
            char *pszEllipsoidLine =
                CPLStrdup(CPLSPrintf("ELLIPSOIDANDDATUM%s",
                                     pszBand));

            // Fetch projection related values from the
            // metadata.
            const char *pszProj =
                CSLFetchNameValue( papszLocalMetadata,
                                   pszProjLine );
            const char *pszParams =
                CSLFetchNameValue( papszLocalMetadata,
                                   pszParamsLine );
            const char *pszZone =
                CSLFetchNameValue( papszLocalMetadata,
                                   pszZoneLine );

#ifdef DEBUG
            const char* pszEllipsoid =
                CSLFetchNameValue( papszLocalMetadata,
                                   pszEllipsoidLine );

            CPLDebug( "HDF4Image",
                      "Projection %s=%s, parameters %s=%s, "
                      "zone %s=%s",
                      pszProjLine, pszProj, pszParamsLine,
                      pszParams, pszZoneLine, pszZone );
            CPLDebug( "HDF4Image", "Ellipsoid %s=%s",
                      pszEllipsoidLine, pszEllipsoid );
#endif

            // Transform all mnemonic codes in the values.
            // Projection is UTM by default
            const long iProjSys = pszProj ? USGSMnemonicToCode(pszProj) : 1L;
            const long iZone = (pszZone && iProjSys == 1L) ? atoi(pszZone): 0L;
#if 0  // Not needed without the WGS84 check.
            char **papszEllipsoid = (pszEllipsoid) ?
                CSLTokenizeString2( pszEllipsoid, ",",
                                    CSLT_HONOURSTRINGS ) : NULL;
#endif

            const long iEllipsoid = 8L; // WGS84 by default
#if 0  // This block is redundant.
            if( papszEllipsoid && CSLCount(papszEllipsoid) > 0 )
            {
                if (EQUAL( papszEllipsoid[0], "WGS84"))
                    iEllipsoid = 8L;
            }
#endif
            char **papszParams = pszParams ?
                CSLTokenizeString2( pszParams, ",", CSLT_HONOURSTRINGS ) : nullptr;
            std::vector<double> adfProjParams(15);
            if( papszParams )
            {
                for( int i = 0; i < 15 && papszParams[i] != nullptr ; i++)
                    adfProjParams[i] = CPLAtof( papszParams[i] );
            }

            // Create projection definition
            oSRS.importFromUSGS( iProjSys, iZone,
                                 adfProjParams.data(), iEllipsoid );
            oSRS.SetLinearUnits( SRS_UL_METER, 1.0 );
            oSRS.exportToWkt( &pszGCPProjection );

            CSLDestroy( papszParams );
            CPLFree( pszEllipsoidLine );
            CPLFree( pszZoneLine );
            CPLFree( pszParamsLine );
            CPLFree( pszProjLine );
        }

        // ASTER Level 3 (DEM)
        else if( eProduct == PROD_ASTER_L3 )
        {
            double dfCenterX = 0.0;
            double dfCenterY = 0.0;

            ReadCoordinates( CSLFetchNameValue(
                                 papszGlobalMetadata, "SCENECENTER" ),
                             &dfCenterY, &dfCenterX );

            // Calculate UTM zone from scene center coordinates
            const int iZone = 30 + static_cast<int>( (dfCenterX + 6.0) / 6.0 );

            // Create projection definition
            if( dfCenterY > 0 )
                oSRS.SetUTM( iZone, TRUE );
            else
                oSRS.SetUTM( - iZone, FALSE );
            oSRS.SetWellKnownGeogCS( "WGS84" );
            oSRS.SetLinearUnits( SRS_UL_METER, 1.0 );
            oSRS.exportToWkt( &pszGCPProjection );
        }

        // MODIS L1B
        else if( eProduct == PROD_MODIS_L1B
                 || eProduct == PROD_MODIS_L2 )
        {
            pszGCPProjection = CPLStrdup( SRS_WKT_WGS84_LAT_LONG );
        }

/* -------------------------------------------------------------------- */
/*  Fill the GCPs list.                                                 */
/* -------------------------------------------------------------------- */
        if( iGCPStepX > 0 )
        {
            nGCPCount = (((nXPoints-1) / iGCPStepX) + 1)
                * (((nYPoints-1) / iGCPStepY) + 1);

            pasGCPList = reinterpret_cast<GDAL_GCP *>(
                CPLCalloc( nGCPCount, sizeof( GDAL_GCP ) ) );
            GDALInitGCPs( nGCPCount, pasGCPList );

            int iGCP = 0;
            for( int i = 0; i < nYPoints; i += iGCPStepY )
            {
                for( int j = 0; j < nXPoints; j += iGCPStepX )
                {
                    const int iGeoOff = i * nXPoints + j;

                    pasGCPList[iGCP].dfGCPX =
                        AnyTypeToDouble(
                            iWrkNumType,
                            reinterpret_cast<void *>(
                                reinterpret_cast<char *>( pLong )
                                + iGeoOff*iDataSize));
                    pasGCPList[iGCP].dfGCPY =
                        AnyTypeToDouble(
                            iWrkNumType,
                            reinterpret_cast<void *>(
                                reinterpret_cast<char*>( pLat )
                                + iGeoOff*iDataSize));

                    // GCPs in Level 1A/1B dataset are in geocentric
                    // coordinates. Convert them in geodetic (we
                    // will convert latitudes only, longitudes
                    // do not need to be converted, because
                    // they are the same).
                    // This calculation valid for WGS84 datum only.
                    if( eProduct == PROD_ASTER_L1A
                        || eProduct == PROD_ASTER_L1B )
                    {
                        pasGCPList[iGCP].dfGCPY =
                            atan(tan(pasGCPList[iGCP].dfGCPY
                                     *PI/180)/0.99330562)*180/PI;
                    }

                    ToGeoref(&pasGCPList[iGCP].dfGCPX,
                             &pasGCPList[iGCP].dfGCPY);

                    pasGCPList[iGCP].dfGCPZ = 0.0;

                    if( pLatticeX && pLatticeY )
                    {
                        pasGCPList[iGCP].dfGCPPixel =
                            AnyTypeToDouble(
                                iLatticeType,
                                reinterpret_cast<void *>(
                                    reinterpret_cast<char *>( pLatticeX )
                                    + iGeoOff*iLatticeDataSize ) ) + 0.5;
                        pasGCPList[iGCP].dfGCPLine =
                            AnyTypeToDouble(
                                iLatticeType,
                                reinterpret_cast<void *>(
                                    reinterpret_cast<char *>( pLatticeY )
                                    + iGeoOff*iLatticeDataSize ) ) + 0.5;
                    }
                    else if( paiOffset && paiIncrement )
                    {
                        pasGCPList[iGCP].dfGCPPixel =
                            paiOffset[iPixelDim] +
                            j * paiIncrement[iPixelDim] + 0.5;
                        pasGCPList[iGCP].dfGCPLine =
                            paiOffset[iLineDim] +
                            i * paiIncrement[iLineDim] + 0.5;
                    }

                    iGCP++;
                }
            }
        }

/* -------------------------------------------------------------------- */
/*      Establish geolocation metadata, but only if there is no         */
/*      lattice.  The lattice destroys the regularity of the grid.      */
/* -------------------------------------------------------------------- */
        if( pLatticeX == nullptr
            && iLatDim != -1 && iLongDim != -1
            && iPixelDim != -1 && iLineDim != -1 )
        {
            SetMetadataItem( "SRS", pszGCPProjection, "GEOLOCATION" );

            CPLString  osWrk;
            osWrk.Printf( "HDF4_EOS:EOS_SWATH_GEOL:\"%s\":%s:%s",
                          pszFilename, pszSubdatasetName,
                          papszGeolocations[iLongDim] );
            SetMetadataItem( "X_DATASET", osWrk, "GEOLOCATION" );
            SetMetadataItem( "X_BAND", "1" , "GEOLOCATION" );

            osWrk.Printf( "HDF4_EOS:EOS_SWATH_GEOL:\"%s\":%s:%s",
                          pszFilename, pszSubdatasetName,
                          papszGeolocations[iLatDim] );
            SetMetadataItem( "Y_DATASET", osWrk, "GEOLOCATION" );
            SetMetadataItem( "Y_BAND", "1" , "GEOLOCATION" );

            if( paiOffset && paiIncrement )
            {
                osWrk.Printf( "%ld",
                              static_cast<long>( paiOffset[iPixelDim] ) );
                SetMetadataItem( "PIXEL_OFFSET", osWrk, "GEOLOCATION" );
                osWrk.Printf( "%ld",
                              static_cast<long>( paiIncrement[iPixelDim] ) );
                SetMetadataItem( "PIXEL_STEP", osWrk, "GEOLOCATION" );

                osWrk.Printf( "%ld", static_cast<long>( paiOffset[iLineDim] ) );
                SetMetadataItem( "LINE_OFFSET", osWrk, "GEOLOCATION" );
                osWrk.Printf( "%ld",
                              static_cast<long>( paiIncrement[iLineDim] ) );
                SetMetadataItem( "LINE_STEP", osWrk, "GEOLOCATION" );
            }
        }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
        CPLFree( pLatticeX );
        CPLFree( pLatticeY );
        CPLFree( pLat );
        CPLFree( pLong );

        if( iGCPStepX == 0 )
        {
            CPLFree( pszGCPProjection );
            pszGCPProjection = nullptr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( paiOffset );
    CPLFree( paiIncrement );
    CPLFree( pszGeoList );
    CSLDestroy( papszGeolocations );

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *HDF4ImageDataset::Open( GDALOpenInfo * poOpenInfo )
{
    if( !STARTS_WITH_CI(poOpenInfo->pszFilename, "HDF4_SDS:") &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "HDF4_GR:") &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "HDF4_GD:") &&
        !STARTS_WITH_CI(poOpenInfo->pszFilename, "HDF4_EOS:") )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->fpL != nullptr )
    {
        VSIFCloseL(poOpenInfo->fpL);
        poOpenInfo->fpL = nullptr;
    }

    HDF4ImageDataset *poDS = new HDF4ImageDataset( );
    CPLMutexHolderD(&hHDF4Mutex);

    char **papszSubdatasetName
        = CSLTokenizeString2( poOpenInfo->pszFilename,
        ":", CSLT_HONOURSTRINGS | CSLT_PRESERVEQUOTES | CSLT_PRESERVEESCAPES);
    if( CSLCount( papszSubdatasetName ) != 4
        && CSLCount( papszSubdatasetName ) != 5
        && CSLCount( papszSubdatasetName ) != 6 )
    {
        CSLDestroy( papszSubdatasetName );
        // Release mutex otherwise we deadlock with GDALDataset own mutex.
        CPLReleaseMutex(hHDF4Mutex);
        delete poDS;
        CPLAcquireMutex(hHDF4Mutex, 1000.0);
        return nullptr;
    }

    {
        // Un-quote filename
        size_t nLenPart2 = strlen(papszSubdatasetName[2]);
        if( papszSubdatasetName[2][0] == '"' &&
                papszSubdatasetName[2][nLenPart2-1] == '"' )
        {
            papszSubdatasetName[2][nLenPart2-1] = 0;
            memmove(papszSubdatasetName[2], papszSubdatasetName[2] + 1,
                    nLenPart2-1);
        }
    }

    /* -------------------------------------------------------------------- */
    /*    Check for drive name in windows HDF4_xx:TYPE:D:\...               */
    /* -------------------------------------------------------------------- */
    if( strlen(papszSubdatasetName[2]) == 1 )
    {
        const size_t nLen = 2 + strlen(papszSubdatasetName[3]) + 1;
        char* pszFilename = reinterpret_cast<char *>( CPLMalloc( nLen ) );
        snprintf(pszFilename, nLen,
                 "%s:%s", papszSubdatasetName[2], papszSubdatasetName[3]);
        CPLFree(papszSubdatasetName[2]);
        CPLFree(papszSubdatasetName[3]);
        papszSubdatasetName[2] = pszFilename;

        /* Move following arguments one rank upper */
        papszSubdatasetName[3] = papszSubdatasetName[4];
        if( papszSubdatasetName[4] != nullptr )
        {
            papszSubdatasetName[4] = papszSubdatasetName[5];
            papszSubdatasetName[5] = nullptr;
        }
    }

    for( int i = 3; papszSubdatasetName[i] != nullptr; i++ )
    {
        // Un-quote and unescape components after filename
        size_t nLenPart = strlen(papszSubdatasetName[i]);
        if( papszSubdatasetName[i][0] == '"' &&
                papszSubdatasetName[i][nLenPart-1] == '"' )
        {
            CPLString osStr(papszSubdatasetName[i]);
            osStr.replaceAll("\\\\", '\\');
            osStr.replaceAll("\\\"", '"');
            if( osStr[0] == '"' && osStr.back() == '"' )
            {
                osStr = osStr.substr(1, osStr.size()-2);
                CPLFree(papszSubdatasetName[i]);
                papszSubdatasetName[i] = CPLStrdup(osStr);
            }
        }
    }

    poDS->pszFilename = CPLStrdup( papszSubdatasetName[2] );

    if( EQUAL( papszSubdatasetName[0], "HDF4_SDS" ) )
        poDS->iDatasetType = HDF4_SDS;
    else if( EQUAL( papszSubdatasetName[0], "HDF4_GR" ) )
        poDS->iDatasetType = HDF4_GR;
    else if( EQUAL( papszSubdatasetName[0], "HDF4_EOS" ) )
        poDS->iDatasetType = HDF4_EOS;
    else
        poDS->iDatasetType = HDF4_UNKNOWN;

    if( EQUAL( papszSubdatasetName[1], "GDAL_HDF4" ) )
        poDS->iSubdatasetType = H4ST_GDAL;
    else if( EQUAL( papszSubdatasetName[1], "EOS_GRID" ) )
        poDS->iSubdatasetType = H4ST_EOS_GRID;
    else if( EQUAL( papszSubdatasetName[1], "EOS_SWATH" ) )
        poDS->iSubdatasetType = H4ST_EOS_SWATH;
    else if( EQUAL( papszSubdatasetName[1], "EOS_SWATH_GEOL" ) )
        poDS->iSubdatasetType = H4ST_EOS_SWATH_GEOL;
    else if( EQUAL( papszSubdatasetName[1], "SEAWIFS_L3" ) )
        poDS->iSubdatasetType= H4ST_SEAWIFS_L3;
    else if( EQUAL( papszSubdatasetName[1], "HYPERION_L1" ) )
        poDS->iSubdatasetType= H4ST_HYPERION_L1;
    else
        poDS->iSubdatasetType = H4ST_UNKNOWN;

/* -------------------------------------------------------------------- */
/*      Is our file still here?                                         */
/* -------------------------------------------------------------------- */
    if( !Hishdf( poDS->pszFilename ) )
    {
        CSLDestroy( papszSubdatasetName );
        // Release mutex otherwise we deadlock with GDALDataset own mutex.
        CPLReleaseMutex(hHDF4Mutex);
        delete poDS;
        CPLAcquireMutex(hHDF4Mutex, 1000.0);
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Collect the remain (post filename) components to treat as       */
/*      the subdataset name.                                            */
/* -------------------------------------------------------------------- */
    CPLString osSubdatasetName = papszSubdatasetName[3];
    if( papszSubdatasetName[4] != nullptr )
    {
        osSubdatasetName += ":";
        osSubdatasetName += papszSubdatasetName[4];
    }

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    double dfNoData = 0.0;
    double dfScale = 1.0;
    double dfOffset = 0.0;
    bool bNoDataSet = false;
    bool bHaveScale = false;
    bool bHaveOffset = false;
    const char *pszUnits = nullptr;
    const char *pszDescription = nullptr;

/* -------------------------------------------------------------------- */
/*      Select SDS or GR to read from.                                  */
/* -------------------------------------------------------------------- */
    if( poDS->iDatasetType == HDF4_EOS )
    {
        if( papszSubdatasetName[4] == nullptr )
        {
            // Release mutex.  Otherwise it will deadlock with GDALDataset's own
            // mutex.
            CPLReleaseMutex(hHDF4Mutex);
            delete poDS;
            CPLAcquireMutex(hHDF4Mutex, 1000.0);
            return nullptr;
        }
        poDS->pszSubdatasetName = CPLStrdup( papszSubdatasetName[3] );
        poDS->pszFieldName = CPLStrdup( papszSubdatasetName[4] );
    }
    else
    {
        CPLAssert( papszSubdatasetName[3] );
        poDS->iDataset = atoi( papszSubdatasetName[3] );
    }
    CSLDestroy( papszSubdatasetName );

    switch ( poDS->iDatasetType )
    {
      case HDF4_EOS:
      {
          switch ( poDS->iSubdatasetType )
          {

/* -------------------------------------------------------------------- */
/*  HDF-EOS Swath.                                                      */
/* -------------------------------------------------------------------- */
            case H4ST_EOS_SWATH:
            case H4ST_EOS_SWATH_GEOL:
            {
                if( poOpenInfo->eAccess == GA_ReadOnly )
                    poDS->hHDF4 = SWopen( poDS->pszFilename, DFACC_READ );
                else
                    poDS->hHDF4 = SWopen( poDS->pszFilename, DFACC_WRITE );

                if( poDS->hHDF4 <= 0 )
                {
                    CPLDebug( "HDF4Image",
                              "Can't open file \"%s\" for swath reading",
                              poDS->pszFilename );
                    // Release mutex otherwise we deadlock with GDALDataset own
                    // mutex.
                    CPLReleaseMutex(hHDF4Mutex);
                    delete poDS;
                    CPLAcquireMutex(hHDF4Mutex, 1000.0);
                    return nullptr;
                }

                const int32 hSW
                    = SWattach( poDS->hHDF4, poDS->pszSubdatasetName );
                if( hSW < 0 )
                {
                    CPLDebug( "HDF4Image", "Can't attach to subdataset %s",
                              poDS->pszSubdatasetName );
                     // Release mutex otherwise we deadlock with GDALDataset own
                     // mutex.
                    CPLReleaseMutex(hHDF4Mutex);
                    delete poDS;
                    CPLAcquireMutex(hHDF4Mutex, 1000.0);
                    return nullptr;
                }

/* -------------------------------------------------------------------- */
/*      Decode the dimension map.                                       */
/* -------------------------------------------------------------------- */
                int32 nStrBufSize = 0;

                if( SWnentries( hSW, HDFE_NENTDIM, &nStrBufSize ) < 0
                    || nStrBufSize <= 0 )
                {
                    CPLDebug( "HDF4Image",
                              "Can't read a number of dimension maps." );
                    // Release mutex otherwise we deadlock with GDALDataset own
                    // mutex.
                    CPLReleaseMutex(hHDF4Mutex);
                    delete poDS;
                    CPLAcquireMutex(hHDF4Mutex, 1000.0);
                    return nullptr;
                }

                char *pszDimList = reinterpret_cast<char *>(
                    CPLMalloc( nStrBufSize + 1 ) );
                if( SWfieldinfo( hSW, poDS->pszFieldName, &poDS->iRank,
                                 poDS->aiDimSizes, &poDS->iNumType,
                                 pszDimList ) < 0 )
                {
                    CPLDebug( "HDF4Image", "Can't read dimension maps." );
                    CPLFree( pszDimList );
                     // Release mutex otherwise we deadlock with GDALDataset own
                     // mutex.
                    CPLReleaseMutex(hHDF4Mutex);
                    delete poDS;
                    CPLAcquireMutex(hHDF4Mutex, 1000.0);
                    return nullptr;
                }
                pszDimList[nStrBufSize] = '\0';

#ifdef DEBUG
                CPLDebug( "HDF4Image",
                          "List of dimensions in swath \"%s\": %s",
                          poDS->pszFieldName, pszDimList );
#endif

                poDS->GetImageDimensions( pszDimList );

#ifdef DEBUG
                CPLDebug( "HDF4Image",
                          "X dimension is %d, Y dimension is %d",
                          poDS->iXDim, poDS->iYDim );
#endif

/* -------------------------------------------------------------------- */
/*  Fetch metadata.                                                     */
/* -------------------------------------------------------------------- */
                // Not H4ST_EOS_SWATH_GEOL.
                if( poDS->iSubdatasetType == H4ST_EOS_SWATH )
                    poDS->GetSwatAttrs( hSW );

/* -------------------------------------------------------------------- */
/*  Fetch NODATA value.                                                 */
/* -------------------------------------------------------------------- */
                void *pNoDataValue =
                    CPLMalloc( poDS->GetDataTypeSize(poDS->iNumType) );
                if( SWgetfillvalue( hSW, poDS->pszFieldName,
                                    pNoDataValue ) != -1 )
                {
                    dfNoData = poDS->AnyTypeToDouble( poDS->iNumType,
                                                      pNoDataValue );
                    bNoDataSet = true;
                }
                else
                {
                    const char *pszNoData =
                        CSLFetchNameValue( poDS->papszLocalMetadata,
                                           "_FillValue" );
                    if( pszNoData )
                    {
                        dfNoData = CPLAtof( pszNoData );
                        bNoDataSet = true;
                    }
                }
                CPLFree( pNoDataValue );

/* -------------------------------------------------------------------- */
/*      Handle Geolocation processing.                                  */
/* -------------------------------------------------------------------- */
                // Not H4ST_SWATH_GEOL.
                if( poDS->iSubdatasetType == H4ST_EOS_SWATH )
                {
                    char **papszDimList =
                        CSLTokenizeString2( pszDimList, ",",
                                            CSLT_HONOURSTRINGS );
                    if( !poDS->ProcessSwathGeolocation( hSW, papszDimList ) )
                    {
                        CPLDebug( "HDF4Image",
                                  "No geolocation available for this swath." );
                    }
                    CSLDestroy( papszDimList );
                }

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
                CPLFree( pszDimList );
                SWdetach( hSW );
            }
            break;

/* -------------------------------------------------------------------- */
/*      HDF-EOS Grid.                                                   */
/* -------------------------------------------------------------------- */
            case H4ST_EOS_GRID:
            {
                if( poOpenInfo->eAccess == GA_ReadOnly )
                    poDS->hHDF4 = GDopen( poDS->pszFilename, DFACC_READ );
                else
                    poDS->hHDF4 = GDopen( poDS->pszFilename, DFACC_WRITE );

                if( poDS->hHDF4 <= 0 )
                {
                    CPLDebug( "HDF4Image",
                              "Can't open file \"%s\" for grid reading",
                              poDS->pszFilename );
                    // Release mutex otherwise we deadlock with GDALDataset own
                    // mutex.
                    CPLReleaseMutex(hHDF4Mutex);
                    delete poDS;
                    CPLAcquireMutex(hHDF4Mutex, 1000.0);
                    return nullptr;
                }

                const int32 hGD
                    = GDattach( poDS->hHDF4, poDS->pszSubdatasetName );

/* -------------------------------------------------------------------- */
/*      Decode the dimension map.                                       */
/* -------------------------------------------------------------------- */
                char szDimList[N_BUF_SIZE] = {};
                GDfieldinfo( hGD, poDS->pszFieldName, &poDS->iRank,
                             poDS->aiDimSizes, &poDS->iNumType, szDimList );
#ifdef DEBUG
                CPLDebug( "HDF4Image",
                          "List of dimensions in grid %s: %s",
                          poDS->pszFieldName, szDimList);
#endif
                poDS->GetImageDimensions( szDimList );

                int32 tilecode = 0;
                int32 tilerank = 0;
                if( GDtileinfo( hGD, poDS->pszFieldName, &tilecode, &tilerank,
                                nullptr ) == 0 )
                {
                    if( tilecode == HDFE_TILE )
                    {
                        int32 *tiledims = reinterpret_cast<int32 *>(
                            CPLCalloc( tilerank , sizeof( int32 ) ) );
                        GDtileinfo( hGD, poDS->pszFieldName, &tilecode,
                                    &tilerank, tiledims );
                        if( ( tilerank == 2 ) && ( poDS->iRank == tilerank  ) )
                        {
                            poDS->nBlockPreferredXSize = tiledims[1];
                            poDS->nBlockPreferredYSize = tiledims[0];
                            poDS->bReadTile = true;
#ifdef DEBUG
                            CPLDebug( "HDF4_EOS:EOS_GRID:",
                                      "tilerank in grid %s: %d",
                                      poDS->pszFieldName,
                                      static_cast<int>( tilerank ) );
                            CPLDebug( "HDF4_EOS:EOS_GRID:",
                                      "tiledimens in grid %s: %d,%d",
                                      poDS->pszFieldName,
                                      static_cast<int>( tiledims[0] ),
                                      static_cast<int>( tiledims[1] ) );
#endif
                        }
#ifdef DEBUG
                        else
                        {
                            CPLDebug( "HDF4_EOS:EOS_GRID:",
                                      "tilerank in grid %s: %d not supported",
                                      poDS->pszFieldName,
                                      static_cast<int>( tilerank ) );
                        }
#endif
                        CPLFree(tiledims);
                    }
                    else
                    {
#ifdef DEBUG
                        CPLDebug( "HDF4_EOS:EOS_GRID:",
                                  "tilecode == HDFE_NOTILE in grid %s: %d",
                                  poDS->pszFieldName ,
                                  static_cast<int>( poDS->iRank ) );
#endif
                    }
                }

#ifdef DEBUG
                else
                {
                    CPLDebug( "HDF4_EOS:EOS_GRID:", "ERROR GDtileinfo %s",
                              poDS->pszFieldName );
                }
#endif

/* -------------------------------------------------------------------- */
/*      Fetch projection information                                    */
/* -------------------------------------------------------------------- */
                int32 iProjCode = 0;
                int32 iZoneCode = 0;
                int32 iSphereCode = 0;
                double adfProjParams[15];

                if( GDprojinfo( hGD, &iProjCode, &iZoneCode,
                                &iSphereCode, adfProjParams) >= 0 )
                {
#ifdef DEBUG
                    CPLDebug( "HDF4Image",
                              "Grid projection: "
                              "projection code: %ld, zone code %ld, "
                              "sphere code %ld",
                              static_cast<long>( iProjCode ),
                              static_cast<long>( iZoneCode ),
                              static_cast<long>( iSphereCode ) );
#endif
                    poDS->oSRS.importFromUSGS( iProjCode, iZoneCode,
                                               adfProjParams, iSphereCode,
                                               USGS_ANGLE_RADIANS );

                    CPLFree( poDS->pszProjection );
                    poDS->oSRS.exportToWkt( &poDS->pszProjection );
                }

/* -------------------------------------------------------------------- */
/*      Fetch geotransformation matrix                                  */
/* -------------------------------------------------------------------- */
                int32 nXSize = 0;
                int32 nYSize = 0;
                double adfUpLeft[2];
                double adfLowRight[2];
                if( GDgridinfo( hGD, &nXSize, &nYSize,
                                adfUpLeft, adfLowRight ) >= 0 )
                {
#ifdef DEBUG
                    CPLDebug( "HDF4Image",
                              "Grid geolocation: "
                              "top left X %f, top left Y %f, "
                              "low right X %f, low right Y %f, "
                              "cols %ld, rows %ld",
                              adfUpLeft[0], adfUpLeft[1],
                              adfLowRight[0], adfLowRight[1],
                              static_cast<long>( nXSize ),
                              static_cast<long>( nYSize ) );
#endif
                    if( iProjCode )
                    {
                        // For projected systems coordinates are in meters.
                        poDS->adfGeoTransform[1] =
                            (adfLowRight[0] - adfUpLeft[0]) / nXSize;
                        poDS->adfGeoTransform[5] =
                            (adfLowRight[1] - adfUpLeft[1]) / nYSize;
                        poDS->adfGeoTransform[0] = adfUpLeft[0];
                        poDS->adfGeoTransform[3] = adfUpLeft[1];
                    }
                    else
                    {
                        // Handle angular geographic coordinates here.
                        poDS->adfGeoTransform[1] =
                            (CPLPackedDMSToDec(adfLowRight[0]) -
                             CPLPackedDMSToDec(adfUpLeft[0])) / nXSize;
                        poDS->adfGeoTransform[5] =
                            (CPLPackedDMSToDec(adfLowRight[1]) -
                             CPLPackedDMSToDec(adfUpLeft[1])) / nYSize;
                        poDS->adfGeoTransform[0] =
                            CPLPackedDMSToDec(adfUpLeft[0]);
                        poDS->adfGeoTransform[3] =
                            CPLPackedDMSToDec(adfUpLeft[1]);
                    }
                    poDS->adfGeoTransform[2] = 0.0;
                    poDS->adfGeoTransform[4] = 0.0;
                    poDS->bHasGeoTransform = true;
                }

/* -------------------------------------------------------------------- */
/*      Fetch metadata.                                                 */
/* -------------------------------------------------------------------- */
                poDS->GetGridAttrs( hGD );

                GDdetach( hGD );

/* -------------------------------------------------------------------- */
/*      Fetch NODATA value.                                             */
/* -------------------------------------------------------------------- */
                void *pNoDataValue =
                    CPLMalloc( poDS->GetDataTypeSize(poDS->iNumType) );
                if( GDgetfillvalue( hGD, poDS->pszFieldName,
                                    pNoDataValue ) != -1 )
                {
                    dfNoData = poDS->AnyTypeToDouble( poDS->iNumType,
                                                      pNoDataValue );
                    bNoDataSet = true;
                }
                else
                {
                    const char *pszNoData =
                        CSLFetchNameValue( poDS->papszLocalMetadata,
                                           "_FillValue" );
                    if( pszNoData )
                    {
                        dfNoData = CPLAtof( pszNoData );
                        bNoDataSet = true;
                    }
                }
                CPLFree( pNoDataValue );
            }
            break;

            default:
              break;
          }

/* -------------------------------------------------------------------- */
/*      Fetch unit type, scale, offset and description                  */
/*      Should be similar among various HDF-EOS kinds.                  */
/* -------------------------------------------------------------------- */
          {
              const char *pszTmp =
                  CSLFetchNameValue( poDS->papszLocalMetadata,
                                     "scale_factor" );
              if( pszTmp )
              {
                  dfScale = CPLAtof( pszTmp );
                  // Some producers (i.e. lndcsm from LEDAPS) emit
                  // files with scale_factor=0 which is crazy to carry
                  // through.
                  if( dfScale == 0.0 )
                    dfScale = 1.0;

                  bHaveScale = (dfScale != 0.0);
              }

              pszTmp =
                  CSLFetchNameValue( poDS->papszLocalMetadata, "add_offset" );
              if( pszTmp )
              {
                  dfOffset = CPLAtof( pszTmp );
                  bHaveOffset = true;
              }

              pszUnits = CSLFetchNameValue( poDS->papszLocalMetadata,
                                            "units" );
              pszDescription = CSLFetchNameValue( poDS->papszLocalMetadata,
                                            "long_name" );
          }
      }
      break;

/* -------------------------------------------------------------------- */
/*  'Plain' HDF scientific datasets.                                    */
/* -------------------------------------------------------------------- */
      case HDF4_SDS:
      {
#ifdef HDF4_HAS_MAXOPENFILES
          // Attempt to increase maximum number of opened HDF files
          intn nCurrMax = 0;
          intn nSysLimit = 0;

          if( SDget_maxopenfiles(&nCurrMax, &nSysLimit) >= 0
              && nCurrMax < nSysLimit )
          {
              SDreset_maxopenfiles( nSysLimit );
          }
#endif // HDF4_HAS_MAXOPENFILES

          if( poOpenInfo->eAccess == GA_ReadOnly )
              poDS->hHDF4 = Hopen( poDS->pszFilename, DFACC_READ, 0 );
          else
              poDS->hHDF4 = Hopen( poDS->pszFilename, DFACC_WRITE, 0 );

          if( poDS->hHDF4 <= 0 )
          {
              // Release mutex otherwise we deadlock with GDALDataset own mutex.
              CPLReleaseMutex(hHDF4Mutex);
              delete poDS;
              CPLAcquireMutex(hHDF4Mutex, 1000.0);
              return nullptr;
          }

          poDS->hSD = SDstart( poDS->pszFilename, DFACC_READ );
          if( poDS->hSD == -1 )
          {
            // Release mutex otherwise we deadlock with GDALDataset own mutex.
              CPLReleaseMutex(hHDF4Mutex);
              delete poDS;
              CPLAcquireMutex(hHDF4Mutex, 1000.0);
              return nullptr;
          }

          if( poDS->ReadGlobalAttributes( poDS->hSD ) != CE_None )
          {
              // Release mutex otherwise we deadlock with GDALDataset own mutex.
              CPLReleaseMutex(hHDF4Mutex);
              delete poDS;
              CPLAcquireMutex(hHDF4Mutex, 1000.0);
              return nullptr;
          }

          int32 nDatasets = 0;
          int32 l_nAttrs = 0;
          if( SDfileinfo( poDS->hSD, &nDatasets, &l_nAttrs ) != 0 )
          {
              // Release mutex otherwise we deadlock with GDALDataset own mutex.
              CPLReleaseMutex(hHDF4Mutex);
              delete poDS;
              CPLAcquireMutex(hHDF4Mutex, 1000.0);
              return nullptr;
          }

          if( poDS->iDataset < 0 || poDS->iDataset >= nDatasets )
          {
              CPLError(CE_Failure, CPLE_AppDefined,
                       "Subdataset index should be between 0 and %ld",
                       static_cast<long int>( nDatasets ) - 1);
              // Release mutex otherwise we deadlock with GDALDataset own mutex.
              CPLReleaseMutex(hHDF4Mutex);
              delete poDS;
              CPLAcquireMutex(hHDF4Mutex, 1000.0);
              return nullptr;
          }

          memset( poDS->aiDimSizes, 0, sizeof(int32) * H4_MAX_VAR_DIMS );
          const int32 iSDS = SDselect( poDS->hSD, poDS->iDataset );
          SDgetinfo( iSDS, poDS->szName, &poDS->iRank, poDS->aiDimSizes,
                     &poDS->iNumType, &poDS->nAttrs);

          // We will duplicate global metadata for every subdataset.
          poDS->papszLocalMetadata =
              CSLDuplicate( poDS->papszGlobalMetadata );

          for( int32 iAttribute = 0; iAttribute < poDS->nAttrs; iAttribute++ )
          {
              char szAttrName[H4_MAX_NC_NAME] = {};
              int32 iAttrNumType = 0;
              int32 nValues = 0;
              SDattrinfo( iSDS, iAttribute, szAttrName,
                          &iAttrNumType, &nValues );
              poDS->papszLocalMetadata =
                  poDS->TranslateHDF4Attributes( iSDS, iAttribute,
                                                 szAttrName, iAttrNumType,
                                                 nValues,
                                                 poDS->papszLocalMetadata );
          }
          poDS->SetMetadata( poDS->papszLocalMetadata, "" );
          SDendaccess( iSDS );

#ifdef DEBUG
          CPLDebug( "HDF4Image",
                    "aiDimSizes[0]=%ld, aiDimSizes[1]=%ld, "
                    "aiDimSizes[2]=%ld, aiDimSizes[3]=%ld",
                    static_cast<long>( poDS->aiDimSizes[0] ),
                    static_cast<long>( poDS->aiDimSizes[1] ),
                    static_cast<long>( poDS->aiDimSizes[2] ),
                    static_cast<long>( poDS->aiDimSizes[3] ) );
#endif
          switch( poDS->iRank )
          {
            case 1:
              poDS->nBandCount = 1;
              poDS->iXDim = 0;
              poDS->iYDim = -1;
              break;
            case 2:
              poDS->nBandCount = 1;
              poDS->iXDim = 1;
              poDS->iYDim = 0;
              break;
            case 3:
              // TODO: We should probably remove the following test as there are
              // valid datasets where the height is lower than the band number.
              // For example:
              //   http://www.iapmw.unibe.ch/research/projects/FriOWL/data/otd/LISOTD_HRAC_V2.2.hdf
              // which is a 720x360 x 365 bands.
              // Use a HACK for now.
              if( poDS->aiDimSizes[0] < poDS->aiDimSizes[2] &&
                  !(poDS->aiDimSizes[0] == 360 &&
                    poDS->aiDimSizes[1] == 720 &&
                    poDS->aiDimSizes[2] == 365) )
              {
                  poDS->iBandDim = 0;
                  poDS->iXDim = 2;
                  poDS->iYDim = 1;
              }
              else
              {
                  if( poDS->aiDimSizes[1] <= poDS->aiDimSizes[0] &&
                      poDS->aiDimSizes[1] <= poDS->aiDimSizes[2] )
                  {
                      poDS->iBandDim = 1;
                      poDS->iXDim = 2;
                      poDS->iYDim = 0;
                  }
                  else
                  {
                      poDS->iBandDim = 2;
                      poDS->iXDim = 1;
                      poDS->iYDim = 0;
                  }
              }
              poDS->nBandCount = poDS->aiDimSizes[poDS->iBandDim];
              break;
            case 4: // FIXME
              poDS->nBandCount = poDS->aiDimSizes[2] * poDS->aiDimSizes[3];
              break;
            default:
              break;
          }

          // We preset this because CaptureNRLGeoTransform needs it.
          poDS->nRasterXSize = poDS->aiDimSizes[poDS->iXDim];
          if( poDS->iYDim >= 0 )
            poDS->nRasterYSize = poDS->aiDimSizes[poDS->iYDim];
          else
            poDS->nRasterYSize = 1;

          // Special case projection info for NRL generated files.
          const char *pszMapProjectionSystem =
              CSLFetchNameValue(poDS->papszGlobalMetadata,
                                "mapProjectionSystem");
          if( pszMapProjectionSystem != nullptr
              && EQUAL(pszMapProjectionSystem,"NRL(USGS)") )
          {
              poDS->CaptureNRLGeoTransform();
          }

          // Special case for coastwatch hdf files.
          if( CSLFetchNameValue( poDS->papszGlobalMetadata,
                                 "gctp_sys" ) != nullptr )
              poDS->CaptureCoastwatchGCTPInfo();

          // Special case for MODIS geolocation
          poDS->ProcessModisSDSGeolocation();

          // Special case for NASA/CCRS Landsat in HDF
          poDS->CaptureL1GMTLInfo();
      }
      break;

/* -------------------------------------------------------------------- */
/*  'Plain' HDF rasters.                                                */
/* -------------------------------------------------------------------- */
      case HDF4_GR:
      {
        // Attempt to increase maximum number of opened HDF files.
#ifdef HDF4_HAS_MAXOPENFILES
        intn nCurrMax = 0;
        intn nSysLimit = 0;

        if( SDget_maxopenfiles(&nCurrMax, &nSysLimit) >= 0
            && nCurrMax < nSysLimit )
        {
            SDreset_maxopenfiles( nSysLimit );
        }
#endif  // HDF4_HAS_MAXOPENFILES

        if( poOpenInfo->eAccess == GA_ReadOnly )
            poDS->hHDF4 = Hopen( poDS->pszFilename, DFACC_READ, 0 );
        else
            poDS->hHDF4 = Hopen( poDS->pszFilename, DFACC_WRITE, 0 );

        if( poDS->hHDF4 <= 0 )
        {
            // Release mutex otherwise we deadlock with GDALDataset own mutex.
            CPLReleaseMutex(hHDF4Mutex);
            delete poDS;
            CPLAcquireMutex(hHDF4Mutex, 1000.0);
            return nullptr;
        }

        poDS->hGR = GRstart( poDS->hHDF4 );
        if( poDS->hGR == -1 )
        {
            // Release mutex otherwise wel deadlock with GDALDataset own mutex.
            CPLReleaseMutex(hHDF4Mutex);
            delete poDS;
            CPLAcquireMutex(hHDF4Mutex, 1000.0);
            return nullptr;
        }

        poDS->iGR = GRselect( poDS->hGR, poDS->iDataset );
        if( GRgetiminfo( poDS->iGR, poDS->szName,
                         &poDS->iRank, &poDS->iNumType,
                         &poDS->iInterlaceMode, poDS->aiDimSizes,
                         &poDS->nAttrs ) != 0 )
        {
            // Release mutex otherwise we deadlock with GDALDataset own mutex.
            CPLReleaseMutex(hHDF4Mutex);
            delete poDS;
            CPLAcquireMutex(hHDF4Mutex, 1000.0);
            return nullptr;
        }

        // We will duplicate global metadata for every subdataset.
        poDS->papszLocalMetadata = CSLDuplicate( poDS->papszGlobalMetadata );

        poDS->SetMetadata( poDS->papszLocalMetadata, "" );
        // Read colour table

        poDS->iPal = GRgetlutid ( poDS->iGR, 0 );
        if( poDS->iPal != -1 )
        {
            GRgetlutinfo( poDS->iPal, &poDS->nComps, &poDS->iPalDataType,
                          &poDS->iPalInterlaceMode, &poDS->nPalEntries );
            if( poDS->nPalEntries && poDS->nComps == 3 &&
                GDALGetDataTypeSizeBytes(GetDataType(poDS->iPalDataType)) == 1 &&
                poDS->nPalEntries <= 256 )
            {
                GRreadlut( poDS->iPal, poDS->aiPaletteData );
                poDS->poColorTable = new GDALColorTable();
                GDALColorEntry oEntry;
                for( int i = 0; i < std::min(static_cast<int>(poDS->nPalEntries), N_COLOR_ENTRIES); i++ )
                {
                    oEntry.c1 = poDS->aiPaletteData[i][0];
                    oEntry.c2 = poDS->aiPaletteData[i][1];
                    oEntry.c3 = poDS->aiPaletteData[i][2];
                    oEntry.c4 = 255;

                    poDS->poColorTable->SetColorEntry( i, &oEntry );
                }
            }
        }

        poDS->iXDim = 0;
        poDS->iYDim = 1;
        poDS->nBandCount = poDS->iRank;
        break;
      }
      default:
        // Release mutex otherwise we'll deadlock with GDALDataset own mutex
        CPLReleaseMutex(hHDF4Mutex);
        delete poDS;
        CPLAcquireMutex(hHDF4Mutex, 1000.0);
        return nullptr;
    }

    poDS->nRasterXSize = poDS->aiDimSizes[poDS->iXDim];
    if( poDS->iYDim >= 0 )
        poDS->nRasterYSize = poDS->aiDimSizes[poDS->iYDim];
    else
        poDS->nRasterYSize = 1;

    if( poDS->iSubdatasetType == H4ST_HYPERION_L1 )
    {
        // XXX: Hyperion SDSs has Height x Bands x Width dimensions scheme
        if( poDS->iRank > 2 )
        {
            poDS->nBandCount = poDS->aiDimSizes[1];
            poDS->nRasterXSize = poDS->aiDimSizes[2];
            poDS->nRasterYSize = poDS->aiDimSizes[0];
        }
        else
        {
            poDS->nBandCount = poDS->aiDimSizes[0];
            poDS->nRasterXSize = poDS->aiDimSizes[1];
            poDS->nRasterYSize = 1;
        }
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int i = 1; i <= poDS->nBandCount; i++ )
    {
        HDF4ImageRasterBand *poBand =
            new HDF4ImageRasterBand( poDS, i,
                                     poDS->GetDataType( poDS->iNumType ) );
        poDS->SetBand( i, poBand );

        if( bNoDataSet )
            poBand->SetNoDataValue( dfNoData );
        if( bHaveScale )
        {
            poBand->bHaveScale = true;
            poBand->dfScale = dfScale;
        }
        if( bHaveOffset )
        {
            poBand->bHaveOffset = true;
            poBand->dfOffset = dfOffset;
        }
        if( pszUnits )
            poBand->osUnitType =  pszUnits;
        if( pszDescription )
            poBand->SetDescription( pszDescription );
    }

/* -------------------------------------------------------------------- */
/*      Now we will handle particular types of HDF products. Every      */
/*      HDF product has its own structure.                              */
/* -------------------------------------------------------------------- */

    switch ( poDS->iSubdatasetType )
    {
/* -------------------------------------------------------------------- */
/*  HDF, created by GDAL.                                               */
/* -------------------------------------------------------------------- */
      case H4ST_GDAL:
      {
          CPLDebug( "HDF4Image",
                    "Input dataset interpreted as GDAL_HDF4" );

          const char  *pszValue =
            CSLFetchNameValue( poDS->papszGlobalMetadata, "Projection" );
          if( pszValue != nullptr )
          {
              CPLFree( poDS->pszProjection );
              poDS->pszProjection = CPLStrdup( pszValue );
          }
          if( (pszValue = CSLFetchNameValue(poDS->papszGlobalMetadata,
                                            "TransformationMatrix")) != nullptr )
          {
              int i = 0;
              char *pszString = const_cast<char *>( pszValue );
              while( *pszValue && i < 6 )
              {
                  poDS->adfGeoTransform[i++] = CPLStrtod(pszString, &pszString);
                  pszString++;
              }
              poDS->bHasGeoTransform = true;
          }
          for( int i = 1; i <= poDS->nBands; i++ )
          {
              if( (pszValue =
                   CSLFetchNameValue(poDS->papszGlobalMetadata,
                                     CPLSPrintf("BandDesc%d", i))) != nullptr )
                  poDS->GetRasterBand( i )->SetDescription( pszValue );
          }
          for( int i = 1; i <= poDS->nBands; i++ )
          {
              if( (pszValue =
                   CSLFetchNameValue(poDS->papszGlobalMetadata,
                                     CPLSPrintf("NoDataValue%d", i))) != nullptr )
                  poDS->GetRasterBand(i)->SetNoDataValue( CPLAtof(pszValue) );
          }
      }
      break;

/* -------------------------------------------------------------------- */
/*      SeaWiFS Level 3 Standard Mapped Image Products.                 */
/*      Organized similar to MODIS Level 3 products.                    */
/* -------------------------------------------------------------------- */
      case H4ST_SEAWIFS_L3:
      {
          CPLDebug( "HDF4Image", "Input dataset interpreted as SEAWIFS_L3" );

          // Read band description
          for ( int i = 1; i <= poDS->nBands; i++ )
          {
              poDS->GetRasterBand( i )->SetDescription(
                  CSLFetchNameValue( poDS->papszGlobalMetadata, "Parameter" ) );
          }

          // Read coordinate system and geotransform matrix.
          poDS->oSRS.SetWellKnownGeogCS( "WGS84" );

          if( EQUAL(CSLFetchNameValue(poDS->papszGlobalMetadata,
                                      "Map Projection"),
                    "Equidistant Cylindrical") )
          {
              poDS->oSRS.SetEquirectangular( 0.0, 0.0, 0.0, 0.0 );
              poDS->oSRS.SetLinearUnits( SRS_UL_METER, 1 );
              CPLFree( poDS->pszProjection );
              poDS->oSRS.exportToWkt( &poDS->pszProjection );
          }

          double dfULX = CPLAtof( CSLFetchNameValue(poDS->papszGlobalMetadata,
                                                    "Westernmost Longitude") );
          double dfULY = CPLAtof( CSLFetchNameValue(poDS->papszGlobalMetadata,
                                                    "Northernmost Latitude") );
          double dfLRX = CPLAtof( CSLFetchNameValue(poDS->papszGlobalMetadata,
                                                    "Easternmost Longitude") );
          double dfLRY = CPLAtof( CSLFetchNameValue(poDS->papszGlobalMetadata,
                                                    "Southernmost Latitude") );
          poDS->ToGeoref( &dfULX, &dfULY );
          poDS->ToGeoref( &dfLRX, &dfLRY );
          poDS->adfGeoTransform[0] = dfULX;
          poDS->adfGeoTransform[3] = dfULY;
          poDS->adfGeoTransform[1] = (dfLRX - dfULX) / poDS->nRasterXSize;
          poDS->adfGeoTransform[5] = (dfULY - dfLRY) / poDS->nRasterYSize;
          if( dfULY > 0)  // Northern hemisphere.
              poDS->adfGeoTransform[5] = - poDS->adfGeoTransform[5];
          poDS->adfGeoTransform[2] = 0.0;
          poDS->adfGeoTransform[4] = 0.0;
          poDS->bHasGeoTransform = true;
      }
      break;

/* -------------------------------------------------------------------- */
/*      Generic SDS                                                     */
/* -------------------------------------------------------------------- */
      case H4ST_UNKNOWN:
      {
          // This is a coastwatch convention.
          if( CSLFetchNameValue( poDS->papszLocalMetadata, "missing_value" ) )
          {
              for( int i = 1; i <= poDS->nBands; i++ )
              {
                  poDS->GetRasterBand(i)->SetNoDataValue(
                      CPLAtof( CSLFetchNameValue(poDS->papszLocalMetadata,
                                                 "missing_value") ) );
              }
          }

          // Coastwatch offset and scale.
          if( CSLFetchNameValue( poDS->papszLocalMetadata, "scale_factor" )
              && CSLFetchNameValue( poDS->papszLocalMetadata, "add_offset" ) )
          {
              for( int i = 1; i <= poDS->nBands; i++ )
              {
                  HDF4ImageRasterBand *poBand
                      = reinterpret_cast<HDF4ImageRasterBand *>(
                          poDS->GetRasterBand(i) );

                  poBand->bHaveScale = true;
                  poBand->bHaveOffset = true;
                  poBand->dfScale =
                      CPLAtof( CSLFetchNameValue( poDS->papszLocalMetadata,
                                                  "scale_factor" ) );
                  // See #4891 regarding offset interpretation.
                  //  poBand->dfOffset = -1 * poBand->dfScale *
                  //      CPLAtof( CSLFetchNameValue( poDS->papszLocalMetadata,
                  //                                  "add_offset" ) );
                  poBand->dfOffset =
                      CPLAtof( CSLFetchNameValue( poDS->papszLocalMetadata,
                                                  "add_offset" ) );
              }
          }

          // This is a modis level3 convention (data from ACT)
          // e.g. data/hdf/act/modis/MODAM2004280160000.L3_NOAA_GMX

          if( CSLFetchNameValue( poDS->papszLocalMetadata,
                                 "scalingSlope" )
              && CSLFetchNameValue( poDS->papszLocalMetadata,
                                    "scalingIntercept" ) )
          {
              CPLString osUnits;

              if( CSLFetchNameValue( poDS->papszLocalMetadata,
                                     "productUnits" ) )
              {
                  osUnits = CSLFetchNameValue( poDS->papszLocalMetadata,
                                               "productUnits" );
              }

              for( int i = 1; i <= poDS->nBands; i++ )
              {
                  HDF4ImageRasterBand *poBand =
                      reinterpret_cast<HDF4ImageRasterBand *>(
                          poDS->GetRasterBand(i) );

                  poBand->bHaveScale = true;
                  poBand->bHaveOffset = true;
                  poBand->dfScale =
                      CPLAtof( CSLFetchNameValue( poDS->papszLocalMetadata,
                                                  "scalingSlope" ) );
                  poBand->dfOffset =
                      CPLAtof( CSLFetchNameValue( poDS->papszLocalMetadata,
                                                  "scalingIntercept" ) );

                  poBand->osUnitType = osUnits;
              }
          }
      }
      break;

/* -------------------------------------------------------------------- */
/*      Hyperion Level 1.                                               */
/* -------------------------------------------------------------------- */
      case H4ST_HYPERION_L1:
      {
          CPLDebug( "HDF4Image", "Input dataset interpreted as HYPERION_L1" );
      }
      break;

      default:

#ifdef DEBUG_VERBOSE
        CPLError( CE_Debug, CPLE_AppDefined,
                  "Unknown subdata type %d", poDS->iSubdatasetType );
#endif
        break;
    }

/* -------------------------------------------------------------------- */
/*      Setup PAM info for this subdataset                              */
/* -------------------------------------------------------------------- */
    poDS->SetPhysicalFilename( poDS->pszFilename );
    poDS->SetSubdatasetName( osSubdatasetName );

    // Release mutex otherwise we'll deadlock with GDALDataset own mutex.
    CPLReleaseMutex(hHDF4Mutex);
    poDS->TryLoadXML();

    poDS->oOvManager.Initialize( poDS, ":::VIRTUAL:::" );
    CPLAcquireMutex(hHDF4Mutex, 1000.0);

    return poDS;
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
    if( nBands == 0 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Unable to export files with zero bands." );
        return nullptr;
    }

    // Try now to create the file to avoid memory leaks if it is
    // the SDK that fails to do it.
    VSILFILE* fpVSIL = VSIFOpenL( pszFilename, "wb" );
    if( fpVSIL == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                "Failed to create %s.", pszFilename );
        return nullptr;
    }
    VSIFCloseL(fpVSIL);
    VSIUnlink(pszFilename);

    HDF4ImageDataset *poDS = new HDF4ImageDataset();

    CPLMutexHolderD(&hHDF4Mutex);

/* -------------------------------------------------------------------- */
/*      Choose rank for the created dataset.                            */
/* -------------------------------------------------------------------- */
    poDS->iRank = 3;
    if( CSLFetchNameValue( papszOptions, "RANK" ) != nullptr &&
        EQUAL( CSLFetchNameValue( papszOptions, "RANK" ), "2" ) )
        poDS->iRank = 2;

    poDS->hSD = SDstart( pszFilename, DFACC_CREATE );
    if( poDS->hSD == -1 )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Can't create HDF4 file %s", pszFilename );
        // Release mutex otherwise we'll deadlock with GDALDataset own mutex.
        CPLReleaseMutex(hHDF4Mutex);
        delete poDS;
        CPLAcquireMutex(hHDF4Mutex, 1000.0);
        return nullptr;
    }
    poDS->iXDim = 1;
    poDS->iYDim = 0;
    poDS->iBandDim = 2;

    int32 aiDimSizes[H4_MAX_VAR_DIMS] = {};
    aiDimSizes[poDS->iXDim] = nXSize;
    aiDimSizes[poDS->iYDim] = nYSize;
    aiDimSizes[poDS->iBandDim] = nBands;

    const char *pszSDSName = nullptr;
    int32 iSDS = -1;

    if( poDS->iRank == 2 )
    {
        for( int iBand = 0; iBand < nBands; iBand++ )
        {
            pszSDSName = CPLSPrintf( "Band%d", iBand );
            switch ( eType )
            {
                case GDT_Float64:
                    iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_FLOAT64,
                                     poDS->iRank, aiDimSizes );
                    break;
                case GDT_Float32:
                    iSDS = SDcreate( poDS-> hSD, pszSDSName, DFNT_FLOAT32,
                                     poDS->iRank, aiDimSizes );
                    break;
                case GDT_UInt32:
                    iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_UINT32,
                                     poDS->iRank, aiDimSizes );
                    break;
                case GDT_UInt16:
                    iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_UINT16,
                                     poDS->iRank, aiDimSizes );
                    break;
                case GDT_Int32:
                    iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_INT32,
                                     poDS->iRank, aiDimSizes );
                    break;
                case GDT_Int16:
                    iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_INT16,
                                     poDS->iRank, aiDimSizes );
                    break;
                case GDT_Byte:
                default:
                    iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_UINT8,
                                     poDS->iRank, aiDimSizes );
                    break;
            }
            SDendaccess( iSDS );
        }
    }
    else if( poDS->iRank == 3 )
    {
        pszSDSName = "3-dimensional Scientific Dataset";
        poDS->iDataset = 0;
        switch ( eType )
        {
            case GDT_Float64:
                iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_FLOAT64,
                                 poDS->iRank, aiDimSizes );
                break;
            case GDT_Float32:
                iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_FLOAT32,
                                 poDS->iRank, aiDimSizes );
                break;
            case GDT_UInt32:
                iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_UINT32,
                                 poDS->iRank, aiDimSizes );
                break;
            case GDT_UInt16:
                iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_UINT16,
                                 poDS->iRank, aiDimSizes );
                break;
            case GDT_Int32:
                iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_INT32,
                                 poDS->iRank, aiDimSizes );
                break;
            case GDT_Int16:
                iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_INT16,
                                 poDS->iRank, aiDimSizes );
                break;
            case GDT_Byte:
            default:
                iSDS = SDcreate( poDS->hSD, pszSDSName, DFNT_UINT8,
                                 poDS->iRank, aiDimSizes );
                break;
        }
    }
    else
    {
        // Should never happen.
        // Release mutex otherwise we'll deadlock with GDALDataset own mutex.
        CPLReleaseMutex(hHDF4Mutex);
        delete poDS;
        CPLAcquireMutex(hHDF4Mutex, 1000.0);
        return nullptr;
    }

    if( iSDS < 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Can't create SDS with rank %ld for file %s",
                  static_cast<long>( poDS->iRank ), pszFilename );
        // Release mutex otherwise we'll deadlock with GDALDataset own mutex.
        CPLReleaseMutex(hHDF4Mutex);
        delete poDS;
        CPLAcquireMutex(hHDF4Mutex, 1000.0);
        return nullptr;
    }

    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->eAccess = GA_Update;
    poDS->iDatasetType = HDF4_SDS;
    poDS->iSubdatasetType = H4ST_GDAL;
    poDS->nBands = nBands;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 1; iBand <= nBands; iBand++ )
        poDS->SetBand( iBand, new HDF4ImageRasterBand( poDS, iBand, eType ) );

    SDsetattr( poDS->hSD, "Signature", DFNT_CHAR8,
               static_cast<int>(strlen(pszGDALSignature)) + 1,
               pszGDALSignature );

    return reinterpret_cast<GDALDataset *>( poDS );
}

/************************************************************************/
/*                        GDALRegister_HDF4Image()                      */
/************************************************************************/

void GDALRegister_HDF4Image()

{
    if( GDALGetDriverByName( "HDF4Image" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "HDF4Image" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "HDF4 Dataset" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/hdf4.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte Int16 UInt16 Int32 UInt32 "
                               "Float32 Float64" );
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "   <Option name='RANK' type='int' description='Rank of output SDS'/>"
        "</CreationOptionList>" );

    poDriver->pfnOpen = HDF4ImageDataset::Open;
    poDriver->pfnCreate = HDF4ImageDataset::Create;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
