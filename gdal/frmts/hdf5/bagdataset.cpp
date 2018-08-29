/******************************************************************************
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Read BAG datasets.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2018, Even Rouault <even dot rouault at spatialys dot com>
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

#include "cpl_port.h"
#include "hdf5dataset.h"
#include "gh5_convenience.h"

#include "cpl_string.h"
#include "cpl_time.h"
#include "gdal_alg.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "iso19115_srs.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"

#include <algorithm>
#include <limits>
#include <utility>
#include <set>

CPL_CVSID("$Id$")

#if defined(H5_VERSION_GE) // added in 1.8.7
# if !H5_VERSION_GE(1,8,13)
#  define H5free_memory(x) CPL_IGNORE_RET_VAL(x)
# endif
#else
#  define H5free_memory(x) CPL_IGNORE_RET_VAL(x)
#endif

struct BAGRefinementGrid
{
    unsigned nIndex = 0;
    unsigned nWidth = 0;
    unsigned nHeight = 0;
    float    fResX = 0.0f;
    float    fResY = 0.0f;
    float    fSWX = 0.0f; // offset from (bottom left corner of) the south west low resolution grid, in center-pixel convention
    float    fSWY = 0.0f; // offset from (bottom left corner of) the south west low resolution grid, in center-pixel convention
};

constexpr float fDEFAULT_NODATA = 1000000.0f;

/************************************************************************/
/*                                h5check()                             */
/************************************************************************/

#ifdef DEBUG
static int h5check(int ret, const char* filename, int line)
{
    if( ret < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "HDF5 API failed at %s:%d", 
                 filename, line);
    }
    return ret;
}

#define H5_CHECK(x) h5check(x, __FILE__, __LINE__)
#else
#define H5_CHECK(x) (x)
#endif

/************************************************************************/
/* ==================================================================== */
/*                               BAGDataset                             */
/* ==================================================================== */
/************************************************************************/
class BAGDataset final: public GDALPamDataset
{
    friend class BAGRasterBand;
    friend class BAGSuperGridBand;
    friend class BAGBaseBand;
    friend class BAGResampledBand;
    friend class BAGInterpolatedBand;

    enum class Population
    {
        MAX,
        MIN,
        MEAN
    };
    Population   m_ePopulation = Population::MAX;
    bool         m_bMask = false;

    bool         m_bIsChild = false;
    std::vector<std::unique_ptr<BAGDataset>> m_apoOverviewDS{};
    std::unique_ptr<BAGDataset> m_poUninterpolatedDS{};

    hid_t        hHDF5 = -1;

    char        *pszProjection = nullptr;
    double       adfGeoTransform[6] = {0,1,0,0,0,1};

    int          m_nLowResWidth = 0;
    int          m_nLowResHeight = 0;

    double       m_dfLowResMinX = 0.0;
    double       m_dfLowResMinY = 0.0;
    double       m_dfLowResMaxX = 0.0;
    double       m_dfLowResMaxY = 0.0;

    void         LoadMetadata();

    char        *pszXMLMetadata = nullptr;
    char        *apszMDList[2]{};

    int          m_nChunkXSizeVarresMD = 0;
    int          m_nChunkYSizeVarresMD = 0;
    void         GetVarresMetadataChunkSizes(int& nChunkXSize,
                                             int& nChunkYSize);

    unsigned     m_nChunkSizeVarresRefinement = 0;
    void         GetVarresRefinementChunkSize(unsigned& nChunkSize);
 
    bool         ReadVarresMetadataValue(int y, int x, hid_t memspace,
                                         BAGRefinementGrid* rgrid,
                                         int height, int width);

    bool         LookForRefinementGrids(CSLConstList l_papszOpenOptions, int nY, int nX);

    hid_t        m_hVarresMetadata = -1;
    hid_t        m_hVarresMetadataDataType = -1;
    hid_t        m_hVarresMetadataDataspace = -1;
    hid_t        m_hVarresMetadataNative = -1;
    std::vector<BAGRefinementGrid> m_aoRefinemendGrids;

    CPLStringList m_aosSubdatasets;

    hid_t        m_hVarresRefinements = -1;
    hid_t        m_hVarresRefinementsDataType = -1;
    hid_t        m_hVarresRefinementsDataspace = -1;
    hid_t        m_hVarresRefinementsNative = -1;
    unsigned     m_nRefinementsSize = 0;

    unsigned     m_nSuperGridRefinementStartIndex = 0;

    unsigned     m_nCachedRefinementStartIndex = 0;
    unsigned     m_nCachedRefinementCount = 0;
    std::vector<float> m_aCachedRefinementValues;
    bool         CacheRefinementValues(unsigned nRefinementIndex);

    bool         GetMeanSupergridsResolution(double& dfResX, double& dfResY);

    double       m_dfResFilterMin = 0;
    double       m_dfResFilterMax = std::numeric_limits<double>::infinity();

public:
    BAGDataset();
    BAGDataset(BAGDataset* poParentDS, int nOvrFactor);
    virtual ~BAGDataset();

    virtual CPLErr GetGeoTransform( double * ) override;
    virtual const char *GetProjectionRef(void) override;
    virtual char      **GetMetadataDomainList() override;
    virtual char      **GetMetadata( const char * pszDomain = "" ) override;

    static GDALDataset  *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );
    static GDALDataset* CreateCopy( const char *pszFilename, GDALDataset *poSrcDS,
                        int bStrict, char ** papszOptions,
                        GDALProgressFunc pfnProgress, void *pProgressData );

    OGRErr ParseWKTFromXML( const char *pszISOXML );
};

/************************************************************************/
/* ==================================================================== */
/*                               BAGRasterBand                          */
/* ==================================================================== */
/************************************************************************/
class BAGRasterBand final: public GDALPamRasterBand
{
    friend class BAGDataset;

    hid_t       hDatasetID;
    hid_t       native;
    hid_t       dataspace;

    bool        bMinMaxSet;
    double      dfMinimum;
    double      dfMaximum;

    bool        m_bHasNoData = false;
    float       m_fNoDataValue = std::numeric_limits<float>::quiet_NaN();

public:
    BAGRasterBand( BAGDataset *, int );
    virtual ~BAGRasterBand();

    bool                    Initialize( hid_t hDataset, const char *pszName );

    virtual CPLErr          IReadBlock( int, int, void * ) override;
    virtual double          GetNoDataValue( int * ) override;

    virtual double GetMinimum( int *pbSuccess = nullptr ) override;
    virtual double GetMaximum( int *pbSuccess = nullptr ) override;
};

/************************************************************************/
/* ==================================================================== */
/*                              BAGBaseBand                             */
/* ==================================================================== */
/************************************************************************/

class BAGBaseBand: public GDALRasterBand
{
    protected:
        bool        m_bHasNoData = false;
        float       m_fNoDataValue = std::numeric_limits<float>::quiet_NaN();

    public:
        BAGBaseBand() = default;
        ~BAGBaseBand() = default;

        double          GetNoDataValue( int * ) override;

        int GetOverviewCount() override;
        GDALRasterBand* GetOverview(int) override;
};

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double BAGBaseBand::GetNoDataValue( int *pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = m_bHasNoData;
    if( m_bHasNoData )
        return m_fNoDataValue;

    return GDALRasterBand::GetNoDataValue(pbSuccess);
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int BAGBaseBand::GetOverviewCount()
{
    BAGDataset* poGDS = cpl::down_cast<BAGDataset*>(poDS);
    return static_cast<int>(poGDS->m_apoOverviewDS.size());
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *BAGBaseBand::GetOverview( int i )

{
    if( i < 0 || i >= GetOverviewCount() )
        return nullptr;
    BAGDataset* poGDS = cpl::down_cast<BAGDataset*>(poDS);
    return poGDS->m_apoOverviewDS[i]->GetRasterBand(nBand);
}

/************************************************************************/
/* ==================================================================== */
/*                             BAGSuperGridBand                         */
/* ==================================================================== */
/************************************************************************/

class BAGSuperGridBand final: public BAGBaseBand
{
    friend class BAGDataset;

public:
    BAGSuperGridBand( BAGDataset *, int, bool bHasNoData, float fNoDataValue);
    virtual ~BAGSuperGridBand();

    CPLErr          IReadBlock( int, int, void * ) override;
};

/************************************************************************/
/* ==================================================================== */
/*                             BAGResampledBand                         */
/* ==================================================================== */
/************************************************************************/

class BAGResampledBand final: public BAGBaseBand
{
    friend class BAGDataset;
    friend class BAGInterpolatedBand;

    bool        m_bMinMaxSet = false;
    double      m_dfMinimum = 0.0;
    double      m_dfMaximum = 0.0;
    float       m_fNoSuperGridValue = 0.0;

public:
    BAGResampledBand( BAGDataset *, int nBandIn,
                      bool bHasNoData, float fNoDataValue,
                      bool bInitializeMinMax);
    virtual ~BAGResampledBand();

    void            InitializeMinMax();

    CPLErr          IReadBlock( int, int, void * ) override;

    double GetMinimum( int *pbSuccess = nullptr ) override;
    double GetMaximum( int *pbSuccess = nullptr ) override;
};

/************************************************************************/
/* ==================================================================== */
/*                          BAGInterpolatedBand                         */
/* ==================================================================== */
/************************************************************************/

class BAGInterpolatedBand final: public BAGBaseBand
{
    BAGResampledBand* m_poUninterpolatedBand = nullptr;

public:
    BAGInterpolatedBand( BAGDataset *, int nBandIn);
    virtual ~BAGInterpolatedBand();

    CPLErr          IReadBlock( int, int, void * ) override;

    double GetMinimum( int *pbSuccess = nullptr ) override;
    double GetMaximum( int *pbSuccess = nullptr ) override;
};

/************************************************************************/
/*                           BAGRasterBand()                            */
/************************************************************************/
BAGRasterBand::BAGRasterBand( BAGDataset *poDSIn, int nBandIn ) :
    hDatasetID(-1),
    native(-1),
    dataspace(-1),
    bMinMaxSet(false),
    dfMinimum(0.0),
    dfMaximum(0.0)
{
    poDS = poDSIn;
    nBand = nBandIn;
}

/************************************************************************/
/*                           ~BAGRasterBand()                           */
/************************************************************************/

BAGRasterBand::~BAGRasterBand()
{
    if( dataspace > 0 )
        H5Sclose(dataspace);

    if( native > 0 )
        H5Tclose(native);

    if( hDatasetID > 0 )
        H5Dclose(hDatasetID);
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

bool BAGRasterBand::Initialize( hid_t hDatasetIDIn, const char *pszName )

{
    GDALRasterBand::SetDescription(pszName);

    hDatasetID = hDatasetIDIn;

    const hid_t datatype = H5Dget_type(hDatasetIDIn);
    dataspace = H5Dget_space(hDatasetIDIn);
    const int n_dims = H5Sget_simple_extent_ndims(dataspace);
    native = H5Tget_native_type(datatype, H5T_DIR_ASCEND);

    eDataType = GH5_GetDataType(native);

    if( n_dims == 2 )
    {
        hsize_t dims[2] = {
            static_cast<hsize_t>(0),
            static_cast<hsize_t>(0)
        };
        hsize_t maxdims[2] = {
            static_cast<hsize_t>(0),
            static_cast<hsize_t>(0)
        };

        H5Sget_simple_extent_dims(dataspace, dims, maxdims);

        nRasterXSize = static_cast<int>(dims[1]);
        nRasterYSize = static_cast<int>(dims[0]);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Dataset not of rank 2.");
        return false;
    }

    nBlockXSize = nRasterXSize;
    nBlockYSize = 1;

    // Check for chunksize, and use it as blocksize for optimized reading.
    const hid_t listid = H5Dget_create_plist(hDatasetIDIn);
    if( listid > 0 )
    {
        if(H5Pget_layout(listid) == H5D_CHUNKED)
        {
            hsize_t panChunkDims[3] = {
              static_cast<hsize_t>(0),
              static_cast<hsize_t>(0),
              static_cast<hsize_t>(0)
            };
            const int nDimSize = H5Pget_chunk(listid, 3, panChunkDims);
            nBlockXSize = static_cast<int>(panChunkDims[nDimSize - 1]);
            nBlockYSize = static_cast<int>(panChunkDims[nDimSize - 2]);
        }

        H5D_fill_value_t fillType = H5D_FILL_VALUE_UNDEFINED;
        if( H5Pfill_value_defined(listid, &fillType) >= 0 &&
            fillType == H5D_FILL_VALUE_USER_DEFINED )
        {
            float fNoDataValue = 0.0f;
            if( H5Pget_fill_value(listid, H5T_NATIVE_FLOAT, &fNoDataValue) >= 0 )
            {
                m_bHasNoData = true;
                m_fNoDataValue = fNoDataValue;
            }
        }

        int nfilters = H5Pget_nfilters(listid);

        char name[120] = {};
        size_t cd_nelmts = 20;
        unsigned int cd_values[20] = {};
        unsigned int flags = 0;
        for( int i = 0; i < nfilters; i++ )
        {
            const H5Z_filter_t filter = H5Pget_filter(
                listid, i, &flags, &cd_nelmts, cd_values, sizeof(name), name);
            if( filter == H5Z_FILTER_DEFLATE )
                poDS->GDALDataset::SetMetadataItem("COMPRESSION", "DEFLATE",
                                      "IMAGE_STRUCTURE");
            else if( filter == H5Z_FILTER_NBIT )
                poDS->GDALDataset::SetMetadataItem("COMPRESSION", "NBIT",
                                                   "IMAGE_STRUCTURE");
            else if( filter == H5Z_FILTER_SCALEOFFSET )
                poDS->GDALDataset::SetMetadataItem("COMPRESSION", "SCALEOFFSET",
                                      "IMAGE_STRUCTURE");
            else if( filter == H5Z_FILTER_SZIP )
                poDS->GDALDataset::SetMetadataItem("COMPRESSION", "SZIP",
                                                   "IMAGE_STRUCTURE");
        }

        H5Pclose(listid);
    }

    // Load min/max information.
    if( EQUAL(pszName,"elevation") &&
        GH5_FetchAttribute(hDatasetIDIn, "Maximum Elevation Value",
                           dfMaximum) &&
        GH5_FetchAttribute(hDatasetIDIn, "Minimum Elevation Value", dfMinimum) )
    {
        bMinMaxSet = true;
    }
    else if( EQUAL(pszName, "uncertainty") &&
             GH5_FetchAttribute(hDatasetIDIn, "Maximum Uncertainty Value",
                                dfMaximum) &&
             GH5_FetchAttribute(hDatasetIDIn, "Minimum Uncertainty Value",
                                dfMinimum) )
    {
        // Some products where uncertainty band is completely set to nodata
        // wrongly declare minimum and maximum to 0.0.
        if( dfMinimum != 0.0 || dfMaximum != 0.0 )
            bMinMaxSet = true;
    }
    else if( EQUAL(pszName, "nominal_elevation") &&
             GH5_FetchAttribute(hDatasetIDIn, "max_value", dfMaximum) &&
             GH5_FetchAttribute(hDatasetIDIn, "min_value", dfMinimum) )
    {
        bMinMaxSet = true;
    }

    return true;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double BAGRasterBand::GetMinimum( int * pbSuccess )

{
    if( bMinMaxSet )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;
        return dfMinimum;
    }

    return GDALRasterBand::GetMinimum(pbSuccess);
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double BAGRasterBand::GetMaximum( int *pbSuccess )

{
    if( bMinMaxSet )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;
        return dfMaximum;
    }

    return GDALRasterBand::GetMaximum(pbSuccess);
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/
double BAGRasterBand::GetNoDataValue( int *pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = m_bHasNoData;
    if( m_bHasNoData )
        return m_fNoDataValue;

    return GDALPamRasterBand::GetNoDataValue(pbSuccess);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/
CPLErr BAGRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void *pImage )
{
    const int nXOff = nBlockXOff * nBlockXSize;
    H5OFFSET_TYPE offset[3] = {
        static_cast<H5OFFSET_TYPE>(
            std::max(0, nRasterYSize - (nBlockYOff + 1) * nBlockYSize)),
        static_cast<H5OFFSET_TYPE>(nXOff),
        static_cast<H5OFFSET_TYPE>(0)
    };

    const int nSizeOfData = static_cast<int>(H5Tget_size(native));
    memset(pImage, 0, nBlockXSize * nBlockYSize * nSizeOfData);

    //  Blocksize may not be a multiple of imagesize.
    hsize_t count[3] = {
        std::min(static_cast<hsize_t>(nBlockYSize), GetYSize() - offset[0]),
        std::min(static_cast<hsize_t>(nBlockXSize), GetXSize() - offset[1]),
        static_cast<hsize_t>(0)
    };

    if( nRasterYSize - (nBlockYOff + 1) * nBlockYSize < 0 )
    {
        count[0] += (nRasterYSize - (nBlockYOff + 1) * nBlockYSize);
    }

    // Select block from file space.
    {
        const herr_t status =
            H5Sselect_hyperslab(dataspace, H5S_SELECT_SET,
                                 offset, nullptr, count, nullptr);
        if( status < 0 )
            return CE_Failure;
    }

    // Create memory space to receive the data.
    hsize_t col_dims[3] = {
        static_cast<hsize_t>(nBlockYSize),
        static_cast<hsize_t>(nBlockXSize),
        static_cast<hsize_t>(0)
    };
    const int rank = 2;
    const hid_t memspace = H5Screate_simple(rank, col_dims, nullptr);
    H5OFFSET_TYPE mem_offset[3] = { 0, 0, 0 };
    const herr_t status =
        H5Sselect_hyperslab(memspace, H5S_SELECT_SET,
                            mem_offset, nullptr, count, nullptr);
    if( status < 0 )
    {
        H5Sclose(memspace);
        return CE_Failure;
    }

    const herr_t status_read =
        H5Dread(hDatasetID, native, memspace, dataspace, H5P_DEFAULT, pImage);

    H5Sclose(memspace);

    // Y flip the data.
    const int nLinesToFlip = static_cast<int>(count[0]);
    const int nLineSize = nSizeOfData * nBlockXSize;
    GByte * const pabyTemp = static_cast<GByte *>(CPLMalloc(nLineSize));
    GByte * const pbyImage = static_cast<GByte *>(pImage);

    for( int iY = 0; iY < nLinesToFlip / 2; iY++ )
    {
        memcpy(pabyTemp, pbyImage + iY * nLineSize, nLineSize);
        memcpy(pbyImage + iY * nLineSize,
               pbyImage + (nLinesToFlip - iY - 1) * nLineSize,
               nLineSize);
        memcpy(pbyImage + (nLinesToFlip - iY - 1) * nLineSize, pabyTemp,
               nLineSize);
    }

    CPLFree(pabyTemp);

    // Return success or failure.
    if( status_read < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "H5Dread() failed for block.");
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                           BAGSuperGridBand()                         */
/************************************************************************/

BAGSuperGridBand::BAGSuperGridBand( BAGDataset *poDSIn, int nBandIn,
                                    bool bHasNoData, float fNoDataValue )
{
    poDS = poDSIn;
    nBand = nBandIn;
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
    eDataType = GDT_Float32;
    GDALRasterBand::SetDescription( nBand == 1 ? "elevation" : "uncertainty" );
    m_bHasNoData = bHasNoData;
    m_fNoDataValue = fNoDataValue;
}

/************************************************************************/
/*                          ~BAGSuperGridBand()                         */
/************************************************************************/

BAGSuperGridBand::~BAGSuperGridBand() = default;

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr BAGSuperGridBand::IReadBlock( int, int nBlockYOff,
                                      void *pImage )
{
    BAGDataset* poGDS = cpl::down_cast<BAGDataset*>(poDS);
    H5OFFSET_TYPE offset[2] = {
        static_cast<H5OFFSET_TYPE>(0),
        static_cast<H5OFFSET_TYPE>(poGDS->m_nSuperGridRefinementStartIndex +
                        (nRasterYSize - 1 - nBlockYOff) * nBlockXSize)
    };
    hsize_t count[2] = {1, static_cast<hsize_t>(nBlockXSize)};
    {
        herr_t status = H5Sselect_hyperslab(
                                    poGDS->m_hVarresRefinementsDataspace,
                                    H5S_SELECT_SET,
                                    offset, nullptr,
                                    count, nullptr);
        if( status < 0 )
            return CE_Failure;
    }

    // Create memory space to receive the data.
    const hid_t memspace = H5Screate_simple(2, count, nullptr);
    H5OFFSET_TYPE mem_offset[2] = { 0, 0 };
    {
        const herr_t status =
            H5Sselect_hyperslab(memspace, H5S_SELECT_SET,
                                mem_offset, nullptr, count, nullptr);
        if( status < 0 )
        {
            H5Sclose(memspace);
            return CE_Failure;
        }
    }

    float* afBuffer = new float[2 * nBlockXSize];
    {
        const herr_t status =
            H5Dread(poGDS->m_hVarresRefinements,
                    poGDS->m_hVarresRefinementsNative,
                    memspace,
                    poGDS->m_hVarresRefinementsDataspace,
                    H5P_DEFAULT, afBuffer);
        if( status < 0 )
        {
            H5Sclose(memspace);
            delete[] afBuffer;
            return CE_Failure;
        }
    }

    GDALCopyWords(afBuffer + nBand - 1, GDT_Float32, 2 * sizeof(float),
                  pImage, GDT_Float32, sizeof(float),
                  nBlockXSize);

    H5Sclose(memspace);
    delete[] afBuffer;
    return CE_None;
}

/************************************************************************/
/*                           BAGResampledBand()                         */
/************************************************************************/

BAGResampledBand::BAGResampledBand( BAGDataset *poDSIn, int nBandIn,
                                    bool bHasNoData, float fNoDataValue,
                                    bool bInitializeMinMax )
{
    poDS = poDSIn;
    nBand = nBandIn;
    nRasterXSize = poDS->GetRasterXSize();
    nRasterYSize = poDS->GetRasterYSize();
    // Mostly for autotest purposes
    const int nBlockSize = std::max(1, atoi(
        CPLGetConfigOption("GDAL_BAG_BLOCK_SIZE", "256")));
    nBlockXSize = std::min(nBlockSize, poDS->GetRasterXSize());
    nBlockYSize = std::min(nBlockSize, poDS->GetRasterYSize());
    if( poDSIn->m_bMask )
    {
        eDataType = GDT_Byte;
    }
    else
    {
        m_bHasNoData = true;
        m_fNoDataValue = bHasNoData ? fNoDataValue : fDEFAULT_NODATA;
        m_fNoSuperGridValue = m_fNoDataValue;
        eDataType = GDT_Float32;
        GDALRasterBand::SetDescription( nBand == 1 ? "elevation" : "uncertainty" );
    }
    if( bInitializeMinMax )
    {
        InitializeMinMax();
    }
}

/************************************************************************/
/*                          ~BAGResampledBand()                         */
/************************************************************************/

BAGResampledBand::~BAGResampledBand() = default;

/************************************************************************/
/*                           InitializeMinMax()                         */
/************************************************************************/

void BAGResampledBand::InitializeMinMax()
{
    BAGDataset* poGDS = cpl::down_cast<BAGDataset*>(poDS);
    if( nBand == 1 &&
        GH5_FetchAttribute(poGDS->m_hVarresRefinements, "max_depth",
                           m_dfMaximum) &&
        GH5_FetchAttribute(poGDS->m_hVarresRefinements, "min_depth",
                           m_dfMinimum) )
    {
        m_bMinMaxSet = true;
    }
    else if( nBand == 2 &&
        GH5_FetchAttribute(poGDS->m_hVarresRefinements, "max_uncrt",
                           m_dfMaximum) &&
        GH5_FetchAttribute(poGDS->m_hVarresRefinements, "min_uncrt",
                           m_dfMinimum) )
    {
        m_bMinMaxSet = true;
    }
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double BAGResampledBand::GetMinimum( int * pbSuccess )

{
    if( m_bMinMaxSet )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;
        return m_dfMinimum;
    }

    return GDALRasterBand::GetMinimum(pbSuccess);
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double BAGResampledBand::GetMaximum( int *pbSuccess )

{
    if( m_bMinMaxSet )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;
        return m_dfMaximum;
    }

    return GDALRasterBand::GetMaximum(pbSuccess);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr BAGResampledBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                     void *pImage )
{
    BAGDataset* poGDS = cpl::down_cast<BAGDataset*>(poDS);
#ifdef DEBUG_VERBOSE
    CPLDebug("BAG", 
             "IReadBlock: nRasterXSize=%d, nBlockXOff=%d, nBlockYOff=%d, nBand=%d",
             nRasterXSize, nBlockXOff, nBlockYOff, nBand);
#endif

    const float fNoDataValue = m_fNoDataValue;
    const float fNoSuperGridValue = m_fNoSuperGridValue;
    float* depthsPtr = nullptr;
    float* uncrtPtr = nullptr;

    GDALRasterBlock* poBlock = nullptr;
    if( poGDS->nBands == 2 )
    {
        if( nBand == 1 )
        {
            depthsPtr = static_cast<float*>(pImage);
            poBlock = poGDS->GetRasterBand(2)->GetLockedBlockRef(
                nBlockXOff, nBlockYOff, TRUE);
            if( poBlock )
            {
                uncrtPtr = static_cast<float*>(poBlock->GetDataRef());
            }
            else
            {
                return CE_Failure;
            }
        }
        else
        {
            uncrtPtr = static_cast<float*>(pImage);
            poBlock = poGDS->GetRasterBand(1)->GetLockedBlockRef(
                nBlockXOff, nBlockYOff, TRUE);
            if( poBlock )
            {
                depthsPtr = static_cast<float*>(poBlock->GetDataRef());
            }
            else
            {
                return CE_Failure;
            }
        }
    }

    if( depthsPtr )
    {
        GDALCopyWords(&fNoSuperGridValue, GDT_Float32, 0,
                      depthsPtr, GDT_Float32, static_cast<int>(sizeof(float)),
                      nBlockXSize * nBlockYSize);
    }

    if( uncrtPtr )
    {
        GDALCopyWords(&fNoSuperGridValue, GDT_Float32, 0,
                      uncrtPtr, GDT_Float32, static_cast<int>(sizeof(float)),
                      nBlockXSize * nBlockYSize);
    }

    std::vector<int> counts;
    if( poGDS->m_bMask )
    {
        memset(pImage, 0, nBlockXSize * nBlockYSize);
    }
    else if( poGDS->m_ePopulation == BAGDataset::Population::MEAN )
    {
        counts.resize(nBlockXSize * nBlockYSize);
    }

    const int nReqCountX = std::min(nBlockXSize,
                              nRasterXSize - nBlockXOff * nBlockXSize);
    const int nReqCountY = std::min(nBlockYSize,
                              nRasterYSize - nBlockYOff * nBlockYSize);
    // Compute extent of block in georeferenced coordinates
    double dfBlockMinX = poGDS->adfGeoTransform[0] +
            nBlockXOff * nBlockXSize * poGDS->adfGeoTransform[1];
    double dfBlockMaxX = dfBlockMinX + nReqCountX * poGDS->adfGeoTransform[1];
    double dfBlockMaxY = poGDS->adfGeoTransform[3] +
            nBlockYOff * nBlockYSize * poGDS->adfGeoTransform[5];
    double dfBlockMinY = dfBlockMaxY + nReqCountY * poGDS->adfGeoTransform[5];

    // Compute min/max indices of intersecting supergrids (origin bottom-left)
    const double dfLowResResX = (poGDS->m_dfLowResMaxX -
                           poGDS->m_dfLowResMinX) / poGDS->m_nLowResWidth;
    const double dfLowResResY = (poGDS->m_dfLowResMaxY -
                           poGDS->m_dfLowResMinY) / poGDS->m_nLowResHeight;
    int nLowResMinIdxX = std::max(0,
        static_cast<int>((dfBlockMinX - poGDS->m_dfLowResMinX) / dfLowResResX));
    int nLowResMinIdxY = std::max(0,
        static_cast<int>((dfBlockMinY - poGDS->m_dfLowResMinY) / dfLowResResY));
    int nLowResMaxIdxX = std::min(poGDS->m_nLowResWidth - 1,
        static_cast<int>((dfBlockMaxX - poGDS->m_dfLowResMinX) / dfLowResResX));
    int nLowResMaxIdxY = std::min(poGDS->m_nLowResHeight - 1,
        static_cast<int>((dfBlockMaxY - poGDS->m_dfLowResMinY) / dfLowResResY));

    // Create memory space to receive the data.
    const int nCountLowResX = nLowResMaxIdxX-nLowResMinIdxX+1;
    const int nCountLowResY = nLowResMaxIdxY-nLowResMinIdxY+1;
    hsize_t countVarresMD[2] = {
        static_cast<hsize_t>(nCountLowResY),
        static_cast<hsize_t>(nCountLowResX) };
    const hid_t memspaceVarresMD = H5Screate_simple(2, countVarresMD, nullptr);
    H5OFFSET_TYPE mem_offset[2] = { static_cast<H5OFFSET_TYPE>(0),
                                    static_cast<H5OFFSET_TYPE>(0) };
    if( H5Sselect_hyperslab(memspaceVarresMD, H5S_SELECT_SET,
                            mem_offset, nullptr, countVarresMD, nullptr) < 0 )
    {
        H5Sclose(memspaceVarresMD);
        if( poBlock != nullptr )
        {
            poBlock->DropLock();
            poBlock = nullptr;
        }
        return CE_Failure;
    }

    std::vector<BAGRefinementGrid> rgrids(nCountLowResY * nCountLowResX);
    if( !(poGDS->ReadVarresMetadataValue(nLowResMinIdxY,
                                         nLowResMinIdxX,
                                         memspaceVarresMD,
                                         rgrids.data(),
                                         nCountLowResY, nCountLowResX)) )
    {
        H5Sclose(memspaceVarresMD);
        if( poBlock != nullptr )
        {
            poBlock->DropLock();
            poBlock = nullptr;
        }
        return CE_Failure;
    }

    H5Sclose(memspaceVarresMD);

    for( int y = nLowResMinIdxY; y <= nLowResMaxIdxY; y++ )
    {
        for( int x = nLowResMinIdxX; x <= nLowResMaxIdxX; x++ )
        {
            const auto& rgrid = rgrids[(y - nLowResMinIdxY) * nCountLowResX +
                                       (x - nLowResMinIdxX)];
            if( rgrid.nWidth == 0 )
            {
                continue;
            }
            const float gridRes = std::max(rgrid.fResX, rgrid.fResY);
            if( !(gridRes > poGDS->m_dfResFilterMin &&
                  gridRes <= poGDS->m_dfResFilterMax) )
            {
                continue;
            }

            // Super grid bounding box with pixel-center convention
            const double dfMinX =
                poGDS->m_dfLowResMinX + x * dfLowResResX + rgrid.fSWX;
            const double dfMaxX = dfMinX + (rgrid.nWidth - 1) * rgrid.fResX;
            const double dfMinY =
                poGDS->m_dfLowResMinY + y * dfLowResResY + rgrid.fSWY;
            const double dfMaxY = dfMinY + (rgrid.nHeight - 1) * rgrid.fResY;

            // Intersection of super grid with block
            const double dfInterMinX = std::max(dfBlockMinX, dfMinX);
            const double dfInterMinY = std::max(dfBlockMinY, dfMinY);
            const double dfInterMaxX = std::min(dfBlockMaxX, dfMaxX);
            const double dfInterMaxY = std::min(dfBlockMaxY, dfMaxY);

            // Min/max indices in the super grid
            const int nMinSrcX = std::max(0,
                static_cast<int>((dfInterMinX - dfMinX) / rgrid.fResX));
            const int nMinSrcY = std::max(0,
                static_cast<int>((dfInterMinY - dfMinY) / rgrid.fResY));
            const int nMaxSrcX = std::min(static_cast<int>(rgrid.nWidth) - 1,
                static_cast<int>((dfInterMaxX - dfMinX + 1e-8) / rgrid.fResX));
            const int nMaxSrcY = std::min(static_cast<int>(rgrid.nHeight) - 1,
                static_cast<int>((dfInterMaxY - dfMinY + 1e-8) / rgrid.fResY));
#ifdef DEBUG_VERBOSE
            CPLDebug("BAG",
                     "y = %d, x = %d, minx = %d, miny = %d, maxx = %d, maxy = %d",
                     y, x, nMinSrcX, nMinSrcY, nMaxSrcX, nMaxSrcY);
#endif
            const double dfCstX = (dfMinX - dfBlockMinX) / poGDS->adfGeoTransform[1];
            const double dfMulX = rgrid.fResX / poGDS->adfGeoTransform[1];

            for( int super_y = nMinSrcY; super_y <= nMaxSrcY; super_y++ )
            {
                const double dfSrcY = dfMinY + super_y * rgrid.fResY;
                const int nTargetY = static_cast<int>(std::floor(
                    (dfBlockMaxY - dfSrcY) / -poGDS->adfGeoTransform[5]));
                if( !(nTargetY >= 0 && nTargetY < nReqCountY) )
                {
                    continue;
                }

                const unsigned nTargetIdxBase = nTargetY * nBlockXSize;
                const unsigned nRefinementIndexase = rgrid.nIndex +
                        super_y * rgrid.nWidth;

                for( int super_x = nMinSrcX; super_x <= nMaxSrcX; super_x++ )
                {
                    /*
                    const double dfSrcX = dfMinX + super_x * rgrid.fResX;
                    const int nTargetX = static_cast<int>(std::floor(
                        (dfSrcX - dfBlockMinX) / poGDS->adfGeoTransform[1]));
                    */
                    const int nTargetX = static_cast<int>(
                        std::floor(dfCstX + super_x * dfMulX));
                    if( !(nTargetX >= 0 && nTargetX < nReqCountX) )
                    {
                        continue;
                    }

                    const unsigned nTargetIdx = nTargetIdxBase + nTargetX;
                    if( poGDS->m_bMask )
                    {
                        static_cast<GByte*>(pImage)[nTargetIdx] = 255;
                        continue;
                    }

                    CPLAssert(depthsPtr);
                    CPLAssert(uncrtPtr);

                    const unsigned nRefinementIndex =
                        nRefinementIndexase + super_x;
                    if( !poGDS->CacheRefinementValues(nRefinementIndex) )
                    {
                        H5Sclose(memspaceVarresMD);
                        return CE_Failure;
                    }

                    const unsigned nOffInArray = nRefinementIndex -
                                        poGDS->m_nCachedRefinementStartIndex;
                    float depth = poGDS->m_aCachedRefinementValues[2*nOffInArray];
                    if( depth == fNoDataValue )
                    {
                        if( depthsPtr[nTargetIdx] == fNoSuperGridValue )
                        {
                            depthsPtr[nTargetIdx] = fNoDataValue;
                        }
                        continue;
                    }

                    if( poGDS->m_ePopulation == BAGDataset::Population::MEAN )
                    {

                        if( counts[nTargetIdx] == 0 )
                        {
                            depthsPtr[nTargetIdx] = depth;
                        }
                        else
                        {
                            depthsPtr[nTargetIdx] += depth;
                        }
                        counts[nTargetIdx] ++;

                        auto uncrt =
                            poGDS->m_aCachedRefinementValues[2*nOffInArray+1];
                        auto& target_uncrt_ptr = uncrtPtr[nTargetIdx];
                        if( uncrt > target_uncrt_ptr ||
                            target_uncrt_ptr == fNoDataValue )
                        {
                            target_uncrt_ptr = uncrt;
                        }
                    }
                    else if(
                        (poGDS->m_ePopulation == BAGDataset::Population::MAX &&
                         depth > depthsPtr[nTargetIdx]) ||
                        (poGDS->m_ePopulation == BAGDataset::Population::MIN &&
                         depth < depthsPtr[nTargetIdx]) ||
                        depthsPtr[nTargetIdx] == fNoDataValue ||
                        depthsPtr[nTargetIdx] == fNoSuperGridValue )
                    {
                        depthsPtr[nTargetIdx] = depth;
                        uncrtPtr[nTargetIdx] =
                            poGDS->m_aCachedRefinementValues[2*nOffInArray+1];
                    }
                }
            }
        }
    }

    if( poGDS->m_ePopulation == BAGDataset::Population::MEAN && depthsPtr )
    {
        for( int i = 0; i < nBlockXSize * nBlockYSize; i++ )
        {
            if( counts[i] )
            {
                depthsPtr[i] /= counts[i];
            }
        }
    }

    if( poBlock != nullptr )
    {
        poBlock->DropLock();
        poBlock = nullptr;
    }

    return CE_None;
}

/************************************************************************/
/*                        BAGInterpolatedBand()                         */
/************************************************************************/

BAGInterpolatedBand::BAGInterpolatedBand( BAGDataset *poDSIn, int nBandIn)
{
    poDS = poDSIn;
    nBand = nBandIn;
    m_poUninterpolatedBand = cpl::down_cast<BAGResampledBand*>(
        poDSIn->m_poUninterpolatedDS->GetRasterBand(nBand));
    m_poUninterpolatedBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    eDataType = m_poUninterpolatedBand->GetRasterDataType();
    GDALRasterBand::SetDescription( m_poUninterpolatedBand->GetDescription() );
    m_poUninterpolatedBand->m_fNoSuperGridValue = std::numeric_limits<float>::infinity();
    m_fNoDataValue = m_poUninterpolatedBand->m_fNoDataValue;
    m_bHasNoData = m_poUninterpolatedBand->m_bHasNoData;
}

/************************************************************************/
/*                        ~BAGInterpolatedBand()                        */
/************************************************************************/

BAGInterpolatedBand::~BAGInterpolatedBand() = default;

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double BAGInterpolatedBand::GetMinimum( int * pbSuccess )

{
    return m_poUninterpolatedBand->GetMinimum(pbSuccess);
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double BAGInterpolatedBand::GetMaximum( int *pbSuccess )

{
    return m_poUninterpolatedBand->GetMaximum(pbSuccess);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

#ifdef OLD_MANUAL_INTERPOLATION_WHICH_CAN_BE_SLOW_WITH_BIG_RADIUS
CPLErr BAGInterpolatedBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                     void *pImage )
{
    BAGDataset* poGDS = cpl::down_cast<BAGDataset*>(poDS);

    GDALRasterBlock* poBlock =
        m_poUninterpolatedBand->GetLockedBlockRef(nBlockXOff, nBlockYOff);
    if( !poBlock )
    {
        return CE_Failure;
    }
    const float* pafSrcValues = static_cast<float*>(poBlock->GetDataRef());
    float* pafValues = static_cast<float*>(pImage);
    const int nReqCountX = std::min(nBlockXSize,
                              nRasterXSize - nBlockXOff * nBlockXSize);
    const int nReqCountY = std::min(nBlockYSize,
                              nRasterYSize - nBlockYOff * nBlockYSize);
    if( nReqCountX < nBlockXSize || nReqCountY < nBlockYSize )
    {
        GDALCopyWords(&poGDS->m_fNoDataValue, GDT_Float32, 0,
                    pImage, GDT_Float32, static_cast<int>(sizeof(float)),
                    nBlockXSize * nBlockYSize);
    }
    const int nXBlocks = (nRasterXSize + nBlockXSize - 1) / nBlockXSize;
    const int nYBlocks = (nRasterYSize + nBlockYSize - 1) / nBlockYSize;

    // Create memory space to receive the data.
    hsize_t countVarresMD[2] = { static_cast<hsize_t>(1), static_cast<hsize_t>(1)};
    const hid_t memspaceVarresMD = H5Screate_simple(2, countVarresMD, nullptr);
    H5OFFSET_TYPE mem_offset[2] = { static_cast<H5OFFSET_TYPE>(0),
                                    static_cast<H5OFFSET_TYPE>(0) };
    if( H5Sselect_hyperslab(memspaceVarresMD, H5S_SELECT_SET,
                            mem_offset, nullptr, countVarresMD, nullptr) < 0 )
    {
        H5Sclose(memspaceVarresMD);
        poBlock->DropLock();
        return CE_Failure;
    }
    const double dfLowResResX = (poGDS->m_dfLowResMaxX -
                    poGDS->m_dfLowResMinX) / poGDS->m_nLowResWidth;
    const double dfLowResResY = (poGDS->m_dfLowResMaxY -
                        poGDS->m_dfLowResMinY) / poGDS->m_nLowResHeight;

    int nLastLowResX = -1;
    int nLastLowResY = -1;
    bool bLastSupergridPresent = false;
    for(int y = 0; y < nReqCountY; y++ )
    {
        const double dfY = poGDS->adfGeoTransform[3] +
            (nBlockYOff * nBlockYSize + y + 0.5) * poGDS->adfGeoTransform[5];
        const int nLowResY = static_cast<int>((dfY - poGDS->m_dfLowResMinY) / dfLowResResY);

        for(int x = 0; x < nReqCountX; x++ )
        {
            if( pafSrcValues[y * nBlockXSize + x] !=
                            m_poUninterpolatedBand->m_fNoSuperGridValue )
            {
                pafValues[y * nBlockXSize + x] =
                    pafSrcValues[y * nBlockXSize + x];
                continue;
            }

            const double dfX = poGDS->adfGeoTransform[0] +
                (nBlockXOff * nBlockXSize + x + 0.5) * poGDS->adfGeoTransform[1];
            const int nLowResX = static_cast<int>((dfX - poGDS->m_dfLowResMinX) / dfLowResResX);
            if( nLowResX < 0 || nLowResY < 0 ||
                nLowResX >= poGDS->m_nLowResWidth ||
                nLowResY >= poGDS->m_nLowResHeight )
            {
                pafValues[y * nBlockXSize + x] = poGDS->m_fNoDataValue;
                continue;
            }
            if( nLowResX != nLastLowResX ||
                nLowResY != nLastLowResY )
            {
                BAGRefinementGrid rgrid;
                nLastLowResX = nLowResX;
                nLastLowResY = nLowResY;
                bLastSupergridPresent =
                    poGDS->ReadVarresMetadataValue(nLowResY, nLowResX,
                                                   memspaceVarresMD,
                                                   &rgrid, 1, 1) &&
                    rgrid.nWidth > 0;
            }
            if( !bLastSupergridPresent )
            {
                pafValues[y * nBlockXSize + x] = poGDS->m_fNoDataValue;
                continue;
            }


            bool bFoundOctant[8] = {};
            bool bStop = false;

#ifdef notdef
            bool bTrace = false;
            if( nBlockXOff * nBlockXSize + x == 3461 &&
                nBlockYOff * nBlockYSize + y == 4116 )
            {
                bTrace = true;
            }
#endif

            unsigned maxDistSquare = std::numeric_limits<unsigned>::max();
            for(unsigned radius = 1; radius <= 8; radius++ )
            {
                for( unsigned idx = 0; idx < 8 * radius; idx++ )
                {
                    int y2, x2;
                    if( idx < 2 * radius )
                    {
                        y2 = y - radius;
                        x2 = x - radius + idx;
                    }
                    else if( idx < 4U * radius )
                    {
                        x2 = x + radius;
                        y2 = y - radius + (idx - 2U * radius);
                    }
                    else if( idx < 6 * radius )
                    {
                        y2 = y + radius;
                        x2 = x - radius + (idx - 4U * radius);
                    }
                    else
                    {
                        x2 = x - radius;
                        y2 = y - radius + (idx - 6U * radius);
                    }

                    float fSrcValue;
                    if( x2 >= 0 && y2 >= 0 && x2 < nReqCountX &&
                        y2 < nReqCountY )
                    {
                        fSrcValue = pafSrcValues[
                            static_cast<unsigned>(y2) *
                                static_cast<unsigned>(nBlockXSize) +
                                    static_cast<unsigned>(x2)];
                    }
                    else
                    {
                        int nBlockXOff2 = nBlockXOff;
                        int nBlockYOff2 = nBlockYOff;
                        if( x2 < 0 )
                            nBlockXOff2--;
                        else if( x2 >= nReqCountX )
                            nBlockXOff2++;
                        if( y2 < 0 )
                            nBlockYOff2--;
                        else if( y2 >= nReqCountY )
                            nBlockYOff2++;
                        if( nBlockXOff2 >= 0 && nBlockYOff2 >= 0 &&
                            nBlockXOff2 < nXBlocks && nBlockYOff2 < nYBlocks )
                        {
                            GDALRasterBlock* poBlock2 =
                                m_poUninterpolatedBand->GetLockedBlockRef(
                                    nBlockXOff2, nBlockYOff2);
                            if( !poBlock2 )
                                continue;
                            const float* pafSrcValues2 =
                                static_cast<float*>(poBlock2->GetDataRef());
                            fSrcValue = pafSrcValues2[
                                (((y2 + nBlockYSize) % nBlockYSize) *
                                    nBlockXSize) +
                                        ((x2 + nBlockXSize) % nBlockXSize)];
                            poBlock2->DropLock();
                        }
                        else
                        {
                            continue;
                        }
                    }

                    if( fSrcValue == poGDS->m_fNoDataValue  )
                    {
                        bStop = true;
                        pafValues[y * nBlockXSize + x] = poGDS->m_fNoDataValue;
                        break;
                    }
                    if( fSrcValue !=
                            m_poUninterpolatedBand->m_fNoSuperGridValue )
                    {
                        int iOctant;
                        unsigned diffXAbs;
                        unsigned diffYAbs;
                        if( y2 < y )
                        {
                            diffYAbs = y - y2;
                            if( x2 < x )
                            {
                                diffXAbs = x - x2;
                                iOctant = 0;
                            }
                            else if( x2 == x )
                            {
                                diffXAbs = 0;
                                iOctant = 1;
                            }
                            else
                            {
                                diffXAbs = x2 - x;
                                iOctant = 2;
                            }
                        }
                        else if( y2 == y )
                        {
                            diffYAbs = 0;
                            if( x2 < x )
                            {
                                diffXAbs = x - x2;
                                iOctant = 7;
                            }
                            else
                            {
                                diffXAbs = x2 - x;
                                iOctant = 3;
                            }
                        }
                        else
                        {
                            diffYAbs = y2 - y;
                            if( x2 < x )
                            {
                                diffXAbs = x - x2;
                                iOctant = 6;
                            }
                            else if( x2 == x )
                            {
                                diffXAbs = 0;
                                iOctant = 5;
                            }
                            else
                            {
                                diffXAbs = x2 - x;
                                iOctant = 4;
                            }
                        }

                        bFoundOctant[iOctant] = true;
                        unsigned distSquare = diffXAbs * diffXAbs +
                                                diffYAbs * diffYAbs;
#ifdef notdef
                        if( bTrace )
                        {
                            fprintf(stderr, "%d,%d -> %d, %u, %f\n", /* ok */
                                    y2, x2, iOctant, distSquare, fSrcValue);
                        }
#endif
                        if( distSquare < maxDistSquare )
                        {
                            pafValues[y * nBlockXSize + x] = fSrcValue;
                            maxDistSquare = distSquare;
                        }

                        if( (bFoundOctant[0] && bFoundOctant[4]) ||
                            (bFoundOctant[1] && bFoundOctant[5]) ||
                            (bFoundOctant[2] && bFoundOctant[6]) ||
                            (bFoundOctant[3] && bFoundOctant[7]) )
                        {
                            bStop = true;
                            break;
                        }
                    }
                }
                if( bStop )
                {
                    break;
                }
            }
            if( !bStop )
            {
                pafValues[y * nBlockXSize + x] = poGDS->m_fNoDataValue;
            }
        }
    }
    poBlock->DropLock();
    H5Sclose(memspaceVarresMD);
    return CE_None;
}
#endif // OLD_MANUAL_INTERPOLATION_WHICH_CAN_BE_SLOW_WITH_BIG_RADIUS


CPLErr BAGInterpolatedBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                        void *pImage )
{
    BAGDataset* poGDS = cpl::down_cast<BAGDataset*>(poDS);
    GDALDriverH hMemDriver = GDALGetDriverByName("MEM");
    if( !hMemDriver )
        return CE_Failure;

    const int nReqCountX = std::min(nBlockXSize,
                              nRasterXSize - nBlockXOff * nBlockXSize);
    const int nReqCountY = std::min(nBlockYSize,
                              nRasterYSize - nBlockYOff * nBlockYSize);
    const int nXBlocks = (nRasterXSize + nBlockXSize - 1) / nBlockXSize;
    const int nYBlocks = (nRasterYSize + nBlockYSize - 1) / nBlockYSize;

    // This should not be larger than the block dimension, otherwise we
    // would need to fetch more neighbour blocks.
    const int nSearchDist = 64;

    // Instantiate a temporary buffer that contains the target block, and
    // its neighbour blocks.
    int nBufferXSize = nBlockXSize;
    int nBufferXInterestOff = 0;
    if( nBlockXOff > 0 )
    {
        nBufferXInterestOff = nBlockXSize;
        nBufferXSize += nBlockXSize;
    }
    if( nBlockXOff + 1 < nXBlocks )
    {
        nBufferXSize += nBlockXSize;
    }

    int nBufferYSize = nBlockYSize;
    int nBufferYInterestOff = 0;
    if( nBlockYOff > 0 )
    {
        nBufferYInterestOff = nBlockYSize;
        nBufferYSize += nBlockYSize;
    }
    if( nBlockYOff + 1 < nYBlocks )
    {
        nBufferYSize += nBlockYSize;
    }

    std::vector<float> afBuffer(nBufferXSize * nBufferYSize);

    // Create a in-memory dataset wrapping afBuffer
    GDALDataset* poDSToInterpolate =
        reinterpret_cast<GDALDriver*>(hMemDriver)->Create(
            "band_to_interpolate", nBufferXSize, nBufferYSize,
            0, GDT_Float32, nullptr);
    {
        char szBuffer[32] = { '\0' };
        int nRet =
            CPLPrintPointer(szBuffer, afBuffer.data(), sizeof(szBuffer));
        szBuffer[nRet] = '\0';
        char szBuffer0[64] = { '\0' };
        snprintf(szBuffer0, sizeof(szBuffer0), "DATAPOINTER=%s", szBuffer);
        char* apszOptions[2] = { szBuffer0, nullptr };
        poDSToInterpolate->AddBand(GDT_Float32, apszOptions);
    }

    // Create memory space to receive the data.
    hsize_t countVarresMD[2] = { static_cast<hsize_t>(1), static_cast<hsize_t>(1)};
    const hid_t memspaceVarresMD = H5Screate_simple(2, countVarresMD, nullptr);
    H5OFFSET_TYPE mem_offset[2] = { static_cast<H5OFFSET_TYPE>(0),
                                    static_cast<H5OFFSET_TYPE>(0) };
    if( H5Sselect_hyperslab(memspaceVarresMD, H5S_SELECT_SET,
                            mem_offset, nullptr, countVarresMD, nullptr) < 0 )
    {
        H5Sclose(memspaceVarresMD);
        return CE_Failure;
    }

    const double dfLowResResX = (poGDS->m_dfLowResMaxX -
                    poGDS->m_dfLowResMinX) / poGDS->m_nLowResWidth;
    const double dfLowResResY = (poGDS->m_dfLowResMaxY -
                    poGDS->m_dfLowResMinY) / poGDS->m_nLowResHeight;

    int nLastLowResX = -1;
    int nLastLowResY = -1;
    bool bLastSupergridPresent = false;
    std::vector<GByte> abyMask(nBufferXSize * nBufferYSize, 1);

    // Loop over blocks in Y direction
    for(int iY = ((nBlockYOff > 0) ? -1 : 0),
            nOutYOffset = 0;
            iY <= ((nBlockYOff + 1 < nYBlocks) ? 1: 0);
            iY++,
            nOutYOffset += nBlockYSize )
    {
        int ymin = 0;
        int ymax = nBlockYSize;
        if( iY == 0 )
        {
            ymax = nReqCountY;
        }
        else if( iY == -1 )
        {
            ymin = std::max(ymin, nBlockYSize - nSearchDist);
        }
        else if( iY == 1 )
        {
            ymax = std::min(ymax, nSearchDist);
        }

        // Loop over blocks in X direction
        for(int iX = ((nBlockXOff > 0) ? -1 : 0),
                nOutXOffset = 0;
                iX <= ((nBlockXOff + 1 < nXBlocks) ? 1: 0);
                iX++,
                nOutXOffset += nBlockXSize )
        {
            int xmin = 0;
            int xmax = nBlockXSize;
            if( iX == 0 )
            {
                xmax = nReqCountX;
            }
            else if( iX == -1 )
            {
                xmin = std::max(xmin, nBlockXSize - nSearchDist);
            }
            else if( iX == 1 )
            {
                xmax = std::min(xmax, nSearchDist);
            }

            // Copy block data into afBuffer
            GDALRasterBlock* poBlock =
                m_poUninterpolatedBand->GetLockedBlockRef(
                    nBlockXOff + iX, nBlockYOff + iY);
            if( !poBlock )
            {
                H5Sclose(memspaceVarresMD);
                return CE_Failure;
            }

            const float* pafSrcValues = static_cast<float*>(poBlock->GetDataRef());
            for( int y = 0; y < nBlockYSize; y++ )
            {
                float* pafDstValues = afBuffer.data() +
                            nBufferXSize * nOutYOffset +
                            nOutXOffset + y * nBufferXSize;
                memcpy(pafDstValues,
                       pafSrcValues + y * nBlockXSize,
                       sizeof(float) * nBlockXSize);

                // Replace m_fNoSuperGridValue by m_fNoDataValue
                // Note: we could likely just make them equal in the first place.
                for( int x = 0; x < nBlockXSize; x++ )
                {
                    if( pafDstValues[x] ==
                            m_poUninterpolatedBand->m_fNoSuperGridValue )
                    {
                        pafDstValues[x] = m_fNoDataValue;
                    }
                }
            }
            poBlock->DropLock();

            for( int y = ymin; y < ymax; y++ )
            {
                const double dfY = poGDS->adfGeoTransform[3] +
                    ((nBlockYOff + iY) * nBlockYSize + y + 0.5) *
                        poGDS->adfGeoTransform[5];
                const int nLowResY = static_cast<int>(
                    (dfY - poGDS->m_dfLowResMinY) / dfLowResResY);

                for( int x = xmin; x < xmax; x++ )
                {
                    const int nMaskOffset = (y + nOutYOffset) * nBufferXSize +
                                            (x + nOutXOffset);

                    // Figure out if the target pixel is within a supergrid
                    // If not, we should not interpolate it
                    const double dfX = poGDS->adfGeoTransform[0] +
                        ((nBlockXOff + iX) * nBlockXSize + x + 0.5) *
                            poGDS->adfGeoTransform[1];
                    const int nLowResX = static_cast<int>(
                        (dfX - poGDS->m_dfLowResMinX) / dfLowResResX);
                    if( nLowResX < 0 || nLowResY < 0 ||
                        nLowResX >= poGDS->m_nLowResWidth ||
                        nLowResY >= poGDS->m_nLowResHeight )
                    {
                        afBuffer[nMaskOffset] = m_fNoDataValue;
                        continue;
                    }
                    if( nLowResX != nLastLowResX ||
                        nLowResY != nLastLowResY )
                    {
                        BAGRefinementGrid rgrid;
                        nLastLowResX = nLowResX;
                        nLastLowResY = nLowResY;
                        bLastSupergridPresent = 
                            poGDS->ReadVarresMetadataValue(nLowResY, nLowResX,
                                                        memspaceVarresMD,
                                                        &rgrid, 1, 1) &&
                            rgrid.nWidth > 0;
                        if( bLastSupergridPresent )
                        {
                            const float gridRes = std::max(rgrid.fResX,
                                                           rgrid.fResY);
                            bLastSupergridPresent =
                                (gridRes > poGDS->m_dfResFilterMin &&
                                gridRes <= poGDS->m_dfResFilterMax);
                        }
                    }
                    if( !bLastSupergridPresent )
                    {
                        afBuffer[nMaskOffset] = m_fNoDataValue;
                        continue;
                    }

                    // If the target pixel is nodata, interpolate it
                    if( afBuffer[nMaskOffset] == m_fNoDataValue )
                    {
                        abyMask[nMaskOffset] = 0;
                    }
                }
            }
        }
    }

    H5Sclose(memspaceVarresMD);

    // Create a in-memory dataset wrapping abyMask
    GDALDataset* poDSMask =
        reinterpret_cast<GDALDriver*>(hMemDriver)->Create(
            "mask", nBufferXSize, nBufferYSize,
            0, GDT_Byte, nullptr);
    {
        char szBuffer[32] = { '\0' };
        int nRet =
            CPLPrintPointer(szBuffer, abyMask.data(), sizeof(szBuffer));
        szBuffer[nRet] = '\0';
        char szBuffer0[64] = { '\0' };
        snprintf(szBuffer0, sizeof(szBuffer0), "DATAPOINTER=%s", szBuffer);
        char* apszOptions[2] = { szBuffer0, nullptr };
        poDSMask->AddBand(GDT_Byte, apszOptions);
    }

    // Run GDALFillNodata() to do the actual interpolation
    {
        const double dfMaxSearchDist = nSearchDist;
        const int nSmoothingIterations = 0;
        char** papszOptions = CSLSetNameValue(nullptr, "TEMP_FILE_DRIVER", "MEM");
        papszOptions = CSLSetNameValue(papszOptions, "NODATA",
                    CPLSPrintf("%.9g", m_fNoDataValue));
        GDALFillNodata(
            GDALRasterBand::ToHandle(poDSToInterpolate->GetRasterBand(1)),
            GDALRasterBand::ToHandle(poDSMask->GetRasterBand(1)),
            dfMaxSearchDist,
            false, /* unused */
            nSmoothingIterations,
            papszOptions,
            nullptr, nullptr /* progress */);
        CSLDestroy(papszOptions);
    }

    // Cleanup temporary in-memory datasets
    delete poDSToInterpolate;
    delete poDSMask;

    // Recopy are of interest from temporary buffer to final buffer
    float* pafValues = static_cast<float*>(pImage);
    for(int iY = 0; iY < nBlockYSize; iY++)
    {
        memcpy(pafValues + iY * nBlockXSize,
               afBuffer.data() +
                (iY + nBufferYInterestOff) * nBufferXSize + nBufferXInterestOff,
               nBlockXSize * sizeof(float));
    }

    return CE_None;
}



/************************************************************************/
/* ==================================================================== */
/*                              BAGDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             BAGDataset()                             */
/************************************************************************/

BAGDataset::BAGDataset() = default;

BAGDataset::BAGDataset(BAGDataset* poParentDS, int nOvrFactor)
{
    m_ePopulation = poParentDS->m_ePopulation;
    m_bMask = poParentDS->m_bMask;
    m_bIsChild = true;
    // m_apoOverviewDS
    hHDF5 = poParentDS->hHDF5;
    pszProjection = poParentDS->pszProjection;
    nRasterXSize = poParentDS->nRasterXSize / nOvrFactor;
    nRasterYSize = poParentDS->nRasterYSize / nOvrFactor;
    adfGeoTransform[0] = poParentDS->adfGeoTransform[0];
    adfGeoTransform[1] = poParentDS->adfGeoTransform[1] *
        poParentDS->nRasterXSize / nRasterXSize;
    adfGeoTransform[2] = poParentDS->adfGeoTransform[2];
    adfGeoTransform[3] = poParentDS->adfGeoTransform[3];
    adfGeoTransform[4] = poParentDS->adfGeoTransform[4];
    adfGeoTransform[5] = poParentDS->adfGeoTransform[5] *
        poParentDS->nRasterYSize / nRasterYSize;
    m_nLowResWidth = poParentDS->m_nLowResWidth;
    m_nLowResHeight = poParentDS->m_nLowResHeight;
    m_dfLowResMinX = poParentDS->m_dfLowResMinX;
    m_dfLowResMinY = poParentDS->m_dfLowResMinY;
    m_dfLowResMaxX = poParentDS->m_dfLowResMaxX;
    m_dfLowResMaxY = poParentDS->m_dfLowResMaxY;
    //char        *pszXMLMetadata = nullptr;
    //char        *apszMDList[2]{};
    m_nChunkXSizeVarresMD = poParentDS->m_nChunkXSizeVarresMD;
    m_nChunkYSizeVarresMD = poParentDS->m_nChunkYSizeVarresMD;
    m_nChunkSizeVarresRefinement = poParentDS->m_nChunkSizeVarresRefinement;

    m_hVarresMetadata = poParentDS->m_hVarresMetadata;
    m_hVarresMetadataDataType = poParentDS->m_hVarresMetadataDataType;
    m_hVarresMetadataDataspace = poParentDS->m_hVarresMetadataDataspace;
    m_hVarresMetadataNative = poParentDS->m_hVarresMetadataNative;
    //m_aoRefinemendGrids;

    //m_aosSubdatasets;

    m_hVarresRefinements = poParentDS->m_hVarresRefinements;
    m_hVarresRefinementsDataType = poParentDS->m_hVarresRefinementsDataType;
    m_hVarresRefinementsDataspace = poParentDS->m_hVarresRefinementsDataspace;
    m_hVarresRefinementsNative = poParentDS->m_hVarresRefinementsNative;
    m_nRefinementsSize = poParentDS->m_nRefinementsSize;

    m_nSuperGridRefinementStartIndex = poParentDS->m_nSuperGridRefinementStartIndex;
    m_dfResFilterMin = poParentDS->m_dfResFilterMin;
    m_dfResFilterMax = poParentDS->m_dfResFilterMax;

    if( poParentDS->GetRasterCount() > 1 )
    {
        GDALDataset::SetMetadataItem("INTERLEAVE", "PIXEL",
                                     "IMAGE_STRUCTURE");
    }
}

/************************************************************************/
/*                            ~BAGDataset()                             */
/************************************************************************/
BAGDataset::~BAGDataset()
{
    FlushCache();

    m_poUninterpolatedDS.reset();
    m_apoOverviewDS.clear();
    if( !m_bIsChild )
    {
        if( m_hVarresMetadataDataType >= 0 )
            H5Tclose(m_hVarresMetadataDataType);

        if( m_hVarresMetadataDataspace >= 0 )
            H5Sclose(m_hVarresMetadataDataspace);

        if( m_hVarresMetadataNative >= 0 )
            H5Tclose(m_hVarresMetadataNative);

        if( m_hVarresMetadata >= 0 )
            H5Dclose(m_hVarresMetadata);

        if( m_hVarresRefinementsDataType >= 0 )
            H5Tclose(m_hVarresRefinementsDataType);

        if( m_hVarresRefinementsDataspace >= 0 )
            H5Sclose(m_hVarresRefinementsDataspace);

        if( m_hVarresRefinementsNative >= 0 )
            H5Tclose(m_hVarresRefinementsNative);

        if( m_hVarresRefinements >= 0 )
            H5Dclose(m_hVarresRefinements);

        if( hHDF5 >= 0 )
            H5Fclose(hHDF5);

        CPLFree(pszProjection);
        CPLFree(pszXMLMetadata);
    }
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int BAGDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( STARTS_WITH(poOpenInfo->pszFilename, "BAG:") )
        return TRUE;

    // Is it an HDF5 file?
    static const char achSignature[] = "\211HDF\r\n\032\n";

    if( poOpenInfo->pabyHeader == nullptr ||
        memcmp(poOpenInfo->pabyHeader, achSignature, 8) != 0 )
        return FALSE;

    // Does it have the extension .bag?
    if( !EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "bag") )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                          GH5DopenNoWarning()                         */
/************************************************************************/

static hid_t GH5DopenNoWarning(hid_t hHDF5, const char* pszDatasetName)
{
    hid_t hDataset;
#ifdef HAVE_GCC_WARNING_ZERO_AS_NULL_POINTER_CONSTANT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif
    H5E_BEGIN_TRY {
        hDataset = H5Dopen(hHDF5, pszDatasetName);
    } H5E_END_TRY;

#ifdef HAVE_GCC_WARNING_ZERO_AS_NULL_POINTER_CONSTANT
#pragma GCC diagnostic pop
#endif
    return hDataset;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *BAGDataset::Open( GDALOpenInfo *poOpenInfo )

{
    // Confirm that this appears to be a BAG file.
    if( !Identify(poOpenInfo) )
        return nullptr;

    // Confirm the requested access is supported.
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The BAG driver does not support update access.");
        return nullptr;
    }

    const char* pszMode = CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                                               "MODE", "AUTO");
    const bool bLowResGrid = EQUAL(pszMode, "LOW_RES_GRID");
    if( bLowResGrid &&
        (   CSLFetchNameValue(poOpenInfo->papszOpenOptions, "MINX") != nullptr ||
            CSLFetchNameValue(poOpenInfo->papszOpenOptions, "MINY") != nullptr ||
            CSLFetchNameValue(poOpenInfo->papszOpenOptions, "MAXX") != nullptr ||
            CSLFetchNameValue(poOpenInfo->papszOpenOptions, "MAXY") != nullptr ||
            CSLFetchNameValue(poOpenInfo->papszOpenOptions, "SUPERGRIDS_INDICES") != nullptr ) )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                    "Open options MINX/MINY/MAXX/MAXY/SUPERGRIDS_INDICES are "
                    "ignored when opening the low resolution grid");
    }

    const bool bListSubDS = !bLowResGrid && (EQUAL(pszMode, "LIST_SUPERGRIDS") ||
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "MINX") != nullptr ||
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "MINY") != nullptr ||
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "MAXX") != nullptr ||
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "MAXY") != nullptr ||
        CSLFetchNameValue(poOpenInfo->papszOpenOptions, "SUPERGRIDS_INDICES") != nullptr);
    const bool bResampledGrid = EQUAL(pszMode, "RESAMPLED_GRID");

    bool bOpenSuperGrid = false;
    int nX = -1;
    int nY = -1;
    CPLString osFilename(poOpenInfo->pszFilename);
    if( STARTS_WITH(poOpenInfo->pszFilename, "BAG:") )
    {
        char **papszTokens =
            CSLTokenizeString2(poOpenInfo->pszFilename, ":",
                            CSLT_HONOURSTRINGS | CSLT_PRESERVEESCAPES);

        if( CSLCount(papszTokens) != 5 )
        {
            CSLDestroy(papszTokens);
            return nullptr;
        }
        bOpenSuperGrid = true;
        osFilename = papszTokens[1];
        nY = atoi(papszTokens[3]);
        nX = atoi(papszTokens[4]);
        CSLDestroy(papszTokens);

        if( CSLFetchNameValue(poOpenInfo->papszOpenOptions, "MINX") != nullptr ||
            CSLFetchNameValue(poOpenInfo->papszOpenOptions, "MINY") != nullptr ||
            CSLFetchNameValue(poOpenInfo->papszOpenOptions, "MAXX") != nullptr ||
            CSLFetchNameValue(poOpenInfo->papszOpenOptions, "MAXY") != nullptr ||
            CSLFetchNameValue(poOpenInfo->papszOpenOptions, "SUPERGRIDS_INDICES") != nullptr )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Open options MINX/MINY/MAXX/MAXY/SUPERGRIDS_INDICES are "
                     "ignored when opening a supergrid");
        }
    }

    // Open the file as an HDF5 file.
    hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
    H5Pset_driver(fapl, HDF5GetFileDriver(), nullptr);
    hid_t hHDF5 = H5Fopen(osFilename, H5F_ACC_RDONLY, fapl);
    H5Pclose(fapl);
    if( hHDF5 < 0 )
        return nullptr;

    // Confirm it is a BAG dataset by checking for the
    // BAG_root/Bag Version attribute.
    const hid_t hBagRoot = H5Gopen(hHDF5, "/BAG_root");
    const hid_t hVersion =
        hBagRoot >= 0 ? H5Aopen_name(hBagRoot, "Bag Version") : -1;

    if( hVersion < 0 )
    {
        if( hBagRoot >= 0 )
            H5Gclose(hBagRoot);
        H5Fclose(hHDF5);
        return nullptr;
    }
    H5Aclose(hVersion);

    // Create a corresponding dataset.
    BAGDataset *const poDS = new BAGDataset();

    poDS->hHDF5 = hHDF5;

    const char* pszNoDataValue = CSLFetchNameValue(
        poOpenInfo->papszOpenOptions, "NODATA_VALUE");
    bool bHasNoData = pszNoDataValue != nullptr;
    float fNoDataValue = bHasNoData ? static_cast<float>(CPLAtof(pszNoDataValue)) : 0.0f;

    // Extract version as metadata.
    CPLString osVersion;

    if( GH5_FetchAttribute(hBagRoot, "Bag Version", osVersion) )
        poDS->GDALDataset::SetMetadataItem("BagVersion", osVersion);

    H5Gclose(hBagRoot);

    // Fetch the elevation dataset and attach as a band.
    int nNextBand = 1;
    const hid_t hElevation = H5Dopen(hHDF5, "/BAG_root/elevation");
    if( hElevation < 0 )
    {
        delete poDS;
        return nullptr;
    }

    BAGRasterBand *poElevBand = new BAGRasterBand(poDS, nNextBand);

    if( !poElevBand->Initialize(hElevation, "elevation") )
    {
        delete poElevBand;
        delete poDS;
        return nullptr;
    }

    poDS->m_nLowResWidth = poElevBand->nRasterXSize;
    poDS->m_nLowResHeight = poElevBand->nRasterYSize;

    if( bOpenSuperGrid || bListSubDS || bResampledGrid )
    {
        if( !bHasNoData )
        {
            int nHasNoData = FALSE;
            double dfNoData = poElevBand->GetNoDataValue(&nHasNoData);
            if( nHasNoData )
            {
                bHasNoData = true;
                fNoDataValue = static_cast<float>(dfNoData);
            }
        }
        delete poElevBand;
        poDS->nRasterXSize = 0;
        poDS->nRasterYSize = 0;
    }
    else
    {
        poDS->nRasterXSize = poElevBand->nRasterXSize;
        poDS->nRasterYSize = poElevBand->nRasterYSize;

        poDS->SetBand(nNextBand++, poElevBand);

        // Try to do the same for the uncertainty band.
        const hid_t hUncertainty = H5Dopen(hHDF5, "/BAG_root/uncertainty");
        BAGRasterBand *poUBand = new BAGRasterBand(poDS, nNextBand);

        if( hUncertainty >= 0 && poUBand->Initialize(hUncertainty, "uncertainty") )
        {
            poDS->SetBand(nNextBand++, poUBand);
        }
        else
        {
            delete poUBand;
        }

        // Try to do the same for the nominal_elevation band.
        hid_t hNominal = GH5DopenNoWarning(hHDF5, "/BAG_root/nominal_elevation");
        BAGRasterBand *const poNBand = new BAGRasterBand(poDS, nNextBand);
        if( hNominal >= 0 && poNBand->Initialize(hNominal, "nominal_elevation") )
        {
            poDS->SetBand(nNextBand++, poNBand);
        }
        else
        {
            delete poNBand;
        }
    }

    poDS->SetDescription(poOpenInfo->pszFilename);

    // Load the XML metadata.
    poDS->LoadMetadata();

    if( bResampledGrid )
    {
        poDS->m_bMask = CPLTestBool(CSLFetchNameValueDef(
            poOpenInfo->papszOpenOptions, "SUPERGRIDS_MASK", "NO"));
    }

    if( !poDS->m_bMask )
    {
        poDS->GDALDataset::SetMetadataItem(GDALMD_AREA_OR_POINT, GDALMD_AOP_POINT);
    }

    // Load for refinements grids for variable resolution datasets
    bool bHasRefinementGrids = false;
    if( bOpenSuperGrid || bListSubDS || bResampledGrid )
    {
        bHasRefinementGrids = poDS->LookForRefinementGrids(
            poOpenInfo->papszOpenOptions, nY, nX);
        if( !bOpenSuperGrid && poDS->m_aosSubdatasets.size() == 2 &&
            EQUAL(pszMode, "AUTO") )
        {
            CPLString osSubDsName = CSLFetchNameValueDef(
                            poDS->m_aosSubdatasets, "SUBDATASET_1_NAME", "");
            delete poDS;
            GDALOpenInfo oOpenInfo(osSubDsName, GA_ReadOnly);
            return Open(&oOpenInfo);
        }
    }
    else
    {
        if( poDS->LookForRefinementGrids(
                poOpenInfo->papszOpenOptions, 0, 0) )
        {
            poDS->GDALDataset::SetMetadataItem("HAS_SUPERGRIDS", "TRUE");
        }
        poDS->m_aosSubdatasets.Clear();
    }

    double dfMinResX = 0.0;
    double dfMinResY = 0.0;
    double dfMaxResX = 0.0;
    double dfMaxResY = 0.0;
    if( poDS->m_hVarresMetadata >= 0 )
    {
        if( !GH5_FetchAttribute( poDS->m_hVarresMetadata,
                                    "min_resolution_x", dfMinResX ) ||
            !GH5_FetchAttribute( poDS->m_hVarresMetadata,
                                    "min_resolution_y", dfMinResY ) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot get min_resolution_x and/or min_resolution_y");
            delete poDS;
            return nullptr;
        }

        if( !GH5_FetchAttribute( poDS->m_hVarresMetadata,
                                    "max_resolution_x", dfMaxResX ) ||
            !GH5_FetchAttribute( poDS->m_hVarresMetadata,
                                    "max_resolution_y", dfMaxResY ) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot get max_resolution_x and/or max_resolution_y");
            delete poDS;
            return nullptr;
        }

        if( !bOpenSuperGrid && !bResampledGrid )
        {
            poDS->GDALDataset::SetMetadataItem("MIN_RESOLUTION_X",
                                               CPLSPrintf("%f", dfMinResX));
            poDS->GDALDataset::SetMetadataItem("MIN_RESOLUTION_Y",
                                               CPLSPrintf("%f", dfMinResY));
            poDS->GDALDataset::SetMetadataItem("MAX_RESOLUTION_X",
                                               CPLSPrintf("%f", dfMaxResX));
            poDS->GDALDataset::SetMetadataItem("MAX_RESOLUTION_Y",
                                               CPLSPrintf("%f", dfMaxResY));
        }
    }

    if( bResampledGrid )
    {
        if( !bHasRefinementGrids )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "No supergrids available. "
                     "RESAMPLED_GRID mode not available");
            delete poDS;
            return nullptr;
        }

        const char* pszValuePopStrategy = CSLFetchNameValueDef(
                    poOpenInfo->papszOpenOptions, "VALUE_POPULATION", "MAX");
        if( EQUAL(pszValuePopStrategy, "MIN") )
        {
            poDS->m_ePopulation = BAGDataset::Population::MIN;
        }
        else if( EQUAL(pszValuePopStrategy, "MEAN") )
        {
            poDS->m_ePopulation = BAGDataset::Population::MEAN;
        }
        else
        {
            poDS->m_ePopulation = BAGDataset::Population::MAX;
        }

        const char* pszResX = CSLFetchNameValue(poOpenInfo->papszOpenOptions, "RESX");
        const char* pszResY = CSLFetchNameValue(poOpenInfo->papszOpenOptions, "RESY");
        const char* pszResStrategy = CSLFetchNameValueDef(
                    poOpenInfo->papszOpenOptions, "RES_STRATEGY", "AUTO");
        double dfDefaultResX = 0.0;
        double dfDefaultResY = 0.0;

        const char* pszResFilterMin = CSLFetchNameValue(
            poOpenInfo->papszOpenOptions, "RES_FILTER_MIN");
        const char* pszResFilterMax = CSLFetchNameValue(
            poOpenInfo->papszOpenOptions, "RES_FILTER_MAX");

        double dfResFilterMin = 0;
        if( pszResFilterMin != nullptr )
        {
            dfResFilterMin = CPLAtof(pszResFilterMin);
            const double dfMaxRes = std::min(dfMaxResX, dfMaxResY);
            if( dfResFilterMin >= dfMaxRes )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot specified RES_FILTER_MIN >= %g",
                         dfMaxRes);
                delete poDS;
                return nullptr;
            }
            poDS->GDALDataset::SetMetadataItem("RES_FILTER_MIN",
                                            CPLSPrintf("%g", dfResFilterMin));
        }

        double dfResFilterMax = std::numeric_limits<double>::infinity();
        if( pszResFilterMax != nullptr )
        {
            dfResFilterMax = CPLAtof(pszResFilterMax);
            const double dfMinRes = std::min(dfMinResX, dfMinResY);
            if( dfResFilterMax < dfMinRes )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot specified RES_FILTER_MAX < %g",
                         dfMinRes);
                delete poDS;
                return nullptr;
            }
            poDS->GDALDataset::SetMetadataItem("RES_FILTER_MAX",
                                            CPLSPrintf("%g", dfResFilterMax));
        }

        if( dfResFilterMin >= dfResFilterMax )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot specified RES_FILTER_MIN >= RES_FILTER_MAX");
            delete poDS;
            return nullptr;
        }

        if( EQUAL(pszResStrategy, "AUTO") && (pszResFilterMin || pszResFilterMax) )
        {
            if( pszResFilterMax )
            {
                dfDefaultResX = dfResFilterMax;
                dfDefaultResY = dfResFilterMax;
            }
            else
            {
                dfDefaultResX = dfMaxResX;
                dfDefaultResY = dfMaxResY;
            }
        }
        else if( EQUAL(pszResStrategy, "AUTO") || EQUAL(pszResStrategy, "MIN") )
        {
            dfDefaultResX = dfMinResX;
            dfDefaultResY = dfMinResY;
        }
        else if( EQUAL(pszResStrategy, "MAX") )
        {
            dfDefaultResX = dfMaxResX;
            dfDefaultResY = dfMaxResY;
        }
        else if( EQUAL(pszResStrategy, "MEAN") )
        {
            if( !poDS->GetMeanSupergridsResolution(dfDefaultResX, dfDefaultResY) )
            {
                delete poDS;
                return nullptr;
            }
        }

        const char* pszMinX = CSLFetchNameValue(poOpenInfo->papszOpenOptions, "MINX");
        const char* pszMinY = CSLFetchNameValue(poOpenInfo->papszOpenOptions, "MINY");
        const char* pszMaxX = CSLFetchNameValue(poOpenInfo->papszOpenOptions, "MAXX");
        const char* pszMaxY = CSLFetchNameValue(poOpenInfo->papszOpenOptions, "MAXY");

        double dfMinX = poDS->m_dfLowResMinX;
        double dfMinY = poDS->m_dfLowResMinY;
        double dfMaxX = poDS->m_dfLowResMaxX;
        double dfMaxY = poDS->m_dfLowResMaxY;
        double dfResX = dfDefaultResX;
        double dfResY = dfDefaultResY;
        if( pszMinX ) dfMinX = CPLAtof(pszMinX);
        if( pszMinY ) dfMinY = CPLAtof(pszMinY);
        if( pszMaxX ) dfMaxX = CPLAtof(pszMaxX);
        if( pszMaxY ) dfMaxY = CPLAtof(pszMaxY);
        if( pszResX ) dfResX = CPLAtof(pszResX);
        if( pszResY ) dfResY = CPLAtof(pszResY);

        if( dfResX <= 0.0 || dfResY <= 0.0 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Invalid resolution: %f x %f", dfResX, dfResY);
            delete poDS;
            return nullptr;
        }
        const double dfRasterXSize = (dfMaxX - dfMinX) / dfResX;
        const double dfRasterYSize = (dfMaxY - dfMinY) / dfResY;
        if( dfRasterXSize <= 1 || dfRasterYSize <= 1 ||
            dfRasterXSize > INT_MAX || dfRasterYSize > INT_MAX )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Invalid raster dimension");
            delete poDS;
            return nullptr;
        }
        poDS->nRasterXSize = static_cast<int>(dfRasterXSize + 0.5);
        poDS->nRasterYSize = static_cast<int>(dfRasterYSize + 0.5);
        poDS->adfGeoTransform[0] = dfMinX;
        poDS->adfGeoTransform[1] = dfResX;
        poDS->adfGeoTransform[3] = dfMaxY;
        poDS->adfGeoTransform[5] = -dfResY;
        if( pszMaxY == nullptr || pszMinY != nullptr )
        {
            // if the constraint is not given by MAXY, we may need to tweak
            // adfGeoTransform[3] / maxy, so that we get the requested MINY
            // value
            poDS->adfGeoTransform[3] += dfMinY - (dfMaxY - poDS->nRasterYSize * dfResY);
        }

        const double dfMinRes = std::min(dfMinResX, dfMinResY);
        if( dfResFilterMin > dfMinRes )
        {
            poDS->m_dfResFilterMin = dfResFilterMin;
        }
        poDS->m_dfResFilterMax = dfResFilterMax;

        // Use min/max BAG refinement metadata items only if the
        // GDAL dataset bounding box is equal or larger to the BAG dataset
        const bool bInitializeMinMax = ( !poDS->m_bMask &&
                                        dfMinX <= poDS->m_dfLowResMinX &&
                                        dfMinY <= poDS->m_dfLowResMinY &&
                                        dfMaxX >= poDS->m_dfLowResMaxX &&
                                        dfMaxY >= poDS->m_dfLowResMaxY );

        const char* pszInterpolation = CSLFetchNameValueDef(
            poOpenInfo->papszOpenOptions, "INTERPOLATION", "NO");
        if( !EQUAL(pszInterpolation, "NO") && !EQUAL(pszInterpolation, "INVDIST") )
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Unsupported value for INTERPOLATION. Assuming NO");
            pszInterpolation = "NO";
        }
        if( !EQUAL(pszInterpolation, "NO") && poDS->m_bMask )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                        "SUPERGRIDS_MASK=NO cannot be used together "
                        "with INTERPOLATION.");
            delete poDS;
            return nullptr;
        }
        if( EQUAL(pszInterpolation, "INVDIST") )
        {
            poDS->m_poUninterpolatedDS.reset(new BAGDataset(poDS, 1));
            poDS->m_poUninterpolatedDS->SetBand(1, new BAGResampledBand(
                poDS->m_poUninterpolatedDS.get(), 1, bHasNoData, fNoDataValue,
                bInitializeMinMax));
            poDS->m_poUninterpolatedDS->SetBand(2, new BAGResampledBand(
                poDS->m_poUninterpolatedDS.get(), 2, bHasNoData, fNoDataValue,
                bInitializeMinMax));

            poDS->SetBand(1, new BAGInterpolatedBand(poDS, 1));
            poDS->SetBand(2, new BAGInterpolatedBand(poDS, 2));
        }
        else
        {
            if( poDS->m_bMask )
            {
                poDS->SetBand(1, new BAGResampledBand(poDS, 1,
                                                      false, 0.0f, false));
            }
            else
            {
                poDS->SetBand(1, new BAGResampledBand(poDS, 1, bHasNoData,
                                                  fNoDataValue,
                                                  bInitializeMinMax));

                poDS->SetBand(2, new BAGResampledBand(poDS, 2, bHasNoData,
                                                      fNoDataValue,
                                                      bInitializeMinMax));
            }
        }

        if( poDS->GetRasterCount() > 1 )
        {
            poDS->GDALDataset::SetMetadataItem(
                "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
        }

        // Mostly for autotest purposes
        const int nMinOvrSize = std::max(1, atoi(
            CPLGetConfigOption("GDAL_BAG_MIN_OVR_SIZE", "256")));
        for(int nOvrFactor = 2;
                poDS->nRasterXSize / nOvrFactor >= nMinOvrSize &&
                poDS->nRasterYSize / nOvrFactor >= nMinOvrSize;
                nOvrFactor *= 2)
        {
            BAGDataset* poOvrDS = new BAGDataset(poDS, nOvrFactor);

            if( EQUAL(pszInterpolation, "INVDIST") )
            {
                poOvrDS->m_poUninterpolatedDS.reset(new BAGDataset(poOvrDS, 1));
                poOvrDS->m_poUninterpolatedDS->SetBand(1, new BAGResampledBand(
                    poOvrDS->m_poUninterpolatedDS.get(), 1, bHasNoData,
                    fNoDataValue, bInitializeMinMax));
                poOvrDS->m_poUninterpolatedDS->SetBand(2, new BAGResampledBand(
                    poOvrDS->m_poUninterpolatedDS.get(), 2, bHasNoData,
                    fNoDataValue, bInitializeMinMax));
            }

            for( int i = 1; i <= poDS->GetRasterCount(); i++ )
            {
                if( EQUAL(pszInterpolation, "INVDIST") )
                {
                    poOvrDS->SetBand(i, new BAGInterpolatedBand(poOvrDS, i));
                }
                else
                {
                    poOvrDS->SetBand(i, new BAGResampledBand(poOvrDS, i,
                                            bHasNoData, fNoDataValue, false));
                }
            }

            if( poOvrDS->GetRasterCount() > 1 )
            {
                poOvrDS->GDALDataset::SetMetadataItem(
                    "INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
            }
            poDS->m_apoOverviewDS.push_back(
                std::unique_ptr<BAGDataset>(poOvrDS));
        }
    }
    else if( bOpenSuperGrid )
    {
        if( poDS->m_aoRefinemendGrids.empty() ||
            nX < 0 || nX >= poDS->m_nLowResWidth ||
            nY < 0 || nY >= poDS->m_nLowResHeight ||
            poDS->m_aoRefinemendGrids[nY * poDS->m_nLowResWidth + nX].nWidth == 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Invalid subdataset");
            delete poDS;
            return nullptr;
        }

        poDS->m_aosSubdatasets.Clear();
        auto pSuperGrid = &poDS->m_aoRefinemendGrids[nY * poDS->m_nLowResWidth + nX];
        poDS->nRasterXSize = static_cast<int>(pSuperGrid->nWidth);
        poDS->nRasterYSize = static_cast<int>(pSuperGrid->nHeight);

        // Convert from pixel-center convention to corner-pixel convention
        const double dfMinX =
            poDS->adfGeoTransform[0] + nX * poDS->adfGeoTransform[1] +
            pSuperGrid->fSWX - pSuperGrid->fResX / 2;
        const double dfMinY =
            poDS->adfGeoTransform[3] +
            poDS->m_nLowResHeight * poDS->adfGeoTransform[5] +
            nY * -poDS->adfGeoTransform[5] +
            pSuperGrid->fSWY - pSuperGrid->fResY / 2;
        const double dfMaxY = dfMinY + pSuperGrid->nHeight * pSuperGrid->fResY;

        poDS->adfGeoTransform[0] = dfMinX;
        poDS->adfGeoTransform[1] = pSuperGrid->fResX;
        poDS->adfGeoTransform[3] = dfMaxY;
        poDS->adfGeoTransform[5] = -pSuperGrid->fResY;
        poDS->m_nSuperGridRefinementStartIndex = pSuperGrid->nIndex;
        poDS->m_aoRefinemendGrids.clear();

        for(int i = 0; i < 2; i++)
        {
            poDS->SetBand(i+1, new BAGSuperGridBand(poDS, i+1,
                                                    bHasNoData, fNoDataValue));
        }

        poDS->GDALDataset::SetMetadataItem("INTERLEAVE", "PIXEL",
                                            "IMAGE_STRUCTURE");

        poDS->SetPhysicalFilename(osFilename);
    }

    // Setup/check for pam .aux.xml.
    poDS->TryLoadXML();

    // Setup overviews.
    poDS->oOvManager.Initialize(poDS, poOpenInfo->pszFilename);

    return poDS;
}

/************************************************************************/
/*                      GetMeanSupergridsResolution()                   */
/************************************************************************/

bool BAGDataset::GetMeanSupergridsResolution(double& dfResX, double& dfResY)
{
    const int nChunkXSize = m_nChunkXSizeVarresMD;
    const int nChunkYSize = m_nChunkYSizeVarresMD;

    dfResX = 0.0;
    dfResY = 0.0;
    int nValidSuperGrids = 0;
    std::vector<BAGRefinementGrid> rgrids(nChunkXSize * nChunkYSize);
    const int county = (m_nLowResHeight + nChunkYSize - 1) / nChunkYSize;
    const int countx = (m_nLowResWidth + nChunkXSize - 1) / nChunkXSize;
    for( int y = 0; y < county; y++ )
    {
        const int nReqCountY = std::min(nChunkYSize,
                            m_nLowResHeight - y * nChunkYSize);
        for( int x = 0; x < countx; x++ )
        {
            const int nReqCountX = std::min(nChunkXSize,
                                m_nLowResWidth - x * nChunkXSize);

            // Create memory space to receive the data.
            hsize_t count[2] = { static_cast<hsize_t>(nReqCountY),
                                 static_cast<hsize_t>(nReqCountX)};
            const hid_t memspace = H5Screate_simple(2, count, nullptr);
            H5OFFSET_TYPE mem_offset[2] = {
                static_cast<H5OFFSET_TYPE>(0),
                static_cast<H5OFFSET_TYPE>(0) };
            if( H5Sselect_hyperslab(memspace, H5S_SELECT_SET,
                            mem_offset, nullptr, count, nullptr) < 0 )
            {
                H5Sclose(memspace);
                return false;
            }

            if( ReadVarresMetadataValue(
                    y * nChunkYSize, x * nChunkXSize,
                    memspace, rgrids.data(),
                    nReqCountY, nReqCountX) )
            {
                for( int i = 0; i < nReqCountX * nReqCountY; i++ )
                {
                    if( rgrids[i].nWidth > 0 )
                    {
                        dfResX += rgrids[i].fResX;
                        dfResY += rgrids[i].fResY;
                        nValidSuperGrids ++;
                    }
                }
            }
            H5Sclose(memspace);
        }
    }

    if( nValidSuperGrids == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                "No valid supergrids");
        return false;
    }

    dfResX /= nValidSuperGrids;
    dfResY /= nValidSuperGrids;
    return true;
}

/************************************************************************/
/*                      GetVarresMetadataChunkSizes()                   */
/************************************************************************/

void BAGDataset::GetVarresMetadataChunkSizes(int& nChunkXSize,
                                             int& nChunkYSize)
{
    const hid_t listid = H5Dget_create_plist(m_hVarresMetadata);
    nChunkXSize = m_nLowResWidth;
    nChunkYSize = std::max(1,
                    std::min(10*1024*1024 / m_nLowResWidth, m_nLowResHeight));
    if( listid > 0 )
    {
        if( H5Pget_layout(listid) == H5D_CHUNKED )
        {
            hsize_t panChunkDims[2] = {0, 0};
            const int nDimSize = H5Pget_chunk(listid, 2, panChunkDims);
            CPL_IGNORE_RET_VAL(nDimSize);
            CPLAssert(nDimSize == 2);
            nChunkXSize = static_cast<int>(panChunkDims[1]);
            nChunkYSize = static_cast<int>(panChunkDims[0]);
        }

        H5Pclose(listid);
    }
}

/************************************************************************/
/*                     GetVarresRefinementChunkSize()                   */
/************************************************************************/

void BAGDataset::GetVarresRefinementChunkSize(unsigned& nChunkSize)
{
    const hid_t listid = H5Dget_create_plist(m_hVarresRefinements);
    nChunkSize = 1024;
    if( listid > 0 )
    {
        if( H5Pget_layout(listid) == H5D_CHUNKED )
        {
            hsize_t panChunkDims[2] = {0, 0};
            const int nDimSize = H5Pget_chunk(listid, 2, panChunkDims);
            CPL_IGNORE_RET_VAL(nDimSize);
            CPLAssert(nDimSize == 2);
            nChunkSize = static_cast<unsigned>(panChunkDims[1]);
        }

        H5Pclose(listid);
    }
}

/************************************************************************/
/*                        CacheRefinementValues()                       */
/************************************************************************/

bool BAGDataset::CacheRefinementValues(unsigned nRefinementIndex)
{
    if( !(nRefinementIndex >= m_nCachedRefinementStartIndex &&
          nRefinementIndex < m_nCachedRefinementStartIndex +
                               m_nCachedRefinementCount) )
    {
        m_nCachedRefinementStartIndex =
            (nRefinementIndex / m_nChunkSizeVarresRefinement) *
                    m_nChunkSizeVarresRefinement;
        m_nCachedRefinementCount =
            std::min(m_nChunkSizeVarresRefinement,
                    m_nRefinementsSize -
                    m_nCachedRefinementStartIndex);
        m_aCachedRefinementValues.resize(2 * m_nCachedRefinementCount);

        hsize_t countVarresRefinements[2] = {
            static_cast<hsize_t>(1),
            static_cast<hsize_t>(m_nCachedRefinementCount)};
        const hid_t memspaceVarresRefinements =
            H5Screate_simple(2, countVarresRefinements, nullptr);
        H5OFFSET_TYPE mem_offset[2] = { static_cast<H5OFFSET_TYPE>(0),
                                        static_cast<H5OFFSET_TYPE>(0) };
        if( H5Sselect_hyperslab(memspaceVarresRefinements,
                                H5S_SELECT_SET,
                                mem_offset, nullptr,
                                countVarresRefinements,
                                nullptr) < 0 )
        {
            H5Sclose(memspaceVarresRefinements);
            return false;
        }

        H5OFFSET_TYPE offsetRefinement[2] = {
            static_cast<H5OFFSET_TYPE>(0),
            static_cast<H5OFFSET_TYPE>(m_nCachedRefinementStartIndex)
        };
        if( H5Sselect_hyperslab(
                m_hVarresRefinementsDataspace,
                H5S_SELECT_SET,
                offsetRefinement, nullptr,
                countVarresRefinements, nullptr) < 0 )
        {
            H5Sclose(memspaceVarresRefinements);
            return false;
        }
        if( H5Dread(m_hVarresRefinements,
                    m_hVarresRefinementsNative,
                    memspaceVarresRefinements,
                    m_hVarresRefinementsDataspace,
                    H5P_DEFAULT,
                    m_aCachedRefinementValues.data()) < 0 )
        {
            H5Sclose(memspaceVarresRefinements);
            return false;
        }
        H5Sclose(memspaceVarresRefinements);
    }

    return true;
}

/************************************************************************/
/*                        ReadVarresMetadataValue()                     */
/************************************************************************/

bool BAGDataset::ReadVarresMetadataValue(int y, int x, hid_t memspace,
                                         BAGRefinementGrid* rgrid,
                                         int height, int width)
{
    constexpr int metadata_elt_size = 3 * 4 + 4 * 4; // 3 uint and 4 float
    std::vector<char> buffer(metadata_elt_size * height * width);

    hsize_t count[2] = { static_cast<hsize_t>(height), static_cast<hsize_t>(width)};
    H5OFFSET_TYPE offset[2] = { static_cast<H5OFFSET_TYPE>(y),
                                static_cast<H5OFFSET_TYPE>(x) };
    if( H5Sselect_hyperslab(m_hVarresMetadataDataspace,
                                    H5S_SELECT_SET,
                                    offset, nullptr,
                                    count, nullptr) < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ReadVarresMetadataValue(): H5Sselect_hyperslab() failed");
        return false;
    }

    if( H5Dread(m_hVarresMetadata, m_hVarresMetadataNative, memspace,
                m_hVarresMetadataDataspace, H5P_DEFAULT, buffer.data()) < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ReadVarresMetadataValue(): H5Dread() failed");
        return false;
    }

    for( int i = 0; i < width * height; i++ )
    {
        const char* src_ptr = buffer.data() + metadata_elt_size * i;
        memcpy(&rgrid[i].nIndex, src_ptr, 4);
        memcpy(&rgrid[i].nWidth, src_ptr + 4, 4);
        memcpy(&rgrid[i].nHeight, src_ptr + 8, 4);
        memcpy(&rgrid[i].fResX, src_ptr + 12, 4);
        memcpy(&rgrid[i].fResY, src_ptr + 16, 4);
        memcpy(&rgrid[i].fSWX, src_ptr + 20, 4);
        memcpy(&rgrid[i].fSWY, src_ptr + 24, 4);
    }
    return true;
}

/************************************************************************/
/*                        LookForRefinementGrids()                      */
/************************************************************************/

bool BAGDataset::LookForRefinementGrids(CSLConstList l_papszOpenOptions,
                                        int nYSubDS, int nXSubDS)

{
    m_hVarresMetadata = GH5DopenNoWarning(hHDF5, "/BAG_root/varres_metadata");
    if( m_hVarresMetadata < 0 )
        return false;
    m_hVarresRefinements = H5Dopen(hHDF5, "/BAG_root/varres_refinements");
    if( m_hVarresRefinements < 0 )
        return false;

    m_hVarresMetadataDataType = H5Dget_type(m_hVarresMetadata);
    if( H5Tget_class(m_hVarresMetadataDataType) != H5T_COMPOUND )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "m_hVarresMetadataDataType is not compound");
        return false;
    }

    const struct
    {
        const char* pszName;
        hid_t eType;
    } asMetadataFields[] =
    {
        { "index", H5T_NATIVE_UINT },
        { "dimensions_x", H5T_NATIVE_UINT },
        { "dimensions_y", H5T_NATIVE_UINT },
        { "resolution_x", H5T_NATIVE_FLOAT },
        { "resolution_y", H5T_NATIVE_FLOAT },
        { "sw_corner_x", H5T_NATIVE_FLOAT },
        { "sw_corner_y", H5T_NATIVE_FLOAT },
    };

    if( H5Tget_nmembers(m_hVarresMetadataDataType) != CPL_ARRAYSIZE(asMetadataFields) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "m_hVarresMetadataDataType has not %u members",
                 static_cast<unsigned>(CPL_ARRAYSIZE(asMetadataFields)));
        return false;
    }

    for( unsigned i = 0; i < CPL_ARRAYSIZE(asMetadataFields); i++ )
    {
        char* pszName = H5Tget_member_name(m_hVarresMetadataDataType, i);
        if( strcmp(pszName, asMetadataFields[i].pszName) != 0 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "asMetadataFields[%u].pszName = %s instead of %s",
                     static_cast<unsigned>(i), pszName,
                     asMetadataFields[i].pszName);
            H5free_memory(pszName);
            return false;
        }
        H5free_memory(pszName);
        hid_t type = H5Tget_member_type(m_hVarresMetadataDataType, i);
        bool bTypeOK = H5Tequal(asMetadataFields[i].eType, type) > 0;
        H5Tclose(type);
        if( !bTypeOK )
        {
             CPLError(CE_Failure, CPLE_NotSupported,
                      "asMetadataFields[%u].eType is not of expected type",
                      i);
             return false;
        }
    }

    m_hVarresMetadataDataspace = H5Dget_space(m_hVarresMetadata);
    if( H5Sget_simple_extent_ndims(m_hVarresMetadataDataspace) != 2 )
    {
        CPLDebug("BAG",
                 "H5Sget_simple_extent_ndims(m_hVarresMetadataDataspace) != 2");
        return false;
    }

    {
        hsize_t dims[2] = { static_cast<hsize_t>(0), static_cast<hsize_t>(0) };
        hsize_t maxdims[2] = { static_cast<hsize_t>(0), static_cast<hsize_t>(0) };

        H5Sget_simple_extent_dims(m_hVarresMetadataDataspace, dims, maxdims);
        if( dims[0] != static_cast<hsize_t>(m_nLowResHeight) ||
            dims[1] != static_cast<hsize_t>(m_nLowResWidth) )
        {
            CPLDebug("BAG",
                    "Unexpected dimension for m_hVarresMetadata");
            return false;
        }
    }

    if( m_nLowResWidth > 10 * 1000 * 1000 / m_nLowResHeight )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Too many refinement grids");
        return false;
    }

    m_hVarresMetadataNative =
        H5Tget_native_type(m_hVarresMetadataDataType, H5T_DIR_ASCEND);


    m_hVarresRefinementsDataType = H5Dget_type(m_hVarresRefinements);
    if( H5Tget_class(m_hVarresRefinementsDataType) != H5T_COMPOUND )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "m_hVarresRefinementsDataType is not compound");
        return false;
    }

    const struct
    {
        const char* pszName;
        hid_t eType;
    } asRefinementsFields[] =
    {
        { "depth", H5T_NATIVE_FLOAT },
        { "depth_uncrt", H5T_NATIVE_FLOAT },
    };

    if( H5Tget_nmembers(m_hVarresRefinementsDataType) !=
                                        CPL_ARRAYSIZE(asRefinementsFields) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "m_hVarresRefinementsDataType has not %u members",
                 static_cast<unsigned>(CPL_ARRAYSIZE(asRefinementsFields)));
        return false;
    }

    for( unsigned i = 0; i < CPL_ARRAYSIZE(asRefinementsFields); i++ )
    {
        char* pszName = H5Tget_member_name(m_hVarresRefinementsDataType, i);
        if( strcmp(pszName, asRefinementsFields[i].pszName) != 0 )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "asRefinementsFields[%u].pszName = %s instead of %s",
                     static_cast<unsigned>(i), pszName,
                     asRefinementsFields[i].pszName);
            H5free_memory(pszName);
            return false;
        }
        H5free_memory(pszName);
        hid_t type = H5Tget_member_type(m_hVarresRefinementsDataType, i);
        bool bTypeOK = H5Tequal(asRefinementsFields[i].eType, type) > 0;
        H5Tclose(type);
        if( !bTypeOK )
        {
             CPLError(CE_Failure, CPLE_NotSupported,
                      "asRefinementsFields[%u].eType is not of expected type",
                      i);
             return false;
        }
    }

    m_hVarresRefinementsDataspace = H5Dget_space(m_hVarresRefinements);
    if( H5Sget_simple_extent_ndims(m_hVarresRefinementsDataspace) != 2 )
    {
        CPLDebug("BAG",
                 "H5Sget_simple_extent_ndims(m_hVarresRefinementsDataspace) != 2");
        return false;
    }

    m_hVarresRefinementsNative =
        H5Tget_native_type(m_hVarresRefinementsDataType, H5T_DIR_ASCEND);

    hsize_t nRefinementsSize;
    {
        hsize_t dims[2] = { static_cast<hsize_t>(0), static_cast<hsize_t>(0) };
        hsize_t maxdims[2] = { static_cast<hsize_t>(0), static_cast<hsize_t>(0) };

        H5Sget_simple_extent_dims(m_hVarresRefinementsDataspace, dims, maxdims);
        if( dims[0] != 1 )
        {
            CPLDebug("BAG",
                    "Unexpected dimension for m_hVarresMetadata");
            return false;
        }
        nRefinementsSize = dims[1];
        m_nRefinementsSize = static_cast<unsigned>(nRefinementsSize);
        CPLDebug("BAG", "m_nRefinementsSize = %u", m_nRefinementsSize);
    }

    GetVarresMetadataChunkSizes(m_nChunkXSizeVarresMD, m_nChunkYSizeVarresMD);
    CPLDebug("BAG", "m_nChunkXSizeVarresMD = %d, m_nChunkYSizeVarresMD = %d",
             m_nChunkXSizeVarresMD, m_nChunkYSizeVarresMD);
    GetVarresRefinementChunkSize(m_nChunkSizeVarresRefinement);
    CPLDebug("BAG", "m_nChunkSizeVarresRefinement = %u",
             m_nChunkSizeVarresRefinement);

    if( EQUAL(CSLFetchNameValueDef(l_papszOpenOptions, "MODE", ""),
              "RESAMPLED_GRID") )
    {
        return true;
    }

    m_aoRefinemendGrids.resize(m_nLowResWidth * m_nLowResHeight);

    const char* pszSUPERGRIDS = CSLFetchNameValue(l_papszOpenOptions,
                                                  "SUPERGRIDS_INDICES");
    std::set<std::pair<int,int>> oSupergrids;
    int nMinX = 0;
    int nMinY = 0;
    int nMaxX = m_nLowResWidth - 1;
    int nMaxY = m_nLowResHeight - 1;
    if( nYSubDS >= 0 && nXSubDS >= 0 )
    {
        nMinX = nXSubDS;
        nMaxX = nXSubDS;
        nMinY = nYSubDS;
        nMaxY = nYSubDS;
    }
    else if( pszSUPERGRIDS )
    {
        char chExpectedChar = '(';
        bool bNextIsY = false;
        bool bNextIsX = false;
        bool bHasY = false;
        bool bHasX = false;
        int nY = 0;
        int i = 0;
        for( ; pszSUPERGRIDS[i]; i++ )
        {
            if( chExpectedChar && pszSUPERGRIDS[i] != chExpectedChar )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Invalid formatting for SUPERGRIDS_INDICES at index %d. "
                         "Expecting %c, got %c",
                         i, chExpectedChar, pszSUPERGRIDS[i]);
                break;
            }
            else if( chExpectedChar == '(' )
            {
                chExpectedChar = 0;
                bNextIsY = true;
            }
            else if( chExpectedChar == ',' )
            {
                chExpectedChar = '(';
            }
            else
            {
                CPLAssert(chExpectedChar == 0);
                if( bNextIsY && pszSUPERGRIDS[i] >= '0' &&
                    pszSUPERGRIDS[i] <= '9' )
                {
                    nY = atoi(pszSUPERGRIDS + i);
                    bNextIsY = false;
                    bHasY = true;
                }
                else if( bNextIsX && pszSUPERGRIDS[i] >= '0' &&
                         pszSUPERGRIDS[i] <= '9' )
                {
                    int nX = atoi(pszSUPERGRIDS + i);
                    bNextIsX = false;
                    oSupergrids.insert(std::pair<int,int>(nY, nX));
                    bHasX = true;
                }
                else if( (bHasX || bHasY) && pszSUPERGRIDS[i] >= '0' &&
                         pszSUPERGRIDS[i] <= '9' )
                {
                    // ok
                }
                else if( bHasY && pszSUPERGRIDS[i] == ',' )
                {
                    bNextIsX = true;
                }
                else if( bHasX && bHasY && pszSUPERGRIDS[i] == ')' )
                {
                    chExpectedChar = ',';
                    bHasX = false;
                    bHasY = false;
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                         "Invalid formatting for SUPERGRIDS_INDICES at index %d. "
                         "Got %c",
                         i, pszSUPERGRIDS[i]);
                    break;
                }
            }
        }
        if( pszSUPERGRIDS[i] == 0 && chExpectedChar != ',' )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Invalid formatting for SUPERGRIDS_INDICES at index %d.",
                     i);
        }

        bool bFirst = true;
        for( const auto& oPair: oSupergrids )
        {
            if( bFirst )
            {
                nMinX = oPair.second;
                nMaxX = oPair.second;
                nMinY = oPair.first;
                nMaxY = oPair.first;
                bFirst = false;
            }
            else
            {
                nMinX = std::min(nMinX, oPair.second);
                nMaxX = std::max(nMaxX, oPair.second);
                nMinY = std::min(nMinY, oPair.first);
                nMaxY = std::max(nMaxY, oPair.first);
            }
        }
    }
    const char* pszMinX = CSLFetchNameValue(l_papszOpenOptions, "MINX");
    const char* pszMinY = CSLFetchNameValue(l_papszOpenOptions, "MINY");
    const char* pszMaxX = CSLFetchNameValue(l_papszOpenOptions, "MAXX");
    const char* pszMaxY = CSLFetchNameValue(l_papszOpenOptions, "MAXY");
    const int nCountBBoxElts = (pszMinX ? 1 : 0) + (pszMinY ? 1 : 0) +
                         (pszMaxX ? 1 : 0) + (pszMaxY ? 1 : 0);
    const bool bHasBoundingBoxFilter =
        !(nYSubDS >= 0 && nXSubDS >= 0) && (nCountBBoxElts == 4);
    double dfFilterMinX = 0.0;
    double dfFilterMinY = 0.0;
    double dfFilterMaxX = 0.0;
    double dfFilterMaxY = 0.0;
    if( nYSubDS >= 0 && nXSubDS >= 0 )
    {
        // do nothing
    }
    else if( bHasBoundingBoxFilter )
    {
        dfFilterMinX = CPLAtof(pszMinX);
        dfFilterMinY = CPLAtof(pszMinY);
        dfFilterMaxX = CPLAtof(pszMaxX);
        dfFilterMaxY = CPLAtof(pszMaxY);

        nMinX = std::max(nMinX, static_cast<int>
            ((dfFilterMinX - adfGeoTransform[0]) / adfGeoTransform[1]));
        nMaxX = std::min(nMaxX, static_cast<int>
            ((dfFilterMaxX - adfGeoTransform[0]) / adfGeoTransform[1]));

        nMinY = std::max(nMinY, static_cast<int>
            ((dfFilterMinY - (adfGeoTransform[3] + m_nLowResHeight * adfGeoTransform[5])) / -adfGeoTransform[5]));
        nMaxY = std::min(nMaxY, static_cast<int>
            ((dfFilterMaxY - (adfGeoTransform[3] + m_nLowResHeight * adfGeoTransform[5])) / -adfGeoTransform[5]));

    }
    else if( nCountBBoxElts > 0 )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Bounding box filter ignored since only part of "
                 "MINX, MINY, MAXX and MAXY has been specified");
    }

    const double dfResFilterMin = CPLAtof(CSLFetchNameValueDef(
        l_papszOpenOptions, "RES_FILTER_MIN", "0"));
    const double dfResFilterMax = CPLAtof(CSLFetchNameValueDef(
        l_papszOpenOptions, "RES_FILTER_MAX", "inf"));

    const int nChunkXSize = m_nChunkXSizeVarresMD;
    const int nChunkYSize = m_nChunkYSizeVarresMD;
    std::vector<BAGRefinementGrid> rgrids(nChunkXSize * nChunkYSize);
    bool bOK = true;
    for( int blockY = nMinY / nChunkYSize;
            bOK && blockY <= nMaxY / nChunkYSize; blockY++ )
    {
        int nReqCountY = std::min(nChunkYSize,
                                    m_nLowResHeight - blockY * nChunkYSize);
        for( int blockX = nMinX / nChunkXSize;
                    bOK && blockX <= nMaxX / nChunkXSize; blockX++ )
        {
            int nReqCountX = std::min(nChunkXSize,
                                    m_nLowResWidth - blockX * nChunkXSize);

            // Create memory space to receive the data.
            hsize_t count[2] = { static_cast<hsize_t>(nReqCountY),
                                 static_cast<hsize_t>(nReqCountX) };
            const hid_t memspace = H5Screate_simple(2, count, nullptr);
            H5OFFSET_TYPE mem_offset[2] = { static_cast<H5OFFSET_TYPE>(0),
                static_cast<H5OFFSET_TYPE>(0) };
            if( H5Sselect_hyperslab(memspace, H5S_SELECT_SET,
                                        mem_offset, nullptr, count, nullptr) < 0 )
            {
                H5Sclose(memspace);
                bOK = false;
                break;
            }

            if( !ReadVarresMetadataValue(blockY * nChunkYSize,
                                         blockX * nChunkXSize,
                                         memspace, rgrids.data(),
                                         nReqCountY, nReqCountX) )
            {
                bOK = false;
                H5Sclose(memspace);
                break;
            }
            H5Sclose(memspace);

            for( int yInBlock = std::max(0, nMinY - blockY * nChunkYSize);
                     bOK && yInBlock <= std::min(nReqCountY - 1,
                                    nMaxY - blockY * nChunkYSize); yInBlock++ )
            {
                for( int xInBlock = std::max(0, nMinX - blockX * nChunkXSize);
                        bOK && xInBlock <= std::min(nReqCountX - 1,
                                    nMaxX - blockX * nChunkXSize); xInBlock++ )
                {
                    int y = yInBlock + blockY * nChunkYSize;
                    int x = xInBlock + blockX * nChunkXSize;
                    auto& rgrid = rgrids[yInBlock * nReqCountX + xInBlock];
                    m_aoRefinemendGrids[y * m_nLowResWidth + x] = rgrid;
                    if( rgrid.nWidth > 0 )
                    {
                        if( rgrid.fResX <= 0 || rgrid.fResY <= 0 )
                        {
                            CPLError(CE_Failure, CPLE_NotSupported,
                                    "Incorrect resolution for supergrid "
                                    "(%d, %d).",
                                    static_cast<int>(y), static_cast<int>(x));
                            bOK = false;
                            break;
                        }
                        if( rgrid.nIndex + static_cast<GUInt64>(rgrid.nWidth) *
                                                    rgrid.nHeight > nRefinementsSize )
                        {
                            CPLError(CE_Failure, CPLE_NotSupported,
                                    "Incorrect index / dimensions for supergrid "
                                    "(%d, %d).",
                                    static_cast<int>(y), static_cast<int>(x));
                            bOK = false;
                            break;
                        }

                        if( rgrid.fSWX < 0.0f ||
                            rgrid.fSWY < 0.0f ||
                            // 0.1 is to deal with numeric imprecisions
                            rgrid.fSWX + (rgrid.nWidth-1-0.1) * rgrid.fResX > adfGeoTransform[1] ||
                            rgrid.fSWY + (rgrid.nHeight-1-0.1) * rgrid.fResY > -adfGeoTransform[5] )
                        {
                            CPLError(CE_Failure, CPLE_NotSupported,
                                    "Incorrect bounds for supergrid "
                                    "(%d, %d): %f, %f, %f, %f.",
                                    static_cast<int>(y), static_cast<int>(x),
                                    rgrid.fSWX, rgrid.fSWY,
                                    rgrid.fSWX + (rgrid.nWidth-1) * rgrid.fResX,
                                    rgrid.fSWY + (rgrid.nHeight-1) * rgrid.fResY);
                            bOK = false;
                            break;
                        }

                        const float gridRes = std::max(rgrid.fResX, rgrid.fResY);
                        if( gridRes < dfResFilterMin || gridRes >= dfResFilterMax )
                        {
                            continue;
                        }

                        const double dfMinX =
                            adfGeoTransform[0] + x * adfGeoTransform[1] + rgrid.fSWX - rgrid.fResX / 2;
                        const double dfMaxX = dfMinX + rgrid.nWidth * rgrid.fResX;
                        const double dfMinY =
                            adfGeoTransform[3] +
                            m_nLowResHeight * adfGeoTransform[5] +
                            y * -adfGeoTransform[5] + rgrid.fSWY - rgrid.fResY / 2;
                        const double dfMaxY = dfMinY + rgrid.nHeight * rgrid.fResY;

                        if( (oSupergrids.empty() ||
                            oSupergrids.find(std::pair<int,int>(
                                static_cast<int>(y), static_cast<int>(x))) !=
                                    oSupergrids.end()) &&
                            (!bHasBoundingBoxFilter ||
                                (dfMinX >= dfFilterMinX && dfMinY >= dfFilterMinY &&
                                dfMaxX <= dfFilterMaxX && dfMaxY <= dfFilterMaxY)) )
                        {
                            const int nIdx = m_aosSubdatasets.size() / 2 + 1;
                            m_aosSubdatasets.AddNameValue(
                                CPLSPrintf("SUBDATASET_%d_NAME", nIdx),
                                CPLSPrintf("BAG:\"%s\":supergrid:%d:%d",
                                        GetDescription(),
                                        static_cast<int>(y), static_cast<int>(x)));
                            m_aosSubdatasets.AddNameValue(
                                CPLSPrintf("SUBDATASET_%d_DESC", nIdx),
                                CPLSPrintf("Supergrid (y=%d, x=%d) from "
                                        "(x=%f,y=%f) to "
                                        "(x=%f,y=%f), resolution (x=%f,y=%f)",
                                        static_cast<int>(y), static_cast<int>(x),
                                        dfMinX, dfMinY, dfMaxX, dfMaxY,
                                        rgrid.fResX, rgrid.fResY));
                        }
                    }
                }
            }
        }
    }

    if( !bOK )
    {
        m_aosSubdatasets.Clear();
        m_aoRefinemendGrids.clear();
        return false;
    }

    CPLDebug("BAG", "variable resolution extensions detected");
    return true;
}

/************************************************************************/
/*                            LoadMetadata()                            */
/************************************************************************/

void BAGDataset::LoadMetadata()

{
    // Load the metadata from the file.
    const hid_t hMDDS = H5Dopen(hHDF5, "/BAG_root/metadata");
    const hid_t datatype = H5Dget_type(hMDDS);
    const hid_t dataspace = H5Dget_space(hMDDS);
    const hid_t native = H5Tget_native_type(datatype, H5T_DIR_ASCEND);

    const int n_dims = H5Sget_simple_extent_ndims(dataspace);
    hsize_t dims[1] = {
        static_cast<hsize_t>(0)
    };
    hsize_t maxdims[1] = {
        static_cast<hsize_t>(0)
    };

    if( n_dims == 1 )
    {
        H5Sget_simple_extent_dims(dataspace, dims, maxdims);

        pszXMLMetadata =
            static_cast<char *>(CPLCalloc(static_cast<int>(dims[0] + 1), 1));

        H5Dread(hMDDS, native, H5S_ALL, dataspace, H5P_DEFAULT, pszXMLMetadata);
    }

    H5Tclose(native);
    H5Sclose(dataspace);
    H5Tclose(datatype);
    H5Dclose(hMDDS);

    if( strlen(pszXMLMetadata) == 0 )
        return;

    // Try to get the geotransform.
    CPLXMLNode *psRoot = CPLParseXMLString(pszXMLMetadata);

    if( psRoot == nullptr )
        return;

    CPLStripXMLNamespace(psRoot, nullptr, TRUE);

    CPLXMLNode *const psGeo = CPLSearchXMLNode(psRoot, "=MD_Georectified");

    if( psGeo != nullptr )
    {
        char **papszCornerTokens = CSLTokenizeStringComplex(
            CPLGetXMLValue(psGeo, "cornerPoints.Point.coordinates", ""), " ,",
            FALSE, FALSE);

        if( CSLCount(papszCornerTokens) == 4 )
        {
            const double dfLLX = CPLAtof(papszCornerTokens[0]);
            const double dfLLY = CPLAtof(papszCornerTokens[1]);
            const double dfURX = CPLAtof(papszCornerTokens[2]);
            const double dfURY = CPLAtof(papszCornerTokens[3]);

            adfGeoTransform[0] = dfLLX;
            adfGeoTransform[1] = (dfURX - dfLLX) / (m_nLowResWidth - 1);
            adfGeoTransform[3] = dfURY;
            adfGeoTransform[5] = (dfLLY - dfURY) / (m_nLowResHeight - 1);

            adfGeoTransform[0] -= adfGeoTransform[1] * 0.5;
            adfGeoTransform[3] -= adfGeoTransform[5] * 0.5;

            m_dfLowResMinX = adfGeoTransform[0];
            m_dfLowResMaxX = m_dfLowResMinX + m_nLowResWidth * adfGeoTransform[1];
            m_dfLowResMaxY = adfGeoTransform[3];
            m_dfLowResMinY = m_dfLowResMaxY + m_nLowResHeight * adfGeoTransform[5];
        }
        CSLDestroy(papszCornerTokens);
    }

    // Try to get the coordinate system.
    OGRSpatialReference oSRS;

    if( OGR_SRS_ImportFromISO19115(&oSRS, pszXMLMetadata) == OGRERR_NONE )
    {
        oSRS.exportToWkt(&pszProjection);
    }
    else
    {
        ParseWKTFromXML(pszXMLMetadata);
    }

    // Fetch acquisition date.
    CPLXMLNode *const psDateTime = CPLSearchXMLNode(psRoot, "=dateTime");
    if( psDateTime != nullptr )
    {
        const char *pszDateTimeValue =
            psDateTime->psChild &&
            psDateTime->psChild->eType == CXT_Element ?
                CPLGetXMLValue(psDateTime->psChild, nullptr, nullptr):
                CPLGetXMLValue(psDateTime, nullptr, nullptr);
        if( pszDateTimeValue )
            GDALDataset::SetMetadataItem("BAG_DATETIME", pszDateTimeValue);
    }

    CPLDestroyXMLNode(psRoot);
}

/************************************************************************/
/*                          ParseWKTFromXML()                           */
/************************************************************************/
OGRErr BAGDataset::ParseWKTFromXML( const char *pszISOXML )
{
    CPLXMLNode *const psRoot = CPLParseXMLString(pszISOXML);

    if( psRoot == nullptr )
        return OGRERR_FAILURE;

    CPLStripXMLNamespace(psRoot, nullptr, TRUE);

    CPLXMLNode *psRSI = CPLSearchXMLNode(psRoot, "=referenceSystemInfo");
    if( psRSI == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to find <referenceSystemInfo> in metadata.");
        CPLDestroyXMLNode(psRoot);
        return OGRERR_FAILURE;
    }

    OGRSpatialReference oSRS;
    oSRS.Clear();

    const char *pszSRCodeString =
        CPLGetXMLValue(psRSI, "MD_ReferenceSystem.referenceSystemIdentifier."
                       "RS_Identifier.code.CharacterString", nullptr);
    if( pszSRCodeString == nullptr )
    {
        CPLDebug("BAG",
                 "Unable to find /MI_Metadata/referenceSystemInfo[1]/"
                 "MD_ReferenceSystem[1]/referenceSystemIdentifier[1]/"
                 "RS_Identifier[1]/code[1]/CharacterString[1] in metadata.");
        CPLDestroyXMLNode(psRoot);
        return OGRERR_FAILURE;
    }

    const char *pszSRCodeSpace =
        CPLGetXMLValue(psRSI, "MD_ReferenceSystem.referenceSystemIdentifier."
                       "RS_Identifier.codeSpace.CharacterString", "");
    if( !EQUAL(pszSRCodeSpace, "WKT") )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Spatial reference string is not in WKT.");
        CPLDestroyXMLNode(psRoot);
        return OGRERR_FAILURE;
    }

    if( oSRS.importFromWkt(pszSRCodeString) != OGRERR_NONE )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed parsing WKT string \"%s\".", pszSRCodeString);
        CPLDestroyXMLNode(psRoot);
        return OGRERR_FAILURE;
    }

    oSRS.exportToWkt(&pszProjection);

    psRSI = CPLSearchXMLNode(psRSI->psNext, "=referenceSystemInfo");
    if( psRSI == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to find second instance of <referenceSystemInfo> "
                 "in metadata.");
        CPLDestroyXMLNode(psRoot);
        return OGRERR_NONE;
    }

    pszSRCodeString =
      CPLGetXMLValue(psRSI, "MD_ReferenceSystem.referenceSystemIdentifier."
                     "RS_Identifier.code.CharacterString", nullptr);
    if( pszSRCodeString == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to find /MI_Metadata/referenceSystemInfo[2]/"
                 "MD_ReferenceSystem[1]/referenceSystemIdentifier[1]/"
                 "RS_Identifier[1]/code[1]/CharacterString[1] in metadata.");
        CPLDestroyXMLNode(psRoot);
        return OGRERR_NONE;
    }

    pszSRCodeSpace =
        CPLGetXMLValue(psRSI, "MD_ReferenceSystem.referenceSystemIdentifier."
                       "RS_Identifier.codeSpace.CharacterString", "");
    if( !EQUAL(pszSRCodeSpace, "WKT") )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Spatial reference string is not in WKT.");
        CPLDestroyXMLNode(psRoot);
        return OGRERR_NONE;
    }

    if( STARTS_WITH_CI(pszSRCodeString, "VERTCS") )
    {
        CPLString oString(pszProjection);
        CPLFree(pszProjection);
        oString += ",";
        oString += pszSRCodeString;
        pszProjection = CPLStrdup(oString);
    }

    CPLDestroyXMLNode(psRoot);

    return OGRERR_NONE;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr BAGDataset::GetGeoTransform( double *padfGeoTransform )

{
    if( adfGeoTransform[0] != 0.0 || adfGeoTransform[3] != 0.0 )
    {
        memcpy(padfGeoTransform, adfGeoTransform, sizeof(double)*6);
        return CE_None;
    }

    return GDALPamDataset::GetGeoTransform(padfGeoTransform);
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *BAGDataset::GetProjectionRef()

{
    if( pszProjection )
        return pszProjection;

    return GDALPamDataset::GetProjectionRef();
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **BAGDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "xml:BAG", NULL);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **BAGDataset::GetMetadata( const char *pszDomain )

{
    if( pszDomain != nullptr && EQUAL(pszDomain,"xml:BAG") )
    {
        apszMDList[0] = pszXMLMetadata;
        apszMDList[1] = nullptr;

        return apszMDList;
    }

    if( pszDomain != nullptr && EQUAL(pszDomain,"SUBDATASETS") )
    {
        return m_aosSubdatasets.List();
    }

    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                      BAGDatasetDriverUnload()                        */
/************************************************************************/

static void BAGDatasetDriverUnload(GDALDriver*)
{
    HDF5UnloadFileDriver();
}

/************************************************************************/
/*                              BAGCreator                              */
/************************************************************************/

class BAGCreator
{
        hid_t m_hdf5 = -1;
        hid_t m_bagRoot = -1;

        static bool SubstituteVariables(CPLXMLNode* psNode, char** papszDict);
        static CPLString GenerateMatadata(GDALDataset *poSrcDS,
                                          char ** papszOptions);
        bool CreateAndWriteMetadata(const CPLString& osXMLMetadata);
        bool CreateTrackingListDataset();
        bool CreateElevationOrUncertainty(GDALDataset *poSrcDS,
                                          int nBand,
                                          const char* pszDSName,
                                          const char* pszMaxAttrName,
                                          const char* pszMinAttrName,
                                          char ** papszOptions,
                                          GDALProgressFunc pfnProgress,
                                          void *pProgressData);
        bool Close();

    public:
        BAGCreator() = default;
        ~BAGCreator();

        bool Create( const char *pszFilename, GDALDataset *poSrcDS,
                     char ** papszOptions,
                     GDALProgressFunc pfnProgress, void *pProgressData );
};

/************************************************************************/
/*                            ~BAGCreator()()                           */
/************************************************************************/

BAGCreator::~BAGCreator()
{
    Close();
}

/************************************************************************/
/*                               Close()                                */
/************************************************************************/

bool BAGCreator::Close()
{
    bool ret = true;
    if( m_bagRoot >= 0 )
    {
        ret = (H5_CHECK(H5Gclose(m_bagRoot)) >=0) && ret;
        m_bagRoot = -1;
    }
    if( m_hdf5 >= 0 )
    {
        ret = (H5_CHECK(H5Fclose(m_hdf5)) >= 0) && ret;
        m_hdf5 = -1;
    }
    return ret;
}

/************************************************************************/
/*                         SubstituteVariables()                        */
/************************************************************************/

bool BAGCreator::SubstituteVariables(CPLXMLNode* psNode, char** papszDict)
{
    if( psNode->eType == CXT_Text && psNode->pszValue &&
        strstr(psNode->pszValue, "${") )
    {
        CPLString osVal(psNode->pszValue);
        size_t nPos = 0;
        while(true)
        {
            nPos = osVal.find("${", nPos);
            if( nPos == std::string::npos )
            {
                break;
            }
            CPLString osKeyName;
            bool bHasDefaultValue = false;
            CPLString osDefaultValue;
            size_t nAfterKeyName = 0;
            for( size_t i = nPos + 2; i < osVal.size(); i++ )
            {
                if( osVal[i] == ':' )
                {
                    osKeyName = osVal.substr(nPos + 2, i - (nPos + 2));
                }
                else if( osVal[i] == '}' )
                {
                    if( osKeyName.empty() )
                    {
                        osKeyName = osVal.substr(nPos + 2, i - (nPos + 2));
                    }
                    else
                    {
                        bHasDefaultValue = true;
                        size_t nStartDefaultVal = nPos + 2 + osKeyName.size() + 1;
                        osDefaultValue = osVal.substr(nStartDefaultVal,
                                                      i - nStartDefaultVal);
                    }
                    nAfterKeyName = i + 1;
                    break;
                }
            }
            if( nAfterKeyName == 0 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid variable name in template");
                return false;
            }

            bool bSubstFound = false;
            for( char** papszIter = papszDict;
                !bSubstFound && papszIter && *papszIter; papszIter++ )
            {
                if( STARTS_WITH_CI(*papszIter, "VAR_") )
                {
                    char* pszKey = nullptr;
                    const char* pszValue = CPLParseNameValue(*papszIter, &pszKey);
                    if( pszKey && pszValue )
                    {
                        const char* pszVarName = pszKey + strlen("VAR_");
                        if( EQUAL(pszVarName, osKeyName) )
                        {
                            bSubstFound = true;
                            osVal = osVal.substr(0, nPos) + pszValue +
                                    osVal.substr(nAfterKeyName);
                        }
                        CPLFree(pszKey);
                    }
                }
            }
            if( !bSubstFound )
            {
                if( bHasDefaultValue )
                {
                    osVal = osVal.substr(0, nPos) + osDefaultValue +
                            osVal.substr(nAfterKeyName);
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                        "%s could not be substituted", osKeyName.c_str());
                    return false;
                }
            }
        }

        if( !osVal.empty() && osVal[0] == '<' && osVal.back() == '>' )
        {
            CPLXMLNode* psSubNode = CPLParseXMLString(osVal);
            if( psSubNode )
            {
                CPLFree(psNode->pszValue);
                psNode->eType = psSubNode->eType;
                psNode->pszValue = psSubNode->pszValue;
                psNode->psChild = psSubNode->psChild;
                psSubNode->pszValue = nullptr;
                psSubNode->psChild = nullptr;
                CPLDestroyXMLNode(psSubNode);
            }
            else
            {
                CPLFree(psNode->pszValue);
                psNode->pszValue = CPLStrdup(osVal);
            }
        }
        else
        {
            CPLFree(psNode->pszValue);
            psNode->pszValue = CPLStrdup(osVal);
        }
    }

    for(CPLXMLNode* psIter = psNode->psChild; psIter; psIter = psIter->psNext)
    {
        if( !SubstituteVariables(psIter, papszDict) )
            return false;
    }
    return true;
}

/************************************************************************/
/*                          GenerateMatadata()                          */
/************************************************************************/

CPLString BAGCreator::GenerateMatadata(GDALDataset *poSrcDS,
                                       char ** papszOptions)
{
    CPLXMLNode* psRoot;
    CPLString osTemplateFilename = CSLFetchNameValueDef(papszOptions,
                                                      "TEMPLATE", "");
    if( !osTemplateFilename.empty() )
    {
        psRoot = CPLParseXMLFile(osTemplateFilename);
    }
    else
    {
        const char* pszDefaultTemplateFilename =
                                CPLFindFile("gdal", "bag_template.xml");
        if( pszDefaultTemplateFilename == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find bag_template.xml and TEMPLATE "
                     "creation option not specified");
            return CPLString();
        }
        psRoot = CPLParseXMLFile(pszDefaultTemplateFilename);
    }
    if( psRoot == nullptr )
        return CPLString();
    CPLXMLTreeCloser oCloser(psRoot);

    CPLXMLNode* psMain = psRoot;
    for(; psMain; psMain = psMain->psNext )
    {
        if( psMain->eType == CXT_Element &&
            !STARTS_WITH(psMain->pszValue, "?") )
        {
            break;
        }
    }
    if( psMain == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find main XML node");
        return CPLString();
    }

    CPLStringList osOptions(papszOptions, FALSE);
    if( osOptions.FetchNameValue("VAR_PROCESS_STEP_DESCRIPTION") == nullptr )
    {
        osOptions.SetNameValue("VAR_PROCESS_STEP_DESCRIPTION",
            CPLSPrintf("Generated by GDAL %s", GDALVersionInfo("RELEASE_NAME")));
    }
    osOptions.SetNameValue("VAR_HEIGHT", CPLSPrintf("%d",
                                                poSrcDS->GetRasterYSize()));
    osOptions.SetNameValue("VAR_WIDTH", CPLSPrintf("%d",
                                                poSrcDS->GetRasterXSize()));

    struct tm brokenDown;
    CPLUnixTimeToYMDHMS(time(nullptr), &brokenDown);
    if( osOptions.FetchNameValue("VAR_DATE") == nullptr )
    {
        osOptions.SetNameValue("VAR_DATE", CPLSPrintf("%04d-%02d-%02d",
                               brokenDown.tm_year + 1900,
                               brokenDown.tm_mon + 1,
                               brokenDown.tm_mday));
    }
    if( osOptions.FetchNameValue("VAR_DATETIME") == nullptr )
    {
        osOptions.SetNameValue("VAR_DATETIME", CPLSPrintf(
                               "%04d-%02d-%02dT%02d:%02d:%02d",
                               brokenDown.tm_year + 1900,
                               brokenDown.tm_mon + 1,
                               brokenDown.tm_mday,
                               brokenDown.tm_hour,
                               brokenDown.tm_min,
                               brokenDown.tm_sec));
    }

    double adfGeoTransform[6];
    poSrcDS->GetGeoTransform(adfGeoTransform);
    osOptions.SetNameValue("VAR_RESX", CPLSPrintf("%.18g",
                                              adfGeoTransform[1]));
    osOptions.SetNameValue("VAR_RESY", CPLSPrintf("%.18g",
                                              fabs(adfGeoTransform[5])));
    osOptions.SetNameValue("VAR_RES", CPLSPrintf("%.18g",
                    std::max(adfGeoTransform[1], fabs(adfGeoTransform[5]))));

    const char* pszProjection = poSrcDS->GetProjectionRef();
    if( pszProjection == nullptr || EQUAL(pszProjection, "") )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "BAG driver requires a source dataset with a projection");
    }
    OGRSpatialReference oSRS;
    oSRS.SetFromUserInput(pszProjection);

    osOptions.SetNameValue("VAR_HORIZ_WKT", pszProjection);

    if( oSRS.IsCompound() )
    {
        auto node = oSRS.GetRoot();
        if( node && node->GetChildCount() == 3 )
        {
            char* pszHorizWKT = nullptr;
            node->GetChild(1)->exportToWkt(&pszHorizWKT);
            char* pszVertWKT = nullptr;
            node->GetChild(2)->exportToWkt(&pszVertWKT);

            oSRS.SetFromUserInput(pszHorizWKT);

            osOptions.SetNameValue("VAR_HORIZ_WKT", pszHorizWKT);
            if( osOptions.FetchNameValue("VAR_VERT_WKT") == nullptr )
            {
                osOptions.SetNameValue("VAR_VERT_WKT", pszVertWKT);
            }
            CPLFree(pszHorizWKT);
            CPLFree(pszVertWKT);
        }
    }

    const char* pszUnits = "m";
    if( oSRS.IsProjected() )
    {
        oSRS.GetLinearUnits(&pszUnits);
        if( EQUAL(pszUnits, "metre") )
            pszUnits = "m";
    }
    else
    {
        pszUnits = "deg";
    }
    osOptions.SetNameValue("VAR_RES_UNIT", pszUnits);

    // Center pixel convention
    double dfMinX = adfGeoTransform[0] + adfGeoTransform[1] / 2;
    double dfMaxX = dfMinX + (poSrcDS->GetRasterXSize() - 1) * adfGeoTransform[1];
    double dfMaxY = adfGeoTransform[3] + adfGeoTransform[5] / 2;
    double dfMinY = dfMaxY + (poSrcDS->GetRasterYSize() - 1) * adfGeoTransform[5];
    if( adfGeoTransform[5] > 0 )
    {
        std::swap(dfMinY, dfMaxY);
    }
    osOptions.SetNameValue("VAR_CORNER_POINTS",
                           CPLSPrintf("%.18g,%.18g %.18g,%.18g",
                                      dfMinX, dfMinY, dfMaxX, dfMaxY));

    double adfCornerX[4] = { dfMinX, dfMinX, dfMaxX, dfMaxX };
    double adfCornerY[4] = { dfMinY, dfMaxY, dfMaxY, dfMinY };
    OGRSpatialReference oSRS_WGS84;
    oSRS_WGS84.SetFromUserInput("WGS84");
    OGRCoordinateTransformation* poCT =
        OGRCreateCoordinateTransformation(&oSRS, &oSRS_WGS84);
    if( !poCT )
        return CPLString();
    if( !poCT->Transform(4, adfCornerX, adfCornerY) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot compute raster extent in geodetic coordinates");
        delete poCT;
        return CPLString();
    }
    delete poCT;
    double dfWest = std::min(std::min(adfCornerX[0], adfCornerX[1]),
                             std::min(adfCornerX[2], adfCornerX[3]));
    double dfSouth = std::min(std::min(adfCornerY[0], adfCornerY[1]),
                              std::min(adfCornerY[2], adfCornerY[3]));
    double dfEast = std::max(std::max(adfCornerX[0], adfCornerX[1]),
                             std::max(adfCornerX[2], adfCornerX[3]));
    double dfNorth = std::max(std::max(adfCornerY[0], adfCornerY[1]),
                              std::max(adfCornerY[2], adfCornerY[3]));
    osOptions.SetNameValue("VAR_WEST_LONGITUDE", CPLSPrintf("%.18g", dfWest));
    osOptions.SetNameValue("VAR_SOUTH_LATITUDE", CPLSPrintf("%.18g", dfSouth));
    osOptions.SetNameValue("VAR_EAST_LONGITUDE", CPLSPrintf("%.18g", dfEast));
    osOptions.SetNameValue("VAR_NORTH_LATITUDE", CPLSPrintf("%.18g", dfNorth));

    if( !SubstituteVariables(psMain, osOptions.List()) )
    {
        return CPLString();
    }

    char* pszXML = CPLSerializeXMLTree(psRoot);
    CPLString osXML(pszXML);
    CPLFree(pszXML);
    return osXML;
}

/************************************************************************/
/*                         CreateAndWriteMetadata()                     */
/************************************************************************/

bool BAGCreator::CreateAndWriteMetadata(const CPLString& osXMLMetadata)
{
    hsize_t dim_init[1] = { 1 + osXMLMetadata.size() };
    hsize_t dim_max[1] = { H5S_UNLIMITED };

    hid_t hDataSpace = H5_CHECK(H5Screate_simple(1, dim_init, dim_max));
    if( hDataSpace < 0 )
        return false;

    hid_t hParams = -1;
    hid_t hDataType = -1;
    hid_t hDatasetID = -1;
    hid_t hFileSpace = -1;
    bool ret = false;
    do
    {
        hParams = H5_CHECK(H5Pcreate (H5P_DATASET_CREATE));
        if( hParams < 0 )
            break;

        hsize_t chunk_dims[1] = { 1024 };
        if( H5_CHECK(H5Pset_chunk (hParams, 1, chunk_dims)) < 0 )
            break;

        hDataType = H5_CHECK(H5Tcopy(H5T_C_S1));
        if( hDataType < 0 )
            break;

        hDatasetID = H5_CHECK(H5Dcreate(m_hdf5, "/BAG_root/metadata",
                               hDataType, hDataSpace, hParams));
        if( hDatasetID < 0)
            break;

        if( H5_CHECK(H5Dextend (hDatasetID, dim_init)) < 0)
            break;

        hFileSpace = H5_CHECK(H5Dget_space(hDatasetID));
        if( hFileSpace < 0 )
            break;

        H5OFFSET_TYPE offset[1] = { 0 };
        if( H5_CHECK(H5Sselect_hyperslab (hFileSpace, H5S_SELECT_SET, offset,
                                 nullptr, dim_init, nullptr)) < 0 )
        {
            break;
        }

        if( H5_CHECK(H5Dwrite (hDatasetID, hDataType, hDataSpace, hFileSpace, 
                           H5P_DEFAULT, osXMLMetadata.data())) < 0 )
        {
            break;
        }

        ret = true;
    }
    while(0);

    if( hParams >= 0 )
        H5_CHECK(H5Pclose(hParams));
    if( hDataType >= 0 )
        H5_CHECK(H5Tclose(hDataType));
    if( hFileSpace >= 0 )
        H5_CHECK(H5Sclose(hFileSpace));
    if( hDatasetID >= 0 )
        H5_CHECK(H5Dclose(hDatasetID));
    H5_CHECK(H5Sclose(hDataSpace));

    return ret;
}

/************************************************************************/
/*                       CreateTrackingListDataset()                    */
/************************************************************************/

bool BAGCreator::CreateTrackingListDataset()
{
    typedef struct 
    {
        uint32_t row;
        uint32_t col;
        float depth;
        float uncertainty;
        uint8_t  track_code;
        uint16_t list_series;
    } TrackingListItem;

    hsize_t dim_init[1] = { 0 };
    hsize_t dim_max[1] = { H5S_UNLIMITED };

    hid_t hDataSpace = H5_CHECK(H5Screate_simple(1, dim_init, dim_max));
    if( hDataSpace < 0 )
        return false;

    hid_t hParams = -1;
    hid_t hDataType = -1;
    hid_t hDatasetID = -1;
    bool ret = false;
    do
    {
        hParams = H5_CHECK(H5Pcreate (H5P_DATASET_CREATE));
        if( hParams < 0 )
            break;

        hsize_t chunk_dims[1] = { 10 };
        if( H5_CHECK(H5Pset_chunk (hParams, 1, chunk_dims)) < 0 )
            break;

        TrackingListItem unusedItem;
        (void)unusedItem.row;
        (void)unusedItem.col;
        (void)unusedItem.depth;
        (void)unusedItem.uncertainty;
        (void)unusedItem.track_code;
        (void)unusedItem.list_series;
        hDataType = H5_CHECK(H5Tcreate(H5T_COMPOUND, sizeof(unusedItem)));
        if( hDataType < 0 )
            break;

        if( H5Tinsert(hDataType, "row",
                      HOFFSET(TrackingListItem, row), H5T_NATIVE_UINT) < 0 ||
            H5Tinsert(hDataType, "col",
                      HOFFSET(TrackingListItem, col), H5T_NATIVE_UINT) < 0 ||
            H5Tinsert(hDataType, "depth", HOFFSET(TrackingListItem, depth),
                      H5T_NATIVE_FLOAT) < 0 ||
            H5Tinsert(hDataType, "uncertainty",
                      HOFFSET(TrackingListItem, uncertainty), H5T_NATIVE_FLOAT) < 0 ||
            H5Tinsert(hDataType, "track_code",
                      HOFFSET(TrackingListItem, track_code), H5T_NATIVE_UCHAR) < 0 ||
            H5Tinsert(hDataType, "list_series",
                      HOFFSET(TrackingListItem, list_series), H5T_NATIVE_SHORT) < 0 )
        {
            break;
        }

        hDatasetID = H5_CHECK(H5Dcreate(m_hdf5, "/BAG_root/tracking_list",
                               hDataType, hDataSpace, hParams));
        if( hDatasetID < 0)
            break;

        if( H5_CHECK(H5Dextend (hDatasetID, dim_init)) < 0)
            break;

        if( !GH5_CreateAttribute(hDatasetID, "Tracking List Length",
                                 H5T_NATIVE_UINT) )
            break;

        if( !GH5_WriteAttribute(hDatasetID, "Tracking List Length", 0U) )
            break;

        ret = true;
    }
    while(0);

    if( hParams >= 0 )
        H5_CHECK(H5Pclose(hParams));
    if( hDataType >= 0 )
        H5_CHECK(H5Tclose(hDataType));
    if( hDatasetID >= 0 )
        H5_CHECK(H5Dclose(hDatasetID));
    H5_CHECK(H5Sclose(hDataSpace));

    return ret;
}

/************************************************************************/
/*                     CreateElevationOrUncertainty()                   */
/************************************************************************/

bool BAGCreator::CreateElevationOrUncertainty(GDALDataset *poSrcDS,
                                              int nBand,
                                              const char* pszDSName,
                                              const char* pszMaxAttrName,
                                              const char* pszMinAttrName,
                                              char ** papszOptions,
                                              GDALProgressFunc pfnProgress,
                                              void *pProgressData)
{
    const int nYSize = poSrcDS->GetRasterYSize();
    const int nXSize = poSrcDS->GetRasterXSize();

    double adfGeoTransform[6];
    poSrcDS->GetGeoTransform(adfGeoTransform);

    hsize_t dims[2] = { static_cast<hsize_t>(nYSize),
                        static_cast<hsize_t>(nXSize) };

    hid_t hDataSpace = H5_CHECK(H5Screate_simple(2, dims, nullptr));
    if( hDataSpace < 0 )
        return false;

    hid_t hParams = -1;
    hid_t hDataType = -1;
    hid_t hDatasetID = -1;
    hid_t hFileSpace = -1;
    bool bDeflate = EQUAL(CSLFetchNameValueDef(
        papszOptions, "COMPRESS", "DEFLATE"), "DEFLATE");
    int nCompressionLevel = atoi(
        CSLFetchNameValueDef(papszOptions, "ZLEVEL", "6"));
    const int nBlockSize = std::min(4096, atoi(
        CSLFetchNameValueDef(papszOptions, "BLOCK_SIZE", "100")));
    const int nBlockXSize = std::min(nXSize, nBlockSize);
    const int nBlockYSize = std::min(nYSize, nBlockSize);
    bool ret = false;
    const float fNoDataValue = fDEFAULT_NODATA;
    do
    {
        hDataType = H5_CHECK(H5Tcopy(H5T_NATIVE_FLOAT));
        if( hDataType < 0 )
            break;

        if( H5_CHECK(H5Tset_order(hDataType, H5T_ORDER_LE)) < 0)
            break;

        hParams = H5_CHECK(H5Pcreate(H5P_DATASET_CREATE));
        if( hParams < 0 )
            break;

        if( H5_CHECK(H5Pset_fill_time(hParams, H5D_FILL_TIME_ALLOC)) < 0)
            break;

        if( H5_CHECK(H5Pset_fill_value(hParams, hDataType, &fNoDataValue)) < 0)
            break;

        if( H5_CHECK(H5Pset_layout(hParams, H5D_CHUNKED)) < 0)
            break;
        hsize_t chunk_size[2] = {
            static_cast<hsize_t>(nBlockYSize),
            static_cast<hsize_t>(nBlockXSize) };
        if( H5_CHECK(H5Pset_chunk(hParams, 2, chunk_size)) < 0)
            break;

        if( bDeflate )
        {
            if( H5_CHECK(H5Pset_deflate(hParams, nCompressionLevel)) < 0)
                break;
        }

        hDatasetID = H5_CHECK(H5Dcreate(m_hdf5, pszDSName,
                               hDataType, hDataSpace, hParams));
        if( hDatasetID < 0)
            break;

        if( !GH5_CreateAttribute(hDatasetID, pszMaxAttrName, H5T_NATIVE_FLOAT) )
            break;

        if( !GH5_CreateAttribute(hDatasetID, pszMinAttrName, H5T_NATIVE_FLOAT) )
            break;

        hFileSpace = H5_CHECK(H5Dget_space(hDatasetID));
        if( hFileSpace < 0 )
            break;

        int nYBlocks = static_cast<int>((nYSize + nBlockYSize - 1) /
                                        nBlockYSize);
        int nXBlocks = static_cast<int>((nXSize + nBlockXSize - 1) /
                                        nBlockXSize);
        std::vector<float> afValues(nBlockYSize * nBlockXSize);
        ret = true;
        const bool bReverseY = adfGeoTransform[5] < 0;

        float fMin = std::numeric_limits<float>::infinity();
        float fMax = -std::numeric_limits<float>::infinity();

        if( nBand == 1 || poSrcDS->GetRasterCount() == 2 )
        {
            int bHasNoData = FALSE;
            const double dfSrcNoData =
                poSrcDS->GetRasterBand(nBand)->GetNoDataValue(&bHasNoData);
            const float fSrcNoData = static_cast<float>(dfSrcNoData);

            for(int iY = 0; ret && iY < nYBlocks; iY++ )
            {
                const int nSrcYOff = bReverseY ?
                    std::max(0, nYSize - (iY + 1) * nBlockYSize) :
                    iY * nBlockYSize;
                const int nReqCountY =
                    std::min(nBlockYSize, nYSize - iY * nBlockYSize);
                for(int iX = 0; iX < nXBlocks; iX++ )
                {
                    const int nReqCountX =
                        std::min(nBlockXSize, nXSize - iX * nBlockXSize);

                    if( poSrcDS->GetRasterBand(nBand)->RasterIO(GF_Read,
                        iX * nBlockXSize,
                        nSrcYOff,
                        nReqCountX, nReqCountY,
                        bReverseY ?
                            afValues.data() + (nReqCountY - 1) * nReqCountX:
                            afValues.data(),
                        nReqCountX, nReqCountY,
                        GDT_Float32, 
                        0,
                        bReverseY ? -4 * nReqCountX : 0,
                        nullptr) != CE_None )
                    {
                        ret = false;
                        break;
                    }

                    for( int i = 0; i < nReqCountY * nReqCountX; i++ )
                    {
                        const float fVal = afValues[i];
                        if( (bHasNoData && fVal == fSrcNoData) || std::isnan(fVal) )
                        {
                            afValues[i] = fNoDataValue;
                        }
                        else
                        {
                            fMin = std::min(fMin, fVal);
                            fMax = std::max(fMax, fVal);
                        }
                    }

                    H5OFFSET_TYPE offset[2] = {
                        static_cast<H5OFFSET_TYPE>(iY * nBlockYSize),
                        static_cast<H5OFFSET_TYPE>(iX * nBlockXSize)
                    };
                    hsize_t count[2] = {
                        static_cast<hsize_t>(nReqCountY),
                        static_cast<hsize_t>(nReqCountX)
                    };
                    if( H5_CHECK(H5Sselect_hyperslab(
                            hFileSpace, H5S_SELECT_SET,
                            offset, nullptr, count, nullptr)) < 0 )
                    {
                        ret = false;
                        break;
                    }

                    hid_t hMemSpace = H5Screate_simple(2, count, nullptr);
                    if( hMemSpace < 0 )
                        break;

                    if( H5_CHECK(H5Dwrite(
                            hDatasetID, hDataType, hMemSpace, hFileSpace, 
                            H5P_DEFAULT, afValues.data())) < 0 )
                    {
                        H5Sclose(hMemSpace);
                        ret = false;
                        break;
                    }

                    H5Sclose(hMemSpace);

                    if( !pfnProgress(
                        static_cast<double>(iY * nXBlocks + iX + 1) /
                            (nXBlocks * nYBlocks), "", pProgressData) )
                    {
                        ret = false;
                        break;
                    }
                }
            }
        }
        if( !ret )
            break;
        ret = false;

        if( fMin > fMax )
            fMin = fMax = fNoDataValue;

        if( !GH5_WriteAttribute(hDatasetID, pszMaxAttrName, fMax) )
            break;

        if( !GH5_WriteAttribute(hDatasetID, pszMinAttrName, fMin) )
            break;

        ret = true;
    }
    while(0);

    if( hParams >= 0 )
        H5_CHECK(H5Pclose(hParams));
    if( hDataType >= 0 )
        H5_CHECK(H5Tclose(hDataType));
    if( hFileSpace >= 0 )
        H5_CHECK(H5Sclose(hFileSpace));
    if( hDatasetID >= 0 )
        H5_CHECK(H5Dclose(hDatasetID));
    H5_CHECK(H5Sclose(hDataSpace));

    return ret;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

bool BAGCreator::Create( const char *pszFilename, GDALDataset *poSrcDS,
                         char ** papszOptions,
                         GDALProgressFunc pfnProgress, void *pProgressData )
{
    const int nBands = poSrcDS->GetRasterCount();
    if( nBands != 1 && nBands != 2 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "BAG driver doesn't support %d bands. Must be 1 or 2.", nBands);
        return false;
    }
    double adfGeoTransform[6];
    if( poSrcDS->GetGeoTransform(adfGeoTransform) != CE_None )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "BAG driver requires a source dataset with a geotransform");
        return false;
    }
    if( adfGeoTransform[2] != 0 || adfGeoTransform[4] != 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "BAG driver requires a source dataset with a non-rotated "
                 "geotransform");
        return false;
    }

    CPLString osXMLMetadata = GenerateMatadata(poSrcDS, papszOptions);
    if( osXMLMetadata.empty() )
    {
        return false;
    }

    hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
    H5Pset_driver(fapl, HDF5GetFileDriver(), nullptr);
    m_hdf5 = H5Fcreate(pszFilename, H5F_ACC_TRUNC, H5P_DEFAULT, fapl);
    H5Pclose(fapl);
    if( m_hdf5 < 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create file");
        return false;
    }

    m_bagRoot = H5_CHECK(H5Gcreate(m_hdf5, "/BAG_root", 0));
    if( m_bagRoot < 0 )
    {
        return false;
    }

    const char* pszVersion = CSLFetchNameValueDef(papszOptions,
                                                  "BAG_VERSION", "1.6.2");
    constexpr unsigned knVersionLength = 32;
    char szVersion[knVersionLength] = {};
    snprintf(szVersion, sizeof(szVersion), "%s", pszVersion);
    if( !GH5_CreateAttribute(m_bagRoot, "Bag Version", H5T_C_S1,
                             knVersionLength) ||
        !GH5_WriteAttribute(m_bagRoot, "Bag Version", szVersion) )
    {
        return false;
    }

    if( !CreateAndWriteMetadata(osXMLMetadata) )
    {
        return false;
    }
    if( !CreateTrackingListDataset() )
    {
        return false;
    }

    void* pScaled = GDALCreateScaledProgress(0, 1. / poSrcDS->GetRasterCount(),
                                             pfnProgress, pProgressData);
    bool bRet;
    bRet = CreateElevationOrUncertainty(poSrcDS, 1, "/BAG_root/elevation",
                                      "Maximum Elevation Value",
                                      "Minimum Elevation Value",
                                      papszOptions,
                                      GDALScaledProgress, pScaled);
    GDALDestroyScaledProgress(pScaled);
    if( !bRet )
    {
        return false;
    }

    pScaled = GDALCreateScaledProgress(1. / poSrcDS->GetRasterCount(), 1.0,
                                       pfnProgress, pProgressData);
    bRet = CreateElevationOrUncertainty(poSrcDS, 2, "/BAG_root/uncertainty",
                                      "Maximum Uncertainty Value",
                                      "Minimum Uncertainty Value",
                                      papszOptions,
                                      GDALScaledProgress, pScaled);
    GDALDestroyScaledProgress(pScaled);
    if( !bRet )
    {
        return false;
    }

    return Close();
}

/************************************************************************/
/*                              CreateCopy()                            */
/************************************************************************/

GDALDataset *
BAGDataset::CreateCopy( const char *pszFilename, GDALDataset *poSrcDS,
                        int /* bStrict */, char ** papszOptions,
                        GDALProgressFunc pfnProgress, void *pProgressData )

{
    if( !BAGCreator().Create(pszFilename, poSrcDS, papszOptions,
                             pfnProgress, pProgressData) )
    {
        return nullptr;
    }

    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    return Open(&oOpenInfo);
}

/************************************************************************/
/*                          GDALRegister_BAG()                          */
/************************************************************************/
void GDALRegister_BAG()

{
    if( !GDAL_CHECK_VERSION("BAG") )
        return;

    if( GDALGetDriverByName("BAG") != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("BAG");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Bathymetry Attributed Grid");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_bag.html");
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "bag");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Float32");

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"   <Option name='MODE' type='string-select' default='AUTO'>"
"       <Value>AUTO</Value>"
"       <Value>LOW_RES_GRID</Value>"
"       <Value>LIST_SUPERGRIDS</Value>"
"       <Value>RESAMPLED_GRID</Value>"
"   </Option>"
"   <Option name='SUPERGRIDS_INDICES' type='string' description="
    "'Tuple(s) (y1,x1),(y2,x2),...  of supergrids, by indices, to expose as subdatasets'/>"
"   <Option name='MINX' type='float' description='Minimum X value of area of interest'/>"
"   <Option name='MINY' type='float' description='Minimum Y value of area of interest'/>"
"   <Option name='MAXX' type='float' description='Maximum X value of area of interest'/>"
"   <Option name='MAXY' type='float' description='Maximum Y value of area of interest'/>"
"   <Option name='RESX' type='float' description="
    "'Horizontal resolution. Only used for MODE=RESAMPLED_GRID'/>"
"   <Option name='RESY' type='float' description="
    "'Vertical resolution (positive value). Only used for MODE=RESAMPLED_GRID'/>"
"   <Option name='RES_STRATEGY' type='string-select' description="
    "'Which strategy to apply to select the resampled grid resolution. "
    "Only used for MODE=RESAMPLED_GRID' default='AUTO'>"
"       <Value>AUTO</Value>"
"       <Value>MIN</Value>"
"       <Value>MAX</Value>"
"       <Value>MEAN</Value>"
"   </Option>"
"   <Option name='RES_FILTER_MIN' type='float' description="
    "'Minimum resolution of supergrids to take into account (excluded bound). "
    "Only used for MODE=RESAMPLED_GRID or LIST_SUPERGRIDS' default='0'/>"
"   <Option name='RES_FILTER_MAX' type='float' description="
    "'Maximum resolution of supergrids to take into account (included bound). "
    "Only used for MODE=RESAMPLED_GRID or LIST_SUPERGRIDS' default='inf'/>"
"   <Option name='VALUE_POPULATION' type='string-select' description="
    "'Which value population strategy to apply to compute the resampled cell "
    "values. Only used for MODE=RESAMPLED_GRID' default='MAX'>"
"       <Value>MIN</Value>"
"       <Value>MAX</Value>"
"       <Value>MEAN</Value>"
"   </Option>"
"   <Option name='SUPERGRIDS_MASK' type='boolean' description="
    "'Whether the dataset should consist of a mask band indicating if a "
    "supergrid node matches each target pixel. Only used for "
    "MODE=RESAMPLED_GRID' default='NO'/>"
"   <Option name='INTERPOLATION' type='string' description="
    "'Interpolation method. Currently only INVDIST supported' default='NO'/>"
"   <Option name='NODATA_VALUE' type='float' default='1000000'/>"
"</OpenOptionList>" );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>"
"  <Option name='VAR_*' type='string' description="
                    "'Value to substitute to a variable in the template'/>"
"  <Option name='TEMPLATE' type='string' description="
                    "'.xml template to use'/>"
"  <Option name='BAG_VERSION' type='string' description="
        "'Version to write in the Bag Version attribute' default='1.6.2'/>"
"  <Option name='COMPRESS' type='string-select' default='DEFLATE'>"
"    <Value>NONE</Value>"
"    <Value>DEFLATE</Value>"
"  </Option>"
"  <Option name='ZLEVEL' type='int' "
    "description='DEFLATE compression level 1-9' default='6' />"
"  <Option name='BLOCK_SIZE' type='int' description='Chunk size' />"
"</CreationOptionList>" );

    poDriver->pfnOpen = BAGDataset::Open;
    poDriver->pfnIdentify = BAGDataset::Identify;
    poDriver->pfnUnloadDriver = BAGDatasetDriverUnload;
    poDriver->pfnCreateCopy = BAGDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
