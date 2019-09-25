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

#include "MRRSDKImpl.h"

#if defined(_WIN32) || defined(WIN32)
#define MIRASTER_SDK_DLL					"MIRasterAPIRT.dll"
#else
#define MIRASTER_SDK_DLL					"libMIRasterAPIRT.so"
#endif

SDKDynamicImpl* SDKDynamicImpl::m_pSDKImpl = nullptr;

SDKDynamicImpl::SDKDynamicImpl()
{
	m_hMIRasterSDKInstance = nullptr;

	ClearAll();
}

SDKDynamicImpl::~SDKDynamicImpl()
{
	Release();
}

SDKDynamicImpl& SDKDynamicImpl::Get()
{
	if (!m_pSDKImpl)
		m_pSDKImpl = new SDKDynamicImpl();

	return *m_pSDKImpl;
}

bool SDKDynamicImpl::Init()
{
	//If SDK is already loaded no need to load it again
	if (m_hMIRasterSDKInstance != nullptr)
		return true;

	//Load APIRT.dll
	{
#if defined(_WIN32) || defined(WIN32)
		m_hMIRasterSDKInstance = LoadLibrary(MIRASTER_SDK_DLL);
#else
		m_hMIRasterSDKInstance = dlopen(MIRASTER_SDK_DLL, RTLD_LAZY);
#endif
	}

	if (m_hMIRasterSDKInstance == NULL) {
		return false;
	}

	//resolve function address here
	{
		//Use non-Handler version of methods
		m_fpOpenRaster_ReadOnly = (pOpenRaster_ReadOnly)GetProcAddress(m_hMIRasterSDKInstance, "MIR_OpenRasterReadOnly");
		m_fpGetStatistics = (pGetStatistics)GetProcAddress(m_hMIRasterSDKInstance, "MIR_GetStatistics");
		m_fpComputeStatistics = (pComputeStatistics)GetProcAddress(m_hMIRasterSDKInstance, "MIR_ComputeStatistics");
		m_fpVerifyRaster = (pVerifyRaster)GetProcAddress(m_hMIRasterSDKInstance, "MIR_VerifyRaster");
		m_fpReleaseData = (pReleaseData)GetProcAddress(m_hMIRasterSDKInstance, "MIR_ReleaseData");
		m_fpReleaseRasterInfo = (pReleaseRasterInfo)GetProcAddress(m_hMIRasterSDKInstance, "MIR_ReleaseRasterInfo");
		m_fpReleaseStatistics = (pReleaseStatistics)GetProcAddress(m_hMIRasterSDKInstance, "MIR_ReleaseStatistics");
		m_fpGetOpenInfo = (pGetOpenInfo)GetProcAddress(m_hMIRasterSDKInstance, "MIR_GetOpenInfo");
		m_fpRasterInfo = (pRasterInfo)GetProcAddress(m_hMIRasterSDKInstance, "MIR_RasterInfo");
		m_fpCloseRaster = (pCloseRaster)GetProcAddress(m_hMIRasterSDKInstance, "MIR_CloseRaster");
		m_fpGetCellSize = (pGetCellSize)GetProcAddress(m_hMIRasterSDKInstance, "MIR_GetCellSize");
		m_fpPopulateCellBlock = (pPopulateCellBlock)GetProcAddress(m_hMIRasterSDKInstance, "MIR_PopulateCellBlock");
		m_fpFieldInfo = (pFieldInfo)GetProcAddress(m_hMIRasterSDKInstance, "MIR_FieldInfo");
		m_fpInfoLevelCount = (pInfoLevelCount)GetProcAddress(m_hMIRasterSDKInstance, "MIR_InfoLevelCount");
		m_fpLevelInfo = (pLevelInfo)GetProcAddress(m_hMIRasterSDKInstance, "MIR_LevelInfo");
		m_fpInfoBandCount = (pInfoBandCount)GetProcAddress(m_hMIRasterSDKInstance, "MIR_InfoBandCount");
		m_fpBandInfo = (pBandInfo)GetProcAddress(m_hMIRasterSDKInstance, "MIR_BandInfo");
		m_fpDataTypeSizeInBytes = (pDataTypeSizeInBytes)GetProcAddress(m_hMIRasterSDKInstance, "MIR_DataTypeSizeInBytes");

		m_fpClassTableGetRecordCount = (pClassTableGetRecordCount)GetProcAddress(m_hMIRasterSDKInstance, "MIR_ClassTableGetRecordCount");
		m_fpClassTableGetRecord = (pClassTableGetRecord)GetProcAddress(m_hMIRasterSDKInstance, "MIR_ClassTableGetRecord");
		m_fpClassTableFindField = (pClassTableFindField)GetProcAddress(m_hMIRasterSDKInstance, "MIR_ClassTableFindField");

		m_fpRBIBeginRead = (pRBIBeginRead)GetProcAddress(m_hMIRasterSDKInstance, "MIR_RBI_BeginRead");
		m_fpRBIEnd = (pRBIEnd)GetProcAddress(m_hMIRasterSDKInstance, "MIR_RBI_End");
		m_fpRBIGetBlock = (pRBIGetBlock)GetProcAddress(m_hMIRasterSDKInstance, "MIR_RBI_GetBlock");
	}

	if (!IsAllValid())
		return false;

	return true;
}

/*
	Release resource
*/

bool SDKDynamicImpl::Release()
{
#if defined(_WIN32) || defined(WIN32)
	bool bRet = FreeLibrary(m_hMIRasterSDKInstance) == TRUE;
#else
	bool bRet = dlclose(m_hMIRasterSDKInstance);
#endif
	m_hMIRasterSDKInstance = nullptr;

	ClearAll();

	return bRet;
}

void SDKDynamicImpl::ClearAll()
{
	m_fpOpenRaster_ReadOnly = nullptr;
	m_fpGetStatistics = nullptr;
	m_fpComputeStatistics = nullptr;
	m_fpVerifyRaster = nullptr;
	m_fpReleaseData = nullptr;
	m_fpReleaseRasterInfo = nullptr;
	m_fpReleaseStatistics = nullptr;
	m_fpGetOpenInfo = nullptr;
	m_fpRasterInfo = nullptr;
	m_fpCloseRaster = nullptr;
	m_fpGetCellSize = nullptr;
	m_fpPopulateCellBlock = nullptr;
	m_fpFieldInfo = nullptr;
	m_fpInfoLevelCount = nullptr;
	m_fpLevelInfo = nullptr;
	m_fpInfoBandCount = nullptr;
	m_fpBandInfo = nullptr;
	m_fpRBIBeginRead = nullptr;
	m_fpRBIEnd = nullptr;
	m_fpRBIGetBlock = nullptr;
	m_fpDataTypeSizeInBytes = nullptr;
	m_fpClassTableGetRecordCount = nullptr;
	m_fpClassTableGetRecord = nullptr;
	m_fpClassTableFindField = nullptr;
}

bool SDKDynamicImpl::IsAllValid()
{
	//if Any of the pointers is not initialized return false
	if (m_fpOpenRaster_ReadOnly == nullptr ||
		m_fpGetStatistics == nullptr ||
		m_fpComputeStatistics == nullptr ||
		m_fpVerifyRaster == nullptr ||
		m_fpReleaseData == nullptr ||
		m_fpReleaseRasterInfo == nullptr ||
		m_fpReleaseStatistics == nullptr ||
		m_fpGetOpenInfo == nullptr ||
		m_fpRasterInfo == nullptr ||
		m_fpCloseRaster == nullptr ||
		m_fpGetCellSize == nullptr ||
		m_fpFieldInfo == nullptr ||
		m_fpInfoLevelCount == nullptr ||
		m_fpLevelInfo == nullptr ||
		m_fpInfoBandCount == nullptr ||
		m_fpBandInfo == nullptr ||
		m_fpRBIBeginRead == nullptr ||
		m_fpRBIEnd == nullptr ||
		m_fpRBIGetBlock == nullptr ||
		m_fpDataTypeSizeInBytes == nullptr ||
		m_fpClassTableGetRecordCount == nullptr ||
		m_fpClassTableGetRecord == nullptr ||
		m_fpClassTableFindField == nullptr)
			return false;

	return true;
}
