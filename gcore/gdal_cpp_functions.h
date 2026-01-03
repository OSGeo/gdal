/******************************************************************************
 *
 * Name:     gdal_cpp_functions.h
 * Project:  GDAL Core
 * Purpose:  Declaration of various semi-primate C++ functions
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_CPP_FUNCTIONS_H_INCLUDED
#define GDAL_CPP_FUNCTIONS_H_INCLUDED

#include "cpl_port.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"

#include "gdal.h"
#include "gdal_gcp.h"
#include "gdal_geotransform.h"

#include <cstddef>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

typedef struct _CPLMutex CPLMutex;

class GDALColorTable;
class GDALDataset;
class GDALDriver;
class GDALMDArray;
class GDALRasterAttributeTable;
class GDALRasterBand;
class OGRSpatialReference;

/* ==================================================================== */
/*      An assortment of overview related stuff.                        */
/* ==================================================================== */

//! @cond Doxygen_Suppress
/* Only exported for drivers as plugin. Signature may change */
CPLErr CPL_DLL GDALRegenerateOverviewsMultiBand(
    int nBands, GDALRasterBand *const *papoSrcBands, int nOverviews,
    GDALRasterBand *const *const *papapoOverviewBands,
    const char *pszResampling, GDALProgressFunc pfnProgress,
    void *pProgressData, CSLConstList papszOptions);

CPLErr CPL_DLL GDALRegenerateOverviewsMultiBand(
    const std::vector<GDALRasterBand *> &apoSrcBands,
    // First level of array is indexed by band (thus aapoOverviewBands.size() must be equal to apoSrcBands.size())
    // Second level is indexed by overview
    const std::vector<std::vector<GDALRasterBand *>> &aapoOverviewBands,
    const char *pszResampling, GDALProgressFunc pfnProgress,
    void *pProgressData, CSLConstList papszOptions);

/************************************************************************/
/*                       GDALOverviewResampleArgs                       */
/************************************************************************/

/** Arguments for overview resampling function. */
// Should not contain any dataset/rasterband object, as this might be
// read in a worker thread.
struct GDALOverviewResampleArgs
{
    //! Datatype of the source band argument
    GDALDataType eSrcDataType = GDT_Unknown;
    //! Datatype of the destination/overview band
    GDALDataType eOvrDataType = GDT_Unknown;
    //! Width in pixel of the destination/overview band
    int nOvrXSize = 0;
    //! Height in pixel of the destination/overview band
    int nOvrYSize = 0;
    //! NBITS value of the destination/overview band (or 0 if not set)
    int nOvrNBITS = 0;
    //! Factor to convert from destination X to source X
    // (source width divided by destination width)
    double dfXRatioDstToSrc = 0;
    //! Factor to convert from destination Y to source Y
    // (source height divided by destination height)
    double dfYRatioDstToSrc = 0;
    //! Sub-pixel delta to add to get source X
    double dfSrcXDelta = 0;
    //! Sub-pixel delta to add to get source Y
    double dfSrcYDelta = 0;
    //! Working data type (data type of the pChunk argument)
    GDALDataType eWrkDataType = GDT_Unknown;
    //! Array of nChunkXSize * nChunkYSize values of mask, or nullptr
    const GByte *pabyChunkNodataMask = nullptr;
    //! X offset of the source chunk in the source band
    int nChunkXOff = 0;
    //! Width in pixel of the source chunk in the source band
    int nChunkXSize = 0;
    //! Y offset of the source chunk in the source band
    int nChunkYOff = 0;
    //! Height in pixel of the source chunk in the source band
    int nChunkYSize = 0;
    //! X Offset of the destination chunk in the destination band
    int nDstXOff = 0;
    //! X Offset of the end (not included) of the destination chunk in the destination band
    int nDstXOff2 = 0;
    //! Y Offset of the destination chunk in the destination band
    int nDstYOff = 0;
    //! Y Offset of the end (not included) of the destination chunk in the destination band
    int nDstYOff2 = 0;
    //! Resampling method
    const char *pszResampling = nullptr;
    //! Whether the source band has a nodata value
    bool bHasNoData = false;
    //! Source band nodata value
    double dfNoDataValue = 0;
    //! Source color table
    const GDALColorTable *poColorTable = nullptr;
    //! Whether a single contributing source pixel at nodata should result
    // in the target pixel to be at nodata too (only taken into account by
    // average resampling)
    bool bPropagateNoData = false;
};

typedef CPLErr (*GDALResampleFunction)(const GDALOverviewResampleArgs &args,
                                       const void *pChunk, void **ppDstBuffer,
                                       GDALDataType *peDstBufferDataType);

GDALResampleFunction GDALGetResampleFunction(const char *pszResampling,
                                             int *pnRadius);

std::string CPL_DLL GDALGetNormalizedOvrResampling(const char *pszResampling);

GDALDataType GDALGetOvrWorkDataType(const char *pszResampling,
                                    GDALDataType eSrcDataType);

CPL_C_START

CPLErr CPL_DLL
HFAAuxBuildOverviews(const char *pszOvrFilename, GDALDataset *poParentDS,
                     GDALDataset **ppoDS, int nBands, const int *panBandList,
                     int nNewOverviews, const int *panNewOverviewList,
                     const char *pszResampling, GDALProgressFunc pfnProgress,
                     void *pProgressData, CSLConstList papszOptions);

CPLErr CPL_DLL GTIFFBuildOverviews(const char *pszFilename, int nBands,
                                   GDALRasterBand *const *papoBandList,
                                   int nOverviews, const int *panOverviewList,
                                   const char *pszResampling,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData,
                                   CSLConstList papszOptions);

CPLErr CPL_DLL GTIFFBuildOverviewsEx(const char *pszFilename, int nBands,
                                     GDALRasterBand *const *papoBandList,
                                     int nOverviews, const int *panOverviewList,
                                     const std::pair<int, int> *pasOverviewSize,
                                     const char *pszResampling,
                                     const char *const *papszOptions,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressData);

int CPL_DLL GDALBandGetBestOverviewLevel(GDALRasterBand *poBand, int &nXOff,
                                         int &nYOff, int &nXSize, int &nYSize,
                                         int nBufXSize, int nBufYSize)
    CPL_WARN_DEPRECATED("Use GDALBandGetBestOverviewLevel2 instead");
int CPL_DLL GDALBandGetBestOverviewLevel2(GDALRasterBand *poBand, int &nXOff,
                                          int &nYOff, int &nXSize, int &nYSize,
                                          int nBufXSize, int nBufYSize,
                                          GDALRasterIOExtraArg *psExtraArg);

int CPL_DLL GDALOvLevelAdjust(int nOvLevel, int nXSize)
    CPL_WARN_DEPRECATED("Use GDALOvLevelAdjust2 instead");
int CPL_DLL GDALOvLevelAdjust2(int nOvLevel, int nXSize, int nYSize);
int CPL_DLL GDALComputeOvFactor(int nOvrXSize, int nRasterXSize, int nOvrYSize,
                                int nRasterYSize);

GDALDataset CPL_DLL *GDALFindAssociatedAuxFile(const char *pszBasefile,
                                               GDALAccess eAccess,
                                               GDALDataset *poDependentDS);

/* ==================================================================== */
/*  Infrastructure to check that dataset characteristics are valid      */
/* ==================================================================== */

int CPL_DLL GDALCheckDatasetDimensions(int nXSize, int nYSize);
int CPL_DLL GDALCheckBandCount(int nBands, int bIsZeroAllowed);

/* Internal use only */

/* CPL_DLL exported, but only for in-tree drivers that can be built as plugins
 */
int CPL_DLL GDALReadWorldFile2(const char *pszBaseFilename,
                               const char *pszExtension,
                               double *padfGeoTransform,
                               CSLConstList papszSiblingFiles,
                               char **ppszWorldFileNameOut);
int CPL_DLL GDALReadTabFile2(const char *pszBaseFilename,
                             double *padfGeoTransform, char **ppszWKT,
                             int *pnGCPCount, GDAL_GCP **ppasGCPs,
                             CSLConstList papszSiblingFiles,
                             char **ppszTabFileNameOut);

void CPL_DLL GDALCopyRasterIOExtraArg(GDALRasterIOExtraArg *psDestArg,
                                      GDALRasterIOExtraArg *psSrcArg);

void CPL_DLL GDALExpandPackedBitsToByteAt0Or1(
    const GByte *CPL_RESTRICT pabyInput, GByte *CPL_RESTRICT pabyOutput,
    size_t nInputBits);

void CPL_DLL GDALExpandPackedBitsToByteAt0Or255(
    const GByte *CPL_RESTRICT pabyInput, GByte *CPL_RESTRICT pabyOutput,
    size_t nInputBits);

CPL_C_END

int CPL_DLL GDALReadWorldFile2(const char *pszBaseFilename,
                               const char *pszExtension, GDALGeoTransform &gt,
                               CSLConstList papszSiblingFiles,
                               char **ppszWorldFileNameOut);

std::unique_ptr<GDALDataset> CPL_DLL
GDALGetThreadSafeDataset(std::unique_ptr<GDALDataset> poDS, int nScopeFlags);

GDALDataset CPL_DLL *GDALGetThreadSafeDataset(GDALDataset *poDS,
                                              int nScopeFlags);

void GDALNullifyOpenDatasetsList();
CPLMutex **GDALGetphDMMutex();
CPLMutex **GDALGetphDLMutex();
void GDALNullifyProxyPoolSingleton();
void GDALSetResponsiblePIDForCurrentThread(GIntBig responsiblePID);
GIntBig GDALGetResponsiblePIDForCurrentThread();

CPLString GDALFindAssociatedFile(const char *pszBasename, const char *pszExt,
                                 CSLConstList papszSiblingFiles, int nFlags);

CPLErr CPL_DLL EXIFExtractMetadata(char **&papszMetadata, void *fpL,
                                   uint32_t nOffset, int bSwabflag,
                                   vsi_l_offset nTIFFHEADER,
                                   uint32_t &nExifOffset,
                                   uint32_t &nInterOffset,
                                   uint32_t &nGPSOffset);

int GDALValidateOpenOptions(GDALDriverH hDriver,
                            const char *const *papszOptionOptions);
int GDALValidateOptions(const char *pszOptionList,
                        const char *const *papszOptionsToValidate,
                        const char *pszErrorMessageOptionType,
                        const char *pszErrorMessageContainerName);

GDALRIOResampleAlg CPL_DLL
GDALRasterIOGetResampleAlg(const char *pszResampling);
const char *GDALRasterIOGetResampleAlg(GDALRIOResampleAlg eResampleAlg);

void GDALRasterIOExtraArgSetResampleAlg(GDALRasterIOExtraArg *psExtraArg,
                                        int nXSize, int nYSize, int nBufXSize,
                                        int nBufYSize);

GDALDataset *GDALCreateOverviewDataset(GDALDataset *poDS, int nOvrLevel,
                                       bool bThisLevelOnly);

// Should cover particular cases of #3573, #4183, #4506, #6578
// Behavior is undefined if fVal1 or fVal2 are NaN (should be tested before
// calling this function)

// TODO: The expression `abs(fVal1 + fVal2)` looks strange; is this a bug?
// Should this be `abs(fVal1) + abs(fVal2)` instead?

inline bool ARE_REAL_EQUAL(float fVal1, float fVal2, int ulp = 2)
{
    using std::abs;
    return fVal1 == fVal2 || /* Should cover infinity */
           abs(fVal1 - fVal2) <
               std::numeric_limits<float>::epsilon() * abs(fVal1 + fVal2) * ulp;
}

// We are using `std::numeric_limits<float>::epsilon()` for backward
// compatibility
inline bool ARE_REAL_EQUAL(double dfVal1, double dfVal2, int ulp = 2)
{
    using std::abs;
    return dfVal1 == dfVal2 || /* Should cover infinity */
           abs(dfVal1 - dfVal2) <
               static_cast<double>(std::numeric_limits<float>::epsilon()) *
                   abs(dfVal1 + dfVal2) * ulp;
}

double GDALAdjustNoDataCloseToFloatMax(double dfVal);

#define DIV_ROUND_UP(a, b) (((a) % (b)) == 0 ? ((a) / (b)) : (((a) / (b)) + 1))

// Number of data samples that will be used to compute approximate statistics
// (minimum value, maximum value, etc.)
#define GDALSTAT_APPROX_NUMSAMPLES 2500

void GDALSerializeGCPListToXML(CPLXMLNode *psParentNode,
                               const std::vector<gdal::GCP> &asGCPs,
                               const OGRSpatialReference *poGCP_SRS);
void GDALDeserializeGCPListFromXML(const CPLXMLNode *psGCPList,
                                   std::vector<gdal::GCP> &asGCPs,
                                   OGRSpatialReference **ppoGCP_SRS);

void GDALSerializeOpenOptionsToXML(CPLXMLNode *psParentNode,
                                   CSLConstList papszOpenOptions);
char CPL_DLL **
GDALDeserializeOpenOptionsFromXML(const CPLXMLNode *psParentNode);

int GDALCanFileAcceptSidecarFile(const char *pszFilename);

bool GDALCanReliablyUseSiblingFileList(const char *pszFilename);

typedef enum
{
    GSF_UNSIGNED_INT,
    GSF_SIGNED_INT,
    GSF_FLOATING_POINT,
} GDALBufferSampleFormat;

bool CPL_DLL GDALBufferHasOnlyNoData(const void *pBuffer, double dfNoDataValue,
                                     size_t nWidth, size_t nHeight,
                                     size_t nLineStride, size_t nComponents,
                                     int nBitsPerSample,
                                     GDALBufferSampleFormat nSampleFormat);

bool CPL_DLL GDALCopyNoDataValue(GDALRasterBand *poDstBand,
                                 GDALRasterBand *poSrcBand,
                                 bool *pbCannotBeExactlyRepresented = nullptr);

double CPL_DLL GDALGetNoDataValueCastToDouble(int64_t nVal);
double CPL_DLL GDALGetNoDataValueCastToDouble(uint64_t nVal);

// Remove me in GDAL 4.0. See GetMetadataItem() implementation
// Internal use in GDAL only !
// Declaration copied in swig/include/gdal.i
void CPL_DLL GDALEnablePixelTypeSignedByteWarning(GDALRasterBandH hBand,
                                                  bool b);

std::string CPL_DLL GDALGetCompressionFormatForJPEG(VSILFILE *fp);
std::string CPL_DLL GDALGetCompressionFormatForJPEG(const void *pBuffer,
                                                    size_t nBufferSize);

GDALRasterAttributeTable CPL_DLL *GDALCreateRasterAttributeTableFromMDArrays(
    GDALRATTableType eTableType,
    const std::vector<std::shared_ptr<GDALMDArray>> &apoArrays,
    const std::vector<GDALRATFieldUsage> &aeUsages);

GDALColorInterp CPL_DLL
GDALGetColorInterpFromSTACCommonName(const char *pszName);
const char CPL_DLL *
GDALGetSTACCommonNameFromColorInterp(GDALColorInterp eInterp);

std::string CPL_DLL GDALGetCacheDirectory();

bool GDALDoesFileOrDatasetExist(const char *pszName,
                                const char **ppszType = nullptr,
                                GDALDriver **ppDriver = nullptr);

std::string CPL_DLL
GDALGetMessageAboutMissingPluginDriver(GDALDriver *poMissingPluginDriver);

std::string GDALPrintDriverList(int nOptions, bool bJSON);

struct GDALColorAssociation
{
    double dfVal;
    int nR;
    int nG;
    int nB;
    int nA;
};

std::vector<GDALColorAssociation> GDALLoadTextColorMap(const char *pszFilename,
                                                       GDALRasterBand *poBand);

namespace GDAL
{
inline CPLErr Combine(CPLErr eErr1, CPLErr eErr2)
{
    return eErr1 == CE_None ? eErr2 : eErr1;
}

inline CPLErr Combine(CPLErr eErr1, int) = delete;

inline CPLErr Combine(CPLErr eErr1, bool b)
{
    return eErr1 == CE_None ? (b ? CE_None : CE_Failure) : eErr1;
}

}  // namespace GDAL

CPLStringList CPL_DLL GDALReadENVIHeader(VSILFILE *fpHdr);
CPLStringList CPL_DLL GDALENVISplitList(const char *pszCleanInput);
void CPL_DLL GDALApplyENVIHeaders(GDALDataset *poDS,
                                  const CPLStringList &aosHeaders,
                                  CSLConstList papszOptions);

//! @endcond

#endif
