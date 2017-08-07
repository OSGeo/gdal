/*
* Copyright (c) 2002-2012, California Institute of Technology.
* All rights reserved.  Based on Government Sponsored Research under contracts NAS7-1407 and/or NAS7-03001.
*
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
*   1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
*   2. Redistributions in binary form must reproduce the above copyright notice,
*      this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
*   3. Neither the name of the California Institute of Technology (Caltech), its operating division the Jet Propulsion Laboratory (JPL),
*      the National Aeronautics and Space Administration (NASA), nor the names of its contributors may be used to
*      endorse or promote products derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE CALIFORNIA INSTITUTE OF TECHNOLOGY BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* Portions copyright 2014-2017 Esri
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

/******************************************************************************
*
* Project:  Meta Raster File Format Driver Implementation, Dataset
* Purpose:  Implementation of GDAL dataset
*
* Author:   Lucian Plesea, Lucian.Plesea@jpl.nasa.gov, lplesea@esri.com
*
******************************************************************************
*
*   The MRF dataset and the band are closely tied together, they should be
*   considered a single class, or a class (dataset) with extensions (bands).
*
*
****************************************************************************/

#include "marfa.h"
#include "cpl_multiproc.h" /* for CPLSleep() */
#include <gdal_priv.h>
#include <assert.h>

#include <algorithm>
#include <vector>

CPL_CVSID("$Id$")

using std::vector;
using std::string;

NAMESPACE_MRF_START

#if GDAL_VERSION_MAJOR >= 2
#define BOOLTEST CPLTestBool
#else
#define BOOLTEST CSLTestBoolean
#endif

// Initialize as invalid
GDALMRFDataset::GDALMRFDataset() :
    zslice(0),
    idxSize(0),
    clonedSource(FALSE),
    nocopy(FALSE),
    bypass_cache(BOOLTEST(CPLGetConfigOption("MRF_BYPASSCACHING", "FALSE"))),
    mp_safe(FALSE),
    hasVersions(FALSE),
    verCount(0),
    bCrystalized(TRUE), // Assume not in create mode
    spacing(0),
    no_errors(0),
    missing(0),
    poSrcDS(NULL),
    level(-1),
    cds(NULL),
    scale(0.0),
    pbuffer(NULL),
    pbsize(0),
    tile(ILSize()),
    bdirty(0),
    bGeoTransformValid(TRUE),
    poColorTable(NULL),
    Quality(0)
{
    //                X0   Xx   Xy  Y0    Yx   Yy
    double gt[6] = { 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };

    memcpy(GeoTransform, gt, sizeof(gt));
    ifp.FP = dfp.FP = NULL;
    dfp.acc = GF_Read;
    ifp.acc = GF_Read;
}

bool GDALMRFDataset::SetPBuffer(unsigned int sz)
{
    if (sz == 0) {
        CPLFree(pbuffer);
        pbuffer = NULL;
    }
    void* pbufferNew = VSIRealloc(pbuffer, sz);
    if( pbufferNew == NULL )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Cannot allocate %u bytes",
                 sz);
        return false;
    }
    pbuffer = pbufferNew;
    pbsize = (pbuffer == NULL) ? 0 : sz;
    return true;
}

//
// Called by dataset destructor or at GDAL termination, to avoid
// closing datasets whose drivers have already been unloaded
//
int GDALMRFDataset::CloseDependentDatasets()
{
    int bHasDroppedRef = GDALPamDataset::CloseDependentDatasets();

    if (poSrcDS)
    {
        bHasDroppedRef = TRUE;
        GDALClose(reinterpret_cast<GDALDatasetH>(poSrcDS));
        poSrcDS = NULL;
    }

    if (cds) {
        bHasDroppedRef = TRUE;
        GDALClose(reinterpret_cast<GDALDatasetH>(cds));
        cds = NULL;
    }

    return bHasDroppedRef;
}

GDALMRFDataset::~GDALMRFDataset()

{   // Make sure everything gets written
    FlushCache();

    CloseDependentDatasets();

    if (ifp.FP)
        VSIFCloseL(ifp.FP);
    if (dfp.FP)
        VSIFCloseL(dfp.FP);

    delete poColorTable;

    // CPLFree ignores being called with NULL
    CPLFree(pbuffer);
    pbsize = 0;
}

#ifdef unused
/*
 *\brief Called before the IRaster IO gets called
 *
 *
 *
 */
CPLErr GDALMRFDataset::AdviseRead(int nXOff, int nYOff, int nXSize, int nYSize,
    int nBufXSize, int nBufYSize,
    GDALDataType eDT,
    int nBandCount, int *panBandList,
    char **papszOptions)
{
    CPLDebug("MRF_IO", "AdviseRead %d, %d, %d, %d, bufsz %d,%d,%d\n",
        nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize, nBandCount);
    return CE_None;
}
#endif

/*
 *\brief Format specific RasterIO, may be bypassed by BlockBasedRasterIO by setting
 * GDAL_FORCE_CACHING to Yes, in which case the band ReadBlock and WriteBLock are called
 * directly
 *
 *
 */
CPLErr GDALMRFDataset::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
    void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
    int nBandCount, int *panBandMap,
#if GDAL_VERSION_MAJOR >= 2
    GSpacing nPixelSpace, GSpacing nLineSpace, GSpacing nBandSpace, GDALRasterIOExtraArg* psExtraArgs
#else
    int nPixelSpace, int nLineSpace, int nBandSpace
#endif
    )
{
    CPLDebug("MRF_IO", "IRasterIO %s, %d, %d, %d, %d, bufsz %d,%d,%d strides P %d, L %d, B %d \n",
        eRWFlag == GF_Write ? "Write" : "Read",
        nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize, nBandCount,
        static_cast<int>(nPixelSpace), static_cast<int>(nLineSpace),
        static_cast<int>(nBandSpace));

    //
    // Call the parent implementation, which splits it into bands and calls their IRasterIO
    //
    return GDALPamDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
        eBufType, nBandCount, panBandMap, nPixelSpace, nLineSpace, nBandSpace
#if GDAL_VERSION_MAJOR >= 2
        ,psExtraArgs
#endif
        );
}

/**
*\brief Build some overviews
*
*  if nOverviews is 0, erase the overviews (reduce to base image only)
*/

CPLErr GDALMRFDataset::IBuildOverviews(
    const char * pszResampling,
    int nOverviews, int * panOverviewList,
    int nBandsIn, int * panBandList,
    GDALProgressFunc pfnProgress, void * pProgressData)

{
    CPLErr       eErr = CE_None;

    CPLDebug("MRF_OVERLAY", "IBuildOverviews %d, bands %d\n", nOverviews, nBandsIn);

    if( nBands != nBandsIn )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "nBands = %d not supported", nBandsIn);
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      If we don't have write access, then create the overviews        */
    /*      externally.                                                     */
    /* -------------------------------------------------------------------- */
    if (GetAccess() != GA_Update)
    {
        CPLDebug("MRF", "File open read-only, creating overviews externally.");

        return GDALDataset::IBuildOverviews(
            pszResampling, nOverviews, panOverviewList,
            nBands, panBandList, pfnProgress, pProgressData);
    }

    /* -------------------------------------------------------------------- */
    /*      If zero overviews were requested, we need to clear all          */
    /*      existing overviews.                                             */
    /*      This should just clear the index file                           */
    /*      Right now it just fails or does nothing                         */
    /* -------------------------------------------------------------------- */

    if (nOverviews == 0)
    {
        if (current.size.l == 0)
            return GDALDataset::IBuildOverviews(pszResampling,
                nOverviews, panOverviewList,
                nBands, panBandList, pfnProgress, pProgressData);
        else
            return CleanOverviews();
    }

    // Array of source bands
    GDALRasterBand **papoBandList =
        (GDALRasterBand **)CPLCalloc(sizeof(void*), nBands);
    // Array of destination bands
    GDALRasterBand **papoOverviewBandList =
        (GDALRasterBand **)CPLCalloc(sizeof(void*), nBands);
    // Triple level pointer, that's what GDAL ROMB wants
    GDALRasterBand ***papapoOverviewBands =
        (GDALRasterBand ***)CPLCalloc(sizeof(void*), nBands);

    int* panOverviewListNew = (int*)CPLMalloc( sizeof(int) * nOverviews );
    memcpy( panOverviewListNew, panOverviewList, sizeof(int) * nOverviews );

    try {  // Throw an error code, to make sure memory gets freed properly
        // Modify the metadata file if it doesn't already have the Rset model set
        if (0.0 == scale) {
            CPLXMLNode *config = ReadConfig();
            try {
                const char* model = CPLGetXMLValue(config, "Rsets.model", "uniform");
                if (!EQUAL(model, "uniform")) {
                    CPLError(CE_Failure, CPLE_AppDefined,
                        "MRF:IBuildOverviews, Overviews not implemented for model %s", model);
                    throw CE_Failure;
                }

                // The scale value is the same as first overview
                scale = strtod(CPLGetXMLValue(config, "Rsets.scale",
                    CPLString().Printf("%d", panOverviewList[0]).c_str()), NULL);

                // Initialize the empty overlays, all of them for a given scale
                // They could already exist, in which case they are not erased
                idxSize = AddOverviews(int(scale));
                if (!CheckFileSize(current.idxfname, idxSize, GA_Update)) {
                    CPLError(CE_Failure, CPLE_AppDefined, "MRF: Can't extend index file");
                    throw CE_Failure;
                }

                //  Set the uniform node, in case it was not set before, and save the new configuration
                CPLSetXMLValue(config, "Rsets.#model", "uniform");
                CPLSetXMLValue(config, "Rsets.#scale", CPLString().Printf("%g", scale).c_str());

                if (!WriteConfig(config)) {
                    CPLError(CE_Failure, CPLE_AppDefined, "MRF: Can't rewrite the metadata file");
                    throw CE_Failure;
                }
                CPLDestroyXMLNode(config);
                config = NULL;
            }
            catch (const CPLErr& ) {
                if (config)
                    CPLDestroyXMLNode(config);
                throw; // Rethrow
            }

            // To avoid issues with blacks overviews, if the user asked to
            // generate overviews 2, 4, ..., we finish the sequence until the
            // last level that will be otherwised initialized to black
            if( !EQUAL(pszResampling, "NONE") &&
                nOverviews != GetRasterBand(1)->GetOverviewCount() &&
                BOOLTEST(CPLGetConfigOption("MRF_ALL_OVERVIEW_LEVELS",
                                               "YES")) )
            {
                bool bIncreasingPowers = true;
                for (int i = 1; i < nOverviews; i++) {
                    if (panOverviewList[i] != (panOverviewList[0] << i)) {
                        bIncreasingPowers = false;
                    }
                }
                if( bIncreasingPowers )
                {
                    CPLDebug("MRF", "Generating %d levels instead of the %d required",
                             GetRasterBand(1)->GetOverviewCount(), nOverviews);
                    nOverviews = GetRasterBand(1)->GetOverviewCount();
                    panOverviewListNew = (int*) CPLRealloc(panOverviewListNew,
                                                    sizeof(int) * nOverviews );
                    for (int i = 1; i < nOverviews; i++) {
                        panOverviewListNew[i] = panOverviewListNew[0] << i;
                    }
                }
            }
        }

        for (int i = 0; i < nOverviews; i++) {

            // Verify that scales are reasonable, val/scale has to be an integer
            if (!IsPower(panOverviewListNew[i], scale)) {
                CPLError(CE_Warning, CPLE_AppDefined,
                    "MRF:IBuildOverviews, overview factor %d is not a power of %f",
                    panOverviewListNew[i], scale);
                continue;
            };

            int srclevel = int(logbase(panOverviewListNew[i], scale) - 0.5);
            GDALMRFRasterBand *b = static_cast<GDALMRFRasterBand *>(GetRasterBand(1));

            // Warn for requests for invalid levels
            if (srclevel >= b->GetOverviewCount()) {
                CPLError(CE_Warning, CPLE_AppDefined,
                    "MRF:IBuildOverviews, overview factor %d is not valid for this dataset",
                    panOverviewListNew[i]);
                continue;
            }

            // Generate the overview using the previous level as the source

            // Use "avg" flag to trigger the internal average sampling
            if (EQUALN("Avg", pszResampling, 3) || EQUALN("NearNb", pszResampling, 4)) {

                int sampling = EQUALN("Avg", pszResampling, 3) ? SAMPLING_Avg : SAMPLING_Near;
                // Internal, using PatchOverview
                if (srclevel > 0)
                    b = static_cast<GDALMRFRasterBand *>(b->GetOverview(srclevel - 1));

                eErr = PatchOverview(0, 0, b->nBlocksPerRow, b->nBlocksPerColumn, srclevel,
                    0, sampling);
                if (eErr == CE_Failure)
                    throw eErr;
            }
            else
            {
                //
                // Use the GDAL method, which is slightly different for bilinear interpolation
                // and also handles nearest mode
                //
                //
                for (int iBand = 0; iBand < nBands; iBand++) {
                    // This is the base level
                    papoBandList[iBand] = GetRasterBand(panBandList[iBand]);
                    // Set up the destination
                    papoOverviewBandList[iBand] =
                        papoBandList[iBand]->GetOverview(srclevel);

                    // Use the previous level as the source, the overviews are 0 based
                    // thus an extra -1
                    if (srclevel > 0)
                        papoBandList[iBand] = papoBandList[iBand]->GetOverview(srclevel - 1);

                    // Hook it up, via triple pointer level
                    papapoOverviewBands[iBand] = &(papoOverviewBandList[iBand]);
                }

                //
                // Ready, generate this overview
                // Note that this function has a bug in GDAL, the block stepping is incorect
                // It can generate multiple overview in one call,
                // Could rewrite this loop so this function only gets called once
                //
                GDALRegenerateOverviewsMultiBand(nBands, papoBandList,
                    1, papapoOverviewBands,
                    pszResampling, pfnProgress, pProgressData);
            }
        }
    }
    catch (const CPLErr& e) {
        eErr = e;
    }

    CPLFree(panOverviewListNew);
    CPLFree(papapoOverviewBands);
    CPLFree(papoOverviewBandList);
    CPLFree(papoBandList);

    return eErr;
}

/*
*\brief blank separated list to vector of doubles
*/
static void list2vec(std::vector<double> &v, const char *pszList) {
    if ((pszList == NULL) || (pszList[0] == 0)) return;
    char **papszTokens = CSLTokenizeString2(pszList, " \t\n\r",
        CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES);
    v.clear();
    for (int i = 0; i < CSLCount(papszTokens); i++)
        v.push_back(CPLStrtod(papszTokens[i], NULL));
    CSLDestroy(papszTokens);
}

void GDALMRFDataset::SetNoDataValue(const char *pszVal) {
    list2vec(vNoData, pszVal);
}

void GDALMRFDataset::SetMinValue(const char *pszVal) {
    list2vec(vMin, pszVal);
}

void GDALMRFDataset::SetMaxValue(const char *pszVal) {
    list2vec(vMax, pszVal);
}

/**
*\brief Idenfity a MRF file, lightweight
*
* Lightweight test, otherwise Open gets called.
*
*/
int GDALMRFDataset::Identify(GDALOpenInfo *poOpenInfo)

{
  if (STARTS_WITH(poOpenInfo->pszFilename, "<MRF_META>"))
        return TRUE;

    CPLString fn(poOpenInfo->pszFilename);
    if (fn.find(":MRF:") != string::npos)
        return TRUE;

    if (poOpenInfo->nHeaderBytes < 10)
        return FALSE;

    const char *pszHeader = reinterpret_cast<char *>(poOpenInfo->pabyHeader);
    fn.assign(pszHeader, pszHeader + poOpenInfo->nHeaderBytes);
    if (STARTS_WITH(fn, "<MRF_META>"))
        return TRUE;

#if defined(LERC) // Could be single LERC tile
    if (LERC_Band::IsLerc(fn))
        return TRUE;
#endif

    return FALSE;
}

/**
*
*\brief Read the XML config tree, from file
*  Caller is responsible for freeing the memory
*
* @return NULL on failure, or the document tree on success.
*
*/
CPLXMLNode *GDALMRFDataset::ReadConfig() const
{
    if (fname[0] == '<')
        return CPLParseXMLString(fname);
    else
        return CPLParseXMLFile(fname);
}

/**
*\brief Write the XML config tree
* Caller is responsible for correctness of data
* and for freeing the memory
*
* @param config The document tree to write
* @return TRUE on success, FALSE otherwise
*/
int GDALMRFDataset::WriteConfig(CPLXMLNode *config)
{
    if (fname[0] == '<') return FALSE;
    return CPLSerializeXMLTreeToFile(config, fname);
}

static void
stringSplit(vector<string> & theStringVector,  // Altered/returned value
const string &theString,
size_t start = 0,
const  char theDelimiter = ' ')
{
    size_t end = theString.find(theDelimiter, start);
    if (string::npos == end) {
        theStringVector.push_back(theString.substr(start));
        return;
    }
    theStringVector.push_back(theString.substr(start, end - start));
    stringSplit(theStringVector, theString, end + 1, theDelimiter);
}

// Returns the number following the prefix if it exists in one of the vector strings
// Otherwise it returns the default
static int getnum(const vector<string> &theStringVector, const char prefix, int def) {
    for (unsigned int i = 0; i < theStringVector.size(); i++)
        if (theStringVector[i][0] == prefix)
            return atoi(theStringVector[i].c_str() + 1);
    return def;
}

/**
*\brief Open a MRF file
*
*/
GDALDataset *GDALMRFDataset::Open(GDALOpenInfo *poOpenInfo)

{
    if( !Identify(poOpenInfo) )
        return NULL;

    CPLXMLNode *config = NULL;
    CPLErr ret = CE_None;
    const char* pszFileName = poOpenInfo->pszFilename;

    int level = -1; // All levels
    int version = 0; // Current
    int zslice = 0;
    string fn; // Used to parse and adjust the file name

    // Different ways to open an MRF
    if (poOpenInfo->nHeaderBytes >= 10) {
        const char *pszHeader = reinterpret_cast<char *>(poOpenInfo->pabyHeader);
        if (STARTS_WITH(pszHeader, "<MRF_META>")) // Regular file name
            config = CPLParseXMLFile(pszFileName);
#if defined(LERC)
        else
            config = LERC_Band::GetMRFConfig(poOpenInfo);
#endif

    }
    else {
        if (EQUALN(pszFileName, "<MRF_META>", 10)) // Content as file name
            config = CPLParseXMLString(pszFileName);
        else
        { // Try Ornate file name
            fn = pszFileName;
            size_t pos = fn.find(":MRF:");
            if (string::npos != pos) { // Tokenize and pick known options
                vector<string> tokens;
                stringSplit(tokens, fn, pos + 5, ':');
                level = getnum(tokens, 'L', -1);
                version = getnum(tokens, 'V', 0);
                zslice = getnum(tokens, 'Z', 0);
                fn.resize(pos); // Cut the ornamentations
                pszFileName = fn.c_str();
                config = CPLParseXMLFile(pszFileName);
            }
        }
    }

    if (!config)
        return NULL;

    GDALMRFDataset *ds = new GDALMRFDataset();
    ds->fname = pszFileName;
    ds->eAccess = poOpenInfo->eAccess;
    ds->level = level;
    ds->zslice = zslice;

    if (level != -1) {
        // Open the whole dataset, then pick one level
        ds->cds = new GDALMRFDataset();
        ds->cds->fname = pszFileName;
        ds->cds->eAccess = ds->eAccess;
        ret = ds->cds->Initialize(config);
        if (ret == CE_None)
            ret = ds->LevelInit(level);
    }
    else
    {
        ret = ds->Initialize(config);
    }

    if (ret == CE_None)
        ds->ProcessOpenOptions(poOpenInfo->papszOpenOptions);

    CPLDestroyXMLNode(config);

    if (ret != CE_None) {
        delete ds;
        return NULL;
    }

    // Open a single version
    if (version != 0)
        ret = ds->SetVersion(version);

    if (ret != CE_None) {
        delete ds;
        return NULL;
    }

    // Tell PAM what our real file name is, to help it find the aux.xml
    ds->SetPhysicalFilename(pszFileName);
    // Don't mess with metadata after this, otherwise PAM will re-write the aux.xml
    ds->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Open external overviews.                                        */
/* -------------------------------------------------------------------- */
    ds->oOvManager.Initialize( ds, pszFileName );

    return ds;
}

// Adjust the band images with the right offset, then adjust the sizes
CPLErr GDALMRFDataset::SetVersion(int version) {
    if (!hasVersions || version > verCount) {
        CPLError(CE_Failure, CPLE_AppDefined, "GDAL MRF: Version number error!");
        return CE_Failure;
    }
    // Size of one version index
    for (int bcount = 1; bcount <= nBands; bcount++) {
        GDALMRFRasterBand *srcband = reinterpret_cast<GDALMRFRasterBand *>(GetRasterBand(bcount));
        srcband->img.idxoffset += idxSize*verCount;
        for (int l = 0; l < srcband->GetOverviewCount(); l++) {
            GDALMRFRasterBand *band = reinterpret_cast<GDALMRFRasterBand *>(srcband->GetOverview(l));
            if( band != NULL )
                band->img.idxoffset += idxSize*verCount;
        }
    }
    hasVersions = 0;
    return CE_None;
}

CPLErr GDALMRFDataset::LevelInit(const int l) {
    // Test that this level does exist
    if (l < 0 || l >= cds->GetRasterBand(1)->GetOverviewCount()) {
        CPLError(CE_Failure, CPLE_AppDefined, "GDAL MRF: Overview not present!");
        return CE_Failure;
    }

    GDALMRFRasterBand *srcband =
        reinterpret_cast<GDALMRFRasterBand *>(cds->GetRasterBand(1)->GetOverview(l));

    // Copy the sizes from this level
    full = srcband->img;
    current = srcband->img;
    current.size.c = cds->current.size.c;
    scale = cds->scale;
    SetProjection(cds->GetProjectionRef());

    SetMetadataItem("INTERLEAVE", OrderName(current.order), "IMAGE_STRUCTURE");
    SetMetadataItem("COMPRESSION", CompName(current.comp), "IMAGE_STRUCTURE");

    bGeoTransformValid = (CE_None == cds->GetGeoTransform(GeoTransform));
    for (int i = 0; i < l; i++) {
        GeoTransform[1] /= scale;
        GeoTransform[5] /= scale;
    }

    nRasterXSize = current.size.x;
    nRasterYSize = current.size.y;
    nBands = current.size.c;

    // Add the bands, copy constructor so they can be closed independently
    for (int i = 1; i <= nBands; i++)
        SetBand(i, new GDALMRFLRasterBand(reinterpret_cast<GDALMRFRasterBand *>
                                            (cds->GetRasterBand(i)->GetOverview(l))));
    return CE_None;
}

// Is the string positive or not
inline bool on(const char *pszValue) {
    if (!pszValue || pszValue[0] == 0)
        return false;
    return EQUAL(pszValue, "ON") || EQUAL(pszValue, "TRUE") || EQUAL(pszValue, "YES");
}

/**
*\brief Initialize the image structure and the dataset from the XML Raster node
*
* @param image the structure to be initialized
* @param ds the parent dataset, some things get inherited
* @param defimage defimage
*
* The structure should be initialized with the default values as much as possible
*
*/

static CPLErr Init_Raster(ILImage &image, GDALMRFDataset *ds, CPLXMLNode *defimage)
{
    CPLXMLNode *node; // temporary
    if (!defimage) {
        CPLError(CE_Failure, CPLE_AppDefined, "GDAL MRF: Can't find raster info");
        return CE_Failure;
    }

    // Size is mandatory
    node = CPLGetXMLNode(defimage, "Size");

    if (node) {
        image.size = ILSize(
            static_cast<int>(getXMLNum(node, "x", -1)),
            static_cast<int>(getXMLNum(node, "y", -1)),
            static_cast<int>(getXMLNum(node, "z", 1)),
            static_cast<int>(getXMLNum(node, "c", 1)),
            0);
    }

    // Basic checks
    if (!node || image.size.x < 1 || image.size.y < 1 ||
        image.size.z < 0 || image.size.c < 0 ||
        !GDALCheckBandCount(image.size.c, FALSE)) {
        CPLError(CE_Failure, CPLE_AppDefined, "Raster size missing or invalid");
        return CE_Failure;
    }

    //  Pagesize, defaults to 512,512,1,c
    image.pagesize = ILSize(
        std::min(512, image.size.x),
        std::min(512, image.size.y),
        1,
        image.size.c);

    node = CPLGetXMLNode(defimage, "PageSize");
    if (node)
    {
        image.pagesize = ILSize(
        static_cast<int>(getXMLNum(node, "x", image.pagesize.x)),
        static_cast<int>(getXMLNum(node, "y", image.pagesize.y)),
        1, // One z at a time, forced
        static_cast<int>(getXMLNum(node, "c", image.pagesize.c)));
        if( image.pagesize.x < 1 ||
            image.pagesize.y < 1 ||
            image.pagesize.c <= 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid PageSize");
            return CE_Failure;
        }
    }

    // Orientation, some other systems might support something
    //   if (!EQUAL(CPLGetXMLValue(defimage,"Orientation","TL"), "TL")) {
    //// GDAL only handles Top Left Images
    //CPLError(CE_Failure, CPLE_AppDefined, "GDAL MRF: Only Top-Left orientation is supported");
    //return CE_Failure;
    //   }

    // Page Encoding, defaults to PNG
    image.comp = CompToken(CPLGetXMLValue(defimage, "Compression", "PNG"));

    if (image.comp == IL_ERR_COMP) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "GDAL MRF: Compression %s is unknown",
            CPLGetXMLValue(defimage, "Compression", NULL));
        return CE_Failure;
    }

    // Is there a palette?
    //
    // GDAL only supports RGB+A palette, the other modes don't work
    //
    // Format is
    // <Palette>
    //   <Size>N</Size> : Optional
    //   <Model>RGBA|RGB|CMYK|HSV|HLS|L</Model> :mandatory
    //   <Entry idx=i c1=v1 c2=v2 c3=v3 c4=v4/> :Optional
    //   <Entry .../>
    // </Palette>
    // the idx attribute is optional, it autoincrements
    // The entries are actually vertices, interpolation takes place inside
    // The palette starts initialized with zeros
    // HSV and HLS are the similar, with c2 and c3 swapped
    // RGB or RGBA are same
    //

    if ((image.pagesize.c == 1) && (NULL != (node = CPLGetXMLNode(defimage, "Palette")))) {
        int entries = static_cast<int>(getXMLNum(node, "Size", 255));
        GDALPaletteInterp eInterp = GPI_RGB;
        // A flag to convert from HLS to HSV
        CPLString pModel = CPLGetXMLValue(node, "Model", "RGB");

        if ((entries > 0) && (entries < 257)) {
            GDALColorEntry ce_start = { 0, 0, 0, 255 }, ce_end = { 0, 0, 0, 255 };

            // Create it and initialize it to nothing
            GDALColorTable *poColorTable = new GDALColorTable(eInterp);
            poColorTable->CreateColorRamp(0, &ce_start, entries - 1, &ce_end);
            // Read the values
            CPLXMLNode *p = CPLGetXMLNode(node, "Entry");
            if (p) {
                // Initialize the first entry
                ce_start = GetXMLColorEntry(p);
                int start_idx = static_cast<int>(getXMLNum(p, "idx", 0));
                if (start_idx < 0) {
                    CPLError(CE_Failure, CPLE_AppDefined,
                        "GDAL MRF: Palette index %d not allowed", start_idx);
                    delete poColorTable;
                    return CE_Failure;
                }
                poColorTable->SetColorEntry(start_idx, &ce_start);
                while (NULL != (p = SearchXMLSiblings(p, "Entry"))) {
                    // For every entry, create a ramp
                    ce_end = GetXMLColorEntry(p);
                    int end_idx = static_cast<int>(getXMLNum(p, "idx", start_idx + 1));
                    if ((end_idx <= start_idx) || (start_idx >= entries)) {
                        CPLError(CE_Failure, CPLE_AppDefined,
                            "GDAL MRF: Index Error at index %d", end_idx);
                        delete poColorTable;
                        return CE_Failure;
                    }
                    poColorTable->CreateColorRamp(start_idx, &ce_start, end_idx, &ce_end);
                    ce_start = ce_end;
                    start_idx = end_idx;
                }
            }

            ds->SetColorTable(poColorTable);
        }
        else {
            CPLError(CE_Failure, CPLE_AppDefined, "GDAL MRF: Palette definition error");
            return CE_Failure;
        }
    }

    // Order of increment
    if( image.pagesize.c != image.size.c && image.pagesize.c != 1 )
    {
        // Fixes heap buffer overflow in GDALMRFRasterBand::ReadInterleavedBlock()
        // See https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=2884
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GDAL MRF: image.pagesize.c = %d and image.size.c = %d",
                 image.pagesize.c, image.size.c);
        return CE_Failure;
    }

    image.order = OrderToken(CPLGetXMLValue(defimage, "Order",
        (image.pagesize.c != image.size.c) ? "BAND" : "PIXEL"));
    if (image.order == IL_ERR_ORD) {
        CPLError(CE_Failure, CPLE_AppDefined, "GDAL MRF: Order %s is unknown",
            CPLGetXMLValue(defimage, "Order", NULL));
        return CE_Failure;
    }

    image.quality = atoi(CPLGetXMLValue(defimage, "Quality", "85"));

    const char * photo_val = CPLGetXMLValue(defimage, "Photometric", NULL);
    if (photo_val)
        ds->SetPhotometricInterpretation(photo_val);

    if (image.quality < 0 || image.quality>99) {
        CPLError(CE_Warning, CPLE_AppDefined, "GDAL MRF: Quality setting error, using default of 85");
        image.quality = 85;
    }

    // Data Type, use GDAL Names
    image.dt = GDALGetDataTypeByName(
        CPLGetXMLValue(defimage, "DataType", GDALGetDataTypeName(image.dt)));
    if (image.dt == GDT_Unknown || GDALGetDataTypeSize(image.dt) == 0 ) {
        CPLError(CE_Failure, CPLE_AppDefined, "GDAL MRF: Image has wrong type");
        return CE_Failure;
    }

    // Check the endianness if needed, assume host order
    if (is_Endianess_Dependent(image.dt, image.comp))
        image.nbo = on(CPLGetXMLValue(defimage, "NetByteOrder", "No"));

    CPLXMLNode *DataValues = CPLGetXMLNode(defimage, "DataValues");
    if (NULL != DataValues) {
        const char *pszValue = CPLGetXMLValue(DataValues, "NoData", NULL);
        if (pszValue) ds->SetNoDataValue(pszValue);
        pszValue = CPLGetXMLValue(DataValues, "min", NULL);
        if (pszValue) ds->SetMinValue(pszValue);
        pszValue = CPLGetXMLValue(DataValues, "max", NULL);
        if (pszValue) ds->SetMaxValue(pszValue);
    }

    // Calculate the page size in bytes
    if( image.pagesize.z <= 0 ||
        image.pagesize.x > INT_MAX / image.pagesize.y ||
        image.pagesize.x * image.pagesize.y > INT_MAX / image.pagesize.z ||
        image.pagesize.x * image.pagesize.y * image.pagesize.z > INT_MAX / image.pagesize.c ||
        image.pagesize.x * image.pagesize.y * image.pagesize.z* image.pagesize.c  > INT_MAX / (GDALGetDataTypeSize(image.dt) / 8) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "MRF page size too big");
        return CE_Failure;
    }
    image.pageSizeBytes = (GDALGetDataTypeSize(image.dt) / 8) *
        image.pagesize.x * image.pagesize.y * image.pagesize.z * image.pagesize.c;

    // Calculate the page count, including the total for the level
    image.pagecount = pcount(image.size, image.pagesize);

    // Data File Name and base offset
    image.datfname = getFname(defimage, "DataFile", ds->GetFname(), ILComp_Ext[image.comp]);
    image.dataoffset = static_cast<int>(
        getXMLNum(CPLGetXMLNode(defimage, "DataFile"), "offset", 0.0));

    // Index File Name and base offset
    image.idxfname = getFname(defimage, "IndexFile", ds->GetFname(), ".idx");
    image.idxoffset = static_cast<int>(
        getXMLNum(CPLGetXMLNode(defimage, "IndexFile"), "offset", 0.0));

    return CE_None;
}

char      **GDALMRFDataset::GetFileList()
{
    char** papszFileList = NULL;

    // Add the header file name if it is real
    VSIStatBufL  sStat;
    if (VSIStatExL(fname, &sStat, VSI_STAT_EXISTS_FLAG) == 0)
        papszFileList = CSLAddString(papszFileList, fname);

    // These two should be real
    // We don't really want to add these files, since they will be erased when an mrf is overwritten
    // This collides with the concept that the data file never shrinks.  Same goes with the index, in case
    // we just want to add things to it.
    //    papszFileList = CSLAddString( papszFileList, full.datfname);
    //    papszFileList = CSLAddString( papszFileList, full.idxfname);
    //    if (!source.empty())
    // papszFileList = CSLAddString( papszFileList, source);

    return papszFileList;
}

// Try to create all the folders in the path in sequence, ignore errors
static void mkdir_r(string const &fname) {
    size_t loc = fname.find_first_of("\\/");
    if( loc == string::npos )
        return;
    while (true) {
        ++ loc;
        loc = fname.find_first_of("\\/", loc);
        if( loc == string::npos )
            break;
        VSIMkdir(fname.substr(0, loc).c_str(), 0);
    }
}

// Returns the dataset index file or null
VSILFILE *GDALMRFDataset::IdxFP() {
    if (ifp.FP != NULL)
        return ifp.FP;

    // If missing is set, we already checked, there is no index
    if (missing)
        return NULL;

    // If name starts with '(' it is not a real file name
    if (current.idxfname[0] == '(')
        return NULL;

    const char *mode = "rb";
    ifp.acc = GF_Read;

    if (eAccess == GA_Update || !source.empty()) {
        mode = "r+b";
        ifp.acc = GF_Write;
    }

    ifp.FP = VSIFOpenL(current.idxfname, mode);

    // If file didn't open for reading and no_errors is set, just return null and make a note
    if (ifp.FP == NULL && eAccess == GA_ReadOnly && no_errors) {
        missing = 1;
        return NULL;
    }

    // need to create the index file
    if (ifp.FP == NULL && !bCrystalized && (eAccess == GA_Update || !source.empty())) {
        mode = "w+b";
        ifp.FP = VSIFOpenL(current.idxfname, mode);
    }

    if (NULL == ifp.FP && !source.empty()) {
        // caching and cloning, try making the folder and attempt again
        mkdir_r(current.idxfname);
        ifp.FP = VSIFOpenL(current.idxfname, mode);
    }

    GIntBig expected_size = idxSize;
    if (clonedSource) expected_size *= 2;

    if (NULL != ifp.FP) {
        if (!bCrystalized && !CheckFileSize(current.idxfname, expected_size, GA_Update)) {
            CPLError(CE_Failure, CPLE_AppDefined, "Can't extend the cache index file %s",
                current.idxfname.c_str());
            return NULL;
        }

        if (source.empty())
            return ifp.FP;

        // Make sure the index is large enough before proceeding
        // Timeout in .1 seconds, can't really guarantee the accuracy
        // So this is about half second, should be sufficient
        int timeout = 5;
        do {
            if (CheckFileSize(current.idxfname, expected_size, GA_ReadOnly))
                return ifp.FP;
            CPLSleep(0.100); /* 100 ms */
        } while (--timeout);

        // If we get here it is a time-out
        CPLError(CE_Failure, CPLE_AppDefined,
            "GDAL MRF: Timeout on fetching cloned index file %s\n", current.idxfname.c_str());
        return NULL;
    }

    // If single tile, and no index file, let the caller figure it out
    if (IsSingleTile())
        return NULL;

    // Error if this is not a caching MRF
    if (source.empty()) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "GDAL MRF: Can't open index file %s\n", current.idxfname.c_str());
        return NULL;
    }

    // Caching/Cloning MRF and index could be read only
    // Is this actually works, we should try again, maybe somebody else just created the file?
    mode = "rb";
    ifp.acc = GF_Read;
    ifp.FP = VSIFOpenL(current.idxfname, mode);
    if (NULL != ifp.FP)
        return ifp.FP;

    // Caching and index file absent, create it
    // Due to a race, multiple processes might do this at the same time, but that is fine
    ifp.FP = VSIFOpenL(current.idxfname, "wb");
    if (NULL == ifp.FP) {
        CPLError(CE_Failure, CPLE_AppDefined, "Can't create the MRF cache index file %s",
            current.idxfname.c_str());
        return NULL;
    }
    VSIFCloseL(ifp.FP);
    ifp.FP = NULL;

    // Make it large enough for caching and for cloning
    if (!CheckFileSize(current.idxfname, expected_size, GA_Update)) {
        CPLError(CE_Failure, CPLE_AppDefined, "Can't extend the cache index file %s",
            current.idxfname.c_str());
        return NULL;
    }

    // Try opening it again in rw mode so we can read and write
    mode = "r+b";
    ifp.acc = GF_Write;
    ifp.FP = VSIFOpenL(current.idxfname.c_str(), mode);

    if (NULL == ifp.FP) {
        CPLError(CE_Failure, CPLE_AppDefined,
            "GDAL MRF: Can't reopen cache index file %s\n", full.idxfname.c_str());
        return NULL;
    }
    return ifp.FP;
}

//
// Returns the dataset data file or null
// Data file is opened either in Read or Append mode, never in straight write
//
VSILFILE *GDALMRFDataset::DataFP() {
    if (dfp.FP != NULL)
        return dfp.FP;
    const char *mode = "rb";
    dfp.acc = GF_Read;

    // Open it for writing if updating or if caching
    if (eAccess == GA_Update || !source.empty()) {
        mode = "a+b";
        dfp.acc = GF_Write;
    }

    dfp.FP = VSIFOpenL(current.datfname, mode);
    if (dfp.FP)
        return dfp.FP;

    // It could be a caching MRF
    if (source.empty())
        goto io_error;

    // Cloud be there but read only, remember that it was open that way
    mode = "rb";
    dfp.acc = GF_Read;
    dfp.FP = VSIFOpenL(current.datfname, mode);
    if (NULL != dfp.FP) {
        CPLDebug("MRF_IO", "Opened %s RO mode %s\n", current.datfname.c_str(), mode);
        return dfp.FP;
    }

    if (source.empty())
        goto io_error;

    // caching, maybe the folder didn't exist
    mkdir_r(current.datfname);
    mode = "a+b";
    dfp.acc = GF_Write;
    dfp.FP = VSIFOpenL(current.datfname, mode);
    if (dfp.FP)
        return dfp.FP;

io_error:
    dfp.FP = NULL;
    CPLError(CE_Failure, CPLE_FileIO,
        "GDAL MRF: %s : %s", strerror(errno), current.datfname.c_str());
    return NULL;
}

// Builds an XML tree from the current MRF.  If written to a file it becomes an MRF
CPLXMLNode * GDALMRFDataset::BuildConfig()
{
    CPLXMLNode *config = CPLCreateXMLNode(NULL, CXT_Element, "MRF_META");

    if (!source.empty()) {
        CPLXMLNode *psCachedSource = CPLCreateXMLNode(config, CXT_Element, "CachedSource");
        // Should wrap the string in CDATA, in case it is XML
        CPLXMLNode *psSource = CPLCreateXMLElementAndValue(psCachedSource, "Source", source);
        if (clonedSource)
            CPLSetXMLValue(psSource, "#clone", "true");
    }

    // Use the full size
    CPLXMLNode *raster = CPLCreateXMLNode(config, CXT_Element, "Raster");

    // Preserve the file names if not the default ones
    if (full.datfname != getFname(GetFname(), ILComp_Ext[full.comp]))
        CPLCreateXMLElementAndValue(raster, "DataFile", full.datfname.c_str());
    if (full.idxfname != getFname(GetFname(), ".idx"))
        CPLCreateXMLElementAndValue(raster, "IndexFile", full.idxfname.c_str());
    if (spacing != 0)
        XMLSetAttributeVal(raster, "Spacing", static_cast<double>(spacing), "%.0f");

    XMLSetAttributeVal(raster, "Size", full.size, "%.0f");
    XMLSetAttributeVal(raster, "PageSize", full.pagesize, "%.0f");

    if (full.comp != IL_PNG)
        CPLCreateXMLElementAndValue(raster, "Compression", CompName(full.comp));

    if (full.dt != GDT_Byte)
        CPLCreateXMLElementAndValue(raster, "DataType", GDALGetDataTypeName(full.dt));

    // special photometric interpretation
    if (!photometric.empty())
        CPLCreateXMLElementAndValue(raster, "Photometric", photometric);

    if (!vNoData.empty() || !vMin.empty() || !vMax.empty() ) {
        CPLXMLNode *values = CPLCreateXMLNode(raster, CXT_Element, "DataValues");
        XMLSetAttributeVal(values, "NoData", vNoData);
        XMLSetAttributeVal(values, "min", vMin);
        XMLSetAttributeVal(values, "max", vMax);
    }

    // palette, if we have one
    if (poColorTable != NULL) {
        const char *pfrmt = "%.0f";
        CPLXMLNode *pal = CPLCreateXMLNode(raster, CXT_Element, "Palette");
        int sz = poColorTable->GetColorEntryCount();
        if (sz != 256)
            XMLSetAttributeVal(pal, "Size", poColorTable->GetColorEntryCount());
        // RGB or RGBA for now
        for (int i = 0; i < sz; i++) {
            CPLXMLNode *entry = CPLCreateXMLNode(pal, CXT_Element, "Entry");
            const GDALColorEntry *ent = poColorTable->GetColorEntry(i);
            // No need to set the index, it is always from 0 no size-1
            XMLSetAttributeVal(entry, "c1", ent->c1, pfrmt);
            XMLSetAttributeVal(entry, "c2", ent->c2, pfrmt);
            XMLSetAttributeVal(entry, "c3", ent->c3, pfrmt);
            if (ent->c4 != 255)
                XMLSetAttributeVal(entry, "c4", ent->c4, pfrmt);
        }
    }

    if (is_Endianess_Dependent(full.dt, full.comp)) // Need to set the order
        CPLCreateXMLElementAndValue(raster, "NetByteOrder",
        (full.nbo || NET_ORDER) ? "TRUE" : "FALSE");

    if (full.quality > 0 && full.quality != 85)
        CPLCreateXMLElementAndValue(raster, "Quality",
            CPLString().Printf("%d", full.quality));

    // Done with the raster node

    if (scale != 0.0) {
        CPLCreateXMLNode(config, CXT_Element, "Rsets");
        CPLSetXMLValue(config, "Rsets.#model", "uniform");
        CPLSetXMLValue(config, "Rsets.#scale", PrintDouble(scale));
    }
    CPLXMLNode *gtags = CPLCreateXMLNode(config, CXT_Element, "GeoTags");

    // Do we have an affine transform different from identity?
    double gt[6];
    if ((GetGeoTransform(gt) == CE_None) &&
        (gt[0] != 0 || gt[1] != 1 || gt[2] != 0 ||
        gt[3] != 0 || gt[4] != 0 || gt[5] != 1))
    {
        double minx = gt[0];
        double maxx = gt[1] * full.size.x + minx;
        double maxy = gt[3];
        double miny = gt[5] * full.size.y + maxy;
        CPLXMLNode *bbox = CPLCreateXMLNode(gtags, CXT_Element, "BoundingBox");
        XMLSetAttributeVal(bbox, "minx", minx);
        XMLSetAttributeVal(bbox, "miny", miny);
        XMLSetAttributeVal(bbox, "maxx", maxx);
        XMLSetAttributeVal(bbox, "maxy", maxy);
    }

    const char *pszProj = GetProjectionRef();
    if (pszProj && (!EQUAL(pszProj, "")))
        CPLCreateXMLElementAndValue(gtags, "Projection", pszProj);

    if ( optlist.Count() != 0 ) {
        CPLString options;
        for (int i = 0; i < optlist.size(); i++) {
            options += optlist[i];
            options += ' ';
        }
        options.resize(options.size() - 1);
        CPLCreateXMLElementAndValue(config, "Options", options);
    }

    return config;
}

/**
* \brief Populates the dataset variables from the XML definition
*
*
*/
CPLErr GDALMRFDataset::Initialize(CPLXMLNode *config)

{
    // We only need a basic initialization here, usually gets overwritten by the image params
    full.dt = GDT_Byte;
    full.hasNoData = false;
    full.NoDataValue = 0;
    Quality = 85;

    CPLErr ret = Init_Raster(full, this, CPLGetXMLNode(config, "Raster"));

    hasVersions = on(CPLGetXMLValue(config, "Raster.versioned", "no"));
    mp_safe = on(CPLGetXMLValue(config, "Raster.mp_safe", "no"));
    spacing = atoi(CPLGetXMLValue(config, "Raster.Spacing", "0"));

    Quality = full.quality;
    if (CE_None != ret)
        return ret;

    // Bounding box
    CPLXMLNode *bbox = CPLGetXMLNode(config, "GeoTags.BoundingBox");
    if (NULL != bbox) {
        double x0, x1, y0, y1;

        x0 = atof(CPLGetXMLValue(bbox, "minx", "0"));
        x1 = atof(CPLGetXMLValue(bbox, "maxx", "1"));
        y1 = atof(CPLGetXMLValue(bbox, "maxy", "1"));
        y0 = atof(CPLGetXMLValue(bbox, "miny", "0"));

        GeoTransform[0] = x0;
        GeoTransform[1] = (x1 - x0) / full.size.x;
        GeoTransform[2] = 0;
        GeoTransform[3] = y1;
        GeoTransform[4] = 0;
        GeoTransform[5] = (y0 - y1) / full.size.y;
        bGeoTransformValid = TRUE;
    }

    SetProjection(CPLGetXMLValue(config, "GeoTags.Projection", ""));

    // Copy the full size to current, data and index are not yet open
    current = full;
    if (current.size.z != 1) {
        SetMetadataItem("ZSIZE", CPLString().Printf("%d", current.size.z), "IMAGE_STRUCTURE");
        SetMetadataItem("ZSLICE", CPLString().Printf("%d", zslice), "IMAGE_STRUCTURE");
        // Capture the zslice in pagesize.l
        current.pagesize.l = zslice;
        // Adjust offset for base image
        if( full.size.z <= 0 )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "GDAL MRF: Invalid Raster.z value");
            return CE_Failure;
        }
        current.idxoffset += sizeof(ILIdx) * current.pagecount.l / full.size.z * zslice;
    }

    // Dataset metadata setup
    SetMetadataItem("INTERLEAVE", OrderName(current.order), "IMAGE_STRUCTURE");
    SetMetadataItem("COMPRESSION", CompName(current.comp), "IMAGE_STRUCTURE");

    if (is_Endianess_Dependent(current.dt, current.comp))
        SetMetadataItem("NETBYTEORDER", current.nbo ? "TRUE" : "FALSE", "IMAGE_STRUCTURE");

    // Open the files for the current image, either RW or RO
    nRasterXSize = current.size.x;
    nRasterYSize = current.size.y;
    nBands = current.size.c;

    if (!nBands || !nRasterXSize || !nRasterYSize) {
        CPLError(CE_Failure, CPLE_AppDefined, "GDAL MRF: Image size missing");
        return CE_Failure;
    }

    // Pick up the source data image, if there is one
    source = CPLGetXMLValue(config, "CachedSource.Source", "");
    // Is it a clone?
    clonedSource = on(CPLGetXMLValue(config, "CachedSource.Source.clone", "no"));
    // Pick up the options, if any
    optlist.Assign(CSLTokenizeString2(CPLGetXMLValue(config, "Options", NULL),
        " \t\n\r", CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES));

    // Load all the options in the IMAGE_STRUCTURE metadata
    for (int i = 0; i < optlist.Count(); i++) {
        CPLString s(optlist[i]);
        size_t nSepPos = s.find_first_of(":=");
        if( nSepPos != std::string::npos )
        {
            s.resize(nSepPos);
            const char *key = s.c_str();
            SetMetadataItem(key, optlist.FetchNameValue(key), "IMAGE_STRUCTURE");
        }
    }

    // We have the options, so we can call rasterband
    for (int i = 1; i <= nBands; i++) {
        // The overviews are low resolution copies of the current one.
        GDALMRFRasterBand *band = newMRFRasterBand(this, current, i);
        if (!band)
            return CE_Failure;

        GDALColorInterp ci = GCI_Undefined;

        // Default color interpretation
        switch (nBands) {
        case 1:
        case 2:
            ci = (i == 1) ? GCI_GrayIndex : GCI_AlphaBand;
            break;
        case 3:
        case 4:
            if (i < 3)
                ci = (i == 1) ? GCI_RedBand : GCI_GreenBand;
            else
                ci = (i == 3) ? GCI_BlueBand : GCI_AlphaBand;
        }

        if (GetColorTable())
            ci = GCI_PaletteIndex;

        // Legacy, deprecated
        if (optlist.FetchBoolean("MULTISPECTRAL", FALSE))
            ci = GCI_Undefined;

        // New style
        if (!photometric.empty() ) {
            if ("MULTISPECTRAL" == photometric)
                ci = GCI_Undefined;
        }

        band->SetColorInterpretation(ci);
        SetBand(i, band);
    }

    CPLXMLNode *rsets = CPLGetXMLNode(config, "Rsets");
    if (NULL != rsets && NULL != rsets->psChild) {
        // We have rsets

        // Regular spaced overlays, until everything fits in a single tile
        if (EQUAL("uniform", CPLGetXMLValue(rsets, "model", "uniform"))) {
            scale = getXMLNum(rsets, "scale", 2.0);
            if (scale <= 1) {
                CPLError(CE_Failure, CPLE_AppDefined, "MRF: zoom factor less than unit not allowed");
                return CE_Failure;
            }
            // Looks like there are overlays
            AddOverviews(int(scale));
        }
        else {
            CPLError(CE_Failure, CPLE_AppDefined, "Unknown Rset definition");
            return CE_Failure;
        }
    }

    idxSize = IdxSize(full, int(scale));

    // If not set by the bands, get a pageSizeBytes buffer
    if (GetPBufferSize() == 0)
    {
        if( !SetPBuffer(current.pageSizeBytes) )
        {
            return CE_Failure;
        }
    }

    if (hasVersions) { // It has versions, but how many?
        verCount = 0; // Assume it only has one
        VSIStatBufL statb;
        //  If the file exists, compute the last version number
        if (0 == VSIStatL(full.idxfname, &statb))
            verCount = int(statb.st_size / idxSize - 1);
    }

    return CE_None;
}

static inline bool has_path(const CPLString &name)
{
    return name.find_first_of("/\\") != string::npos;
}

static inline bool is_absolute(const CPLString &name)
{
    return (name.find_first_of("/\\") == 0) ||
        (name[1] == ':' && isalpha(name[0])) ||
        name.find("<MRF_META>") != string::npos;
}

// Add the folder part of path to the beginning of the source, if it is relative
static inline void make_absolute(CPLString &name, const CPLString &path)
{
    if (!is_absolute(path) && (path.find_first_of("/\\") != string::npos))
        name = path.substr(0, path.find_last_of("/\\") + 1) + name;
}

/**
*\brief Get the source dataset, open it if necessary
*/
GDALDataset *GDALMRFDataset::GetSrcDS() {
    if (poSrcDS) return poSrcDS;
    if (source.empty()) return NULL;
    // Make the source absolute path
    if (has_path(fname)) make_absolute(source, fname);
    poSrcDS = (GDALDataset *)GDALOpenShared(source.c_str(), GA_ReadOnly);
    if (0 == source.find("<MRF_META>") && has_path(fname))
    {// XML MRF source, might need to patch the file names with the current one
        GDALMRFDataset *psDS = reinterpret_cast<GDALMRFDataset *>(poSrcDS);
        make_absolute(psDS->current.datfname, fname);
        make_absolute(psDS->current.idxfname, fname);
    }
    mp_safe = true; // Turn on MP safety
    return poSrcDS;
}

/**
*\brief Add or verify that all overlays exits
*
* @return size of the index file
*/

GIntBig GDALMRFDataset::AddOverviews(int scaleIn) {
    // Fit the overlays
    ILImage img = current;
    while (1 != img.pagecount.x*img.pagecount.y)
    {
        // Adjust raster data for next level
        // Adjust the offsets for indices left at this level
        img.idxoffset += sizeof(ILIdx) * img.pagecount.l / img.size.z * (img.size.z - zslice);

        // Next overview size
        img.size.x = pcount(img.size.x, scaleIn);
        img.size.y = pcount(img.size.y, scaleIn);
        img.size.l++; // Increment the level
        img.pagecount = pcount(img.size, img.pagesize);

        // And adjust the offset again, within next level
        img.idxoffset += sizeof(ILIdx) * img.pagecount.l / img.size.z * zslice;

        // Create and register the the overviews for each band
        for (int i = 1; i <= nBands; i++) {
            GDALMRFRasterBand *b = reinterpret_cast<GDALMRFRasterBand *>(GetRasterBand(i));
            if (!(b->GetOverview(static_cast<int>(img.size.l) - 1)))
                b->AddOverview(newMRFRasterBand(this, img, i, static_cast<int>(img.size.l)));
        }
    }

    // Last adjustment, should be a single set of c and leftover z tiles
    return img.idxoffset + sizeof(ILIdx) * img.pagecount.l / img.size.z * (img.size.z - zslice);
}

// CreateCopy implemented based on Create
GDALDataset *GDALMRFDataset::CreateCopy(const char *pszFilename,
    GDALDataset *poSrcDS, int /*bStrict*/, char **papszOptions,
    GDALProgressFunc pfnProgress, void *pProgressData)
{
    ILImage img;

    int x = poSrcDS->GetRasterXSize();
    int y = poSrcDS->GetRasterYSize();
    int nBands = poSrcDS->GetRasterCount();
    if( nBands == 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "nBands == 0 not supported");
        return NULL;
    }
    GDALRasterBand *poSrcBand1 = poSrcDS->GetRasterBand(1);

    GDALDataType dt = poSrcBand1->GetRasterDataType();
    // Have our own options, to modify as we want
    char **options = CSLDuplicate(papszOptions);

    const char *pszValue = poSrcDS->GetMetadataItem("INTERLEAVE", "IMAGE_STRUCTURE");
    options = CSLAddIfMissing(options, "INTERLEAVE", pszValue ? pszValue : "PIXEL");
    int xb, yb;
    poSrcBand1->GetBlockSize(&xb, &yb);

    // Keep input block size if it exists and not explicitly set
    if (CSLFetchNameValue(options, "BLOCKSIZE") == NULL && xb != x && yb != y) {
        options = CSLAddIfMissing(options, "BLOCKXSIZE", PrintDouble(xb, "%d").c_str());
        options = CSLAddIfMissing(options, "BLOCKYSIZE", PrintDouble(yb, "%d").c_str());
    }

    GDALMRFDataset *poDS = NULL;
    try {
        poDS = reinterpret_cast<GDALMRFDataset *>(
            Create(pszFilename, x, y, nBands, dt, options));

        if (poDS == NULL || poDS->bCrystalized)
            throw CPLString().Printf("Can't create %s",pszFilename);

        img = poDS->current; // Deal with the current one here

        // Copy data values from source
        for (int i = 0; i < poDS->nBands; i++) {
            int bHas;
            double dfData;
            GDALRasterBand *srcBand = poSrcDS->GetRasterBand(i + 1);
            GDALRasterBand *mBand = poDS->GetRasterBand(i + 1);
            dfData = srcBand->GetNoDataValue(&bHas);
            if (bHas) {
                poDS->vNoData.push_back(dfData);
                mBand->SetNoDataValue(dfData);
            }
            dfData = srcBand->GetMinimum(&bHas);
            if (bHas)
                poDS->vMin.push_back(dfData);
            dfData = srcBand->GetMaximum(&bHas);
            if (bHas)
                poDS->vMax.push_back(dfData);

            // Copy the band metadata, PAM will handle it
            char **meta = srcBand->GetMetadata("IMAGE_STRUCTURE");
            if (CSLCount(meta))
                mBand->SetMetadata(meta,"IMAGE_STRUCTURE");

            meta = srcBand->GetMetadata();
            if (CSLCount(meta))
                mBand->SetMetadata(meta);
        }

        // Geotags
        double gt[6];
        if (CE_None == poSrcDS->GetGeoTransform(gt))
            poDS->SetGeoTransform(gt);

        const char *pszProj = poSrcDS->GetProjectionRef();
        if (pszProj && pszProj[0])
            poDS->SetProjection(pszProj);

        // Color palette if we only have one band
        if (1 == nBands && GCI_PaletteIndex == poSrcBand1->GetColorInterpretation())
            poDS->SetColorTable(poSrcBand1->GetColorTable()->Clone());

        // Finally write the XML in the right file name
        poDS->Crystalize();
    }
    catch (CPLString(e)) {
        if (poDS)
            delete poDS;
        CPLError(CE_Failure, CPLE_ObjectNull, "%s", e.c_str());
        poDS = NULL;
    }

    CSLDestroy(options);

    if (!poDS)
      return NULL;

    char** papszFileList = poDS->GetFileList();
    poDS->oOvManager.Initialize(poDS, poDS->GetPhysicalFilename(), papszFileList);
    CSLDestroy(papszFileList);

    CPLErr err = CE_None;
    // Have PAM copy all, but skip the mask
    int nCloneFlags = GCIF_PAM_DEFAULT & ~GCIF_MASK;

    // If copy is disabled, we're done, we just created an empty MRF
    if (!on(CSLFetchNameValue(papszOptions, "NOCOPY"))) {
      // Use the GDAL copy call
      // Need to flag the dataset as compressed (COMPRESSED=TRUE) to force block writes
      // This might not be what we want, if the input and out order is truly separate
      nCloneFlags |= GCIF_MASK; // We do copy the data, so copy the mask too if necessary
      char **papszCWROptions = NULL;
      papszCWROptions = CSLAddNameValue(papszCWROptions, "COMPRESSED", "TRUE");
      err = GDALDatasetCopyWholeRaster((GDALDatasetH)poSrcDS,
        (GDALDatasetH)poDS, papszCWROptions, pfnProgress, pProgressData);

      CSLDestroy(papszCWROptions);
    }


    if (CE_None == err)
      err = poDS->CloneInfo(poSrcDS, nCloneFlags);

    if (CE_Failure == err) {
        delete poDS;
        return NULL;
    }

    return poDS;
}


// Apply create options to the current dataset, only valid during creation
void GDALMRFDataset::ProcessOpenOptions(char **papszOptions)
{
    CPLStringList opt(papszOptions, FALSE);
    no_errors = opt.FetchBoolean("NOERRORS", FALSE);
}

// Apply create options to the current dataset, only valid during creation
void GDALMRFDataset::ProcessCreateOptions(char **papszOptions)
{
    assert(!bCrystalized);
    CPLStringList opt(papszOptions, FALSE);
    ILImage &img(full);

    const char *val = opt.FetchNameValue("COMPRESS");
    if (val && IL_ERR_COMP == (img.comp = CompToken(val)))
        throw CPLString("GDAL MRF: Error setting compression");

    val = opt.FetchNameValue("INTERLEAVE");
    if (val && IL_ERR_ORD == (img.order = OrderToken(val)))
        throw CPLString("GDAL MRF: Error setting interleave");

    val = opt.FetchNameValue("QUALITY");
    if (val) img.quality = atoi(val);

    val = opt.FetchNameValue("ZSIZE");
    if (val) img.size.z = atoi(val);

    val = opt.FetchNameValue("BLOCKXSIZE");
    if (val) img.pagesize.x = atoi(val);

    val = opt.FetchNameValue("BLOCKYSIZE");
    if (val) img.pagesize.y = atoi(val);

    val = opt.FetchNameValue("BLOCKSIZE");
    if (val) img.pagesize.x = img.pagesize.y = atoi(val);

    img.nbo = opt.FetchBoolean("NETBYTEORDER", FALSE);

    val = opt.FetchNameValue("CACHEDSOURCE");
    if (val) {
        source = val;
        nocopy = opt.FetchBoolean("NOCOPY", FALSE);
    }

    val = opt.FetchNameValue("UNIFORM_SCALE");
    if (val) scale = atoi(val);

    val = opt.FetchNameValue("PHOTOMETRIC");
    if (val) photometric = val;

    val = opt.FetchNameValue("DATANAME");
    if (val) img.datfname = val;

    val = opt.FetchNameValue("INDEXNAME");
    if (val) img.idxfname = val;

    val = opt.FetchNameValue("SPACING");
    if (val) spacing = atoi(val);

    optlist.Assign(CSLTokenizeString2(opt.FetchNameValue("OPTIONS"),
        " \t\n\r", CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES));

    // General Fixups
    if (img.order == IL_Interleaved)
        img.pagesize.c = img.size.c;

    // Compression dependent fixups
#if defined(LERC)
    if (IL_LERC == img.comp)
        img.pagesize.c = 1;
#endif
}

/**
 *\brief Create an MRF dataset, some settings can be changed later
 * papszOptions might be anything that an MRF might take
 * Still missing are the georeference ...
 *
 */

GDALDataset *
GDALMRFDataset::Create(const char * pszName,
    int nXSize, int nYSize, int nBands,
    GDALDataType eType, char ** papszOptions)

{   // Pending create
    if( nBands == 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "nBands == 0 not supported");
        return NULL;
    }

    GDALMRFDataset *poDS = new GDALMRFDataset();
    CPLErr err = CE_None;
    poDS->fname = pszName;
    poDS->nBands = nBands;

    // Don't know what to do with these in this call
    //int level = -1;
    //int version = 0;

    size_t pos = poDS->fname.find(":MRF:");
    if (string::npos != pos) { // Tokenize and pick known options
        vector<string> tokens;
        stringSplit(tokens, poDS->fname, pos + 5, ':');
        //level = getnum(tokens, 'L', -1);
        //version = getnum(tokens, 'V', 0);
        poDS->zslice = getnum(tokens, 'Z', 0);
        poDS->fname.resize(pos); // Cut the ornamentations
    }

    // Use the full, set some initial parameters
    ILImage &img = poDS->full;
    img.size = ILSize(nXSize, nYSize, 1, nBands);
    img.comp = IL_PNG;
    img.order = (nBands < 5) ? IL_Interleaved : IL_Separate;
    img.pagesize = ILSize(512, 512, 1, 1);
    img.quality = 85;
    img.dt = eType;
    img.dataoffset = 0;
    img.idxoffset = 0;
    img.hasNoData = false;
    img.nbo = FALSE;

    // Set the guard that tells us it needs saving before IO can take place
    poDS->bCrystalized = FALSE;

    // Process the options, anything that an MRF might take

    try {
        // Adjust the dataset and the full image
        poDS->ProcessCreateOptions(papszOptions);

        // Set default file names
        if (img.datfname.empty())
            img.datfname = getFname(poDS->GetFname(), ILComp_Ext[img.comp]);
        if (img.idxfname.empty())
            img.idxfname = getFname(poDS->GetFname(), ".idx");

        poDS->eAccess = GA_Update;
    }

    catch (const CPLString& e) {
        CPLError(CE_Failure, CPLE_OpenFailed, "%s", e.c_str());
        delete poDS;
        return NULL;
    }

    poDS->current = poDS->full;
    poDS->SetDescription(poDS->GetFname());

    // Build a MRF XML and initialize from it, this creates the bands
    CPLXMLNode *config = poDS->BuildConfig();
    err = poDS->Initialize(config);
    CPLDestroyXMLNode(config);

    if (CPLE_None != err) {
        delete poDS;
        return NULL;
    }

    // If not set by the band, get a pageSizeBytes buffer
    if (poDS->GetPBufferSize() == 0)
    {
        if( !poDS->SetPBuffer(poDS->current.pageSizeBytes) )
        {
            delete poDS;
            return NULL;
        }
    }

    // Tell PAM what our real file name is, to help it find the aux.xml
    poDS->SetPhysicalFilename(poDS->GetFname());

    return poDS;
}

void GDALMRFDataset::Crystalize()

{
    if (bCrystalized || eAccess != GA_Update)
        return;

    // No need to write to disk if there is no filename.  This is a
    // memory only dataset.
    if (strlen(GetDescription()) == 0
        || EQUALN(GetDescription(), "<MRF_META>", 10))
        return;

    CPLXMLNode *config = BuildConfig();
    WriteConfig(config);
    CPLDestroyXMLNode(config);
    if (!nocopy && (!IdxFP() || !DataFP()))
        throw CPLString().Printf("MRF: %s", strerror(errno));
    bCrystalized = TRUE;
}

// Copy the first index at the end of the file and bump the version count
CPLErr GDALMRFDataset::AddVersion()
{
    VSILFILE *l_ifp = IdxFP();

    void *tbuff = CPLMalloc(static_cast<size_t>(idxSize));
    VSIFSeekL(l_ifp, 0, SEEK_SET);
    VSIFReadL(tbuff, 1, static_cast<size_t>(idxSize), l_ifp);
    verCount++; // The one we write
    VSIFSeekL(l_ifp, idxSize * verCount, SEEK_SET); // At the end, this can mess things up royally
    VSIFWriteL(tbuff, 1, static_cast<size_t>(idxSize), l_ifp);
    CPLFree(tbuff);
    return CE_None;
}

//
// Write a tile at the end of the data file
// If buff and size are zero, it is equivalent to erasing the tile
// If only size is zero, it is a special empty tile,
// when used for caching, offset should be 1
//
// To make it multi-processor safe, open the file in append mode
// and verify after write
//
CPLErr GDALMRFDataset::WriteTile(void *buff, GUIntBig infooffset, GUIntBig size)
{
    CPLErr ret = CE_None;
    ILIdx tinfo = { 0, 0 };

    VSILFILE *l_dfp = DataFP();
    VSILFILE *l_ifp = IdxFP();

    // Pointer to verfiy buffer, if it doesn't exist everything worked fine
    void *tbuff = NULL;

    if (l_ifp == NULL || l_dfp == NULL)
        return CE_Failure;

    // If it has versions, might need to start a new one
    if (hasVersions) {
        int new_version = false; // Assume no need to build new version
        int new_tile = false;

        // Read the current tile info
        VSIFSeekL(l_ifp, infooffset, SEEK_SET);
        VSIFReadL(&tinfo, 1, sizeof(ILIdx), l_ifp);

        if (verCount != 0) { // We need at least two versions before we test buffers
            ILIdx prevtinfo = { 0, 0 };

            // Read the previous one
            VSIFSeekL(l_ifp, infooffset + verCount * idxSize, SEEK_SET);
            VSIFReadL(&prevtinfo, 1, sizeof(ILIdx), l_ifp);

            // current and previous tiles are different, might create version
            if (tinfo.size != prevtinfo.size || tinfo.offset != prevtinfo.offset)
                new_version = true;
        }
        else
            new_version = true; // No previous, might create version

        // tinfo contains the current info or 0,0
        if (tinfo.size == GIntBig(net64(size))) { // Might be the same, read and compare
            if (size != 0) {
                tbuff = CPLMalloc(static_cast<size_t>(size));
                // Use the temporary buffer, we can't have a versioned cache !!
                VSIFSeekL(l_dfp, infooffset, SEEK_SET);
                VSIFReadL(tbuff, 1, static_cast<size_t>(size), l_dfp);
                // Need to write it if not the same
                new_tile = (0 != memcmp(buff, tbuff, static_cast<size_t>(size)));
                CPLFree(tbuff);
                tbuff = NULL;
            }
            else {
                // Writing a null tile on top of a null tile, does it count?
                if (tinfo.offset != GIntBig(net64(GUIntBig(buff))))
                    new_tile = true;
            }
        }
        else {
            new_tile = true; // Need to write it because it is different
            if (verCount == 0 && tinfo.size == 0)
                new_version = false; // Don't create a version if current is empty and there is no previous
        }

        if (!new_tile)
            return CE_None; // No reason to write

        // Do we need to start a new version before writing the tile?
        if (new_version)
            AddVersion();
    }

    // Convert to net format
    tinfo.size = net64(size);

    if (size) do {

        // These statements are the critical MP section for the data file
        VSIFSeekL(l_dfp, 0, SEEK_END);
        GUIntBig offset = VSIFTellL(l_dfp);

        if( spacing != 0 )
        {
            // This should not be true in MP safe mode.
            // Use the same buffer, MRF doesn't care about the spacing content.
            // TODO(lplesea): Make sure size doesn't overflow.
            const int pad =
                static_cast<int>(size) >= spacing
                ? spacing
                : static_cast<int>(size);
            if( pad != spacing )
                CPLError(CE_Warning, CPLE_FileIO,
                         "MRF spacing failed, check the output");
            offset += pad;
            VSIFWriteL(buff, 1, spacing, l_dfp);
        }

        if (static_cast<size_t>(size) != VSIFWriteL(buff, 1, static_cast<size_t>(size), l_dfp))
            ret = CE_Failure;
        // End of critical section

        tinfo.offset = net64(offset);
        //
        // For MP ops, check that we can read it back properly, otherwise we're done
        // This makes the caching MRF MP safe, without using explicit locks
        //
        if (mp_safe) {
            // Allocate the temp buffer if we haven't done so already
            if (!tbuff)
                tbuff = CPLMalloc(static_cast<size_t>(size));
            VSIFSeekL(l_dfp, offset, SEEK_SET);
            VSIFReadL(tbuff, 1, static_cast<size_t>(size), l_dfp);
            // If memcmp returns zero, verify passed
            if (!memcmp(buff, tbuff, static_cast<size_t>(size))) {
                CPLFree(tbuff);
                tbuff = NULL; // Triggers the loop termination
            }
            // Otherwise the tbuf stays allocated and try to write again
            // This works only if the file is opened in append mode
        }
    } while (tbuff);

    // At this point, the data is in the datafile

    // Special case
    // Any non-zero will do, use 1 to only consume one bit
    if (NULL != buff && 0 == size)
        tinfo.offset = net64(GUIntBig(buff));

    VSIFSeekL(l_ifp, infooffset, SEEK_SET);
    if (sizeof(tinfo) != VSIFWriteL(&tinfo, 1, sizeof(tinfo), l_ifp))
        ret = CE_Failure;

    return ret;
}

CPLErr GDALMRFDataset::SetGeoTransform(double *gt)

{
    if (GetAccess() == GA_Update)
    {
        memcpy(GeoTransform, gt, 6 * sizeof(double));
        bGeoTransformValid = TRUE;
        return CE_None;
    }
    CPLError(CE_Failure, CPLE_NotSupported,
        "SetGeoTransform called on read only file");
    return CE_Failure;
}

bool GDALMRFDataset::IsSingleTile()
{
    if (current.pagecount.l != 1 || !source.empty() || NULL == DataFP())
        return FALSE;
    return 0 == reinterpret_cast<GDALMRFRasterBand *>(GetRasterBand(1))->GetOverviewCount();
}

/*
*  Returns 0,1,0,0,0,1 even if it was not set
*/
CPLErr GDALMRFDataset::GetGeoTransform(double *gt)
{
    memcpy(gt, GeoTransform, 6 * sizeof(double));
    if (GetMetadata("RPC") || GetGCPCount())
        bGeoTransformValid = FALSE;
    if (!bGeoTransformValid) return CE_Failure;
    return CE_None;
}

/**
*\brief Read a tile index
*
* It handles the non-existent index case, for no compression
* The bias is non-zero only when the cloned index is read
*/

CPLErr GDALMRFDataset::ReadTileIdx(ILIdx &tinfo, const ILSize &pos, const ILImage &img, const GIntBig bias)

{
    VSILFILE *l_ifp = IdxFP();

    // Initialize the tinfo structure, in case the files are missing
    if (missing)
      return CE_None;

    GIntBig offset = bias + IdxOffset(pos, img);
    if (l_ifp == NULL && img.comp == IL_NONE ) {
        tinfo.size = current.pageSizeBytes;
        tinfo.offset = offset * tinfo.size;
        return CE_None;
    }

    if (l_ifp == NULL && IsSingleTile()) {
        tinfo.offset = 0;
        VSILFILE *l_dfp = DataFP(); // IsSingleTile() checks that fp is valid
        VSIFSeekL(l_dfp, 0, SEEK_END);
        tinfo.size = VSIFTellL(l_dfp);

        // It should be less than the pagebuffer
        tinfo.size = std::min(tinfo.size, static_cast<GIntBig>(pbsize));
        return CE_None;
    }

    if (l_ifp == NULL) {
        CPLError(CE_Failure, CPLE_FileIO, "Can't open index file");
        return CE_Failure;
    }

    VSIFSeekL(l_ifp, offset, SEEK_SET);
    if (1 != VSIFReadL(&tinfo, sizeof(ILIdx), 1, l_ifp))
        return CE_Failure;
    // Convert them to native form
    tinfo.offset = net64(tinfo.offset);
    tinfo.size = net64(tinfo.size);

    if (0 == bias || 0 != tinfo.size || 0 != tinfo.offset)
        return CE_None;

    // zero size and zero offset in sourced index means that this portion is un-initialized

    // Should be cloned and the offset within the cloned index
    offset -= bias;
    assert(offset < bias);
    assert(clonedSource);

    // Read this block from the remote index, prepare it and store it in the right place
    // The block size in bytes, should be a multiple of 16, to have full index entries
    const int CPYSZ = 32768;
    // Adjust offset to the start of the block
    offset = (offset / CPYSZ) * CPYSZ;
    GIntBig size = std::min(size_t(CPYSZ), size_t(bias - offset));
    size /= sizeof(ILIdx); // In records
    vector<ILIdx> buf(static_cast<size_t>(size));
    ILIdx *buffer = &buf[0]; // Buffer to copy the source to the clone index

    // Fetch the data from the cloned index
    GDALMRFDataset *pSrc = static_cast<GDALMRFDataset *>(GetSrcDS());
    if( pSrc == NULL )
    {
        CPLError(CE_Failure, CPLE_FileIO, "Can't open cloned source index");
        return CE_Failure; // Source reported the error
    }

    VSILFILE *srcidx = pSrc->IdxFP();
    if (!srcidx) {
        CPLError(CE_Failure, CPLE_FileIO, "Can't open cloned source index");
        return CE_Failure; // Source reported the error
    }

    VSIFSeekL(srcidx, offset, SEEK_SET);
    size = VSIFReadL(buffer, sizeof(ILIdx), static_cast<size_t>(size), srcidx);
    if (size != GIntBig(buf.size())) {
        CPLError(CE_Failure, CPLE_FileIO, "Can't read cloned source index");
        return CE_Failure; // Source reported the error
    }

    // Mark the empty records as checked, by making the offset non-zero
    for (vector<ILIdx>::iterator it = buf.begin(); it != buf.end(); ++it) {
        if (it->offset == 0 && it->size == 0)
            it->offset = net64(1);
    }

    // Write it in the right place in the local index file
    VSIFSeekL(l_ifp, bias + offset, SEEK_SET);
    size = VSIFWriteL(&buf[0], sizeof(ILIdx), static_cast<size_t>(size), l_ifp);
    if (size != GIntBig(buf.size())) {
        CPLError(CE_Failure, CPLE_FileIO, "Can't write to cloning MRF index");
        return CE_Failure; // Source reported the error
    }

    // Cloned index updated, restart this function, it will work now
    return ReadTileIdx(tinfo, pos, img, bias);
}

NAMESPACE_MRF_END
