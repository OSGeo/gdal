/******************************************************************************************************************************
	MapInfo Professional MRR Raster API

	Copyright © 1985-2016,Pitney Bowes Software Inc.
	All rights reserved.
	Confidential Property of Pitney Bowes Software
	
******************************************************************************************************************************/

#include "gdal_pam.h"
#include "APIDef.h"

class MRRDataset : public GDALPamDataset
{
	friend class MRRRasterBand;
	uint32_t				nDatasetHandle;
	uint32_t				nInfoHandle;
	uint32_t				nXBlocksCount;				//block count in X direction
	uint32_t				nYBlocksCount;				//block count in Y direction
	int64_t					nCellAtGridOriginX;			//Cell Offset in X direction
	int64_t					nCellAtGridOriginY;			//Cell Offset in Y direction
	double					dCellSizeX, dCellSizeY, dOriginX, dOriginY;
	char*					pszProjection;
	bool					bCategoriesInitialized;
	char**					pszCategories;
	GDALColorTable*			pColorTable;


	MRRDataset				(){}
	const uint32_t&			GetDSHandle() const { return nDatasetHandle; }
	const uint32_t&			GetInfoHandle() const { return nInfoHandle; }
	const unsigned int &	GetXBlocks() { return nXBlocksCount; }
	const unsigned int &	GetYBlocks() { return nYBlocksCount; }
	void					PopulateColorTable(const uint32_t& nFieldIndex);
	void					PopulateCategories(const uint32_t& nFieldIndex);
	GDALColorTable*			GetColorTable() const { return pColorTable; }
	char**					GetCategoryNames(const uint32_t& nField);

public:
	MRRDataset				(const uint32_t& nDatasetHandle, const uint32_t& nInfoHandle);
	~MRRDataset				();

	CPLErr					GetGeoTransform(double * padfTransform);
	const char *			GetProjectionRef();

	//Static methods
	static GDALDataset*		OpenMRR(GDALOpenInfo *);
	static int				IdentifyMRR(GDALOpenInfo *);
};


