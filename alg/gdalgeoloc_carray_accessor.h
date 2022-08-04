/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  C-Array storage of geolocation array and backmap
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

/*! @cond Doxygen_Suppress */

/************************************************************************/
/*                        GDALGeoLocCArrayAccessors                     */
/************************************************************************/

class GDALGeoLocCArrayAccessors
{
    typedef class GDALGeoLocCArrayAccessors AccessorType;

    GDALGeoLocTransformInfo*  m_psTransform;
    double                   *m_padfGeoLocX = nullptr;
    double                   *m_padfGeoLocY = nullptr;
    float                    *m_pafBackMapX = nullptr;
    float                    *m_pafBackMapY = nullptr;
    float                    *m_wgtsBackMap = nullptr;

    bool                      LoadGeoloc(bool bIsRegularGrid);

public:

    template<class Type> struct CArrayAccessor
    {
        Type*  m_array;
        size_t m_nXSize;

        CArrayAccessor(Type* array, size_t nXSize): m_array(array), m_nXSize(nXSize) {}

        inline Type Get(int nX, int nY, bool* pbSuccess = nullptr)
        {
            if( pbSuccess )
                *pbSuccess = true;
            return m_array[nY * m_nXSize + nX];
        }

        inline bool Set(int nX, int nY, Type val)
        {
            m_array[nY * m_nXSize + nX] = val;
            return true;
        }
    };

    CArrayAccessor<double> geolocXAccessor;
    CArrayAccessor<double> geolocYAccessor;
    CArrayAccessor<float>  backMapXAccessor;
    CArrayAccessor<float>  backMapYAccessor;
    CArrayAccessor<float>  backMapWeightAccessor;

    explicit GDALGeoLocCArrayAccessors(GDALGeoLocTransformInfo* psTransform):
        m_psTransform(psTransform),
        geolocXAccessor(nullptr, 0),
        geolocYAccessor(nullptr, 0),
        backMapXAccessor(nullptr, 0),
        backMapYAccessor(nullptr, 0),
        backMapWeightAccessor(nullptr, 0)
    {
    }

    ~GDALGeoLocCArrayAccessors()
    {
        VSIFree(m_pafBackMapX);
        VSIFree(m_pafBackMapY);
        VSIFree(m_padfGeoLocX);
        VSIFree(m_padfGeoLocY);
        VSIFree(m_wgtsBackMap);
    }

    GDALGeoLocCArrayAccessors(const GDALGeoLocCArrayAccessors&) = delete;
    GDALGeoLocCArrayAccessors& operator= (const GDALGeoLocCArrayAccessors&) = delete;

    bool         Load(bool bIsRegularGrid, bool bUseQuadtree);

    bool         AllocateBackMap();

    GDALDataset* GetBackmapDataset();
    static void  FlushBackmapCaches() {}
    static void  ReleaseBackmapDataset(GDALDataset* poDS) { delete poDS; }

    void         FreeWghtsBackMap();
};

/************************************************************************/
/*                         AllocateBackMap()                            */
/************************************************************************/

bool GDALGeoLocCArrayAccessors::AllocateBackMap()
{
    m_pafBackMapX = static_cast<float *>(
        VSI_MALLOC3_VERBOSE(m_psTransform->nBackMapWidth,
                            m_psTransform->nBackMapHeight, sizeof(float)));
    m_pafBackMapY = static_cast<float *>(
        VSI_MALLOC3_VERBOSE(m_psTransform->nBackMapWidth,
                            m_psTransform->nBackMapHeight, sizeof(float)));

    m_wgtsBackMap = static_cast<float *>(
        VSI_MALLOC3_VERBOSE(m_psTransform->nBackMapWidth,
                            m_psTransform->nBackMapHeight, sizeof(float)));

    if( m_pafBackMapX == nullptr ||
        m_pafBackMapY == nullptr ||
        m_wgtsBackMap == nullptr)
    {
        return false;
    }

    const size_t nBMXYCount = static_cast<size_t>(m_psTransform->nBackMapWidth) *
                                m_psTransform->nBackMapHeight;
    for( size_t i = 0; i < nBMXYCount; i++ )
    {
        m_pafBackMapX[i] = 0;
        m_pafBackMapY[i] = 0;
        m_wgtsBackMap[i] = 0.0;
    }

    backMapXAccessor.m_array = m_pafBackMapX;
    backMapXAccessor.m_nXSize = m_psTransform->nBackMapWidth;

    backMapYAccessor.m_array = m_pafBackMapY;
    backMapYAccessor.m_nXSize = m_psTransform->nBackMapWidth;

    backMapWeightAccessor.m_array = m_wgtsBackMap;
    backMapWeightAccessor.m_nXSize = m_psTransform->nBackMapWidth;

    return true;
}

/************************************************************************/
/*                         FreeWghtsBackMap()                           */
/************************************************************************/

void GDALGeoLocCArrayAccessors::FreeWghtsBackMap()
{
    VSIFree(m_wgtsBackMap);
    m_wgtsBackMap = nullptr;
    backMapWeightAccessor.m_array = nullptr;
    backMapWeightAccessor.m_nXSize = 0;
}

/************************************************************************/
/*                        GetBackmapDataset()                           */
/************************************************************************/

GDALDataset* GDALGeoLocCArrayAccessors::GetBackmapDataset()
{
    auto poMEMDS = MEMDataset::Create( "",
                              m_psTransform->nBackMapWidth,
                              m_psTransform->nBackMapHeight,
                              0, GDT_Float32, nullptr );

    for( int i = 1; i <= 2; i++ )
    {
        void* ptr = (i == 1) ? m_pafBackMapX : m_pafBackMapY;
        GDALRasterBandH hMEMBand = MEMCreateRasterBandEx( poMEMDS,
                                                      i, static_cast<GByte*>(ptr),
                                                      GDT_Float32,
                                                      0, 0,
                                                      false );
        poMEMDS->AddMEMBand(hMEMBand);
        poMEMDS->GetRasterBand(i)->SetNoDataValue(INVALID_BMXY);
    }
    return poMEMDS;
}

/************************************************************************/
/*                             Load()                                   */
/************************************************************************/

bool GDALGeoLocCArrayAccessors::Load(bool bIsRegularGrid, bool bUseQuadtree)
{
    return LoadGeoloc(bIsRegularGrid) &&
           ((bUseQuadtree && GDALGeoLocBuildQuadTree(m_psTransform)) ||
            (!bUseQuadtree && GDALGeoLoc<AccessorType>::GenerateBackMap(m_psTransform)));
}

/************************************************************************/
/*                          LoadGeoloc()                                */
/************************************************************************/

bool GDALGeoLocCArrayAccessors::LoadGeoloc(bool bIsRegularGrid)

{
    const int nXSize = m_psTransform->nGeoLocXSize;
    const int nYSize = m_psTransform->nGeoLocYSize;

    m_padfGeoLocY = static_cast<double *>(
        VSI_MALLOC3_VERBOSE(sizeof(double), nXSize, nYSize));
    m_padfGeoLocX = static_cast<double *>(
        VSI_MALLOC3_VERBOSE(sizeof(double), nXSize, nYSize));

    if( m_padfGeoLocX == nullptr ||
        m_padfGeoLocY == nullptr )
    {
        return false;
    }

    if( bIsRegularGrid )
    {
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

        for( size_t j = 0; j < static_cast<size_t>(nYSize); j++ )
        {
            memcpy( m_padfGeoLocX + j * nXSize,
                    padfTempX,
                    nXSize * sizeof(double) );
        }

        if( eErr == CE_None )
        {
            eErr = GDALRasterIO( m_psTransform->hBand_Y, GF_Read,
                                 0, 0, nYSize, 1,
                                 padfTempY, nYSize, 1,
                                 GDT_Float64, 0, 0 );

            for( size_t j = 0; j < static_cast<size_t>(nYSize); j++ )
            {
                for( size_t i = 0; i < static_cast<size_t>(nXSize); i++ )
                {
                    m_padfGeoLocY[j * nXSize + i] = padfTempY[j];
                }
            }
        }

        CPLFree(padfTempX);
        CPLFree(padfTempY);

        if( eErr != CE_None )
            return false;
    }
    else
    {
        if( GDALRasterIO( m_psTransform->hBand_X, GF_Read,
                          0, 0, nXSize, nYSize,
                          m_padfGeoLocX, nXSize, nYSize,
                          GDT_Float64, 0, 0 ) != CE_None
            || GDALRasterIO( m_psTransform->hBand_Y, GF_Read,
                             0, 0, nXSize, nYSize,
                             m_padfGeoLocY, nXSize, nYSize,
                             GDT_Float64, 0, 0 ) != CE_None )
            return false;
    }

    geolocXAccessor.m_array = m_padfGeoLocX;
    geolocXAccessor.m_nXSize = m_psTransform->nGeoLocXSize;

    geolocYAccessor.m_array = m_padfGeoLocY;
    geolocYAccessor.m_nXSize = m_psTransform->nGeoLocXSize;

    return GDALGeoLoc<GDALGeoLocCArrayAccessors>::LoadGeolocFinish(m_psTransform);
}

/*! @endcond */
