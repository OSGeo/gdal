/******************************************************************************
 * $Id$
 *
 * Project:  Anatrack Ranges Edge File Translator
 * Purpose:  Implements OGREdgDataSource class
 * Author:   Nick Casey, nick@anatrack.com
 *
 ******************************************************************************
 * Copyright (c) 2020, Nick Casey <nick at anatrack.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_edg.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          OGREdgDataSource()                          */
/************************************************************************/

OGREdgDataSource::OGREdgDataSource() :
	pszName(nullptr),
	poLayer(nullptr),
	nLayers(0),
	psDestinationFilename(""),
	bUpdate(false)
{}

/************************************************************************/
/*                         ~OGREdgDataSource()                          */
/************************************************************************/

OGREdgDataSource::~OGREdgDataSource()
{
	delete poLayer;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGREdgDataSource::Open(const char *pszFilename)
{
	nLayers = 1;
	poLayer = new OGREdgLayer(pszFilename, nullptr, false);

	return TRUE;
}




/************************************************************************/
/*                               Create()                               */
/*                                                                      */
/*      Create a new datasource.                                      */
/************************************************************************/

int OGREdgDataSource::Create(const char *pszDSName, char ** papszOptions )
{
	psDestinationFilename = pszDSName;
	return TRUE;
}


/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *OGREdgDataSource::ICreateLayer(const char * pszLayerName,
	OGRSpatialReference *poSRS,
	OGRwkbGeometryType eType,
	CPL_UNUSED char ** papszOptions)
{
	
	if (eType != wkbPolygon    
		&& eType != wkbMultiPolygon)
	{
		CPLError(CE_Failure,
			CPLE_NotSupported,
			"unsupported geometry type %s", OGRGeometryTypeToName(eType));
		return nullptr;
	}

	//	Close the previous layer (if there is one open). It will be written to the file.
	if (GetLayerCount() > 0)
	{
		delete poLayer;
	}

	CPLString sPath = CPLGetPath(psDestinationFilename);
	CPLString sBasename = CPLGetBasename(psDestinationFilename);
	CPLString sExtension = CPLGetExtension(psDestinationFilename);

	// if not edg extension, keep full filename but add edg later
	if (!EQUAL(sExtension, "edg")) 
		sBasename += sExtension;

	// add a layer identifier for any layer beyond the first
	if (GetLayerCount() > 0) { 
		sBasename += CPLSPrintf(".%i", nLayers + 1);
	}

	CPLString sFilename = CPLFormFilename(sPath, sBasename, "edg");

	/* -------------------------------------------------------------------- */
	/*      Create the layer object.                                        */
	/* -------------------------------------------------------------------- */
	
	poLayer = new OGREdgLayer(sFilename, poSRS, true);
	nLayers++;

	return poLayer;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGREdgDataSource::TestCapability(const char * pszCap)
{
	if (EQUAL(pszCap, ODsCCreateLayer))
		return TRUE;

	return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGREdgDataSource::GetLayer(int iLayer)
{
	if (iLayer != 0)
		return nullptr;

	return poLayer;
}
