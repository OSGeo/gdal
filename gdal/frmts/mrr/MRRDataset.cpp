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
#include "ogr_spatialref.h"

CPL_C_START
void	GDALRegister_MRR(void);
CPL_C_END

//////////////////////////////////////////////////////////////////////////
//					Method declarations
GDALDataType AdjustBandDataType(MIR_DataType& mirDataType);
bool MRRInitialize(bool logError = true);
void GDALDeregister_MRR(GDALDriver *);
//////////////////////////////////////////////////////////////////////////

/*
	It returns the appropriate GDAL data type for the MIR data types.
	If data type is not supported in the GDAL this mehtod promotes (modify) the MIRDataType.
*/
GDALDataType AdjustBandDataType(MIR_DataType& mirDataType)
{
	GDALDataType gdalDataType;

	switch (mirDataType)
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
		gdalDataType = GDALDataType::GDT_Byte;
		break;

	case MIR_DataType::MIR_RED_ALPHA:
	case MIR_DataType::MIR_BLUE_ALPHA:
	case MIR_DataType::MIR_GREEN_ALPHA:
	case MIR_DataType::MIR_GREY_ALPHA:
	case MIR_DataType::MIR_UNSIGNED_INT16:
		gdalDataType = GDALDataType::GDT_UInt16;
		break;

	case MIR_DataType::MIR_SIGNED_INT16:
		gdalDataType = GDALDataType::GDT_Int16;
		break;

	case MIR_DataType::MIR_UNSIGNED_INT32:
		gdalDataType = GDALDataType::GDT_UInt32;
		break;

	case MIR_DataType::MIR_SIGNED_INT32:
		gdalDataType = GDALDataType::GDT_Int32;
		break;

	case MIR_DataType::MIR_REAL4:
		gdalDataType = GDALDataType::GDT_Float32;
		break;

	case MIR_DataType::MIR_SIGNED_INT64:
	case MIR_DataType::MIR_UNSIGNED_INT64:
	case MIR_DataType::MIR_REAL8:
	case MIR_DataType::MIR_REAL_LONG:
		mirDataType = MIR_DataType::MIR_REAL8;
		gdalDataType = GDALDataType::GDT_Float64;
		break;

	case MIR_DataType::MIR_BGR:
	case MIR_DataType::MIR_BGRA:
	case MIR_DataType::MIR_RGB:
	case MIR_DataType::MIR_RGBA:
		gdalDataType = GDALDataType::GDT_UInt32;
		break;

		//GDAL doesn't have this data type, so promote to 16 bit
	case MIR_DataType::MIR_SIGNED_INT8:
		gdalDataType = GDALDataType::GDT_Int16;
		mirDataType = MIR_DataType::MIR_SIGNED_INT16;
		break;

		//8 bytes should be sufficient for all types
	default:
		mirDataType = MIR_DataType::MIR_REAL8;
		gdalDataType = GDALDataType::GDT_Float64;
	}

	return gdalDataType;
}

bool MRRInitialize(bool logError)
{
	if (SDKDynamicImpl::Get().Init() == false)
	{
		if (logError)
		{
			CPLError(CE_Failure, CPLE_AppDefined, "Unable to load MapInfo MRR SDK \n");
		}
		return false;
	}

	return true;
}

MRRDataset::MRRDataset(const uint32_t& dsHandle, const uint32_t& infoHandle) : nDatasetHandle(dsHandle), nInfoHandle(infoHandle)
{
	pszProjection = CPLStrdup("");
	dCellSizeX = dCellSizeY = dOriginX = dOriginY = 0.0;
	nCellAtGridOriginX = nCellAtGridOriginY = 0;
	pColorTable = nullptr;
	bCategoriesInitialized = false;
	pszCategories = nullptr;
}

MRRDataset::~MRRDataset()
{
	SDKDynamicImpl::Get().ReleaseRasterInfo()(nInfoHandle);
	SMIR_FinalisationOptions cFinalize;
	SDKDynamicImpl::Get().CloseRaster()(nDatasetHandle, nullptr, InvalidTracker);

	CSLDestroy(pszCategories);

	CPLFree(pszProjection);

	if (pColorTable)
		delete pColorTable;
	pColorTable = nullptr;
}

/*
	Populate color entries.
*/
void MRRDataset::PopulateColorTable(const uint32_t& nFieldIndex)
{
	pColorTable = new GDALColorTable();

	uint32_t nRecordCount = 0;
	auto nDSHandle = GetDSHandle();
	if (SDKDynamicImpl::Get().ClassTableGetRecordCount()(nDSHandle, nFieldIndex, nRecordCount) == MIRSuccess)
	{
		uint32_t nClassTableRGBField = 0;
		if (SDKDynamicImpl::Get().ClassTableFindField()(nDSHandle, nFieldIndex, MIR_ClassTableFieldType::MIR_TFT_Colour, nClassTableRGBField) == MIRSuccess)
		{
			for (uint32_t nRec = 0; nRec < nRecordCount; nRec++)
			{
				GDALColorEntry colorEntry;
				MIR_DataType nDataType;
				uint32_t nDataSize;
				uint32_t *pData = nullptr;

				if (SDKDynamicImpl::Get().ClassTableGetRecord()(nDSHandle, nFieldIndex, nClassTableRGBField, nRec, (uint8_t**)&pData, nDataType, nDataSize) == MIRSuccess)
				{
					colorEntry.c1 = (uint8_t)((*pData & 0x000000ff));			//red
					colorEntry.c2 = (uint8_t)((*pData & 0x0000ff00) >> 8);		//green
					colorEntry.c3 = (uint8_t)((*pData & 0x00ff0000) >> 16);		//blue
					colorEntry.c4 = (short)255;									//alpha

					SDKDynamicImpl::Get().ReleaseData()((uint8_t**)&pData);
				}

				pColorTable->SetColorEntry(nRec, &colorEntry);
			}
		}
		else
		{
			MIR_ClassTableFieldType nTFTColours[] = { MIR_ClassTableFieldType::MIR_TFT_ColourR, MIR_ClassTableFieldType::MIR_TFT_ColourG, MIR_ClassTableFieldType::MIR_TFT_ColourB };
			uint32_t nClassTableFields[3] = { 0 };

			for (int nB = 0; nB < 3; nB++)
				SDKDynamicImpl::Get().ClassTableFindField()(nDSHandle, nFieldIndex, nTFTColours[nB], nClassTableFields[nB]);

			for (uint32_t nRec = 0; nRec < nRecordCount; nRec++)
			{
				GDALColorEntry colorEntry;
				uint32_t *pData = nullptr;
				MIR_DataType nDataType;
				uint32_t nDataSize;

				if (SDKDynamicImpl::Get().ClassTableGetRecord()(nDSHandle, nFieldIndex, nClassTableFields[0], nRec, (uint8_t**)&pData, nDataType, nDataSize) == MIRSuccess)
				{
					colorEntry.c1 = (uint8_t)((*pData));		//red
					SDKDynamicImpl::Get().ReleaseData()((uint8_t**)&pData);
				}
				if (SDKDynamicImpl::Get().ClassTableGetRecord()(nDSHandle, nFieldIndex, nClassTableFields[1], nRec, (uint8_t**)&pData, nDataType, nDataSize) == MIRSuccess)
				{
					colorEntry.c2 = (uint8_t)((*pData));		//green
					SDKDynamicImpl::Get().ReleaseData()((uint8_t**)&pData);
				}
				if (SDKDynamicImpl::Get().ClassTableGetRecord()(nDSHandle, nFieldIndex, nClassTableFields[2], nRec, (uint8_t**)&pData, nDataType, nDataSize) == MIRSuccess)
				{
					colorEntry.c3 = (uint8_t)((*pData));		//blue
					SDKDynamicImpl::Get().ReleaseData()((uint8_t**)&pData);
				}

				colorEntry.c4 = (short)255;						//alpha
				pColorTable->SetColorEntry(nRec, &colorEntry);
			}
		}
	}
}

/*
	Populate categories.
*/
void MRRDataset::PopulateCategories(const uint32_t& nFieldIndex)
{
	if (bCategoriesInitialized)
		return;

	uint32_t nRecordCount = 0;
	auto nDSHandle = GetDSHandle();
	if (SDKDynamicImpl::Get().ClassTableGetRecordCount()(nDSHandle, nFieldIndex, nRecordCount) == MIRSuccess)
	{
		{
			uint32_t nClassTableLabelField = 0;
			if (SDKDynamicImpl::Get().ClassTableFindField()(nDSHandle, nFieldIndex, MIR_ClassTableFieldType::MIR_TFT_Label, nClassTableLabelField) == MIRSuccess)
			{
				pszCategories = (char**)CPLCalloc(nRecordCount + 2, sizeof(char*));

				for (uint32_t nRec = 0; nRec < nRecordCount; nRec++)
				{
					MIR_DataType nDataType;
					uint32_t nDataSize;
					uint8_t *pData = nullptr;

					if (SDKDynamicImpl::Get().ClassTableGetRecord()(nDSHandle, nFieldIndex, nClassTableLabelField, nRec, (uint8_t**)&pData, nDataType, nDataSize) == MIRSuccess)
					{
						char* pStr = (char*)CPLMalloc(nDataSize + 1);
						strncpy(pStr, (char*)pData, nDataSize);
						pStr[nDataSize] = '\0';
						pszCategories[nRec] = pStr;

						SDKDynamicImpl::Get().ReleaseData()((uint8_t**)&pData);
					}
					else
					{
						pszCategories[nRec] = CPLStrdup("");
					}
				}

				//Let's not leave any entry to null, mark them as ""
				for (uint32_t nID = 0; nID < nRecordCount; nID++)
				{
					if (pszCategories[nID] == nullptr)
						pszCategories[nID] = CPLStrdup("");
				}

				pszCategories[nRecordCount + 1] = nullptr;
			}
		}
	}

	bCategoriesInitialized = true;
}

char**	MRRDataset::GetCategoryNames(const CPL_UNUSED uint32_t& nField)
{
	return pszCategories;
}


GDALDataset *MRRDataset::OpenMRR(GDALOpenInfo * poOpenInfo)
{
	if (MRRInitialize() == false)
		return NULL;

	// -------------------------------------------------------------------- //
	//      Confirm that the file is a valid MRR dataset.					//
	// -------------------------------------------------------------------- //

	if (!IdentifyMRR(poOpenInfo))
		return NULL;

	// -------------------------------------------------------------------- //
	//      Confirm the requested access is supported.                      //
	// -------------------------------------------------------------------- //
	if (poOpenInfo->eAccess == GA_Update)
	{
		CPLError(CE_Failure, CPLE_NotSupported, "MapInfo MRR driver does not support update access to existing files"
			" datasets.\n");
		return NULL;
	}

	// -------------------------------------------------------------------- //
	//      Create a corresponding GDALDataset.                             //
	// -------------------------------------------------------------------- //

	uint32_t nDSHandle = MIRInvalidHandle, nInfoHandle = MIRInvalidHandle;
	{
		wchar_t *pwszFilename = CPLRecodeToWChar(poOpenInfo->pszFilename, CPL_ENC_UTF8, CPL_ENC_UCS2);
		if (SDKDynamicImpl::Get().OpenRaster_ReadOnly()(pwszFilename, nDSHandle, MIR_RasterSupportMode::MIR_Support_Full,MIR_FieldType::MIR_FIELD_Default,InvalidTracker) != MIRSuccess)
		{
			CPLError(CE_Failure, CPLE_OpenFailed, "MapInfo MRR driver is unable to open the file.\n");
			CPLFree(pwszFilename);
			return NULL;
		}
		CPLFree(pwszFilename);
	}

	MRRDataset*      poDS = nullptr;
	const uint32_t nFieldIndex = 0;
	if (SDKDynamicImpl::Get().GetOpenInfo()(nDSHandle, nInfoHandle, InvalidTracker) == MIRSuccess)
	{
		SMIR_RasterInfo *pRasterInfo = nullptr;
		SMIR_FieldInfo* pFieldInfo = nullptr;
		uint32_t nXBlockSize, nYBlockSize;

		SDKDynamicImpl::Get().RasterInfo()(nInfoHandle, &pRasterInfo);

		SDKDynamicImpl::Get().FieldInfo()(nInfoHandle, 0, &pFieldInfo);
		uint32_t nBandCount = SDKDynamicImpl::Get().InfoBandCount()(nInfoHandle, nFieldIndex);

		poDS = new MRRDataset(nDSHandle, nInfoHandle);
		poDS->nRasterXSize = (int)pRasterInfo->nGridSizeX;
		poDS->nRasterYSize = (int)pRasterInfo->nGridSizeY;
		nXBlockSize = (uint32_t)pRasterInfo->nBaseTileSizeX;
		nYBlockSize = (uint32_t)pRasterInfo->nBaseTileSizeY;
		poDS->nXBlocksCount = (uint32_t)ceil((double)(pRasterInfo->nGridSizeX * 1.0 / nXBlockSize));
		poDS->nYBlocksCount = (uint32_t)ceil((double)(pRasterInfo->nGridSizeY * 1.0 / nYBlockSize));

		poDS->dCellSizeX = pFieldInfo->cCellSizeX.m_dDecimal;
		poDS->dCellSizeY = pFieldInfo->cCellSizeY.m_dDecimal;
		poDS->dOriginX = pFieldInfo->cTileOriginX.m_dDecimal;
		poDS->dOriginY = pFieldInfo->cTileOriginY.m_dDecimal;

		poDS->nCellAtGridOriginX = pFieldInfo->nCellAtGridOriginX;
		poDS->nCellAtGridOriginY = pFieldInfo->nCellAtGridOriginY;

		//Convert projection to WKT
		OGRSpatialReference oSRS;
		auto pCoordSys = CPLRecodeFromWChar(pRasterInfo->sCoordinateSystem, CPL_ENC_UCS2, CPL_ENC_UTF8);
		if (pCoordSys && oSRS.importFromMICoordSys(pCoordSys) == OGRERR_NONE)
		{
			CPLFree(poDS->pszProjection);
			oSRS.exportToWkt(&poDS->pszProjection);
		}
		poDS->SetProjection(poDS->pszProjection);
		CPLFree(pCoordSys);

		// -------------------------------------------------------------------- //
		//      Mount Bands, Create band information objects.                                //
		// -------------------------------------------------------------------- //

		switch (pFieldInfo->nType)
		{
			//Mount all bands
		case MIR_FieldType::MIR_FIELD_Continuous:
		{
			uint32_t nGDALBandIndex = 1;
			for (uint32_t nBand = 0; nBand < nBandCount; nBand++)
			{
				SMIR_BandInfo* pBandInfo = nullptr;
				if (SDKDynamicImpl::Get().BandInfo()(nInfoHandle, nFieldIndex, nBand, &pBandInfo) == MIRSuccess)
				{
					auto nMIRDataType = pBandInfo->nDataType;
					auto nGDALDataType = AdjustBandDataType(nMIRDataType);
					poDS->SetBand(nGDALBandIndex, new MRRRasterBand(poDS, pFieldInfo->nType, nFieldIndex, nBand, 0, nMIRDataType,
						nGDALDataType, (int)pRasterInfo->nGridSizeX, (int)pRasterInfo->nGridSizeY, nXBlockSize, nYBlockSize));
					nGDALBandIndex++;
				}
			}
		}
		break;

		//If band count is >= 4 , mount 3 bands starting from 1st index.
		//If there is only one band, mount 0th band.
		case MIR_FieldType::MIR_FIELD_Image:
		{
			if (nBandCount >= 4)
			{
				uint32_t nGDALBandIndex = 1;
				uint32_t nMRRBandIndex = 1;
				for (uint32_t nB = 1; nB <= nBandCount; nB++)
				{
					SMIR_BandInfo* pBandInfo = nullptr;
					if (SDKDynamicImpl::Get().BandInfo()(nInfoHandle, nFieldIndex, nB, &pBandInfo) == MIRSuccess)
					{
						auto nMIRDataType = pBandInfo->nDataType;
						auto nGDALDataType = AdjustBandDataType(nMIRDataType);
						poDS->SetBand(nGDALBandIndex, new MRRRasterBand(poDS, pFieldInfo->nType, nFieldIndex, nMRRBandIndex, 0, nMIRDataType,
							nGDALDataType, (int)pRasterInfo->nGridSizeX, (int)pRasterInfo->nGridSizeY, nXBlockSize, nYBlockSize));
						nGDALBandIndex++;
						nMRRBandIndex++;
					}
				}
			}
			else if (nBandCount == 1)
			{
				uint32_t nGDALBandIndex = 1;
				uint32_t nMRRBandIndex = 0;
				SMIR_BandInfo* pBandInfo = nullptr;
				if (SDKDynamicImpl::Get().BandInfo()(nInfoHandle, nFieldIndex, nMRRBandIndex, &pBandInfo) == MIRSuccess)
				{
					auto nMIRDataType = pBandInfo->nDataType;
					auto nGDALDataType = AdjustBandDataType(nMIRDataType);

					poDS->SetBand(nGDALBandIndex, new MRRRasterBand(poDS, pFieldInfo->nType, nFieldIndex, nMRRBandIndex, 0, nMIRDataType,
						nGDALDataType, (int)pRasterInfo->nGridSizeX, (int)pRasterInfo->nGridSizeY, nXBlockSize, nYBlockSize));
					nGDALBandIndex++;
				}
			}
		}
		break;

		//Mount 0th band, populate Colour table too.
		case MIR_FieldType::MIR_FIELD_ImagePalette:
		{
			uint32_t nGDALBandIndex = 1;
			uint32_t nMRRBandIndex = 0;
			SMIR_BandInfo* pBandInfo = nullptr;
			if (SDKDynamicImpl::Get().BandInfo()(nInfoHandle, nFieldIndex, nMRRBandIndex, &pBandInfo) == MIRSuccess)
			{
				auto nMIRDataType = pBandInfo->nDataType;
				auto nGDALDataType = AdjustBandDataType(nMIRDataType);
				poDS->SetBand(nGDALBandIndex, new MRRRasterBand(poDS, pFieldInfo->nType, nFieldIndex, nMRRBandIndex, 0, nMIRDataType,
					nGDALDataType, (int)pRasterInfo->nGridSizeX, (int)pRasterInfo->nGridSizeY, nXBlockSize, nYBlockSize));
				nGDALBandIndex++;
			}

			///////////////////////////////////////////////////////
			//Initialize color table here.
			///////////////////////////////////////////////////////
			{
				poDS->PopulateColorTable(nFieldIndex);
			}
		}
		break;

		//Mount 0th band, populate Colour table and Categories.
		case MIR_FieldType::MIR_FIELD_Classified:
		{
			uint32_t nGDALBandIndex = 1;
			uint32_t nMRRBandIndex = 0;
			SMIR_BandInfo* pBandInfo = nullptr;
			if (SDKDynamicImpl::Get().BandInfo()(nInfoHandle, nFieldIndex, nMRRBandIndex, &pBandInfo) == MIRSuccess)
			{
				auto nMIRDataType = pBandInfo->nDataType;
				auto nGDALDataType = AdjustBandDataType(nMIRDataType);
				poDS->SetBand(nGDALBandIndex, new MRRRasterBand(poDS, pFieldInfo->nType, nFieldIndex, nMRRBandIndex, 0, nMIRDataType,
					nGDALDataType, (int)pRasterInfo->nGridSizeX, (int)pRasterInfo->nGridSizeY, nXBlockSize, nYBlockSize));
				nGDALBandIndex++;
			}

			///////////////////////////////////////////////////////
			//Initialize color table here.
			///////////////////////////////////////////////////////
			{
				poDS->PopulateColorTable(nFieldIndex);
			}

			///////////////////////////////////////////////////////
			//Initialize class table (categories) here.
			///////////////////////////////////////////////////////
			{
				poDS->PopulateCategories(nFieldIndex);
			}
		}
		break;

		default:
			break;
		}

		// -------------------------------------------------------------------- //
		//      Initialize any PAM information.                                 //
		// -------------------------------------------------------------------- //
		poDS->SetDescription(poOpenInfo->pszFilename);

		// -------------------------------------------------------------------- //
		//      Initialize default overviews.                                   //
		// -------------------------------------------------------------------- //
		poDS->oOvManager.Initialize(poDS, poOpenInfo->pszFilename);
	}

	return(poDS);
}



//                              IdentifyMRR()                           
//Return 1 if the passed file is certainly recognized by the driver 
//Return 0 if the passed file is certainly NOT recognized by the driver 
//Return - 1 if the passed file may be or may not be recognized by the driver and that a potentially costly test must be done with pfnOpen.

int MRRDataset::IdentifyMRR(GDALOpenInfo * poOpenInfo)
{
	if (MRRInitialize() == false)
		return 0;

	if (EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "mrr"))
	{
		const char* pszFileName = poOpenInfo->pszFilename;
		{
			wchar_t *pwszFilename = CPLRecodeToWChar(pszFileName, CPL_ENC_UTF8, CPL_ENC_UCS2);
			bool bIdentify = (SDKDynamicImpl::Get().VerifyRaster() && SDKDynamicImpl::Get().VerifyRaster()(pwszFilename) == MIRSuccess);
			CPLFree(pwszFilename);
			return bIdentify ? 1 : 0;
		}
	}

	return 0;
}

/************************************************************************/
/*                              GetGeoTransform()                              */
/************************************************************************/

CPLErr	MRRDataset::GetGeoTransform(double * padfTransform)
{
	padfTransform[0] = dOriginX;			/* X Origin (top left corner) */
	padfTransform[1] = dCellSizeX;			/* X Pixel size */
	padfTransform[2] = 0;
	padfTransform[3] = dOriginY + (dCellSizeY * nRasterYSize);			/* Y Origin (top left corner) */
	padfTransform[4] = 0;
	padfTransform[5] = -dCellSizeY;			/* Y Pixel Size */

	return CE_None;
}

const char *	MRRDataset::GetProjectionRef()
{
	return pszProjection;
}

void GDALDeregister_MRR(GDALDriver *)
{
}

void GDALRegister_MRR()
{
	GDALDriver  *poDriver;

	if (!GDAL_CHECK_VERSION("MRR"))
		return;

	if (MRRInitialize(false) == false)
		return;

	if (GDALGetDriverByName("MRR") == NULL)
	{
		poDriver = new GDALDriver();

		poDriver->SetDescription("MRR");
		poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
		poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "MapInfo Multi Resolution Raster");
		poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_mrr.html");
		poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "mrr");

		poDriver->pfnOpen = MRRDataset::OpenMRR;
		poDriver->pfnIdentify = MRRDataset::IdentifyMRR;
		poDriver->pfnUnloadDriver = GDALDeregister_MRR;

		GetGDALDriverManager()->RegisterDriver(poDriver);
	}
}
