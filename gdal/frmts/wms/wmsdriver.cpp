#include "stdinc.h"

GDALDataset *GDALWMSDatasetOpen(GDALOpenInfo *poOpenInfo) {
	CPLXMLNode *config = NULL;
	CPLErr ret = CE_None;

	/*if ((poOpenInfo->nHeaderBytes == 0) && EQUALN((const char *) poOpenInfo->pszFilename, "wms:", 4)) {
	psService = CPLParseXMLString( poOpenInfo->pszFilename );
	} else */
	if ((poOpenInfo->nHeaderBytes >= 10) && EQUALN((const char *) poOpenInfo->pabyHeader, "<GDAL_WMS>", 10)) {
		config = CPLParseXMLFile(poOpenInfo->pszFilename);
		if (config == NULL) return NULL;
	} else return NULL;

	GDALWMSDataset *ds = new GDALWMSDataset();
	ret = ds->Initialize(config);
	if (ret != CE_None) {
		delete ds;
		ds = 0;
	}
	CPLDestroyXMLNode(config);

	return ds;
}

void GDALRegister_WMS() {
	GDALDriver *driver;

	if (GDALGetDriverByName("WMS") == NULL) {
		driver = new GDALDriver();
		driver->SetDescription("WMS");
		driver->SetMetadataItem(GDAL_DMD_LONGNAME, "OGC Web Map Service");
		driver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_wms.html");
		driver->pfnOpen = GDALWMSDatasetOpen;
		GetGDALDriverManager()->RegisterDriver(driver);

		GDALWMSMiniDriverManager *const mdm = GetGDALWMSMiniDriverManager();
		mdm->Register(new GDALWMSMiniDriverFactory_WMS());
	}
}
