/*
 * $Id$
 *
 * Test Mainline.
 *
 * Created by Mateusz Loskot, mloskot@taxussi.com.pl
 *
 * Copyright (c) 2006 Taxus SI Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom 
 * the Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
 * THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * MIT License:
 * http://opensource.org/licenses/mit-license.php
 *
 * Contact:
 * Taxus SI Ltd.
 * http://www.taxussi.com.pl
 *
 */

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

