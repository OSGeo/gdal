/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Dataset storage of geolocation array and backmap
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
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

#include "gdalcachedpixelaccessor.h"

/*! @cond Doxygen_Suppress */

/************************************************************************/
/*                        GDALGeoLocDatasetAccessors                    */
/************************************************************************/

class GDALGeoLocDatasetAccessors
{
    typedef class GDALGeoLocDatasetAccessors AccessorType;

    static constexpr int TILE_SIZE = 1024;

    GDALGeoLocTransformInfo*  m_psTransform;

    CPLStringList    m_aosGTiffCreationOptions{};

    GDALDataset*     m_poGeolocTmpDataset = nullptr;
    GDALDataset*     m_poBackmapTmpDataset = nullptr;
    GDALDataset*     m_poBackmapWeightsTmpDataset = nullptr;

    GDALGeoLocDatasetAccessors(const GDALGeoLocDatasetAccessors&) = delete;
    GDALGeoLocDatasetAccessors& operator= (const GDALGeoLocDatasetAccessors&) = delete;

    bool         LoadGeoloc(bool bIsRegularGrid);

public:
    GDALCachedPixelAccessor<double, TILE_SIZE> geolocXAccessor;
    GDALCachedPixelAccessor<double, TILE_SIZE> geolocYAccessor;
    GDALCachedPixelAccessor<float, TILE_SIZE>  backMapXAccessor;
    GDALCachedPixelAccessor<float, TILE_SIZE>  backMapYAccessor;
    GDALCachedPixelAccessor<float, TILE_SIZE>  backMapWeightAccessor;

    explicit GDALGeoLocDatasetAccessors(GDALGeoLocTransformInfo* psTransform):
        m_psTransform(psTransform),
        geolocXAccessor(nullptr),
        geolocYAccessor(nullptr),
        backMapXAccessor(nullptr),
        backMapYAccessor(nullptr),
        backMapWeightAccessor(nullptr)
    {
        m_aosGTiffCreationOptions.SetNameValue("TILED", "YES");
        m_aosGTiffCreationOptions.SetNameValue("INTERLEAVE", "BAND");
        m_aosGTiffCreationOptions.SetNameValue("BLOCKXSIZE", CPLSPrintf("%d", TILE_SIZE));
        m_aosGTiffCreationOptions.SetNameValue("BLOCKYSIZE", CPLSPrintf("%d", TILE_SIZE));
    }

               ~GDALGeoLocDatasetAccessors();

    bool         Load(bool bIsRegularGrid, bool bUseQuadtree);

    bool         AllocateBackMap();

    GDALDataset* GetBackmapDataset();
    void         FlushBackmapCaches();
    static void  ReleaseBackmapDataset(GDALDataset*) {}

    void         FreeWghtsBackMap();
};

/************************************************************************/
/*                    ~GDALGeoLocDatasetAccessors()                     */
/************************************************************************/

GDALGeoLocDatasetAccessors::~GDALGeoLocDatasetAccessors()
{
    geolocXAccessor.ResetModifiedFlag();
    geolocYAccessor.ResetModifiedFlag();
    backMapXAccessor.ResetModifiedFlag();
    backMapYAccessor.ResetModifiedFlag();

    FreeWghtsBackMap();

    delete m_poGeolocTmpDataset;
    delete m_poBackmapTmpDataset;
}

/************************************************************************/
/*                         AllocateBackMap()                            */
/************************************************************************/

bool GDALGeoLocDatasetAccessors::AllocateBackMap()
{
    auto poDriver = GDALDriver::FromHandle(GDALGetDriverByName("GTiff"));
    if( poDriver == nullptr )
        return false;

    m_poBackmapTmpDataset = poDriver->Create(
        CPLResetExtension(CPLGenerateTempFilename(nullptr), "tif"),
        m_psTransform->nBackMapWidth,
        m_psTransform->nBackMapHeight,
        2,
        GDT_Float32,
        m_aosGTiffCreationOptions.List());
    if( m_poBackmapTmpDataset == nullptr )
    {
        return false;
    }
    m_poBackmapTmpDataset->MarkSuppressOnClose();
    VSIUnlink(m_poBackmapTmpDataset->GetDescription());
    auto poBandX = m_poBackmapTmpDataset->GetRasterBand(1);
    auto poBandY = m_poBackmapTmpDataset->GetRasterBand(2);

    backMapXAccessor.SetBand(poBandX);
    backMapYAccessor.SetBand(poBandY);

    m_poBackmapWeightsTmpDataset = poDriver->Create(
        CPLResetExtension(CPLGenerateTempFilename(nullptr), "tif"),
        m_psTransform->nBackMapWidth,
        m_psTransform->nBackMapHeight,
        1,
        GDT_Float32,
        m_aosGTiffCreationOptions.List());
    if( m_poBackmapWeightsTmpDataset == nullptr )
    {
        return false;
    }
    m_poBackmapWeightsTmpDataset->MarkSuppressOnClose();
    VSIUnlink(m_poBackmapWeightsTmpDataset->GetDescription());
    backMapWeightAccessor.SetBand(m_poBackmapWeightsTmpDataset->GetRasterBand(1));

    return true;
}

/************************************************************************/
/*                         FreeWghtsBackMap()                           */
/************************************************************************/

void GDALGeoLocDatasetAccessors::FreeWghtsBackMap()
{
    if( m_poBackmapWeightsTmpDataset )
    {
        backMapWeightAccessor.ResetModifiedFlag();
        delete m_poBackmapWeightsTmpDataset;
        m_poBackmapWeightsTmpDataset = nullptr;
    }
}

/************************************************************************/
/*                        GetBackmapDataset()                           */
/************************************************************************/

GDALDataset* GDALGeoLocDatasetAccessors::GetBackmapDataset()
{
    auto poBandX = m_poBackmapTmpDataset->GetRasterBand(1);
    auto poBandY = m_poBackmapTmpDataset->GetRasterBand(2);
    poBandX->SetNoDataValue(INVALID_BMXY);
    poBandY->SetNoDataValue(INVALID_BMXY);
    return m_poBackmapTmpDataset;
}

/************************************************************************/
/*                       FlushBackmapCaches()                           */
/************************************************************************/

void GDALGeoLocDatasetAccessors::FlushBackmapCaches()
{
    backMapXAccessor.FlushCache();
    backMapYAccessor.FlushCache();
}

/************************************************************************/
/*                             Load()                                   */
/************************************************************************/

bool GDALGeoLocDatasetAccessors::Load(bool bIsRegularGrid, bool bUseQuadtree)
{
    return LoadGeoloc(bIsRegularGrid) &&
           ((bUseQuadtree && GDALGeoLocBuildQuadTree(m_psTransform)) ||
            (!bUseQuadtree && GDALGeoLoc<AccessorType>::GenerateBackMap(m_psTransform)));
}

/************************************************************************/
/*                          LoadGeoloc()                                */
/************************************************************************/

bool GDALGeoLocDatasetAccessors::LoadGeoloc(bool bIsRegularGrid)

{
    if( bIsRegularGrid )
    {
        const int nXSize = m_psTransform->nGeoLocXSize;
        const int nYSize = m_psTransform->nGeoLocYSize;

        auto poDriver = GDALDriver::FromHandle(GDALGetDriverByName("GTiff"));
        if( poDriver == nullptr )
            return false;

        m_poGeolocTmpDataset = poDriver->Create(
            CPLResetExtension(CPLGenerateTempFilename(nullptr), "tif"),
            nXSize,
            nYSize,
            2,
            GDT_Float64,
            m_aosGTiffCreationOptions.List());
        if( m_poGeolocTmpDataset == nullptr )
        {
            return false;
        }
        m_poGeolocTmpDataset->MarkSuppressOnClose();
        VSIUnlink(m_poGeolocTmpDataset->GetDescription());

        auto poXBand = m_poGeolocTmpDataset->GetRasterBand(1);
        auto poYBand = m_poGeolocTmpDataset->GetRasterBand(2);

        // Case of regular grid.
        // The XBAND contains the x coordinates for all lines.
        // The YBAND contains the y coordinates for all columns.

        double* padfTempX = static_cast<double *>(
            VSI_MALLOC2_VERBOSE(nXSize, sizeof(double)));
        double* padfTempY = static_cast<double *>(
            VSI_MALLOC2_VERBOSE(nYSize, sizeof(double)));
        if( padfTempX == nullptr || padfTempY == nullptr )
        {
            CPLFree(padfTempX);
            CPLFree(padfTempY);
            return false;
        }

        CPLErr eErr =
            GDALRasterIO( m_psTransform->hBand_X, GF_Read,
                          0, 0, nXSize, 1,
                          padfTempX, nXSize, 1,
                          GDT_Float64, 0, 0 );

        for( int j = 0; j < nYSize; j++ )
        {
             if( poXBand->RasterIO(GF_Write,
                              0, j, nXSize, 1,
                              padfTempX,
                              nXSize, 1,
                              GDT_Float64,
                              0, 0, nullptr) != CE_None )
             {
                 eErr = CE_Failure;
                 break;
             }
        }

        if( eErr == CE_None )
        {
            eErr = GDALRasterIO( m_psTransform->hBand_Y, GF_Read,
                                 0, 0, nYSize, 1,
                                 padfTempY, nYSize, 1,
                                 GDT_Float64, 0, 0 );

            for( int i = 0; i < nXSize; i++ )
            {
                 if( poYBand->RasterIO(GF_Write,
                                  i, 0, 1, nYSize,
                                  padfTempY,
                                  1, nYSize,
                                  GDT_Float64,
                                  0, 0, nullptr) != CE_None )
                 {
                     eErr = CE_Failure;
                     break;
                 }
            }
        }

        CPLFree(padfTempX);
        CPLFree(padfTempY);

        if( eErr != CE_None )
            return false;

        geolocXAccessor.SetBand( poXBand );
        geolocYAccessor.SetBand( poYBand );

    }
    else
    {
        geolocXAccessor.SetBand( GDALRasterBand::FromHandle(m_psTransform->hBand_X) );
        geolocYAccessor.SetBand( GDALRasterBand::FromHandle(m_psTransform->hBand_Y) );
    }

    return GDALGeoLoc<GDALGeoLocDatasetAccessors>::LoadGeolocFinish(m_psTransform);
}

/*! @endcond */
