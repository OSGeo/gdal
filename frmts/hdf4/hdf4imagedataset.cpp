/******************************************************************************
 * $Id$
 *
 * Project:  Hierarchical Data Format Release 4 (HDF4)
 * Purpose:  Read subdatasets of HDF4 file.
 *           This driver initially based on code supplied by Markus Neteler
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <dron@ak4719.spb.edu>
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
 * Revision 1.64  2006/10/23 19:10:00  fwarmerdam
 * Added HXsetdir() call so external datasets work properly.
 *
 * Revision 1.63  2006/10/19 03:21:22  fwarmerdam
 * Added error checking in IReadBlock().
 *
 * Revision 1.62  2006/09/05 16:44:10  dron
 * Support for 4D HDF-EOS Grid datasets.
 *
 * Revision 1.61  2006/08/25 15:11:37  fwarmerdam
 * Fixed problem with poDS->iNumType getting reset improperly for
 * swath datasets (related to geolocation refactoring).
 *
 * Revision 1.60  2006/08/03 14:21:09  dron
 * Use right Long/Lat dimensions when GEOLOCATION records created.
 *
 * Revision 1.59  2006/08/02 14:40:47  fwarmerdam
 * Isolate SWATH geolocation/gcp support in a method.  Incorporate support
 * for subsetting GCPs.  Add support for exposing swath geolocation arrays
 * via RFC4 style metadata.
 *
 * Revision 1.58  2006/06/24 17:13:58  dron
 * Fixed handling of non-image bands of Hyperion sensor.
 *
 * Revision 1.57  2006/05/02 16:42:49  fwarmerdam
 * fix to preserve backslashes when parsing, and various initializations
 *
 * Revision 1.55  2005/11/11 01:11:24  fwarmerdam
 * Added support for Coastwatch offset/scale.
 *
 * Revision 1.54  2005/10/27 16:41:54  fwarmerdam
 * added scale/offset/units support for selected modis products
 *
 * Revision 1.53  2005/10/27 15:43:13  fwarmerdam
 * Fixed bug with NRL geotransform calculation due to nRasterXSize
 * and nRasterYSize not having been set properly yet.
 *
 * Revision 1.52  2005/10/13 15:21:42  fwarmerdam
 * Don't try to use papszMetadata directly.
 *
 * Revision 1.51  2005/04/21 13:42:24  dron
 * Added suppodt for ASTER 3D Orto product.
 *
 * Revision 1.50  2005/04/21 12:52:39  dron
 * Added support for ASTER DE product.
 *
 * Revision 1.49  2005/04/20 14:19:20  dron
 * Added support for ASTER Level 1A products.
 *
 * Revision 1.48  2005/04/19 14:08:09  dron
 * Support for ASTER Level 1B/Level 2 datasets.
 *
 * Revision 1.47  2005/03/30 11:55:36  dron
 * Fixes in Swath geolocation; fetch NoData value for Swath; fetch attributes.
 *
 * Revision 1.46  2005/03/29 12:49:50  dron
 * Fixes in HDF-EOS Grid georeferencing; fetch NoData for Grid datasets.
 *
 * Revision 1.45  2005/03/28 13:33:50  dron
 * Restore support for HDF-EOS Swath GCPs extraction (preliminary version).
 *
 * Revision 1.44  2005/03/21 14:07:39  dron
 * More fixes for geotransformation handling.
 *
 * Revision 1.43  2005/03/21 11:05:02  dron
 * Added 1/2 pixel shift of origin of projected Grid datasets.
 *
 * Revision 1.42  2005/03/14 21:32:26  fwarmerdam
 * Added special support for pulling parent metadata down in MODIS L1B
 * mode, and using the GRing to derive corner coordinates.
 *
 * Revision 1.41  2005/02/24 01:49:24  fwarmerdam
 * Establish iBandDim for rank-3 HDFEOS datasets (swath/grid).
 *
 * Revision 1.40  2005/01/27 16:57:58  fwarmerdam
 * added coastwatch missing_value support
 *
 * Revision 1.39  2005/01/25 20:38:09  fwarmerdam
 * Added coastwatch convention support.
 *
 * Revision 1.38  2005/01/24 17:09:45  fwarmerdam
 * added support for "NRL(USGS) mapProjectionSystem" datasets (geotransform)
 *
 * Revision 1.37  2004/11/26 19:39:18  fwarmerdam
 * avoid initialization warning from purify
 *
 * Revision 1.36  2004/09/08 17:55:13  dron
 * Few problems fixed.
 *
 * Revision 1.35  2004/06/19 21:37:31  dron
 * Use HDF-EOS library for appropriate datasets; major cpde rewrite.
 *
 * Revision 1.34  2003/11/07 15:48:49  dron
 * GetDataType() moved to HDF4Dataset class.
 *
 * Revision 1.33  2003/10/31 19:12:47  dron
 * Read properly GR datasets with multiple samples per pixel.
 *
 * Revision 1.32  2003/10/25 10:34:39  dron
 * Disable GCP collecting for MODIS_L1B datasets.
 *
 * Revision 1.31  2003/09/28 05:45:02  dron
 * More logic to distinguish dimesions.
 */

#include <string.h>
#include <math.h>

#include "hdf.h"
#include "mfhdf.h"

#include "HdfEosDef.h"

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
    PROD_MODIS_L1B
};

/************************************************************************/
/* ==================================================================== */
/*                              HDF4ImageDataset                        */
/* ==================================================================== */
/************************************************************************/

class HDF4ImageDataset : public HDF4Dataset
{
    friend class HDF4ImageRasterBand;

    char        *pszFilename;
    int32       iGR, iPal, iDataset;
    int32       iRank, iNumType, nAttrs,
                iInterlaceMode, iPalInterlaceMode, iPalDataType;
    int32       nComps, nPalEntries;
    int32       aiDimSizes[MAX_VAR_DIMS];
    int         iXDim, iYDim, iBandDim, i4Dim;
    char        **papszLocalMetadata;
#define    N_COLOR_ENTRIES    256
    uint8       aiPaletteData[N_COLOR_ENTRIES][3]; // XXX: Static array for now
    char        szName[65];
    char        *pszSubdatasetName;
    char        *pszFieldName;

    GDALColorTable *poColorTable;

    OGRSpatialReference oSRS;
    int         bHasGeoTransform;
    double      adfGeoTransform[6];
    char        *pszProjection;
    char        *pszGCPProjection;
    GDAL_GCP    *pasGCPList;
    int         nGCPCount;

    static long         USGSMnemonicToCode( const char* );
    void                ReadCoordinates( const char*, double*, double* );
    void                ToGeoref( double *, double * );
    char**              GetSwatAttrs( int32 hSW, char **papszMetadata );
    char**              GetGridAttrs( int32 hGD, char **papszMetadata );
    void                CaptureNRLGeoTransform(void);
    void                CaptureCoastwatchGCTPInfo(void);
    int                 ProcessSwathGeolocation( int32 hSW, 
                                                 char **papszDimList );

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

    int         bHaveScaleAndOffset;
    double      dfScale;
    double      dfOffset;

    CPLString   osUnitType;

  public:

                HDF4ImageRasterBand( HDF4ImageDataset *, int, GDALDataType );
    
    virtual CPLErr          IReadBlock( int, int, void * );
    virtual CPLErr          IWriteBlock( int, int, void * );
    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
    virtual double	    GetNoDataValue( int * );
    virtual CPLErr	    SetNoDataValue( double );
    virtual double 	    GetOffset( int *pbSuccess );
    virtual double          GetScale( int *pbSuccess );
    virtual const char     *GetUnitType();
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

    bHaveScaleAndOffset = FALSE;
    dfScale = 1.0;
    dfOffset = 0.0;

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
    int32               aiStart[MAX_NC_DIMS], aiEdges[MAX_NC_DIMS];
    CPLErr              eErr = CE_None;

    if( poGDS->eAccess == GA_Update )
    {
        memset( pImage, 0,
                nBlockXSize * nBlockYSize * GDALGetDataTypeSize(eDataType) / 8 );
        return CE_None;
    }

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
    switch ( poGDS->iDatasetType )
    {
      case HDF4_SDS:
      {
          int32   iSDS = SDselect( poGDS->hSD, poGDS->iDataset );
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
              aiStart[3] = 0/* range: 0--aiDimSizes[3]-1 */;
              aiEdges[3] = 1;
              aiStart[2] = 0/* range: 0--aiDimSizes[2]-1 */;
              aiEdges[2] = 1;
              aiStart[1] = nBlockYOff; aiEdges[1] = nBlockYSize;
              aiStart[0] = nBlockXOff; aiEdges[0] = nBlockXSize;
              break;
            case 3: // 3Dim: volume
              aiStart[poGDS->iBandDim] = nBand - 1;
              aiEdges[poGDS->iBandDim] = 1;
                    
              aiStart[poGDS->iYDim] = nBlockYOff;
              aiEdges[poGDS->iYDim] = nBlockYSize;
                    
              aiStart[poGDS->iXDim] = nBlockXOff;
              aiEdges[poGDS->iXDim] = nBlockXSize;
              break;
            case 2: // 2Dim: rows/cols
              aiStart[poGDS->iYDim] = nBlockYOff;
              aiEdges[poGDS->iYDim] = nBlockYSize;
                    
              aiStart[poGDS->iXDim] = nBlockXOff;
              aiEdges[poGDS->iXDim] = nBlockXSize;
              break;
          }
                
          // Read HDF SDS array
          if( SDreaddata( iSDS, aiStart, NULL, aiEdges, pImage ) < 0 )
          {
              CPLError( CE_Failure, CPLE_AppDefined, 
                        "SDreaddata() failed for block." );
              eErr = CE_Failure;
          }
                
          SDendaccess( iSDS );
      }
      break;

      case HDF4_GR:
      {
          int     nDataTypeSize =
              GDALGetDataTypeSize(poGDS->GetDataType(poGDS->iNumType)) / 8;
          GByte    *pbBuffer = (GByte *)
              CPLMalloc(nBlockXSize*nBlockYSize*poGDS->iRank*nBlockYSize);
          int     i, j;
            
          aiStart[poGDS->iYDim] = nBlockYOff;
          aiEdges[poGDS->iYDim] = nBlockYSize;
            
          aiStart[poGDS->iXDim] = nBlockXOff;
          aiEdges[poGDS->iXDim] = nBlockXSize;

          if( GRreadimage(poGDS->iGR, aiStart, NULL, aiEdges, pbBuffer) < 0 )
          {
              CPLError( CE_Failure, CPLE_AppDefined, 
                        "GRreaddata() failed for block." );
              eErr = CE_Failure;
          }
          else
          {
              for ( i = 0, j = (nBand - 1) * nDataTypeSize;
                    i < nBlockXSize * nDataTypeSize;
                    i += nDataTypeSize, j += poGDS->nBands * nDataTypeSize )
                  memcpy( (GByte *)pImage + i, pbBuffer + j, nDataTypeSize );
          }

          CPLFree( pbBuffer );
      }
      break;

      case HDF4_EOS:
      {
          switch ( poGDS->iSubdatasetType )
          {
            case EOS_GRID:
            {
                int32   hGD;

                hGD = GDattach( poGDS->hHDF4,
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
                                
                    aiStart[poGDS->iYDim] = nBlockYOff;
                    aiEdges[poGDS->iYDim] = nBlockYSize;
                                
                    aiStart[poGDS->iXDim] = nBlockXOff;
                    aiEdges[poGDS->iXDim] = nBlockXSize;
                    break;
                  case 3: // 3Dim: volume
                    aiStart[poGDS->iBandDim] = nBand - 1;
                    aiEdges[poGDS->iBandDim] = 1;
                                
                    aiStart[poGDS->iYDim] = nBlockYOff;
                    aiEdges[poGDS->iYDim] = nBlockYSize;
                                
                    aiStart[poGDS->iXDim] = nBlockXOff;
                    aiEdges[poGDS->iXDim] = nBlockXSize;
                    break;
                  case 2: // 2Dim: rows/cols
                    aiStart[poGDS->iYDim] = nBlockYOff;
                    aiEdges[poGDS->iYDim] = nBlockYSize;
                                
                    aiStart[poGDS->iXDim] = nBlockXOff;
                    aiEdges[poGDS->iXDim] = nBlockXSize;
                    break;
                }
                if( GDreadfield( hGD, poGDS->pszFieldName,
                                 aiStart, NULL, aiEdges, pImage ) < 0 )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "GDreadfield() failed for block." );
                    eErr = CE_Failure;
                }
                GDdetach( hGD );
            }
            break;
                    
            case EOS_SWATH:
            case EOS_SWATH_GEOL:
            {
                int32   hSW;

                hSW = SWattach( poGDS->hHDF4,
                                poGDS->pszSubdatasetName );
                switch ( poGDS->iRank )
                {
                  case 3: // 3Dim: volume
                    aiStart[poGDS->iBandDim] = nBand - 1;
                    aiEdges[poGDS->iBandDim] = 1;
                                
                    aiStart[poGDS->iYDim] = nBlockYOff;
                    aiEdges[poGDS->iYDim] = nBlockYSize;
                                
                    aiStart[poGDS->iXDim] = nBlockXOff;
                    aiEdges[poGDS->iXDim] = nBlockXSize;
                    break;
                  case 2: // 2Dim: rows/cols
                    aiStart[poGDS->iYDim] = nBlockYOff;
                    aiEdges[poGDS->iYDim] = nBlockYSize;
                                
                    aiStart[poGDS->iXDim] = nBlockXOff;
                    aiEdges[poGDS->iXDim] = nBlockXSize;
                    break;
                }
                if( SWreadfield( hSW, poGDS->pszFieldName,
                                 aiStart, NULL, aiEdges, pImage ) < 0 )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "GDreadfield() failed for block." );
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
    HDF4ImageDataset    *poGDS = (HDF4ImageDataset *)poDS;
    int32               aiStart[MAX_NC_DIMS], aiEdges[MAX_NC_DIMS];
    CPLErr              eErr = CE_None;

    CPLAssert( poGDS != NULL
               && nBlockXOff >= 0
               && nBlockYOff >= 0
               && pImage != NULL );

    switch ( poGDS->iRank )
    {
        case 3:
            {
                int32   iSDS = SDselect( poGDS->hSD, poGDS->iDataset );

                aiStart[poGDS->iBandDim] = nBand - 1;
                aiEdges[poGDS->iBandDim] = 1;
            
                aiStart[poGDS->iYDim] = nBlockYOff;
                aiEdges[poGDS->iYDim] = nBlockYSize;
            
                aiStart[poGDS->iXDim] = nBlockXOff;
                aiEdges[poGDS->iXDim] = nBlockXSize;

                if ( (SDwritedata( iSDS, aiStart, NULL,
                                   aiEdges, (VOIDP)pImage )) < 0 )
                    eErr = CE_Failure;

                SDendaccess( iSDS );
            }
            break;

        case 2:
            {
                int32 iSDS = SDselect( poGDS->hSD, nBand - 1 );
                aiStart[poGDS->iYDim] = nBlockYOff;
                aiEdges[poGDS->iYDim] = nBlockYSize;
            
                aiStart[poGDS->iXDim] = nBlockXOff;
                aiEdges[poGDS->iXDim] = nBlockXSize;

                if ( (SDwritedata( iSDS, aiStart, NULL,
                                   aiEdges, (VOIDP)pImage )) < 0 )
                    eErr = CE_Failure;

                SDendaccess( iSDS );
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
    HDF4ImageDataset    *poGDS = (HDF4ImageDataset *) poDS;

    return poGDS->poColorTable;
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp HDF4ImageRasterBand::GetColorInterpretation()
{
    HDF4ImageDataset    *poGDS = (HDF4ImageDataset *) poDS;

    if ( poGDS->iDatasetType == HDF4_SDS )
        return GCI_GrayIndex;
    else if ( poGDS->iDatasetType == HDF4_GR )
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
/*                            GetUnitType()                             */
/************************************************************************/

const char *HDF4ImageRasterBand::GetUnitType()

{
    if( osUnitType.size() > 0 )
        return osUnitType;
    else
        return GDALRasterBand::GetUnitType();
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double HDF4ImageRasterBand::GetOffset( int *pbSuccess )

{
    if( bHaveScaleAndOffset )
    {
        if( pbSuccess != NULL )
            *pbSuccess = TRUE;
        return dfOffset;
    }
    else
        return GDALRasterBand::GetOffset( pbSuccess );
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double HDF4ImageRasterBand::GetScale( int *pbSuccess )

{
    if( bHaveScaleAndOffset )
    {
        if( pbSuccess != NULL )
            *pbSuccess = TRUE;
        return dfScale;
    }
    else
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

HDF4ImageDataset::HDF4ImageDataset()
{
    pszFilename = NULL;
    hSD = 0;
    hGR = 0;
    iGR = 0;
    iBandDim = -1;
    iDatasetType = HDF4_UNKNOWN;
    pszSubdatasetName = NULL;
    pszFieldName = NULL;
    papszLocalMetadata = NULL;
    poColorTable = NULL;
    pszProjection = CPLStrdup( "" );
    pszGCPProjection = CPLStrdup( "" );
    bHasGeoTransform = FALSE;
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
    if ( hSD > 0 )
        SDend( hSD );
    if ( iGR > 0 )
        GRendaccess( iGR );
    if ( hGR > 0 )
        GRend( hGR );
    if ( pszSubdatasetName )
        CPLFree( pszSubdatasetName );
    if ( pszFieldName )
        CPLFree( pszFieldName );
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
    if ( hHDF4 )
    {
        switch ( iDatasetType )
        {
            case HDF4_EOS:
                switch ( iSubdatasetType )
                {
                    case EOS_SWATH:
                    case EOS_SWATH_GEOL:
                        SWclose( hHDF4 );
                        break;
                    case EOS_GRID:
                        GDclose( hHDF4 );
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

    if ( !bHasGeoTransform )
        return CE_Failure;

    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr HDF4ImageDataset::SetGeoTransform( double * padfTransform )
{
    bHasGeoTransform = TRUE;
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
    if( GetMetadata() )
    {
        char    **papszMeta = GetMetadata();

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
/*                        USGSMnemonicToCode()                          */
/************************************************************************/

long HDF4ImageDataset::USGSMnemonicToCode( const char* pszMnemonic )
{
    if ( EQUAL(pszMnemonic, "UTM") )
        return 1L;
    else if ( EQUAL(pszMnemonic, "LAMCC") )
        return 4L;
    else if ( EQUAL(pszMnemonic, "PS") )
        return 6L;
    else if ( EQUAL(pszMnemonic, "PC") )
        return 7L;
    else if ( EQUAL(pszMnemonic, "TM") )
        return 9L;
    else if ( EQUAL(pszMnemonic, "EQRECT") )
        return 17L;
    else if ( EQUAL(pszMnemonic, "OM") )
        return 20L;
    else if ( EQUAL(pszMnemonic, "SOM") )
        return 22L;
    else
        return 1L;  // UTM by default
}

/************************************************************************/
/*                              ToGeoref()                              */
/************************************************************************/

void HDF4ImageDataset::ToGeoref( double *pdfGeoX, double *pdfGeoY )
{
    OGRCoordinateTransformation *poTransform = NULL;
    OGRSpatialReference *poLatLong = NULL;
    poLatLong = oSRS.CloneGeogCS();
    poTransform = OGRCreateCoordinateTransformation( poLatLong, &oSRS );
    
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
/*                       CaptureNRLGeoTransform()                       */
/*                                                                      */
/*      Capture geotransform and coordinate system from NRL (Navel      */
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
    double adfXY[8];
    static char *apszItems[] = {
        "mapUpperLeft", "mapUpperRight", "mapLowerLeft", "mapLowerRight" };
    int iCorner;

    for( iCorner = 0; iCorner < 4; iCorner++ )
    {
        const char *pszCornerLoc = 
            CSLFetchNameValue( papszGlobalMetadata, apszItems[iCorner] );

        if( pszCornerLoc == NULL )
            return;

        char **papszTokens = CSLTokenizeStringComplex( pszCornerLoc, ",",
                                                       FALSE, FALSE );
        if( CSLCount( papszTokens ) != 2 )
            return;

        adfXY[iCorner*2+0] = atof(papszTokens[1]);
        adfXY[iCorner*2+1] = atof(papszTokens[0]);

        CSLDestroy( papszTokens );
    }

    if( adfXY[0*2+0] == adfXY[2*2+0] && adfXY[0*2+1] == adfXY[1*2+1] )
    {
        bHasGeoTransform = TRUE;
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
    if( CSLFetchNameValue( papszGlobalMetadata, "gctp_sys" ) == NULL 
        || CSLFetchNameValue( papszGlobalMetadata, "gctp_zone" ) == NULL 
        || CSLFetchNameValue( papszGlobalMetadata, "gctp_parm" ) == NULL 
        || CSLFetchNameValue( papszGlobalMetadata, "gctp_datum" ) == NULL 
        || CSLFetchNameValue( papszGlobalMetadata, "et_affine" ) == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Grab USGS/GCTP Parameters.                                      */
/* -------------------------------------------------------------------- */
    int nSys, nZone, nDatum, iParm;
    double adfParms[15];
    char **papszTokens;

    nSys = atoi( CSLFetchNameValue( papszGlobalMetadata, "gctp_sys" ) );
    nZone = atoi( CSLFetchNameValue( papszGlobalMetadata, "gctp_zone" ) );
    nDatum = atoi( CSLFetchNameValue( papszGlobalMetadata, "gctp_datum" ) );

    papszTokens = CSLTokenizeStringComplex( 
        CSLFetchNameValue( papszGlobalMetadata, "gctp_parm" ), ",", 
        FALSE, FALSE );
    if( CSLCount(papszTokens) < 15 )
        return;

    for( iParm = 0; iParm < 15; iParm++ )
        adfParms[iParm] = atof(papszTokens[iParm]);
    CSLDestroy( papszTokens );

/* -------------------------------------------------------------------- */
/*      Convert into an SRS.                                            */
/* -------------------------------------------------------------------- */

    if( oSRS.importFromUSGS( nSys, nZone, adfParms, nDatum ) != OGRERR_NONE )
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
        return;

    // We don't seem to have proper ef_affine docs so I don't 
    // know which of these two coefficients goes where. 
    if( atof(papszTokens[0]) != 0.0 || atof(papszTokens[3]) != 0.0 )
        return;
        
    bHasGeoTransform = TRUE;
    adfGeoTransform[0] = atof(papszTokens[4]);
    adfGeoTransform[1] = atof(papszTokens[2]);
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = atof(papszTokens[5]);
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = atof(papszTokens[1]);

    // Middle of pixel adjustment. 
    adfGeoTransform[0] -= adfGeoTransform[1] * 0.5;
    adfGeoTransform[3] -= adfGeoTransform[5] * 0.5;
}

/************************************************************************/
/*                            GetSwatAttrs()                            */
/************************************************************************/

char**  HDF4ImageDataset::GetSwatAttrs( int32 hSW, char **papszMetadata )
{
    int32       nStrBufSize = 0;

    if ( SWinqattrs( hSW, NULL, &nStrBufSize ) > 0 && nStrBufSize > 0 )
    {
        char    *pszAttrList;
        char    **papszAttributes;
        int     i, nAttrs;

        pszAttrList = (char *)CPLMalloc( nStrBufSize + 1 );
        SWinqattrs( hSW, pszAttrList, &nStrBufSize );

#if DEBUG
        CPLDebug( "HDF4Image", "List of attributes in swath %s: %s",
                  pszFieldName, pszAttrList );
#endif

        papszAttributes = CSLTokenizeString2( pszAttrList, ",",
                                              CSLT_HONOURSTRINGS );
        nAttrs = CSLCount( papszAttributes );
        for ( i = 0; i < nAttrs; i++ )
        {
            int32       iNumType, nValues;
            void	*pData = NULL;
            char	*pszTemp = NULL;

            SWattrinfo( hSW, papszAttributes[i], &iNumType, &nValues );

            if ( iNumType == DFNT_CHAR8 || iNumType == DFNT_UCHAR8 )
                pData = CPLMalloc( (nValues + 1) * GetDataTypeSize(iNumType) );
            else
                pData = CPLMalloc( nValues * GetDataTypeSize(iNumType) );

            SWreadattr( hSW, papszAttributes[i], pData );

            if ( iNumType == DFNT_CHAR8 || iNumType == DFNT_UCHAR8 )
            {
                ((char *)pData)[nValues] = '\0';
                papszMetadata = CSLAddNameValue( papszMetadata,
                                                 papszAttributes[i], 
                                                 (const char *) pData );
            }
            else
            {
                pszTemp = SPrintArray( GetDataType(iNumType), pData,
                                       nValues, ", " );
                papszMetadata = CSLAddNameValue( papszMetadata,
                                                 papszAttributes[i], pszTemp );
                if ( pszTemp )
                    CPLFree( pszTemp );
            }
            
            if ( pData )
                CPLFree( pData );

        }

        CSLDestroy( papszAttributes );
        CPLFree( pszAttrList );
    }

    return papszMetadata;
}

/************************************************************************/
/*                            GetGridAttrs()                            */
/************************************************************************/

char**  HDF4ImageDataset::GetGridAttrs( int32 hGD, char **papszMetadata )
{
    int32       nStrBufSize = 0;

    if ( GDinqattrs( hGD, NULL, &nStrBufSize ) > 0 && nStrBufSize > 0 )
    {
        char    *pszAttrList;
        char    **papszAttributes;
        int     i, nAttrs;

        pszAttrList = (char *)CPLMalloc( nStrBufSize + 1 );
        GDinqattrs( hGD, pszAttrList, &nStrBufSize );
#if DEBUG
        CPLDebug( "HDF4Image", "List of attributes in grid %s: %s",
                  pszFieldName, pszAttrList );
#endif
        papszAttributes = CSLTokenizeString2( pszAttrList, ",",
                                              CSLT_HONOURSTRINGS );
        nAttrs = CSLCount( papszAttributes );
        for ( i = 0; i < nAttrs; i++ )
        {
            int32       iNumType, nValues;
            void	*pData = NULL;
            char	*pszTemp = NULL;

            GDattrinfo( hGD, papszAttributes[i], &iNumType, &nValues );

            if ( iNumType == DFNT_CHAR8 || iNumType == DFNT_UCHAR8 )
                pData = CPLMalloc( (nValues + 1) * GetDataTypeSize(iNumType) );
            else
                pData = CPLMalloc( nValues * GetDataTypeSize(iNumType) );

            GDreadattr( hGD, papszAttributes[i], pData );

            if ( iNumType == DFNT_CHAR8 || iNumType == DFNT_UCHAR8 )
            {
                ((char *)pData)[nValues] = '\0';
                papszMetadata = CSLAddNameValue( papszMetadata,
                                                 papszAttributes[i], 
                                                 (const char *) pData );
            }
            else
            {
                pszTemp = SPrintArray( GetDataType(iNumType), pData,
                                       nValues, ", " );
                papszMetadata = CSLAddNameValue( papszMetadata,
                                                 papszAttributes[i], pszTemp );
                if ( pszTemp )
                    CPLFree( pszTemp );
            }
            
            if ( pData )
                CPLFree( pData );

        }

        CSLDestroy( papszAttributes );
        CPLFree( pszAttrList );
    }

    return papszMetadata;
}

/************************************************************************/
/*                      ProcessSwathGeolocation()                       */
/*                                                                      */
/*      Handle the swath geolocation data for a swath.  Attach          */
/*      geolocation metadata corresponding to it (if there is no        */
/*      lattice), and also attach it as GCPs.  This is only invoked     */
/*      for EOS_SWATH, not EOS_SWATH_GEOL datasets.                     */
/************************************************************************/

int HDF4ImageDataset::ProcessSwathGeolocation( 
    int32 hSW, char **papszDimList )

{
    char    szXGeo[8192] = "";
    char    szYGeo[8192] = "";
    char    szPixel[8192]= "";
    char    szLine[8192] = "";
    char    *pszGeoList = NULL;
    char    szGeoDimList[8192] = "";
    int32   iWrkNumType;
    int32   nDataFields, nDimMaps;
    void    *pLat = NULL, *pLong = NULL;
    void    *pLatticeX = NULL, *pLatticeY = NULL;
    int32   iLatticeType, iLatticeDataSize, iRank;
    int32   nLatCount = 0, nLongCount = 0;
    int32   nXPoints, nYPoints;
    int32   nStrBufSize;
    int32   aiDimSizes[MAX_VAR_DIMS];
    int     i, j, iDataSize, iPixelDim, iLineDim, iLongDim, iLatDim;
    int32   *paiRank = NULL, *paiNumType = NULL,
        *paiOffset = NULL, *paiIncrement = NULL;
    char    **papszGeolocations = NULL, **papszDimMap = NULL;
    char    *pszDimMaps = NULL;

/* -------------------------------------------------------------------- */
/*  Determine a product name.                                           */
/* -------------------------------------------------------------------- */
    const char *pszProduct =
        CSLFetchNameValue( papszLocalMetadata, "SHORTNAME" );

    HDF4EOSProduct eProduct = PROD_UNKNOWN;
    if ( pszProduct )
    {
        if ( EQUALN(pszProduct, "ASTL1A", 6) )
            eProduct = PROD_ASTER_L1A;
        else if ( EQUALN(pszProduct, "ASTL1B", 6) )
            eProduct = PROD_ASTER_L1B;
        else if ( EQUALN(pszProduct, "AST_04", 6)
                  || EQUALN(pszProduct, "AST_05", 6)
                  || EQUALN(pszProduct, "AST_06", 6)
                  || EQUALN(pszProduct, "AST_07", 6)
                  || EQUALN(pszProduct, "AST_08", 6)
                  || EQUALN(pszProduct, "AST_09", 6)
                  || EQUALN(pszProduct, "AST13", 5)
                  || EQUALN(pszProduct, "AST3", 4) )
            eProduct = PROD_ASTER_L2;
        else if ( EQUALN(pszProduct, "AST14", 5) )
            eProduct = PROD_ASTER_L3;
        else if ( EQUALN(pszProduct, "MOD02", 5)
                  || EQUALN(pszProduct, "MYD02", 5) )
            eProduct = PROD_MODIS_L1B;
    }
    
/* -------------------------------------------------------------------- */
/*      xx                                                              */
/* -------------------------------------------------------------------- */
    nDataFields = SWnentries( hSW, HDFE_NENTGFLD, &nStrBufSize );
    pszGeoList = (char *)CPLMalloc( nStrBufSize + 1 );
    paiRank = (int32 *)CPLMalloc( nDataFields * sizeof(int32) );
    paiNumType = (int32 *)CPLMalloc( nDataFields * sizeof(int32) );
    if ( nDataFields !=
         SWinqgeofields(hSW, pszGeoList, paiRank, paiNumType) )
    {
        CPLDebug( "HDF4Image",
                  "Can't get the list of geolocation fields in swath %s",
                  pszSubdatasetName );
    }

#if DEBUG
    else
    {
        char    *pszTmp;
        CPLDebug( "HDF4Image",
                  "Number of geolocation fields in swath %s: %d",
                  pszSubdatasetName, nDataFields );
        CPLDebug( "HDF4Image",
                  "List of geolocation fields in swath %s: %s",
                  pszSubdatasetName, pszGeoList );
        pszTmp = SPrintArray( GDT_UInt32, paiRank,
                              nDataFields, "," );
        CPLDebug( "HDF4Image",
                  "Geolocation fields ranks: %s", pszTmp );
        CPLFree( pszTmp );
    }
#endif

    papszGeolocations = CSLTokenizeString2( pszGeoList, ",",
                                            CSLT_HONOURSTRINGS );
    // Read geolocation data
    nDimMaps = SWnentries( hSW, HDFE_NENTMAP, &nStrBufSize );
    pszDimMaps = (char *)CPLMalloc( nStrBufSize + 1 );
    paiOffset = (int32 *)CPLMalloc( nDimMaps * sizeof(int32) );
    memset( paiOffset, 0, nDimMaps * sizeof(int32) );
    paiIncrement = (int32 *)CPLMalloc( nDimMaps * sizeof(int32) );
    memset( paiIncrement, 0, nDimMaps * sizeof(int32) );
    if ( nDimMaps !=
         SWinqmaps(hSW, pszDimMaps, paiOffset, paiIncrement) )
    {
        CPLDebug( "HDF4Image",
                  "Can't get the list of geolocation maps in swath %s",
                  pszSubdatasetName );
    }

#if DEBUG
    else
    {
        char    *pszTmp;
            
        CPLDebug( "HDF4Image",
                  "List of geolocation maps in swath %s: %s",
                  pszSubdatasetName, pszDimMaps );
        pszTmp = SPrintArray( GDT_Int32, paiOffset,
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

    papszDimMap = CSLTokenizeString2( pszDimMaps, ",",
                                      CSLT_HONOURSTRINGS );
    for ( i = 0; i < CSLCount(papszDimMap); i++ )
    {
        if ( strstr(papszDimMap[i], papszDimList[iXDim]) )
        {
            strncpy( szPixel, papszDimList[iXDim], 8192 );
            strncpy( szXGeo, papszDimMap[i], 8192 );
            char *pszTemp = strchr( szXGeo, '/' );
            if ( pszTemp )
                *pszTemp = '\0';
        }
        else if ( strstr(papszDimMap[i], papszDimList[iYDim]) )
        {
            strncpy( szLine, papszDimList[iYDim], 8192 );
            strncpy( szYGeo, papszDimMap[i], 8192 );
            char *pszTemp = strchr( szYGeo, '/' );
            if ( pszTemp )
                *pszTemp = '\0';
        }
    }

    for ( i = 0; i < CSLCount(papszGeolocations); i++ )
    {
        char    **papszGeoDimList = NULL;

        SWfieldinfo( hSW, papszGeolocations[i], &iRank,
                     aiDimSizes, &iWrkNumType, szGeoDimList );
        papszGeoDimList = CSLTokenizeString2( szGeoDimList,
                                              ",", CSLT_HONOURSTRINGS );

#if DEBUG
        CPLDebug( "HDF4Image",
                  "List of dimensions in geolocation field %s: %s",
                  papszGeolocations[i], szGeoDimList );
#endif

        if (szXGeo[0] == 0 || szYGeo[0] == 0)
            return FALSE;

        nXPoints = aiDimSizes[CSLFindString( papszGeoDimList, szXGeo )];
        nYPoints = aiDimSizes[CSLFindString( papszGeoDimList, szYGeo )];

        if ( EQUAL(szPixel, papszDimList[iXDim]) )
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
        if ( strstr( papszGeolocations[i], "Latitude" ) )
        {
            iLatDim = i;
            nLatCount = nXPoints * nYPoints;
            pLat = CPLMalloc( nLatCount * iDataSize );
            if (SWreadfield( hSW, papszGeolocations[i], NULL,
                             NULL, NULL, (VOIDP)pLat ) < 0)
            {
                CPLDebug( "HDF4Image",
                          "Can't read geolocation field %s",
                          papszGeolocations[i]);
                CPLFree( pLat );
                pLat = NULL;
            }
        }
        else if ( strstr( papszGeolocations[i], "Longitude" ) )
        {
            iLongDim = i;
            nLongCount = nXPoints * nYPoints;
            pLong = CPLMalloc( nLongCount * iDataSize );
            if (SWreadfield( hSW, papszGeolocations[i], NULL,
                             NULL, NULL, (VOIDP)pLong ) < 0)
            {
                CPLDebug( "HDF4Image",
                          "Can't read geolocation field %s",
                          papszGeolocations[i]);
                CPLFree( pLong );
                pLong = NULL;
            }
        }

        CSLDestroy( papszGeoDimList );

    }

/* -------------------------------------------------------------------- */
/*      Do we have a lattice table?                                     */
/* -------------------------------------------------------------------- */
    if (SWfieldinfo(hSW, "LatticePoint", &iRank, aiDimSizes,
                    &iLatticeType, szGeoDimList) == 0
        && iRank == 3
        && nXPoints == aiDimSizes[1]
        && nYPoints == aiDimSizes[0]
        && aiDimSizes[2] == 2 )
    {
        int32   iStart[MAX_NC_DIMS], iEdges[MAX_NC_DIMS];

        iLatticeDataSize =
            GetDataTypeSize( iLatticeType );

        iStart[1] = 0;
        iEdges[1] = nXPoints;

        iStart[0] = 0;
        iEdges[0] = nYPoints;

        iStart[2] = 0;
        iEdges[2] = 1;
                        
        pLatticeX = CPLMalloc( nLatCount * iLatticeDataSize );
        if (SWreadfield( hSW, "LatticePoint", iStart, NULL,
                         iEdges, (VOIDP)pLatticeX ) < 0)
        {
            CPLDebug( "HDF4Image", "Can't read lattice field" );
            CPLFree( pLatticeX );
            pLatticeX = NULL;
        }

        iStart[2] = 1;
        iEdges[2] = 1;

        pLatticeY = CPLMalloc( nLatCount * iLatticeDataSize );
        if (SWreadfield( hSW, "LatticePoint", iStart, NULL,
                         iEdges, (VOIDP)pLatticeY ) < 0)
        {
            CPLDebug( "HDF4Image", "Can't read lattice field" );
            CPLFree( pLatticeY );
            pLatticeY = NULL;
        }

    }

/* -------------------------------------------------------------------- */
/*      Determine whether to use no, partial or full GCPs.              */
/* -------------------------------------------------------------------- */
    const char *pszGEOL_AS_GCPS = CPLGetConfigOption( "GEOL_AS_GCPS", 
                                                      "PARTIAL" );
    int iGCPStepX, iGCPStepY;

    if( EQUAL(pszGEOL_AS_GCPS,"NONE") )
    {
        iGCPStepX = iGCPStepY = 0;
    }
    else if( EQUAL(pszGEOL_AS_GCPS,"FULL") )
    {
        iGCPStepX = iGCPStepY = 1;
    }
    else
    {
        // aim for 10x10 grid or so.
        iGCPStepX = MAX(1,((nXPoints-1) / 11));
        iGCPStepY = MAX(1,((nYPoints-1) / 11));
    }

/* -------------------------------------------------------------------- */
/*  Fetch projection information for various datasets.                  */
/* -------------------------------------------------------------------- */
    if ( nLatCount && nLongCount && nLatCount == nLongCount
         && pLat && pLong )
    {
        if ( pszGCPProjection )
            CPLFree( pszGCPProjection );

        // ASTER Level 1A
        if ( eProduct == PROD_ASTER_L1A )
        {
            pszGCPProjection = CPLStrdup( "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9108\"]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",\"4326\"]]" );
        }

        // ASTER Level 1B, Level 2
        else if ( eProduct == PROD_ASTER_L1B
                  || eProduct == PROD_ASTER_L2 )
        {
            // Constuct the metadata keys.
            // A band number is taken from the field name.
            char *pszBand = strpbrk( pszFieldName,
                                     "0123456789" );

            if ( !pszBand )
                pszBand = "";

            char *pszProjLine =
                CPLStrdup(CPLSPrintf("MPMETHOD%s", pszBand));
            char *pszParmsLine = 
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
            const char *pszParms =
                CSLFetchNameValue( papszLocalMetadata,
                                   pszParmsLine );
            const char *pszZone =
                CSLFetchNameValue( papszLocalMetadata,
                                   pszZoneLine );
            const char* pszEllipsoid =
                CSLFetchNameValue( papszLocalMetadata,
                                   pszEllipsoidLine );

#if DEBUG
            CPLDebug( "HDF4Image",
                      "Projection %s=%s, parameters %s=%s, "
                      "zone %s=%s",
                      pszProjLine, pszProj, pszParmsLine,
                      pszParms, pszZoneLine, pszZone );
            CPLDebug( "HDF4Image", "Ellipsoid %s=%s",
                      pszEllipsoidLine, pszEllipsoid );
#endif

            // Transform all mnemonical codes in the values.
            int i, nParms;
            // Projection is UTM by default
            long iProjSys = (pszProj) ?
                USGSMnemonicToCode(pszProj) : 1L;
            long iZone =
                (pszZone && iProjSys == 1L) ? atoi(pszZone): 0L;
            char **papszEllipsoid = (pszEllipsoid) ?
                CSLTokenizeString2( pszEllipsoid, ",",
                                    CSLT_HONOURSTRINGS ) : NULL;
                            
            long iEllipsoid = 8L; // WGS84 by default
            if ( papszEllipsoid
                 && CSLCount(papszEllipsoid) > 0 )
            {
                if (EQUAL( papszEllipsoid[0], "WGS84"))
                    iEllipsoid = 8L;
            }
                            
            double adfProjParms[15];
            char **papszParms = (pszParms) ?
                CSLTokenizeString2( pszParms, ",",
                                    CSLT_HONOURSTRINGS ) : NULL;
            nParms = CSLCount(papszParms);
            if (nParms >= 15)
                nParms = 15;
            for (i = 0; i < nParms; i++)
                adfProjParms[i] = atof(papszParms[i]);
            for (; i < 15; i++)
                adfProjParms[i] = 0.0;

            // Create projection definition
            oSRS.importFromUSGS( iProjSys, iZone,
                                 adfProjParms, iEllipsoid );
            oSRS.SetLinearUnits( SRS_UL_METER, 1.0 );
            oSRS.exportToWkt( &pszGCPProjection );

            CSLDestroy( papszParms );
            CPLFree( pszZoneLine );
            CPLFree( pszParmsLine );
            CPLFree( pszProjLine );
        }

        // ASTER Level 3 (DEM)
        else if ( eProduct == PROD_ASTER_L3 )
        {
            double  dfCenterX, dfCenterY;
            int     iZone;
                            
            ReadCoordinates( CSLFetchNameValue( 
                                 papszGlobalMetadata, "SCENECENTER" ),
                             &dfCenterY, &dfCenterX );
                            
            // Calculate UTM zone from scene center coordinates
            iZone = 30 + (int) ((dfCenterX + 6.0) / 6.0);
           
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
        else if ( eProduct == PROD_MODIS_L1B )
        {
            pszGCPProjection = CPLStrdup( "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9108\"]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",\"4326\"]]" );
        }
    }

/* -------------------------------------------------------------------- */
/*  Fill the GCPs list.                                                 */
/* -------------------------------------------------------------------- */
    if( iGCPStepX > 0 )
    {
        nGCPCount = (((nXPoints-1) / iGCPStepX) + 1)
            * (((nYPoints-1) / iGCPStepY) + 1);

        pasGCPList = (GDAL_GCP *) CPLCalloc( nGCPCount, sizeof( GDAL_GCP ) );
        GDALInitGCPs( nGCPCount, pasGCPList );

        int iGCP = 0; 
        for ( i = 0; i < nYPoints; i += iGCPStepY )
        {
            for ( j = 0; j < nXPoints; j += iGCPStepX )
            {
                int iGeoOff =  i * nXPoints + j;
 
                pasGCPList[iGCP].dfGCPX =
                    AnyTypeToDouble(iWrkNumType,
                                    (void *)((char*)pLong+ iGeoOff*iDataSize));
                pasGCPList[iGCP].dfGCPY =
                    AnyTypeToDouble(iWrkNumType,
                                    (void *)((char*)pLat + iGeoOff*iDataSize));
                                
                // GCPs in Level 1A/1B dataset are in geocentric
                // coordinates. Convert them in geodetic (we
                // will convert latitudes only, longitudes
                // do not need to be converted, because
                // they are the same).
                // This calculation valid for WGS84 datum only.
                if ( eProduct == PROD_ASTER_L1A
                     || eProduct == PROD_ASTER_L1B )
                {
                    pasGCPList[iGCP].dfGCPY = 
                        atan(tan(pasGCPList[iGCP].dfGCPY
                                 *PI/180)/0.99330562)*180/PI;
                }

                ToGeoref(&pasGCPList[iGCP].dfGCPX,
                         &pasGCPList[iGCP].dfGCPY);

                pasGCPList[iGCP].dfGCPZ = 0.0;

                if ( pLatticeX && pLatticeY )
                {
                    pasGCPList[iGCP].dfGCPPixel =
                        AnyTypeToDouble(iLatticeType,
                                        (void *)((char *)pLatticeX
                                                 + iGeoOff*iLatticeDataSize))+0.5;
                    pasGCPList[iGCP].dfGCPLine =
                        AnyTypeToDouble(iLatticeType,
                                        (void *)((char *)pLatticeY
                                                 + iGeoOff*iLatticeDataSize))+0.5;
                }
                else
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
    if( pLatticeX == NULL )
    {
        CPLString  osWrk;

        SetMetadataItem( "SRS", pszGCPProjection, "GEOLOCATION" );
        
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

        osWrk.Printf( "%d", paiOffset[iPixelDim] );
        SetMetadataItem( "PIXEL_OFFSET", osWrk, "GEOLOCATION" );
        osWrk.Printf( "%d", paiIncrement[iPixelDim] );
        SetMetadataItem( "PIXEL_STEP", osWrk, "GEOLOCATION" );

        osWrk.Printf( "%d", paiOffset[iLineDim] );
        SetMetadataItem( "LINE_OFFSET", osWrk, "GEOLOCATION" );
        osWrk.Printf( "%d", paiIncrement[iLineDim] );
        SetMetadataItem( "LINE_STEP", osWrk, "GEOLOCATION" );
    }
                    
/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( pLatticeX );
    CPLFree( pLatticeY );
    CPLFree( pLat );
    CPLFree( pLong );

    CPLFree( paiOffset );
    CPLFree( paiIncrement );
    CPLFree( paiNumType );
    CPLFree( paiRank );
    
    CSLDestroy( papszGeolocations );
    CPLFree( pszGeoList );

    if( iGCPStepX == 0 )
    {
        CPLFree( pszGCPProjection );
        pszGCPProjection = NULL;
    }
    
    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *HDF4ImageDataset::Open( GDALOpenInfo * poOpenInfo )
{
    int     i;
    
    if( !EQUALN( poOpenInfo->pszFilename, "HDF4_SDS:", 9 ) &&
        !EQUALN( poOpenInfo->pszFilename, "HDF4_GR:", 8 ) &&
        !EQUALN( poOpenInfo->pszFilename, "HDF4_GD:", 8 ) &&
        !EQUALN( poOpenInfo->pszFilename, "HDF4_EOS:", 9 ) )
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
                                              ":", CSLT_HONOURSTRINGS | CSLT_PRESERVEESCAPES);
    if ( CSLCount( papszSubdatasetName ) != 4
         && CSLCount( papszSubdatasetName ) != 5 )
    {
        CSLDestroy( papszSubdatasetName );
        return NULL;
    }

    poDS->pszFilename = CPLStrdup( papszSubdatasetName[2] );

    if( EQUAL( papszSubdatasetName[0], "HDF4_SDS" ) )
        poDS->iDatasetType = HDF4_SDS;
    else if ( EQUAL( papszSubdatasetName[0], "HDF4_GR" ) )
        poDS->iDatasetType = HDF4_GR;
    else if ( EQUAL( papszSubdatasetName[0], "HDF4_EOS" ) )
        poDS->iDatasetType = HDF4_EOS;
    else
        poDS->iDatasetType = HDF4_UNKNOWN;
    
    if( EQUAL( papszSubdatasetName[1], "GDAL_HDF4" ) )
        poDS->iSubdatasetType = GDAL_HDF4;
    else if( EQUAL( papszSubdatasetName[1], "EOS_GRID" ) )
        poDS->iSubdatasetType = EOS_GRID;
    else if( EQUAL( papszSubdatasetName[1], "EOS_SWATH" ) )
        poDS->iSubdatasetType = EOS_SWATH;
    else if( EQUAL( papszSubdatasetName[1], "EOS_SWATH_GEOL" ) )
        poDS->iSubdatasetType = EOS_SWATH_GEOL;
    else if( EQUAL( papszSubdatasetName[1], "SEAWIFS_L3" ) )
        poDS->iSubdatasetType= SEAWIFS_L3;
    else if( EQUAL( papszSubdatasetName[1], "HYPERION_L1" ) )
        poDS->iSubdatasetType= HYPERION_L1;
    else
        poDS->iSubdatasetType = UNKNOWN;

    // Is our file still here?
    if ( !Hishdf( poDS->pszFilename ) )
    {
        CSLDestroy( papszSubdatasetName );
        CPLFree( poDS->pszFilename );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    int32       iAttribute, nValues, iAttrNumType;
    char        szAttrName[MAX_NC_NAME];
    double      dfNoData = 0.0;
    int         bNoDataSet = FALSE;
    
/* -------------------------------------------------------------------- */
/*      Select SDS or GR to read from.                                  */
/* -------------------------------------------------------------------- */
    if ( poDS->iDatasetType == HDF4_EOS )
    {
        poDS->pszSubdatasetName = CPLStrdup( papszSubdatasetName[3] );
        poDS->pszFieldName = CPLStrdup( papszSubdatasetName[4] );
    }
    else
        poDS->iDataset = atoi( papszSubdatasetName[3] );
    CSLDestroy( papszSubdatasetName );

    switch ( poDS->iDatasetType )
    {
      case HDF4_EOS:
      {
          void    *pNoDataValue = NULL;

          switch ( poDS->iSubdatasetType )
          {

/* -------------------------------------------------------------------- */
/*  HDF-EOS Swath.                                                      */
/* -------------------------------------------------------------------- */
            case EOS_SWATH:
            case EOS_SWATH_GEOL:
            {
                int32   hHDF4, hSW, nDimensions, nStrBufSize;
                char    **papszDimList = NULL, **papszDimMap = NULL;
                char    *pszDimList = NULL, *pszDimMaps = NULL;
                int     nDimCount;
                    
                if( poOpenInfo->eAccess == GA_ReadOnly )
                    poDS->hHDF4 = SWopen( poDS->pszFilename, DFACC_READ );
                else
                    poDS->hHDF4 = SWopen( poDS->pszFilename, DFACC_WRITE );
                    
                if( poDS->hHDF4 <= 0 )
                {
                    CPLDebug( "HDF4Image", "Can't open HDF4 file %s",
                              poDS->pszFilename );
                    CPLFree( poDS->pszFilename );
                    CPLFree( poDS->pszSubdatasetName );
                    CPLFree( poDS->pszFieldName );
                    return( NULL );
                }

                hSW = SWattach( poDS->hHDF4, poDS->pszSubdatasetName );
                if( hSW < 0 )
                {
                    CPLDebug( "HDF4Image", "Can't attach to subdataset %s",
                              poDS->pszSubdatasetName );
                    CPLFree( poDS->pszFilename );
                    CPLFree( poDS->pszSubdatasetName );
                    CPLFree( poDS->pszFieldName );
                    return( NULL );
                }

                nDimensions = SWnentries( hSW, HDFE_NENTDIM, &nStrBufSize );
                pszDimList = (char *)CPLMalloc( nStrBufSize + 1 );
                SWfieldinfo( hSW, poDS->pszFieldName, &poDS->iRank,
                             poDS->aiDimSizes, &poDS->iNumType, pszDimList );
#if DEBUG
                CPLDebug( "HDF4Image",
                          "List of dimensions in swath %s: %s",
                          poDS->pszFieldName, pszDimList );
#endif

                papszDimList = CSLTokenizeString2( pszDimList, ",",
                                                   CSLT_HONOURSTRINGS );
                nDimCount = CSLCount( papszDimList );

                poDS->papszLocalMetadata = 
                    poDS->GetSwatAttrs( hSW, poDS->papszLocalMetadata );

                // Search for the "Bands" name or take the first dimension
                // as a number of bands
                if ( poDS->iRank == 2 )
                    poDS->nBands = 1;
                else
                {
                    poDS->nBands = poDS->aiDimSizes[0];
                    /*for ( i = 0; i < nDimCount; i++ )
                      {
                      if ( EQUALN( papszDimList[i], "Band", 4 ) )
                      {
                      poDS->nBands = poDS->aiDimSizes[i];
                      break;
                      }
                      }*/
                }

                // Search for the "XDim" and "YDim" names or take the last
                // two dimensions as X and Y sizes
                /* FIXME: this heuristic does not work in all cases. Should be
                 * thought out very carefully.
                 *
                  for ( i = 0; i < nDimCount; i++ )
                  {
                  if ( EQUALN( papszDimList[i], "X", 1 ) )
                  poDS->iXDim = i;
                  else if ( EQUALN( papszDimList[i], "Y", 1 ) )
                  poDS->iYDim = i;
                  }*/
                poDS->iXDim = nDimCount - 1;
                poDS->iYDim = nDimCount - 2;
                if( nDimCount >= 3 )
                    poDS->iBandDim = nDimCount - 3;

#if DEBUG
                CPLDebug( "HDF4Image",
                          "X dimension is %d, Y dimension is %d",
                          poDS->iXDim, poDS->iYDim );
#endif

/* -------------------------------------------------------------------- */
/*  Fetch NODATA value.                                                 */
/* -------------------------------------------------------------------- */
                pNoDataValue =
                    CPLMalloc( poDS->GetDataTypeSize(poDS->iNumType) );
                if ( SWgetfillvalue( hSW, poDS->pszFieldName,
                                     pNoDataValue ) != -1 )
                {
                    dfNoData = poDS->AnyTypeToDouble( poDS->iNumType,
                                                      pNoDataValue );
                    bNoDataSet = TRUE;
                }
                CPLFree( pNoDataValue );

/* -------------------------------------------------------------------- */
/*  Fetch metadata.                                                     */
/* -------------------------------------------------------------------- */
                if( poDS->iSubdatasetType == EOS_SWATH )
                {
                    EHidinfo( poDS->hHDF4, &hHDF4, &poDS->hSD );
                    poDS->ReadGlobalAttributes( poDS->hSD );
                    poDS->papszLocalMetadata =
                        CSLDuplicate( poDS->papszGlobalMetadata );
                    poDS->papszLocalMetadata = 
                        poDS->GetSwatAttrs( hSW, poDS->papszLocalMetadata );
                    poDS->SetMetadata( poDS->papszLocalMetadata );
                }

/* -------------------------------------------------------------------- */
/*      Handle Geolocation processing.                                  */
/* -------------------------------------------------------------------- */
                if( poDS->iSubdatasetType == EOS_SWATH ) /* Not SWATH_GEOL */
                {
                    if( !poDS->ProcessSwathGeolocation( hSW, papszDimList ) )
                        return NULL;
                }

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
                CSLDestroy( papszDimMap );
                CPLFree( pszDimMaps );
                CPLFree( pszDimList );

                CSLDestroy( papszDimList );
                SWdetach( hSW );
            }
            break;

/* -------------------------------------------------------------------- */
/*  HDF-EOS Grid.                                                       */
/* -------------------------------------------------------------------- */
            case EOS_GRID:
            {
                int32   hHDF4, hGD,
                    iProjCode = 0, iZoneCode = 0, iSphereCode = 0;
                int32   nXSize, nYSize;
                char    szDimList[8192];
                char    **papszDimList = NULL;
                int     nDimCount;
                double  adfUpLeft[2], adfLowRight[2], adfProjParms[15];
                    
                if( poOpenInfo->eAccess == GA_ReadOnly )
                    poDS->hHDF4 = GDopen( poDS->pszFilename, DFACC_READ );
                else
                    poDS->hHDF4 = GDopen( poDS->pszFilename, DFACC_WRITE );
                    
                if( poDS->hHDF4 <= 0 )
                    return( NULL );

                hGD = GDattach( poDS->hHDF4, poDS->pszSubdatasetName );
                GDfieldinfo( hGD, poDS->pszFieldName, &poDS->iRank,
                             poDS->aiDimSizes, &poDS->iNumType, szDimList );
#if DEBUG
                CPLDebug( "HDF4Image",
                          "List of dimensions in grid %s: %s",
                          poDS->pszFieldName, szDimList);
#endif
                papszDimList = CSLTokenizeString2( szDimList, ",",
                                                   CSLT_HONOURSTRINGS );
                nDimCount = CSLCount( papszDimList );

                // Search for the "Band" word in the name of dimension
                // or take the first one as a number of bands
                if ( poDS->iRank == 2 )
                    poDS->nBands = 1;
                else
                {
                    for ( i = 0; i < nDimCount; i++ )
                    {
                        if ( strstr( papszDimList[i], "Band" ) )
                        {
                            poDS->iBandDim = i;
                            poDS->nBands = poDS->aiDimSizes[i];
                            // Handle 4D datasets
                            if ( poDS->iRank > 3 && i < nDimCount - 1 )
                            {
                                // FIXME: is there a better way to search for
                                // the 4th dimension?
                                poDS->i4Dim = i + 1;
                                poDS->nBands *= poDS->aiDimSizes[poDS->i4Dim];
                            }
                            break;
                        }
                    }

                    if ( i == nDimCount )
                            poDS->nBands = poDS->aiDimSizes[0];
                }

                // Search for the "XDim" and "YDim" names or take the last
                // two dimensions as X and Y sizes
                poDS->iXDim = nDimCount - 1;
                poDS->iYDim = nDimCount - 2;

                for ( i = 0; i < nDimCount; i++ )
                {
                    if ( EQUALN( papszDimList[i], "X", 1 ) )
                        poDS->iXDim = i;
                    else if ( EQUALN( papszDimList[i], "Y", 1 ) )
                        poDS->iYDim = i;
                }

                // Fetch projection information
                if ( GDprojinfo( hGD, &iProjCode, &iZoneCode,
                                 &iSphereCode, adfProjParms) >= 0 )
                {
#if DEBUG
                    CPLDebug( "HDF4Image",
                              "Grid projection: "
                              "projection code: %d, zone code %d, "
                              "sphere code %d",
                              iProjCode, iZoneCode, iSphereCode );
#endif
                    poDS->oSRS.importFromUSGS( iProjCode, iZoneCode,
                                               adfProjParms, iSphereCode );
                        
                    if ( poDS->pszProjection )
                        CPLFree( poDS->pszProjection );
                    poDS->oSRS.exportToWkt( &poDS->pszProjection );
                }

                // Fetch geotransformation matrix
                if ( GDgridinfo( hGD, &nXSize, &nYSize,
                                 adfUpLeft, adfLowRight ) >= 0 )
                {
#if DEBUG
                    CPLDebug( "HDF4Image",
                              "Grid geolocation: "
                              "top left X %f, top left Y %f, "
                              "low right X %f, low right Y %f, "
                              "cols %d, rows %d",
                              adfUpLeft[0], adfUpLeft[1],
                              adfLowRight[0], adfLowRight[1],
                              nXSize, nYSize );
#endif
                    if ( iProjCode )
                    {
                        // For projected systems coordinates are in meters
                        poDS->adfGeoTransform[1] =
                            (adfLowRight[0] - adfUpLeft[0]) / nXSize;
                        poDS->adfGeoTransform[5] =
                            (adfLowRight[1] - adfUpLeft[1]) / nYSize;
                        poDS->adfGeoTransform[0] = adfUpLeft[0];
                        poDS->adfGeoTransform[3] = adfUpLeft[1];
                    }
                    else
                    {
                        // Handle angular geographic coordinates here
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
                    poDS->bHasGeoTransform = TRUE;
                }

/* -------------------------------------------------------------------- */
/*  Fetch NODATA value.                                                 */
/* -------------------------------------------------------------------- */
                pNoDataValue =
                    CPLMalloc( poDS->GetDataTypeSize(poDS->iNumType) );
                if ( GDgetfillvalue( hGD, poDS->pszFieldName,
                                     pNoDataValue ) != -1 )
                {
                    dfNoData = poDS->AnyTypeToDouble( poDS->iNumType,
                                                      pNoDataValue );
                    bNoDataSet = TRUE;
                }
                CPLFree( pNoDataValue );

/* -------------------------------------------------------------------- */
/*  Fetch metadata.                                                     */
/* -------------------------------------------------------------------- */
                EHidinfo( poDS->hHDF4, &hHDF4, &poDS->hSD );
                poDS->ReadGlobalAttributes( poDS->hSD );
                poDS->papszLocalMetadata =
                    CSLDuplicate( poDS->papszGlobalMetadata );
                poDS->papszLocalMetadata = 
                    poDS->GetGridAttrs( hGD, poDS->papszLocalMetadata );
                poDS->SetMetadata( poDS->papszLocalMetadata );

                CSLDestroy( papszDimList );
                GDdetach( hGD );
            }
            break;
                
            default:
              break;
          }
      }
      break;

/* -------------------------------------------------------------------- */
/*  'Plain' HDF scientific datasets.                                    */
/* -------------------------------------------------------------------- */
      case HDF4_SDS:
      {
          int32   iSDS;

/* -------------------------------------------------------------------- */
/*  Part, common for all types of SDSs.                                 */
/* -------------------------------------------------------------------- */
          if( poOpenInfo->eAccess == GA_ReadOnly )
              poDS->hHDF4 = Hopen( poDS->pszFilename, DFACC_READ, 0 );
          else
              poDS->hHDF4 = Hopen( poDS->pszFilename, DFACC_WRITE, 0 );
            
          if( poDS->hHDF4 <= 0 )
              return( NULL );

          poDS->hSD = SDstart( poDS->pszFilename, DFACC_READ );
          if ( poDS->hSD == -1 )
              return NULL;
                
          if ( poDS->ReadGlobalAttributes( poDS->hSD ) != CE_None )
              return NULL;

          memset( poDS->aiDimSizes, 0, sizeof(int32) * MAX_VAR_DIMS );
          iSDS = SDselect( poDS->hSD, poDS->iDataset );
          SDgetinfo( iSDS, poDS->szName, &poDS->iRank, poDS->aiDimSizes,
                     &poDS->iNumType, &poDS->nAttrs);

          // We will duplicate global metadata for every subdataset
          poDS->papszLocalMetadata =
              CSLDuplicate( poDS->papszGlobalMetadata );

          for ( iAttribute = 0; iAttribute < poDS->nAttrs; iAttribute++ )
          {
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
                    "aiDimSizes[0]=%d, aiDimSizes[1]=%d, "
                    "aiDimSizes[2]=%d, aiDimSizes[3]=%d",
                    poDS->aiDimSizes[0], poDS->aiDimSizes[1],
                    poDS->aiDimSizes[2], poDS->aiDimSizes[3] );
#endif

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
              poDS->nBands = poDS->aiDimSizes[poDS->iBandDim];
              break;
            case 4: // FIXME
              poDS->nBands = poDS->aiDimSizes[2] * poDS->aiDimSizes[3];
              break;
            default:
              break;
          }

          // We preset this because CaptureNRLGeoTransform needs it.
          poDS->nRasterXSize = poDS->aiDimSizes[poDS->iXDim];
          poDS->nRasterYSize = poDS->aiDimSizes[poDS->iYDim];

          // Special case projection info for NRL generated files. 
          const char *pszMapProjectionSystem =
              CSLFetchNameValue(poDS->papszGlobalMetadata, 
                                "mapProjectionSystem");
          if( pszMapProjectionSystem != NULL 
              && EQUAL(pszMapProjectionSystem,"NRL(USGS)") )
          {
              poDS->CaptureNRLGeoTransform();
          }

          // Special cast for coastwatch hdf files. 
          if( CSLFetchNameValue( poDS->papszGlobalMetadata, 
                                 "gctp_sys" ) != NULL )
              poDS->CaptureCoastwatchGCTPInfo();
      }
      break;

/* -------------------------------------------------------------------- */
/*  'Plain' HDF rasters.                                                */
/* -------------------------------------------------------------------- */
      case HDF4_GR:
        if( poOpenInfo->eAccess == GA_ReadOnly )
            poDS->hHDF4 = Hopen( poDS->pszFilename, DFACC_READ, 0 );
        else
            poDS->hHDF4 = Hopen( poDS->pszFilename, DFACC_WRITE, 0 );
            
        if( poDS->hHDF4 <= 0 )
            return( NULL );

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
    }
    
    poDS->nRasterXSize = poDS->aiDimSizes[poDS->iXDim];
    poDS->nRasterYSize = poDS->aiDimSizes[poDS->iYDim];

    if ( poDS->iSubdatasetType == HYPERION_L1 )
    {
        // XXX: Hyperion SDSs has Height x Bands x Width dimensions scheme
        if ( poDS->iRank > 2 )
        {
            poDS->nBands = poDS->aiDimSizes[1];
            poDS->nRasterXSize = poDS->aiDimSizes[2];
            poDS->nRasterYSize = poDS->aiDimSizes[0];
        }
        else
        {
            poDS->nBands = poDS->aiDimSizes[0];
            poDS->nRasterXSize = poDS->aiDimSizes[1];
            poDS->nRasterYSize = 1;
        }
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( i = 1; i <= poDS->nBands; i++ )
    {
        HDF4ImageRasterBand *poBand = new HDF4ImageRasterBand( poDS, i,
                                                               poDS->GetDataType( poDS->iNumType ) );
        poDS->SetBand( i, poBand );
        if ( bNoDataSet )
            poBand->SetNoDataValue( dfNoData );
    }

/* -------------------------------------------------------------------- */
/*      Now we will handle particular types of HDF products. Every      */
/*      HDF product has its own structure.                              */
/* -------------------------------------------------------------------- */

    // Variables for reading georeferencing
    double          dfULX, dfULY, dfLRX, dfLRY;

    switch ( poDS->iSubdatasetType )
    {
/* -------------------------------------------------------------------- */
/*  HDF, created by GDAL.                                               */
/* -------------------------------------------------------------------- */
      case GDAL_HDF4:
      {
          const char  *pszValue;

          CPLDebug( "HDF4Image",
                    "Input dataset interpreted as GDAL_HDF4" );

          if ( (pszValue =
                CSLFetchNameValue(poDS->papszGlobalMetadata,
                                  "Projection")) )
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
              poDS->bHasGeoTransform = TRUE;
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
                  poDS->GetRasterBand(i)->SetNoDataValue(atof(pszValue));
          }
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
          poDS->oSRS.SetWellKnownGeogCS( "WGS84" );
            
          if ( EQUAL(CSLFetchNameValue(poDS->papszGlobalMetadata,
                                       "Map Projection"),
                     "Equidistant Cylindrical") )
          {
              poDS->oSRS.SetEquirectangular( 0.0, 0.0, 0.0, 0.0 );
              poDS->oSRS.SetLinearUnits( SRS_UL_METER, 1 );
              if ( poDS->pszProjection )
                  CPLFree( poDS->pszProjection );
              poDS->oSRS.exportToWkt( &poDS->pszProjection );
          }

          dfULX = atof( CSLFetchNameValue(poDS->papszGlobalMetadata,
                                          "Westernmost Longitude") );
          dfULY = atof( CSLFetchNameValue(poDS->papszGlobalMetadata,
                                          "Northernmost Latitude") );
          dfLRX = atof( CSLFetchNameValue(poDS->papszGlobalMetadata,
                                          "Easternmost Longitude") );
          dfLRY = atof( CSLFetchNameValue(poDS->papszGlobalMetadata,
                                          "Southernmost Latitude") );
          poDS->ToGeoref( &dfULX, &dfULY );
          poDS->ToGeoref( &dfLRX, &dfLRY );
          poDS->adfGeoTransform[0] = dfULX;
          poDS->adfGeoTransform[3] = dfULY;
          poDS->adfGeoTransform[1] = (dfLRX - dfULX) / poDS->nRasterXSize;
          poDS->adfGeoTransform[5] = (dfULY - dfLRY) / poDS->nRasterYSize;
          if ( dfULY > 0)     // Northern hemisphere
              poDS->adfGeoTransform[5] = - poDS->adfGeoTransform[5];
          poDS->adfGeoTransform[2] = 0.0;
          poDS->adfGeoTransform[4] = 0.0;
          poDS->bHasGeoTransform = TRUE;
      }
      break;


/* -------------------------------------------------------------------- */
/*	Generic SDS							*/
/* -------------------------------------------------------------------- */
      case UNKNOWN:
      {

          // This is a coastwatch convention.
          if( CSLFetchNameValue( poDS->papszLocalMetadata, "missing_value" ) )
          {
              int i;
              for( i = 1; i <= poDS->nBands; i++ )
              {
                  poDS->GetRasterBand(i)->SetNoDataValue(
                      atof( CSLFetchNameValue(poDS->papszLocalMetadata, 
                                              "missing_value") ) );
              }
          }

          // Coastwatch offset and scale.
          if( CSLFetchNameValue( poDS->papszLocalMetadata, "scale_factor" ) 
              && CSLFetchNameValue( poDS->papszLocalMetadata, "add_offset" ) )
          {
              for( i = 1; i <= poDS->nBands; i++ )
              {
                  HDF4ImageRasterBand *poBand = 
                      (HDF4ImageRasterBand *) poDS->GetRasterBand(i);

                  poBand->bHaveScaleAndOffset = TRUE;
                  poBand->dfScale = 
                      atof( CSLFetchNameValue( poDS->papszLocalMetadata, 
                                               "scale_factor" ) );
                  poBand->dfOffset = -1 * poBand->dfScale * 
                      atof( CSLFetchNameValue( poDS->papszLocalMetadata, 
                                               "add_offset" ) );
              }
          }

          // this is a modis level3 convention (data from ACT)
          // Eg data/hdf/act/modis/MODAM2004280160000.L3_NOAA_GMX

          if( CSLFetchNameValue( poDS->papszLocalMetadata, 
                                 "scalingSlope" ) 
              && CSLFetchNameValue( poDS->papszLocalMetadata, 
                                    "scalingIntercept" ) )
          {
              int i;
              CPLString osUnits;
              
              if( CSLFetchNameValue( poDS->papszLocalMetadata, 
                                     "productUnits" ) )
              {
                  osUnits = CSLFetchNameValue( poDS->papszLocalMetadata, 
                                               "productUnits" );
              }

              for( i = 1; i <= poDS->nBands; i++ )
              {
                  HDF4ImageRasterBand *poBand = 
                      (HDF4ImageRasterBand *) poDS->GetRasterBand(i);

                  poBand->bHaveScaleAndOffset = TRUE;
                  poBand->dfScale = 
                      atof( CSLFetchNameValue( poDS->papszLocalMetadata, 
                                               "scalingSlope" ) );
                  poBand->dfOffset = 
                      atof( CSLFetchNameValue( poDS->papszLocalMetadata, 
                                               "scalingIntercept" ) );

                  poBand->osUnitType = osUnits;
              }
          }
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
    int32               iSDS = -1;
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
    else if ( poDS->iRank == 3 )
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
    else                                            // Should never happen
        return NULL;

    if ( iSDS < 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Can't create SDS with rank %d for file %s",
                  poDS->iRank, pszFilename );
        return NULL;
    }

    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->eAccess = GA_Update;
    poDS->iDatasetType = HDF4_SDS;
    poDS->iSubdatasetType = GDAL_HDF4;
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

