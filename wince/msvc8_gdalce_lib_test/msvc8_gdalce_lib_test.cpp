#include "stdafx.h"
#include <windows.h>
#include <commctrl.h>
#include <assert.h>
#include <ogrsf_frmts.h>

int _tmain(int argc, _TCHAR* argv[])
{
	RegisterOGRShape();

	///////////////////////////////////////////////////////
	// Data source
	OGRDataSource* pDS = NULL;
	pDS = OGRSFDriverRegistrar::Open("\\My Documents\\point.shp", FALSE);
	if (NULL == pDS)
	{
		printf("Open failed!\n");
		return 0;
	}

	///////////////////////////////////////////////////////
	// Layer from data source    
	OGRLayer* pLayer = NULL;
	pLayer = pDS->GetLayerByName("point");
	if (NULL == pLayer)
	{
		printf("Layer not found\n");
		OGRDataSource::DestroyDataSource(pDS);
		return -1;
	}

	///////////////////////////////////////////////////////
	// Read features
	OGRFeature* pFeature = NULL;
	pLayer->ResetReading();

	while ((pFeature = pLayer->GetNextFeature()) != NULL)
	{
		OGRFeatureDefn* pFDefn = pLayer->GetLayerDefn();
		assert(pFDefn != NULL);

		for (int iField = 0; iField < pFDefn->GetFieldCount(); ++iField)
		{
			OGRFieldDefn* pFieldDefn = pFDefn->GetFieldDefn(iField);
			assert(pFieldDefn != NULL);

			if (pFieldDefn->GetType() == OFTInteger)
				printf( "%d,", pFeature->GetFieldAsInteger(iField));
			else if (pFieldDefn->GetType() == OFTReal)
				printf("%.3f,", pFeature->GetFieldAsDouble(iField));
			else if (pFieldDefn->GetType() == OFTString)
				printf("%s,", pFeature->GetFieldAsString(iField));
			else
				printf("%s,", pFeature->GetFieldAsString(iField));
		}

		printf("\n");

		OGRGeometry* pGeometry = NULL;
		pGeometry = pFeature->GetGeometryRef();
		if (pGeometry != NULL 
			&& wkbFlatten(pGeometry->getGeometryType()) == wkbPoint)
		{
			OGRPoint* pPoint = static_cast<OGRPoint*>(pGeometry);

			printf("%.8f , %.8f\n", pPoint->getX(), pPoint->getY());
		}
		else
		{
			printf("no point geometry\n");
		}       

		OGRFeature::DestroyFeature(pFeature);
	}

	OGRDataSource::DestroyDataSource(pDS);

	return 0;	
}

