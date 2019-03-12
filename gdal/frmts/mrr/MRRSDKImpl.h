/******************************************************************************************************************************
	MapInfo Professional MRR Raster API

	Copyright � 1985-2016,Pitney Bowes Software Inc.
	All rights reserved.
	Confidential Property of Pitney Bowes Software
	
******************************************************************************************************************************/

#pragma once
#ifndef __SDKDYNAMICIMPL_H_
#define __SDKDYNAMICIMPL_H_

#include "APIDef.h"
#if defined(_WIN32) || defined(WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#define HINSTANCE void*
#define GetProcAddress(x,y) dlsym(x,y)
#endif

/*
	Responsible for Raster SDK dynamic loading.
*/
class SDKDynamicImpl
{
private:
	//If any change happens in the signature of these API's we need to reflect that here
	//Resource management
	typedef MIRResult(*pReleaseStatistics)			(SMIR_Statistics** ppStats);
	typedef MIRResult(*pReleaseData)				(unsigned char** ppData);
	typedef MIRResult(*pReleaseRasterInfo)			(unsigned int nInfoHandle);

	//	Dataset access
	typedef MIRResult(*pVerifyRaster)				(const wchar_t* pwsFileURL);
	typedef MIRResult(*pOpenRaster_ReadOnly)		(const wchar_t* pwsFileURL, unsigned int& nDatasetHandle, MIR_RasterSupportMode eRasterSupportMode, unsigned int nProgressTrackerHandle);
	typedef MIRResult(*pGetStatistics)				(unsigned int nDatasetHandle, unsigned int nField, unsigned int nBand, int nResolution, SMIR_Statistics** ppStats,
		MIR_StatisticsMode nMode, bool bRecalculate_if_invalid, uint32_t nBinCount, uint32_t nProgressTrackerHandle);
	typedef MIRResult(*pComputeStatistics)			(unsigned int nDatasetHandle, unsigned int nField, unsigned int nBand, int nResolution, time_t nFirstTime,
		time_t nLastTime, SMIR_Statistics** ppStats, MIR_StatisticsMode nMode, uint32_t nBinCount, uint32_t nProgressTrackerHandle);
	typedef MIRResult(*pCloseRaster)				(unsigned int nDatasetHandle, SMIR_FinalisationOptions* cFinalise, unsigned int nProgressTrackerHandle);

	//Iterator
	typedef MIRResult(*pRBIBeginRead)				(uint32_t nRasterHandle, uint32_t& nItHandle, uint32_t nField, time_t nStartTime, time_t nEndTime, int32_t nResolution, bool bGridCellCoords);
	typedef MIRResult(*pRBIEnd)						(uint32_t nItHandle);
	typedef MIRResult(*pRBIGetBlock)				(uint32_t nItHandle, uint32_t nBand, int64_t nCellX, int64_t nCellY, uint32_t nWidth, uint32_t nHeight, uint8_t** ppDataArray, uint8_t** ppValidArray, MIR_DataType nDataType);

	//Dataset field information
	typedef MIRResult(*pGetCellSize)				(unsigned int nDatasetHandle, unsigned int nField, int nResolution, double& dXCell, double& dYCell);
	typedef MIRResult(*pGetOpenInfo)				(unsigned int nDatasetHandle, unsigned int& nInfoHandle, unsigned int nProgressTrackerHandle);

	//	Acquire raster info data for get/set
	typedef MIRResult(*pRasterInfo)					(unsigned int nInfoHandle, SMIR_RasterInfo** ppRasterInfo);
	typedef MIRResult(*pFieldInfo)					(unsigned int nInfoHandle, unsigned int nField, SMIR_FieldInfo** ppFieldInfo);
	typedef unsigned int(*pInfoBandCount)			(unsigned int nInfoHandle, unsigned int nField);
	typedef MIRResult(*pBandInfo)					(unsigned int nInfoHandle, unsigned int nField, unsigned int nBand, SMIR_BandInfo** ppBandInfo);
	typedef unsigned int(*pInfoLevelCount)			(unsigned int nInfoHandle, unsigned int nField, unsigned int nEvent);
	typedef MIRResult(*pLevelInfo)					(unsigned int nInfoHandle, unsigned int nField, unsigned int nEvent, unsigned int nLevel, SMIR_LevelInfo** ppLevelInfo);

	typedef MIRResult(*pSetCacheSize)				(uint64_t nMegabytes);
	typedef int(*pDataTypeSize_InBytes)				(MIR_DataType nDataType);
	typedef MIRResult(*pPopulateCellBlock)			(unsigned int nDatasetHandle, unsigned char** ppData, unsigned char** ppValid,
		int nDestDataType, int64_t nCol, int64_t nRow, uint64_t nCols,
		uint64_t nRows, uint64_t nDestCols, uint64_t nDestRows,
		unsigned int nField, unsigned int nBand, MIR_InterpolationMethod nInterpolationMethod,
		time_t nFirstTime, time_t nLastTime);
	typedef int32_t(*pDataTypeSizeInBytes)			(MIR_DataType nDataType);

	typedef MIRResult(*pClassTableGetRecordCount)	(uint32_t nRasterHandle, uint32_t nField, uint32_t& nRecordCount);
	typedef MIRResult(*pClassTableGetRecord)		(uint32_t nRasterHandle, uint32_t nField, uint32_t nTableField, uint32_t nRecord, uint8_t** ppData, MIR_DataType& nDataType, uint32_t& nDataSize);
	typedef MIRResult(*pClassTableFindField)		(uint32_t nRasterHandle, uint32_t nField, MIR_ClassTableFieldType eType, uint32_t& nTableField);

	//function pointers
	pOpenRaster_ReadOnly			m_fpOpenRaster_ReadOnly;
	pGetStatistics					m_fpGetStatistics;
	pComputeStatistics				m_fpComputeStatistics;
	pReleaseStatistics				m_fpReleaseStatistics;
	pReleaseData					m_fpReleaseData;
	pReleaseRasterInfo				m_fpReleaseRasterInfo;
	pVerifyRaster					m_fpVerifyRaster;
	pGetCellSize					m_fpGetCellSize;
	pGetOpenInfo					m_fpGetOpenInfo;
	pRasterInfo						m_fpRasterInfo;
	pCloseRaster					m_fpCloseRaster;
	pPopulateCellBlock				m_fpPopulateCellBlock;
	pFieldInfo						m_fpFieldInfo;
	pInfoLevelCount					m_fpInfoLevelCount;
	pLevelInfo						m_fpLevelInfo;
	pInfoBandCount					m_fpInfoBandCount;
	pBandInfo						m_fpBandInfo;
	pSetCacheSize					m_fpSetCacheSize;
	pDataTypeSizeInBytes		m_fpDataTypeSizeInBytes;
	pRBIBeginRead					m_fpRBIBeginRead;
	pRBIEnd							m_fpRBIEnd;
	pRBIGetBlock					m_fpRBIGetBlock;
	pClassTableGetRecord			m_fpClassTableGetRecord;
	pClassTableGetRecordCount		m_fpClassTableGetRecordCount;
	pClassTableFindField			m_fpClassTableFindField;

	HINSTANCE						m_hMIRasterSDKInstance;			//handle of the MRR Raster SDK dll.
	static	SDKDynamicImpl*			m_pSDKImpl;

	void							ClearAll();
	bool							IsAllValid();
	SDKDynamicImpl();
	~SDKDynamicImpl();

public:

	static SDKDynamicImpl&	Get();

	bool Init();
	bool Release();


	pOpenRaster_ReadOnly			OpenRaster_ReadOnly() { return m_fpOpenRaster_ReadOnly; }
	pGetStatistics					GetStatistics()	{ return m_fpGetStatistics; }
	pComputeStatistics				ComputeStatistics() { return m_fpComputeStatistics; }
	pReleaseStatistics				ReleaseStatistics() { return m_fpReleaseStatistics; }
	pReleaseData					ReleaseData() { return m_fpReleaseData; }
	pReleaseRasterInfo				ReleaseRasterInfo() { return m_fpReleaseRasterInfo; }
	pVerifyRaster					VerifyRaster() { return m_fpVerifyRaster; }
	pGetCellSize					GetCellSize() { return m_fpGetCellSize; }
	pGetOpenInfo					GetOpenInfo() { return m_fpGetOpenInfo; }
	pRasterInfo						RasterInfo() { return m_fpRasterInfo; }
	pCloseRaster					CloseRaster() { return m_fpCloseRaster; }
	pPopulateCellBlock				PopulateCellBlock() { return m_fpPopulateCellBlock; }

	pFieldInfo						FieldInfo() { return m_fpFieldInfo; }
	pInfoLevelCount					InfoLevelCount() { return m_fpInfoLevelCount; }
	pLevelInfo						LevelInfo() { return m_fpLevelInfo; }
	pInfoBandCount					InfoBandCount() { return m_fpInfoBandCount; }
	pBandInfo						BandInfo() { return m_fpBandInfo; }
	pSetCacheSize					SetCacheSize() { return m_fpSetCacheSize; }
	pDataTypeSizeInBytes			DataTypeSizeInBytes() { return m_fpDataTypeSizeInBytes; }
 
	//Iterator
	pRBIBeginRead					RBIBeginRead() { return m_fpRBIBeginRead; }
	pRBIEnd							RBIEnd() { return m_fpRBIEnd; }
	pRBIGetBlock					RBIGetBlock() { return m_fpRBIGetBlock; }

	//ClassTable methods
	pClassTableGetRecord			ClassTableGetRecord() { return m_fpClassTableGetRecord; }
	pClassTableGetRecordCount		ClassTableGetRecordCount() { return m_fpClassTableGetRecordCount; }
	pClassTableFindField			ClassTableFindField() { return m_fpClassTableFindField; }
};

#endif