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

#if defined(_WIN32) && !defined(unix)
	#pragma warning(disable:4251)
#endif

#include "cpl_port.h"

#include <cerrno>
#include <cstring>
#include <map>
#include <string>
#include <utility>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"

#include "OGREFAL.h"

CPL_CVSID("$Id: OGREFALdriver.cpp 38032 2017-04-16 08:26:01Z rouault $");

static void OGREFALUnloadAll();
EFALHANDLE OGREFALGetSession(GUIntBig nEFALSession);
void OGREFALReleaseSession(EFALHANDLE);

static CPLMutex* hMutexEFALLIB = nullptr;
EFALLIB * efallib = nullptr;

/************************************************************************/
/*                         OGREFALDriverIdentify()                       */
/************************************************************************/

static int OGREFALDriverIdentify(GDALOpenInfo* poOpenInfo)
{

	if (!poOpenInfo->bStatOK)
		return FALSE;
	if (poOpenInfo->bIsDirectory)
		return -1;  // Unsure.
	if (poOpenInfo->fpL == nullptr)
		return FALSE;

	if (STARTS_WITH_CI(poOpenInfo->pszFilename, "/vsi"))
	{
		return FALSE;
	}

	const CPLString osExt = OGREFALDataSource::GetRealExtension(poOpenInfo->pszFilename);

	if (EQUAL(osExt, "tab"))
	{
		for (int i = 0; i < poOpenInfo->nHeaderBytes; i++)
		{
			const char *pszLine = (const char *)poOpenInfo->pabyHeader + i;
			if (STARTS_WITH_CI(pszLine, "Definition Table"))
				return TRUE;
		}
		return FALSE;
	}
	else
	{
		return FALSE;
	}
}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGREFALDriverOpen(GDALOpenInfo* poOpenInfo)
{
	if (!OGREFALDriverIdentify(poOpenInfo))
		return nullptr;

	OGREFALDataSource *poDS = new OGREFALDataSource();

	if (!poDS->Open(poOpenInfo, TRUE))
	{
		delete poDS;
		poDS = nullptr;
	}

	return poDS;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

static GDALDataset *OGREFALDriverCreate(const char * pszName,
	CPL_UNUSED int /*nBands*/,
	CPL_UNUSED int /*nXSize*/,
	CPL_UNUSED int /*nYSize*/,
	CPL_UNUSED GDALDataType /*eDT*/,
	char **papszOptions)
{
	if (STARTS_WITH_CI(pszName, "/vsi"))
	{
		return nullptr;
	}

	// Try to create the data source.
	OGREFALDataSource   *poDS = new OGREFALDataSource();
	if (!poDS->Create(pszName, papszOptions))
	{
		delete poDS;
		return nullptr;
	}

	return poDS;
}

/************************************************************************/
/*                              Delete()                                */
/************************************************************************/

static CPLErr OGREFALDriverDelete(const char *pszDataSource)
{
	GDALDataset *poDS = nullptr;
	{
		// Make sure that the file opened by GDALOpenInfo is closed
		// when the object goes out of scope
		GDALOpenInfo oOpenInfo(pszDataSource, GA_ReadOnly);
		poDS = OGREFALDriverOpen(&oOpenInfo);
	}
	if (poDS == nullptr)
		return CE_Failure;
	char **papszFileList = poDS->GetFileList();
	delete poDS;

	char **papszIter = papszFileList;
	while (papszIter && *papszIter)
	{
		VSIUnlink(*papszIter);
		papszIter++;
	}
	CSLDestroy(papszFileList);

	VSIStatBufL sStatBuf;
	if (VSIStatL(pszDataSource, &sStatBuf) == 0 && VSI_ISDIR(sStatBuf.st_mode))
	{
		VSIRmdir(pszDataSource);
	}
	return CE_Failure;
}

/************************************************************************/
/*                           OGREFALDriverUnload()                       */
/************************************************************************/

static void OGREFALDriverUnload(GDALDriver* /*poDriver*/)
{
	OGREFALUnloadAll();
}

/************************************************************************/
/*                           RegisterOGREFAL()                           */
/************************************************************************/
void RegisterOGREFAL()

{
	CPLMutexHolderD(&hMutexEFALLIB);

	efallib = EFALLIB::Create();
	if ((efallib == nullptr) ||
		(!efallib->HasGetRowCountProc()) ||
		(!efallib->HasCoordSys2PRJStringProc()) ||
		(!efallib->HasCoordSys2MBStringProc()) ||
		(!efallib->HasPRJ2CoordSysStringProc()) ||
		(!efallib->HasMB2CoordSysStringProc())) {
		// EFAL is not present (or is an older version that does not have the newer methods we depend upon so don't register the driver.
		return;
	}

	if (GDALGetDriverByName("MapInfo EFAL") != nullptr)
		return;

	GDALDriver *poDriver = new GDALDriver();

	poDriver->SetDescription("MapInfo EFAL");
	poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "MapInfo EFAL");
	poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drv_efal.html");
	poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "tab");
	// GDAL_DMD_CONNECTION_PREFIX
	poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
		"<CreationOptionList>"
		"  <Option name='FORMAT' type='string-select' description='type of MapInfo format'>"
		"    <Value>NATIVE</Value>"
		"    <Value>NATIVEX</Value>"
		"  </Option>"
		"  <Option name='CHARSET' type='string-select' description='type of character encoding to use for new tables. The default is NEUTRAL for Native and UTF8 for NativeX'>"
		"    <Value>NATIVE</Value>"
		"    <Value>ISO8859_1</Value>"
		"    <Value>ISO8859_2</Value>"
		"    <Value>ISO8859_3</Value>"
		"    <Value>ISO8859_4</Value>"
		"    <Value>ISO8859_5</Value>"
		"    <Value>ISO8859_6</Value>"
		"    <Value>ISO8859_7</Value>"
		"    <Value>ISO8859_8</Value>"
		"    <Value>ISO8859_9</Value>"
		"    <Value>WLATIN1</Value>"
		"    <Value>WLATIN2</Value>"
		"    <Value>WARABIC</Value>"
		"    <Value>WCYRILLIC</Value>"
		"    <Value>WGREEK</Value>"
		"    <Value>WHEBREW</Value>"
		"    <Value>WTURKISH</Value>"
		"    <Value>WTCHINESE</Value>"
		"    <Value>WSCHINESE</Value>"
		"    <Value>WJAPANESE</Value>"
		"    <Value>WKOREAN</Value>"
#if 0	// Not supported in EFAL
		"    <Value>MROMAN</Value>"
		"    <Value>MARABIC</Value>"
		"    <Value>MGREEK</Value>"
		"    <Value>MHEBREW</Value>"
		"    <Value>MCENTEURO</Value>"
		"    <Value>MCROATIAN</Value>"
		"    <Value>MCYRILLIC</Value>"
		"    <Value>MICELANDIC</Value>"
		"    <Value>MTHAI</Value>"
		"    <Value>MTURKISH</Value>"
		"    <Value>MTCHINESE</Value>"
		"    <Value>MJAPANESE</Value>"
		"    <Value>MKOREAN</Value>"
#endif
		"    <Value>CP437</Value>"
		"    <Value>CP850</Value>"
		"    <Value>CP852</Value>"
		"    <Value>CP857</Value>"
		"    <Value>CP860</Value>"
		"    <Value>CP861</Value>"
		"    <Value>CP863</Value>"
		"    <Value>CP865</Value>"
		"    <Value>CP855</Value>"
		"    <Value>CP864</Value>"
		"    <Value>CP869</Value>"
#if 0	// Not supported in EFAL
		"    <Value>LICS</Value>"
		"    <Value>LMBCS</Value>"
		"    <Value>LMBCS1</Value>"
		"    <Value>LMBCS2</Value>"
		"    <Value>MSCHINESE</Value>"
		"    <Value>UTCHINESE</Value>"
		"    <Value>USCHINESE</Value>"
		"    <Value>UJAPANESE</Value>"
		"    <Value>UKOREAN</Value>"
#endif
		"    <Value>WTHAI</Value>"
		"    <Value>WBALTICRIM</Value>"
		"    <Value>WVIETNAMESE</Value>"
		"    <Value>UTF8</Value>"
		"    <Value>UTF16</Value>"
		"  </Option>"
		"  <Option name='BLOCKSIZE' type='int' description='.map block size' min='512' max='32256' default='16384'/>"
		"</CreationOptionList>");
	// GDAL_DMD_OPENOPTIONLIST 
	poDriver->SetMetadataItem(GDAL_DMD_OPENOPTIONLIST,
		"<OpenOptionList>"
		"  <Option name='MODE' type='string' description='Open mode. "
		"READ-ONLY - open for read-only, "
		"LOCK-READ - open for read-only with files locked open (which will improve read performance but prevent writes from other threads/applications), "
		"READ-WRITE - open for read and write, "
		"LOCK-WRITE - open for read and write with the files locked for writing."
		"' default='READ-WRITE'/>"
		"</OpenOptionList>");
	poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES,
		"Integer Integer64 Real String Date DateTime Time");
	poDriver->SetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST,
		"<LayerCreationOptionList>"
		"  <Option name='BOUNDS' type='string' description='Custom bounds. Expected format is xmin,ymin,xmax,ymax'/>"
		"</LayerCreationOptionList>");
	// GDAL_DMD_CREATIONFIELDDATASUBTYPES
	// GDAL_DCAP_OPEN 
	// GDAL_DCAP_CREATE
	// GDAL_DCAP_CREATECOPY 
	// GDAL_DCAP_VIRTUALIO
	poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
	// GDAL_DCAP_GNM
	// GDAL_DCAP_NONSPATIAL
	// GDAL_DCAP_FEATURE_STYLES 


	poDriver->pfnOpen = OGREFALDriverOpen;
	poDriver->pfnIdentify = OGREFALDriverIdentify;
	poDriver->pfnCreate = OGREFALDriverCreate;
	poDriver->pfnDelete = OGREFALDriverDelete;
	poDriver->pfnUnloadDriver = OGREFALDriverUnload;

	GetGDALDriverManager()->RegisterDriver(poDriver);
}

/************************************************************************/
/*                       OGR EFAL Session Management                    */
/************************************************************************/
static CPLMutex* hMutexEFALSessionManage = nullptr;
static std::map<GUIntBig, EFALHANDLE> * mapEFALSessions;
static std::map<EFALHANDLE, int> * mapEFALSessionRefCount;

void OGREFALReleaseSession(EFALHANDLE hSession)
{
	CPLMutexHolderD(&hMutexEFALSessionManage);
	// These should exist already if we're releasing a session....
	if ((mapEFALSessions != nullptr) && (mapEFALSessionRefCount != nullptr))
	{
		std::map<EFALHANDLE, int>::iterator oIterRefCount = mapEFALSessionRefCount->find(hSession);
		if (oIterRefCount != mapEFALSessionRefCount->end())
		{
			int refCount = oIterRefCount->second;
			// refCount should be greater than zero...
			if (refCount > 0)
			{
				refCount--;
			}
			if (refCount == 0)
			{
				mapEFALSessionRefCount->erase(oIterRefCount);
				std::map<GUIntBig, EFALHANDLE>::iterator oIter = mapEFALSessions->begin();
				while(oIter != mapEFALSessions->end())
				{
					if (oIter->second == hSession)
					{
						break;
					}
					++oIter;
				}

				if (oIter != mapEFALSessions->end()) 
				{
					mapEFALSessions->erase(oIter->first);
					efallib->DestroySession(hSession);
				}
			}
			else
			{
				(*mapEFALSessionRefCount)[hSession] = refCount;
			}
		}
	}
}

EFALHANDLE OGREFALGetSession(GUIntBig nEFALSession)
{
	CPLMutexHolderD(&hMutexEFALSessionManage);

	// If the maps don't exist, create them
	if (mapEFALSessions == nullptr)
		mapEFALSessions = new std::map<GUIntBig, EFALHANDLE>();

	if (mapEFALSessionRefCount == nullptr)
		mapEFALSessionRefCount = new std::map<EFALHANDLE, int>();


	// Initialize our session
	EFALHANDLE hSession = 0;

	// Look for the session by name in the map and use it or create a new session and register it
	std::map<GUIntBig, EFALHANDLE>::iterator oIter = mapEFALSessions->find(nEFALSession);
	if (oIter != mapEFALSessions->end())
	{
		hSession = oIter->second;
	}
	else
	{
		hSession = efallib->InitializeSession(nullptr); // TODO Error String Handler???
		(*mapEFALSessions)[nEFALSession] = hSession;
	}

	// Add a reference to our session
	std::map<EFALHANDLE, int>::iterator oIterRefCount = mapEFALSessionRefCount->find(hSession);
	int refCount = 1 + ((oIterRefCount != mapEFALSessionRefCount->end()) ? oIterRefCount->second : 0);
	(*mapEFALSessionRefCount)[hSession] = refCount;

	return hSession;
}

static void OGREFALUnloadAll()
{
	delete mapEFALSessions;
	mapEFALSessions = nullptr;

	delete mapEFALSessionRefCount;
	mapEFALSessionRefCount = nullptr;

	if (hMutexEFALSessionManage != nullptr)
		CPLDestroyMutex(hMutexEFALSessionManage);
	hMutexEFALSessionManage = nullptr;
}
