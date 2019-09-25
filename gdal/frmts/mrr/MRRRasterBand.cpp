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

#include "MRRDataset.h"
#include "MRRRasterBand.h"
#include "MRRSDKImpl.h"
#include <limits>

/*
	Fill data array with NoDataValue If cell is not valid.
*/

//////////////////////////////////////////////////////////////////////////
//					Method declarations
void FilterDataArray(const int& nXSize, const int& nYSize, uint8_t* pDataArray, uint8_t* pValidArray, const MIR_DataType& nDataType, const double& dNoDataValue);
MIR_DataType ConvertToMIRDataTypes(const GDALDataType& gdalDataType);
double DataTypeNoDataVal(const MIR_DataType& mirDataType);
MIR_InterpolationMethod GetInterpMethod(const GDALRIOResampleAlg& resampleAlgo);
CPLErr MIRReadBlock(const uint32_t& nItHandle, const uint32_t& nBand, const int64_t& nCellX, const int64_t& nCellY,
	const int& nBlockXSize, const int& nBlockYSize, const MIR_DataType& nDataType, const uint32_t& nSizeInByte, void* pImage, const double& dNoDataValue);
//////////////////////////////////////////////////////////////////////////


void FilterDataArray(const int& nXSize, const int& nYSize, uint8_t* pDataArray, uint8_t* pValidArray, const MIR_DataType& nDataType, const double& dNoDataValue)
{
	for (int nY = 0; nY < nYSize; nY++)
	{
		auto nRowPos = nXSize*nY;
		for (int nX = 0; nX < nXSize; nX++)
		{
			auto nCellPos = nRowPos + nX;
			if (pValidArray[nCellPos] != 1)
			{
				switch (nDataType)
				{
				case MIR_DataType::MIR_BIT1:					//GDAL doesn't have these data types, so promote to 8 bit
				case MIR_DataType::MIR_BIT2:
				case MIR_DataType::MIR_BIT4:
				case MIR_DataType::MIR_RED:
				case MIR_DataType::MIR_GREEN:
				case MIR_DataType::MIR_BLUE:
				case MIR_DataType::MIR_ALPHA:
				case MIR_DataType::MIR_GREY:
				case MIR_DataType::MIR_UNSIGNED_INT8:
					((uint8_t*)pDataArray)[nCellPos] = (uint8_t)dNoDataValue;
					break;

				case MIR_DataType::MIR_RED_ALPHA:
				case MIR_DataType::MIR_BLUE_ALPHA:
				case MIR_DataType::MIR_GREEN_ALPHA:
				case MIR_DataType::MIR_GREY_ALPHA:
				case MIR_DataType::MIR_UNSIGNED_INT16:
					((uint16_t*)pDataArray)[nCellPos] = (uint16_t)dNoDataValue;
					break;

				case MIR_DataType::MIR_SIGNED_INT16:
					((int16_t*)pDataArray)[nCellPos] = (int16_t)dNoDataValue;
					break;

				case MIR_DataType::MIR_UNSIGNED_INT32:
					((uint32_t*)pDataArray)[nCellPos] = (uint32_t)dNoDataValue;
					break;

				case MIR_DataType::MIR_SIGNED_INT32:
					((int32_t*)pDataArray)[nCellPos] = (int32_t)dNoDataValue;
					break;

				case MIR_DataType::MIR_REAL4:
					((float*)pDataArray)[nCellPos] = (float)dNoDataValue;
					break;

				case MIR_DataType::MIR_SIGNED_INT64:
				case MIR_DataType::MIR_UNSIGNED_INT64:
				case MIR_DataType::MIR_REAL8:
				case MIR_DataType::MIR_REAL_LONG:
					((double*)pDataArray)[nCellPos] = (double)dNoDataValue;
					break;

				case MIR_DataType::MIR_BGR:
				case MIR_DataType::MIR_BGRA:
				case MIR_DataType::MIR_RGB:
				case MIR_DataType::MIR_RGBA:
					((uint32_t*)pDataArray)[nCellPos] = (uint32_t)dNoDataValue;
					break;

					//GDAL doesn't have this data type, so promote to 16 bit
				case MIR_DataType::MIR_SIGNED_INT8:
					((int16_t*)pDataArray)[nCellPos] = (std::numeric_limits<int16_t>::max)();
					break;

					//8 bytes should be sufficient for rest of the types.
				default:
					((double*)pDataArray)[nCellPos] = (double)dNoDataValue;

				}
			}
		}
	}
}

/*
	Convert GDALDataType to corresponding MIRDataType
*/
MIR_DataType ConvertToMIRDataTypes(const GDALDataType& gdalDataType)
{
	switch (gdalDataType)
	{
	case GDALDataType::GDT_Byte:
		return MIR_DataType::MIR_UNSIGNED_INT8;
	case GDALDataType::GDT_Int16:
		return MIR_DataType::MIR_SIGNED_INT16;
	case GDALDataType::GDT_UInt16:
		return MIR_DataType::MIR_UNSIGNED_INT16;
	case GDALDataType::GDT_Int32:
		return MIR_DataType::MIR_SIGNED_INT32;
	case GDALDataType::GDT_UInt32:
		return MIR_DataType::MIR_UNSIGNED_INT32;
	case GDALDataType::GDT_Float32:
		return MIR_DataType::MIR_REAL4;
	default:
	case GDALDataType::GDT_Float64:
		return MIR_DataType::MIR_REAL8;
	}
}

/*
	Returns NoDataValue for the datatype.
*/
double DataTypeNoDataVal(const MIR_DataType& mirDataType)
{
	switch (mirDataType)
	{
	case MIR_DataType::MIR_BIT1:				//GDAL doesn't have these data types, so promote to 8 bit
	case MIR_DataType::MIR_BIT2:
	case MIR_DataType::MIR_BIT4:
	case MIR_DataType::MIR_RED:
	case MIR_DataType::MIR_GREEN:
	case MIR_DataType::MIR_BLUE:
	case MIR_DataType::MIR_ALPHA:
	case MIR_DataType::MIR_GREY:
	case MIR_DataType::MIR_UNSIGNED_INT8:
		return (std::numeric_limits<uint8_t>::max)();

	case MIR_DataType::MIR_RED_ALPHA:
	case MIR_DataType::MIR_BLUE_ALPHA:
	case MIR_DataType::MIR_GREEN_ALPHA:
	case MIR_DataType::MIR_GREY_ALPHA:
	case MIR_DataType::MIR_UNSIGNED_INT16:
		return (std::numeric_limits<uint16_t>::max)();

	case MIR_DataType::MIR_SIGNED_INT16:
		return (std::numeric_limits<int16_t>::max)();

	case MIR_DataType::MIR_UNSIGNED_INT32:
		return (std::numeric_limits<uint32_t>::max)();

	case MIR_DataType::MIR_SIGNED_INT32:
		return (std::numeric_limits<int32_t>::max)();

	case MIR_DataType::MIR_REAL4:
		return (std::numeric_limits<float>::max)();

	case MIR_DataType::MIR_SIGNED_INT64:
	case MIR_DataType::MIR_UNSIGNED_INT64:
	case MIR_DataType::MIR_REAL8:
		return (std::numeric_limits<double>::max)();

	case MIR_DataType::MIR_BGR:
	case MIR_DataType::MIR_BGRA:
	case MIR_DataType::MIR_RGB:
	case MIR_DataType::MIR_RGBA:
		return (std::numeric_limits<uint32_t>::max)();

		//GDAL doesn't have this data type, so promote to 16 bit
	case MIR_DataType::MIR_SIGNED_INT8:
		return (std::numeric_limits<int16_t>::max)();

		//8 bytes should be sufficient for rest of the types.
	default:
		return (std::numeric_limits<double>::max)();
	}
}

MIR_InterpolationMethod GetInterpMethod(const GDALRIOResampleAlg& resampleAlgo)
{
	switch (resampleAlgo)
	{
	case GDALRIOResampleAlg::GRIORA_NearestNeighbour:
		return MIR_InterpolationMethod::Interp_Nearest;
	case GDALRIOResampleAlg::GRIORA_Bilinear:
		return MIR_InterpolationMethod::Interp_Linear;
	case GDALRIOResampleAlg::GRIORA_Cubic:
		return MIR_InterpolationMethod::Interp_Cubic;
	case GDALRIOResampleAlg::GRIORA_CubicSpline:
		return MIR_InterpolationMethod::Interp_CubicOperator;
	default:
		return MIR_InterpolationMethod::Interp_Default;
	}
}

CPLErr MIRReadBlock(const uint32_t& nItHandle, const uint32_t& nBand, const int64_t& nCellX, const int64_t& nCellY,
	const int& nBlockXSize, const int& nBlockYSize, const MIR_DataType& nDataType, const uint32_t& nSizeInByte, void* pImage, const double& dNoDataValue)
{
	uint8_t *pszRecord = nullptr;
	uint8_t *pszValid = nullptr;

	if (SDKDynamicImpl::Get().RBIGetBlock()(nItHandle, nBand, nCellX, nCellY, nBlockXSize, nBlockYSize, &pszRecord, &pszValid, nDataType, true) == MIRSuccess)
	{
		FilterDataArray(nBlockXSize, nBlockYSize, pszRecord, pszValid, nDataType, dNoDataValue);

		uint8_t *pDstImage = (uint8_t *)pImage;
		uint8_t *pSrcImage = (uint8_t *)pszRecord + nBlockXSize*(nBlockYSize)*nSizeInByte;

		for (int nY = 0; nY < nBlockYSize; nY++)
		{
			pSrcImage -= nBlockXSize*nSizeInByte;
			memcpy(pDstImage, pSrcImage, nBlockXSize*nSizeInByte);
			pDstImage += nBlockXSize*nSizeInByte;
		}

		SDKDynamicImpl::Get().ReleaseData()(&pszRecord);
		SDKDynamicImpl::Get().ReleaseData()(&pszValid);
	}

	return CE_None;
}

MRRRasterBand::MRRRasterBand(MRRDataset *pDS, const MIR_FieldType & nType, const int& nFieldIndex, const int& nBandIndex, const int& nLevel,
	const MIR_DataType& nMirDataType, const GDALDataType& nGDALBandDataType, const int& nXSize, const int& nYSize, const uint32_t& nXBlockSize, const uint32_t& nYBlockSize)
{
	this->poDS = pDS;
	nFieldType = nType;
	eDataType = nGDALBandDataType;

	nRasterXSize = nXSize;
	nRasterYSize = nYSize;

	nBlockXSize = nXBlockSize;
	nBlockYSize = nYBlockSize;

	nField = nFieldIndex;
	nMRRBandIndex = nBandIndex;
	nEvent = 0;

	nXBlocksCount = (uint32_t)ceil((double)(nRasterXSize * 1.0 / nBlockXSize));
	nYBlocksCount = (uint32_t)ceil((double)(nRasterYSize * 1.0 / nBlockYSize));
	nXBlocksCount = nXBlocksCount == 0 ? 1 : nXBlocksCount;
	nYBlocksCount = nYBlocksCount == 0 ? 1 : nYBlocksCount;

	nResolution = nLevel;
	nMIRDataType = nMirDataType;
	nSizeInBytes = SDKDynamicImpl::Get().DataTypeSizeInBytes()(nMirDataType);

	pStatistics = nullptr;
	bIteratorInitialized = false;
	nIteratorHandle = 0;

	SetNoDataValue(DataTypeNoDataVal(nMIRDataType));

	//In case of base level define overviews.
	if (nLevel == 0)
	{
		auto nLevels = SDKDynamicImpl::Get().InfoLevelCount()(pDS->nInfoHandle, nField, nEvent);
		for (uint32_t nL = 1; nL < nLevels; nL++)
		{
			SMIR_LevelInfo* pLevelInfo = nullptr;
			SDKDynamicImpl::Get().LevelInfo()(pDS->nInfoHandle, nField, nEvent, nL, &pLevelInfo);
			if (pLevelInfo)
			{
				auto nLevelWidth = pLevelInfo->nCellBBoxXMax - pLevelInfo->nCellBBoxXMin;
				auto nLevelHeight = pLevelInfo->nCellBBoxYMax - pLevelInfo->nCellBBoxYMin;

				auto nLevelBlockSizeX = nBlockXSize > nLevelWidth ? nLevelWidth : nBlockXSize;
				auto nLevelBlockSizeY = nBlockYSize > nLevelHeight ? nLevelHeight : nBlockYSize;

				vOverviewBands.push_back(std::unique_ptr<MRRRasterBand>(new MRRRasterBand(pDS, nType, nField, nBandIndex, nL, nMirDataType, nGDALBandDataType,
					(int)nLevelWidth, (int)nLevelHeight, (uint32_t)nLevelBlockSizeX, (uint32_t)nLevelBlockSizeY)));
			}
		}
	}
}

MRRRasterBand::~MRRRasterBand()
{
	ReleaseStats();

	ReleaseIterator();
}

GDALRasterBand *MRRRasterBand::GetOverview(int iOverview)
{
	if (iOverview >= 0 && iOverview < (int)vOverviewBands.size() && vOverviewBands[iOverview].get())
		return vOverviewBands[iOverview].get();
	else
		return NULL;
}

bool MRRRasterBand::BeginIterator()
{
	if (bIteratorInitialized)
		return true;

	bIteratorInitialized = SDKDynamicImpl::Get().RBIBeginRead()(((MRRDataset *)poDS)->GetDSHandle(), GetIterator(), nField, INT64_MIN, INT64_MAX, nResolution, false) == MIRSuccess;

	return bIteratorInitialized;
}

bool MRRRasterBand::ReleaseIterator()
{
	if (bIteratorInitialized)
		return (SDKDynamicImpl::Get().RBIEnd()(GetIterator()) == MIRSuccess);

	bIteratorInitialized = false;

	return true;
}

void MRRRasterBand::ReleaseStats()
{
	if (pStatistics != nullptr)
		SDKDynamicImpl::Get().ReleaseStatistics()(&pStatistics);
	pStatistics = nullptr;
}

CPLErr MRRRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff, void * pImage)
{
	CPLErr nResult = CE_Failure;

	if (BeginIterator())
	{
		int64_t nCellX = nBlockXOff * nBlockXSize;
		int64_t nCellY = (nYBlocksCount - nBlockYOff - 1) * nBlockYSize;

		nResult = MIRReadBlock(GetIterator(), MRRBandIndex(), nCellX, nCellY, nBlockXSize, nBlockYSize, nMIRDataType, nSizeInBytes, pImage, GetNoDataValue());

		if (nResult != CE_None)
		{
			CPLError(CE_Failure, CPLE_AppDefined, "Unable to read block. \n");
		}
	}

	return nResult;
}

CPLErr MRRRasterBand::IRasterIO(GDALRWFlag eRWFlag,
	int nXOff, int nYOff,
	int nXSize, int nYSize,				//Source size X and Y
	void * pData,
	int nBufXSize, int nBufYSize,		//Destination size X and Y
	GDALDataType eBufType,
	GSpacing nPixelSpace, GSpacing nLineSpace,
	GDALRasterIOExtraArg* psExtraArg)
{
	if (eRWFlag == GF_Write)
		return CE_Failure;

	/* if (nPixelSpace != (GDALGetDataTypeSize(eBufType) / 8))
		return GDALRasterBand::IRasterIO(eRWFlag,
			nXOff, nYOff, nXSize, nYSize,
			pData, nBufXSize, nBufYSize,
			eBufType,
			nPixelSpace, nLineSpace,
			psExtraArg); */

	int nMRRXOffset = nXOff + (int)((MRRDataset*)poDS)->nCellAtGridOriginX;
	int nMRRRYOffset = ((MRRDataset*)poDS)->nRasterYSize - nYOff - nYSize + (int)((MRRDataset*)poDS)->nCellAtGridOriginY;
	MIR_InterpolationMethod nInterpMethod = psExtraArg != nullptr ? GetInterpMethod(psExtraArg->eResampleAlg) : MIR_InterpolationMethod::Interp_Default;

	if (nPixelSpace == 0)
		nPixelSpace = GDALGetDataTypeSize(eBufType) / 8;

	if (nLineSpace == 0)
		nLineSpace = nPixelSpace * nBufXSize;

	CPLDebug("MRRRasterBand",
		"RasterIO(nBand=%d,nlevel=%d,nXOff=%d,nYOff=%d,nXSize=%d,nYSize=%d -> %dx%d)",
		MRRBandIndex(), nResolution, nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize);

	uint8_t* pDataArray = nullptr;
	uint8_t* pValidArray = nullptr;

	auto nDataType = ConvertToMIRDataTypes(eBufType);

	auto nSizeInByte = GDALGetDataTypeSize(eBufType) / 8;
	if (nSizeInByte == SDKDynamicImpl::Get().DataTypeSizeInBytes()(nDataType))
	{
		if (SDKDynamicImpl::Get().PopulateCellBlock()(((MRRDataset*)poDS)->GetDSHandle(), &pDataArray, &pValidArray, nDataType,
			nMRRXOffset, nMRRRYOffset, nXSize, nYSize, nBufXSize, nBufYSize, nField, MRRBandIndex(), nInterpMethod,
			std::numeric_limits<time_t>::lowest(), (std::numeric_limits<time_t>::max)()) == MIRSuccess)
		{
			auto dNoDataValue = GetNoDataValue();
			FilterDataArray(nBufXSize, nBufYSize, pDataArray, pValidArray, nDataType, dNoDataValue);

			uint8_t *pDstImage = (uint8_t *)pData;
			uint8_t *pSrcImage = (uint8_t *)pDataArray + nBufXSize*(nBufYSize)*nSizeInByte;

			for (int nY = 0; nY < nBufYSize; nY++)
			{
				pSrcImage -= nBufXSize*nSizeInByte;
				memcpy(pDstImage, pSrcImage, nBufXSize*nSizeInByte);
				pDstImage += nBufXSize*nSizeInByte;
			}

			SDKDynamicImpl::Get().ReleaseData()(&pDataArray);
			SDKDynamicImpl::Get().ReleaseData()(&pValidArray);

			return CE_None;
		}
	}

	return CE_Failure;
}

bool MRRRasterBand::StatisticsEnsureInitialized(bool bSummary, int bApproxOk, CPL_UNUSED bool bCompute, int nBins)
{
	bool bResult = true;
	int nBuckets = nBins == 0 ? (int)InvalidBinCount : nBins;

	//Check if we need to initialize
	if (GetStats() == nullptr || (GetStats() != nullptr && (int)GetStats()->cEWHistogram.nBinCount != nBuckets && nBuckets != (int)InvalidBinCount))
	{
		//Release statistics if required.
		ReleaseStats();

		auto nStatsMode = MIR_StatisticsMode::MIR_StatsMode_Distribution;
		if (bSummary)
		{
			nStatsMode = MIR_StatisticsMode::MIR_StatsMode_Summary;
		}

		if (SDKDynamicImpl::Get().GetStatistics()(((MRRDataset*)poDS)->GetDSHandle(), nField, MRRBandIndex(), nResolution, &pStatistics,
			nStatsMode, false, nBuckets, InvalidTracker) == MIRSuccess)
		{
			bResult = true;
		}
		else
		{
			uint64_t nBaseLevelCellCount = ((MRRDataset*)poDS)->nRasterXSize * ((MRRDataset*)poDS)->nRasterYSize;
			bool bSmall = (nBaseLevelCellCount < (uint64_t(1) << (20)));
			if (bSmall)
			{
				if (SDKDynamicImpl::Get().GetStatistics()(((MRRDataset *)poDS)->GetDSHandle(), nField, MRRBandIndex(), nResolution, &pStatistics,
					MIR_StatisticsMode::MIR_StatsMode_Distribution, true, nBuckets, InvalidTracker) == MIRSuccess)
					bResult = true;
			}
			else if (bApproxOk == 1)
			{
				int nApproxHighResolution = SDKDynamicImpl::Get().InfoLevelCount()(((MRRDataset *)poDS)->GetInfoHandle(), nField, nEvent);

				for (; nApproxHighResolution >= 0; --nApproxHighResolution)
				{
					SMIR_LevelInfo *pLevelInfo = nullptr;
					SDKDynamicImpl::Get().LevelInfo()(((MRRDataset *)poDS)->GetDSHandle(), nField, nEvent, nApproxHighResolution, &pLevelInfo);
					auto nXSize = pLevelInfo->nCellBBoxXMax - pLevelInfo->nCellBBoxXMin;
					auto nYSize = pLevelInfo->nCellBBoxYMax - pLevelInfo->nCellBBoxYMin;

					//	Statistics come from lower resolution levels that have 1M cells or more
					if (nXSize*nYSize >= 1048576)
						break;
				}

				if (SDKDynamicImpl::Get().ComputeStatistics()(((MRRDataset *)poDS)->GetDSHandle(), nField, MRRBandIndex(), nApproxHighResolution,
					std::numeric_limits<time_t>::lowest(), (std::numeric_limits<time_t>::max)(), &pStatistics, nStatsMode, nBuckets, InvalidTracker) == MIRSuccess)
					bResult = true;
			}
		}

		if (!pStatistics || pStatistics->nStatMode == MIR_StatisticsMode::MIR_StatsMode_None)
			if (SDKDynamicImpl::Get().GetStatistics()(((MRRDataset *)poDS)->GetDSHandle(), nField, MRRBandIndex(), nResolution, &pStatistics, nStatsMode, true, nBuckets, InvalidTracker) == MIRSuccess)
				bResult = false;
	}

	return bResult;
}

double MRRRasterBand::GetMinimum(int* pbSuccess)
{
	//Initialize if required
	StatisticsEnsureInitialized(true, 0);

	if (GetStats())
	{
		if (pbSuccess)
			*pbSuccess = TRUE;
		return GetStats()->dMin;
	}
	return GDALPamRasterBand::GetMinimum(pbSuccess);
}

double MRRRasterBand::GetMaximum(int* pbSuccess)
{
	//Initialize if required
	StatisticsEnsureInitialized(true, 0);

	if (GetStats())
	{
		if (pbSuccess)
			*pbSuccess = TRUE;
		return GetStats()->dMax;
	}
	return GDALPamRasterBand::GetMaximum(pbSuccess);
}

CPLErr MRRRasterBand::ComputeRasterMinMax(int bApproxOK, double* adfMinMax)
{
	//Initialize if required
	StatisticsEnsureInitialized(true, bApproxOK);

	double  dfMin = 0.0;
	double  dfMax = 0.0;
	int          bSuccessMin, bSuccessMax;

	dfMin = GetMinimum(&bSuccessMin);
	dfMax = GetMaximum(&bSuccessMax);

	if (bSuccessMin && bSuccessMax)
	{
		adfMinMax[0] = dfMin;
		adfMinMax[1] = dfMax;
		return CE_None;
	}

	return CE_Failure;
}

CPLErr MRRRasterBand::GetStatistics(int bApproxOK, int bForce,
	double *pdfMin, double *pdfMax,
	double *pdfMean, double *padfStdDev)
{
	StatisticsEnsureInitialized(true, bApproxOK, bForce != 0);

	if (GetStats())
	{
		if (pdfMin)
			*pdfMin = GetStats()->dMin;
		if (pdfMax)
			*pdfMax = GetStats()->dMax;
		if (pdfMean)
			*pdfMean = GetStats()->dMean;
		if (padfStdDev)
			*padfStdDev = GetStats()->dStdDev;

		return CE_None;
	}

	if (!bForce && bApproxOK)
		return CE_Warning;
	else
		return CE_Failure;
}

CPLErr MRRRasterBand::ComputeStatistics(int bApproxOK,
	double *pdfMin, double *pdfMax,
	double *pdfMean, double *pdfStdDev,
	GDALProgressFunc, CPL_UNUSED void *pProgressData)
{
	StatisticsEnsureInitialized(true, bApproxOK);

	if (GetStats())
	{
		if (pdfMin)
			*pdfMin = GetStats()->dMin;
		if (pdfMax)
			*pdfMax = GetStats()->dMax;
		if (pdfMean)
			*pdfMean = GetStats()->dMean;
		if (pdfStdDev)
			*pdfStdDev = GetStats()->dStdDev;

		return CE_None;
	}

	return CE_Warning;
}

CPLErr MRRRasterBand::GetDefaultHistogram(double *pdfMin, double *pdfMax, int *pnBuckets, GUIntBig ** ppanHistogram,
	int bForce, GDALProgressFunc pfnProgress, void *pProgressData)
{
	StatisticsEnsureInitialized(false, 0, bForce != 0);

	if (GetStats())
	{
		if (pdfMin)
			*pdfMin = GetStats()->dMin;
		if (pdfMax)
			*pdfMax = GetStats()->dMax;

		*pnBuckets = GetStats()->cEWHistogram.nBinCount;

		*ppanHistogram = (GUIntBig *)CPLCalloc(sizeof(GUIntBig), *pnBuckets);

		for (size_t i = 0; i < (size_t)*pnBuckets; i++) {
			(*ppanHistogram)[i] = (GUIntBig)GetStats()->cEWHistogram.pvcBins[i].dCount;
		}

		return CE_None;
	}

	return GDALPamRasterBand::GetDefaultHistogram(pdfMin, pdfMax,
		pnBuckets, ppanHistogram,
		bForce,
		pfnProgress,
		pProgressData);
}

/*
	In case of Imagery MRR
		If band count is >= 4, we mount 3 bands starting from 1st index, so 1st is a red band.
		If there is only one band in imagery raster, we mount 0th band and that would be Gray.
*/
GDALColorInterp MRRRasterBand::GetColorInterpretation()
{
	switch (nFieldType)
	{
	case MIR_FieldType::MIR_FIELD_Continuous:
		return GDALColorInterp::GCI_Undefined;

	case MIR_FieldType::MIR_FIELD_Image:
	{
		switch (MRRBandIndex())
		{
		case 0:
			return GDALColorInterp::GCI_GrayIndex;
		case 1:
			return GDALColorInterp::GCI_RedBand;
		case 2:
			return GDALColorInterp::GCI_GreenBand;
		case 3:
			return GDALColorInterp::GCI_BlueBand;
		default:
			return GDALColorInterp::GCI_Undefined;
		}
	}
	break;

	case MIR_FieldType::MIR_FIELD_ImagePalette:
	case MIR_FieldType::MIR_FIELD_Classified:
		return GDALColorInterp::GCI_PaletteIndex;
		
	default:
		return GDALColorInterp::GCI_Undefined;
	}

	return GDALColorInterp::GCI_Undefined;
}

GDALColorTable* MRRRasterBand::GetColorTable()
{
	return ((MRRDataset *)poDS)->GetColorTable();
}

char**	MRRRasterBand::GetCategoryNames()
{
	if (nFieldType != MIR_FieldType::MIR_FIELD_Classified)
		return nullptr;

	return ((MRRDataset *)poDS)->GetCategoryNames(nField);
}