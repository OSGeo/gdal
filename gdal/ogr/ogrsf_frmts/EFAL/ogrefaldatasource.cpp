/*****************************************************************************
* Copyright 2016 Pitney Bowes Inc.
*
* Licensed under the MIT License (the “License”); you may not use this file
* except in the compliance with the License.
* You may obtain a copy of the License at https://opensource.org/licenses/MIT

* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an “AS IS” WITHOUT
* WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*****************************************************************************/


#if defined(_WIN32) || defined(WIN32) || defined(WIN64)
#pragma warning(disable:4251)
#endif

#include "cpl_port.h"
#include "OGREFAL.h"

#include <cerrno>
#include <cstring>
#include <string>

#include "cpl_csv.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "cpl_vsi_virtual.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_spatialref.h"
#include "ogreditablelayer.h"
#include "ogrsf_frmts.h"
#include "from_mitab.h"

CPL_CVSID("$Id: ogrcsvdatasource.cpp 37367 2017-02-13 05:32:41Z goatbar $");
static GUIntBig counter = 0;

extern EFALHANDLE OGREFALGetSession(GUIntBig);
extern void OGREFALReleaseSession(EFALHANDLE hSession);
/************************************************************************/
/*                          OGRCSVDataSource()                          */
/************************************************************************/

OGREFALDataSource::OGREFALDataSource() :
    pszName(nullptr),
    pszDirectory(nullptr),
    papoLayers(nullptr),
    nLayers(0),
    bUpdate(FALSE),
    efalOpenMode(EfalOpenMode::EFAL_READ_WRITE),
    bSingleFile(FALSE),
    bSingleLayerAlreadyCreated(FALSE),
    bCreateNativeX(false),
    charset(Ellis::MICHARSET::CHARSET_WLATIN1),
    nBlockSize(16384)
{
}

/************************************************************************/
/*                         ~OGRCSVDataSource()                          */
/************************************************************************/
OGREFALDataSource::~OGREFALDataSource()
{
    for (int i = 0; i < nLayers; i++)
        delete papoLayers[i];
    CPLFree(papoLayers);

    CPLFree(pszName);
    CPLFree(pszDirectory);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGREFALDataSource::TestCapability(const char * pszCap)
{
    if (EQUAL(pszCap, ODsCCreateLayer))
        return bUpdate && (!bSingleFile || ((OGREFALLayer *)papoLayers[0])->IsNew());
    else if (EQUAL(pszCap, ODsCRandomLayerRead))
        return TRUE;
    else if (EQUAL(pszCap, ODsCRandomLayerWrite))
        return (bUpdate || (efalOpenMode == EfalOpenMode::EFAL_LOCK_WRITE) || (efalOpenMode == EfalOpenMode::EFAL_READ_WRITE));
    else
        /*
        ODsCDeleteLayer: True if this datasource can delete existing layers.
        ODsCCreateGeomFieldAfterCreateLayer: True if the layers of this datasource support CreateGeomField() just after layer creation.
        ODsCCurveGeometries: True if this datasource supports curve geometries.
        ODsCTransactions: True if this datasource supports (efficient) transactions.
        ODsCEmulatedTransactions: True if this datasource supports transactions through emulation.
        */
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/
OGRLayer *OGREFALDataSource::GetLayer(int iLayer)
{
    if (iLayer < 0 || iLayer >= GetLayerCount())
        return nullptr;
    return papoLayers[iLayer];
}

/************************************************************************/
/*                          GetRealExtension()                          */
/************************************************************************/
CPLString OGREFALDataSource::GetRealExtension(CPLString osFilename)
{
    CPLString osExt = CPLGetExtension(osFilename);
    // TODO - do we support zip files?
    return osExt;
}

char **OGREFALDataSource::GetFileList()
{
    VSIStatBufL sStatBuf;
    CPLStringList osList;

    if (VSIStatL(pszName, &sStatBuf) == 0 && VSI_ISDIR(sStatBuf.st_mode))
    {
        static const char * const apszExtensions[] =
        { "tab", "map", "ind", "dat", "id", nullptr };
        char **papszDirEntries = VSIReadDir(pszName);

        for (int iFile = 0;
            papszDirEntries != nullptr && papszDirEntries[iFile] != nullptr;
            iFile++)
        {
            if (CSLFindString(apszExtensions,
                CPLGetExtension(papszDirEntries[iFile])) != -1)
            {
                osList.AddString(
                    CPLFormFilename(pszName, papszDirEntries[iFile], nullptr));
            }
        }

        CSLDestroy(papszDirEntries);
    }
    else
    {
        static const char *const apszTABExtensions[] = { "tab", "map", "ind", "dat", "id", nullptr };
        const char *const *papszExtensions = apszTABExtensions;
        const char *const *papszIter = papszExtensions;
        while (*papszIter)
        {
            const char *pszFile = CPLResetExtension(pszName, *papszIter);
            if (VSIStatL(pszFile, &sStatBuf) != 0)
            {
                pszFile = CPLResetExtension(pszName, CPLString(*papszIter).toupper());
                if (VSIStatL(pszFile, &sStatBuf) != 0)
                {
                    pszFile = nullptr;
                }
            }
            if (pszFile)
                osList.AddString(pszFile);
            papszIter++;
        }
    }
    return osList.StealList();
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/
int OGREFALDataSource::Open(GDALOpenInfo* poOpenInfo, int bTestOpen)
{
    CPLAssert(pszName == nullptr);

    pszName = CPLStrdup(poOpenInfo->pszFilename);
    pszDirectory = CPLStrdup(CPLGetPath(pszName));

    bUpdate = poOpenInfo->eAccess == GA_Update;

    const char * papszReadOptions = CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "MODE", "READ-WRITE");

    if (EQUAL(papszReadOptions, "READ-ONLY"))
    {
        efalOpenMode = EfalOpenMode::EFAL_READ_ONLY;
    }
    else if (EQUAL(papszReadOptions, "LOCK-READ"))
    {
        efalOpenMode = EfalOpenMode::EFAL_LOCK_READ;
    }
    else if (EQUAL(papszReadOptions, "READ-WRITE"))
    {
        efalOpenMode = EfalOpenMode::EFAL_READ_WRITE;
    }
    else if (EQUAL(papszReadOptions, "LOCK-WRITE"))
    {
        efalOpenMode = EfalOpenMode::EFAL_LOCK_WRITE;
    }

    // If it is a single tab file, try to open 
    if (!poOpenInfo->bIsDirectory)
    {
        /* ****************************************************************************
         * TODO: May need to reopen this or do a better test if EFAL cannot open it???
         * **************************************************************************** */
        VSIFCloseL(poOpenInfo->fpL);
        poOpenInfo->fpL = nullptr;

        EFALHANDLE hSession = OGREFALGetSession(++counter);
        if (hSession == (EFALHANDLE)nullptr)
        {
            return FALSE;
        }

        wchar_t* pwszFilename = CPLRecodeToWChar(poOpenInfo->pszFilename, CPL_ENC_UTF8, CPL_ENC_UCS2);
        EFALHANDLE hTable = efallib->OpenTable(hSession, pwszFilename);
        CPLFree(pwszFilename);

        if (hTable == (EFALHANDLE)nullptr)
        {
            OGREFALReleaseSession(hSession);
            return FALSE;
        }

        OGREFALLayer *poLayer = new OGREFALLayer(hSession, hTable, efalOpenMode);

        bSingleFile = TRUE;
        bSingleLayerAlreadyCreated = TRUE;

        nLayers = 1;
        papoLayers = static_cast<OGRLayer **>(CPLMalloc(sizeof(void *)));
        papoLayers[0] = poLayer;
    }
    // Otherwise, we need to scan the whole directory for tab files
    else
    {
        char **papszFileList = VSIReadDir(pszName);

        pszDirectory = CPLStrdup(pszName);

        for (int iFile = 0;
            papszFileList != nullptr && papszFileList[iFile] != nullptr;
            iFile++)
        {
            const char *pszExtension = CPLGetExtension(papszFileList[iFile]);

            if (!EQUAL(pszExtension, "tab"))
                continue;

            EFALHANDLE hSession = OGREFALGetSession(++counter);
            if (hSession == (EFALHANDLE)nullptr)
            {
                OGREFALReleaseSession(hSession);
                continue;
            }

            wchar_t* pwszFilename = CPLRecodeToWChar(CPLFormFilename(pszDirectory, papszFileList[iFile], nullptr), CPL_ENC_UTF8, CPL_ENC_UCS2);
            EFALHANDLE hTable = efallib->OpenTable(hSession, pwszFilename);
            CPLFree(pwszFilename);

            if (hTable == (EFALHANDLE)nullptr)
            {
                OGREFALReleaseSession(hSession);
                continue;
            }

            OGREFALLayer *poLayer = new OGREFALLayer(hSession, hTable, efalOpenMode);

            nLayers++;
            papoLayers = static_cast<OGRLayer **>(
                CPLRealloc(papoLayers, sizeof(void *) * nLayers));
            papoLayers[nLayers - 1] = poLayer;
        }

        CSLDestroy(papszFileList);

        if (nLayers == 0)
        {
            if (!bTestOpen)
                CPLError(CE_Failure, CPLE_OpenFailed,
                    "No mapinfo files found in directory %s.",
                    pszDirectory);

            return FALSE;
        }
    }
    return TRUE;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/
/*
This method attempts to create a new layer on the dataset with the indicated name, coordinate system, geometry type.
This method is reserved to implementation by drivers.
The papszOptions argument can be used to control driver specific creation options.
These options are normally documented in the format specific documentation.

Parameters
    pszName    the name for the new layer. This should ideally not match any existing layer on the datasource.
    poSpatialRef    the coordinate system to use for the new layer, or nullptr if no coordinate system is available.
    eGType    the geometry type for the layer. Use wkbUnknown if there are no constraints on the types geometry to be written.
    papszOptions    a StringList of name=value options. Options are driver specific.
*/
OGRLayer *
OGREFALDataSource::ICreateLayer(const char *pszLayerName,
    OGRSpatialReference *poSpatialRef,
    OGRwkbGeometryType eGType,
    char ** papszOptions)
{
    /* -------------------------------------------------------------------- */
    /*      Verify we are in update mode.                                   */
    /* -------------------------------------------------------------------- */
    if (!bUpdate)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
            "Data source %s opened read-only.\n"
            "New layer %s cannot be created.",
            pszName, pszLayerName);

        return nullptr;
    }
    // If it's a single file mode file, then we may have already
    // instantiated the low level layer.   We would just need to
    // reset the coordinate system and (potentially) bounds.
    OGREFALLayer *poLayer = nullptr;
    char *pszFullFilename = nullptr;

    if (bSingleFile)
    {
        if (bSingleLayerAlreadyCreated)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Unable to create new layers in this single file dataset.");
            return nullptr;
        }

        bSingleLayerAlreadyCreated = TRUE;

        poLayer = (OGREFALLayer *)papoLayers[0];
    }
    else
    {
        /* -------------------------------------------------------------------- */
        /*      Verify that the datasource is a directory.                      */
        /* -------------------------------------------------------------------- */
        VSIStatBufL sStatBuf;

        if ((VSIStatL(pszName, &sStatBuf) != 0
            || !VSI_ISDIR(sStatBuf.st_mode)))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                "Attempt to create layer against a non-directory datasource.");
            return nullptr;
        }


        EFALHANDLE hSession = OGREFALGetSession(++counter);
        if (hSession == (EFALHANDLE)nullptr)
        {
            return nullptr;
        }

        pszFullFilename = CPLStrdup(CPLFormFilename(pszDirectory, pszLayerName, "tab"));
        poLayer = new OGREFALLayer(hSession, pszLayerName, pszFullFilename, bCreateNativeX, nBlockSize, charset);

        nLayers++;
        papoLayers = static_cast<OGRLayer **>(
            CPLRealloc(papoLayers, sizeof(void*)*nLayers));
        papoLayers[nLayers - 1] = poLayer;

        CPLFree(pszFullFilename);
    }

    poLayer->SetDescription(poLayer->GetName());

    // Assign the coordinate system (if provided) and set reasonable bounds.
    if (poSpatialRef != nullptr)
    {
        // Pull out the bounds if supplied
        const char *pszOpt = nullptr;
        if ((pszOpt = CSLFetchNameValue(papszOptions, "BOUNDS")) != nullptr) {
            double dfBounds[4];
            if (CPLsscanf(pszOpt, "%lf,%lf,%lf,%lf", &dfBounds[0],
                &dfBounds[1],
                &dfBounds[2],
                &dfBounds[3]) != 4)
            {
                CPLError(
                    CE_Failure, CPLE_IllegalArg,
                    "Invalid BOUNDS parameter, expected min_x,min_y,max_x,max_y");
            }
            else
            {
                poLayer->SetBounds(dfBounds[0], dfBounds[1], dfBounds[2], dfBounds[3]);
            }
        }
    }

    if (eGType != OGRwkbGeometryType::wkbNone) {
        poLayer->SetSpatialRef(poSpatialRef);
        poLayer->GetLayerDefn()->GetGeomFieldDefn(0)->SetName("OBJ");
    }
    return poLayer;
}


/************************************************************************/
/*                          Create()                                    */
/************************************************************************/
int OGREFALDataSource::Create(const char *pszFileName, char ** papszOptions)
{
    pszName = CPLStrdup(pszFileName);

    const char *pszOpt = CSLFetchNameValue(papszOptions, "FORMAT");
    if (pszOpt != nullptr && EQUAL(pszOpt, "NATIVEX")) {
        bCreateNativeX = TRUE;
        charset = Ellis::MICHARSET::CHARSET_UTF8;
    }

    pszOpt = CSLFetchNameValue(papszOptions, "CHARSET");
    if (pszOpt != nullptr)
    {
        if (EQUAL(pszOpt, "NEUTRAL")) charset = Ellis::MICHARSET::CHARSET_NEUTRAL;
        else if (EQUAL(pszOpt, "ISO8859_1")) charset = Ellis::MICHARSET::CHARSET_ISO8859_1;
        else if (EQUAL(pszOpt, "ISO8859_2")) charset = Ellis::MICHARSET::CHARSET_ISO8859_2;
        else if (EQUAL(pszOpt, "ISO8859_3")) charset = Ellis::MICHARSET::CHARSET_ISO8859_3;
        else if (EQUAL(pszOpt, "ISO8859_4")) charset = Ellis::MICHARSET::CHARSET_ISO8859_4;
        else if (EQUAL(pszOpt, "ISO8859_5")) charset = Ellis::MICHARSET::CHARSET_ISO8859_5;
        else if (EQUAL(pszOpt, "ISO8859_6")) charset = Ellis::MICHARSET::CHARSET_ISO8859_6;
        else if (EQUAL(pszOpt, "ISO8859_7")) charset = Ellis::MICHARSET::CHARSET_ISO8859_7;
        else if (EQUAL(pszOpt, "ISO8859_8")) charset = Ellis::MICHARSET::CHARSET_ISO8859_8;
        else if (EQUAL(pszOpt, "ISO8859_9")) charset = Ellis::MICHARSET::CHARSET_ISO8859_9;
        else if (EQUAL(pszOpt, "WLATIN1")) charset = Ellis::MICHARSET::CHARSET_WLATIN1;
        else if (EQUAL(pszOpt, "WLATIN2")) charset = Ellis::MICHARSET::CHARSET_WLATIN2;
        else if (EQUAL(pszOpt, "WARABIC")) charset = Ellis::MICHARSET::CHARSET_WARABIC;
        else if (EQUAL(pszOpt, "WCYRILLIC")) charset = Ellis::MICHARSET::CHARSET_WCYRILLIC;
        else if (EQUAL(pszOpt, "WGREEK")) charset = Ellis::MICHARSET::CHARSET_WGREEK;
        else if (EQUAL(pszOpt, "WHEBREW")) charset = Ellis::MICHARSET::CHARSET_WHEBREW;
        else if (EQUAL(pszOpt, "WTURKISH")) charset = Ellis::MICHARSET::CHARSET_WTURKISH;
        else if (EQUAL(pszOpt, "WTCHINESE")) charset = Ellis::MICHARSET::CHARSET_WTCHINESE;
        else if (EQUAL(pszOpt, "WSCHINESE")) charset = Ellis::MICHARSET::CHARSET_WSCHINESE;
        else if (EQUAL(pszOpt, "WJAPANESE")) charset = Ellis::MICHARSET::CHARSET_WJAPANESE;
        else if (EQUAL(pszOpt, "WKOREAN")) charset = Ellis::MICHARSET::CHARSET_WKOREAN;
#if 0    // Not supported in EFAL
        else if (EQUAL(pszOpt, "MROMAN")) charset = Ellis::MICHARSET::CHARSET_MROMAN;
        else if (EQUAL(pszOpt, "MARABIC")) charset = Ellis::MICHARSET::CHARSET_MARABIC;
        else if (EQUAL(pszOpt, "MGREEK")) charset = Ellis::MICHARSET::CHARSET_MGREEK;
        else if (EQUAL(pszOpt, "MHEBREW")) charset = Ellis::MICHARSET::CHARSET_MHEBREW;
        else if (EQUAL(pszOpt, "MCENTEURO")) charset = Ellis::MICHARSET::CHARSET_MCENTEURO;
        else if (EQUAL(pszOpt, "MCROATIAN")) charset = Ellis::MICHARSET::CHARSET_MCROATIAN;
        else if (EQUAL(pszOpt, "MCYRILLIC")) charset = Ellis::MICHARSET::CHARSET_MCYRILLIC;
        else if (EQUAL(pszOpt, "MICELANDIC")) charset = Ellis::MICHARSET::CHARSET_MICELANDIC;
        else if (EQUAL(pszOpt, "MTHAI")) charset = Ellis::MICHARSET::CHARSET_MTHAI;
        else if (EQUAL(pszOpt, "MTURKISH")) charset = Ellis::MICHARSET::CHARSET_MTURKISH;
        else if (EQUAL(pszOpt, "MTCHINESE")) charset = Ellis::MICHARSET::CHARSET_MTCHINESE;
        else if (EQUAL(pszOpt, "MJAPANESE")) charset = Ellis::MICHARSET::CHARSET_MJAPANESE;
        else if (EQUAL(pszOpt, "MKOREAN")) charset = Ellis::MICHARSET::CHARSET_MKOREAN;
#endif
        else if (EQUAL(pszOpt, "CP437")) charset = Ellis::MICHARSET::CHARSET_CP437;
        else if (EQUAL(pszOpt, "CP850")) charset = Ellis::MICHARSET::CHARSET_CP850;
        else if (EQUAL(pszOpt, "CP852")) charset = Ellis::MICHARSET::CHARSET_CP852;
        else if (EQUAL(pszOpt, "CP857")) charset = Ellis::MICHARSET::CHARSET_CP857;
        else if (EQUAL(pszOpt, "CP860")) charset = Ellis::MICHARSET::CHARSET_CP860;
        else if (EQUAL(pszOpt, "CP861")) charset = Ellis::MICHARSET::CHARSET_CP861;
        else if (EQUAL(pszOpt, "CP863")) charset = Ellis::MICHARSET::CHARSET_CP863;
        else if (EQUAL(pszOpt, "CP865")) charset = Ellis::MICHARSET::CHARSET_CP865;
        else if (EQUAL(pszOpt, "CP855")) charset = Ellis::MICHARSET::CHARSET_CP855;
        else if (EQUAL(pszOpt, "CP864")) charset = Ellis::MICHARSET::CHARSET_CP864;
        else if (EQUAL(pszOpt, "CP869")) charset = Ellis::MICHARSET::CHARSET_CP869;
#if 0    // Not supported in EFAL
        else if (EQUAL(pszOpt, "LICS")) charset = Ellis::MICHARSET::CHARSET_LICS;
        else if (EQUAL(pszOpt, "LMBCS")) charset = Ellis::MICHARSET::CHARSET_LMBCS;
        else if (EQUAL(pszOpt, "LMBCS1")) charset = Ellis::MICHARSET::CHARSET_LMBCS1;
        else if (EQUAL(pszOpt, "LMBCS2")) charset = Ellis::MICHARSET::CHARSET_LMBCS2;
        else if (EQUAL(pszOpt, "MSCHINESE")) charset = Ellis::MICHARSET::CHARSET_MSCHINESE;
        else if (EQUAL(pszOpt, "UTCHINESE")) charset = Ellis::MICHARSET::CHARSET_UTCHINESE;
        else if (EQUAL(pszOpt, "USCHINESE")) charset = Ellis::MICHARSET::CHARSET_USCHINESE;
        else if (EQUAL(pszOpt, "UJAPANESE")) charset = Ellis::MICHARSET::CHARSET_UJAPANESE;
        else if (EQUAL(pszOpt, "UKOREAN")) charset = Ellis::MICHARSET::CHARSET_UKOREAN;
#endif
        else if (EQUAL(pszOpt, "WTHAI")) charset = Ellis::MICHARSET::CHARSET_WTHAI;
        else if (EQUAL(pszOpt, "WBALTICRIM")) charset = Ellis::MICHARSET::CHARSET_WBALTICRIM;
        else if (EQUAL(pszOpt, "WVIETNAMESE")) charset = Ellis::MICHARSET::CHARSET_WVIETNAMESE;
        else if (EQUAL(pszOpt, "UTF8") && bCreateNativeX) charset = Ellis::MICHARSET::CHARSET_UTF8;
        else if (EQUAL(pszOpt, "UTF16") && bCreateNativeX) charset = Ellis::MICHARSET::CHARSET_UTF16;
    }

    nBlockSize = atoi(CSLFetchNameValueDef(papszOptions, "BLOCKSIZE", "16384"));
    // Map file block size should be less than or equal to 32K (32768 bytes) 
    if (nBlockSize > 32768) nBlockSize = 32768;
    if (nBlockSize < 512) nBlockSize = 512;

    bUpdate = TRUE;
    efalOpenMode = EfalOpenMode::EFAL_LOCK_WRITE;

    // Create a new empty directory.
    VSIStatBufL sStat;

    if (strlen(CPLGetExtension(pszName)) == 0)
    {
        if (VSIStatL(pszName, &sStat) == 0)
        {
            if (!VSI_ISDIR(sStat.st_mode))
            {
                CPLError(CE_Failure, CPLE_OpenFailed,
                    "Attempt to create dataset named %s,\n"
                    "but that is an existing file.",
                    pszName);
                return FALSE;
            }
        }
        else
        {
            if (VSIMkdir(pszName, 0755) != 0)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                    "Unable to create directory %s.",
                    pszName);
                return FALSE;
            }
        }

        pszDirectory = CPLStrdup(pszName);
    }
    // Create a new single file.
    else
    {
        pszDirectory = CPLStrdup(CPLGetPath(pszName));
        if (strlen(pszDirectory) > 0)
        {
            if (VSIStatL(pszDirectory, &sStat) == 0)
            {
                if (!VSI_ISDIR(sStat.st_mode))
                {
                    CPLError(CE_Failure, CPLE_OpenFailed,
                        "Attempt to create dataset named %s,\n"
                        "but that is an existing file.",
                        pszDirectory);
                    CPLFree(pszDirectory);
                    return FALSE;
                }
            }
            else
            {
                if (VSIMkdir(pszDirectory, 0755) != 0)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                        "Unable to create directory %s.",
                        pszDirectory);
                    CPLFree(pszDirectory);
                    return FALSE;
                }
            }
        }

        EFALHANDLE hSession = OGREFALGetSession(++counter);
        if (hSession == (EFALHANDLE)nullptr)
        {
            CPLFree(pszDirectory);
            return FALSE;
        }

        // member variable pszName should be the layer name (alias) and argument pszFileName should be the TAB filename 
        char * pszLayerName = EFAL_GDAL_DRIVER::TABGetBasename(pszFileName);
        OGREFALLayer *poLayer = new OGREFALLayer(hSession, pszLayerName, pszFileName, bCreateNativeX, nBlockSize, charset);
        CPLFree(pszLayerName);

        nLayers = 1;
        papoLayers = static_cast<OGRLayer **>(CPLMalloc(sizeof(void *)));
        papoLayers[0] = poLayer;
        bSingleFile = true;
    }

    return TRUE;
}
