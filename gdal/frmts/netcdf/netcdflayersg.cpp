/******************************************************************************
 *
 * Project:  netCDF read/write Driver
 * Purpose:  GDAL bindings over netCDF library.
 * Author:   Winor Chen <wchen329 at wisc.edu>
 *
 ******************************************************************************
 * Copyright (c) 2019, Winor Chen <wchen329 at wisc.edu>
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
#include "netcdfsg.h"
#include "netcdfdataset.h"
#include "ogr_core.h"

namespace nccfdriver
{
	static OGRwkbGeometryType RawToOGR(geom_t type)
	{
		OGRwkbGeometryType ret = wkbNone;

		switch(type)
		{
			case NONE:
				break;
			case LINE:
				ret = wkbLineString;
				break;
			case MULTILINE:
				ret = wkbMultiLineString;
				break;
			case POLYGON:
				ret = wkbPolygon;
				break;
			case MULTIPOLYGON:
				ret = wkbMultiPolygon;
				break;
			case POINT:
				ret = wkbPoint;
				break;
			case MULTIPOINT:
				ret = wkbMultiPoint;
				break;
			case UNSUPPORTED:
				break;
		}

		return ret;	
	}	

}

CPLErr netCDFDataset::DetectAndFillSGLayers(int ncid)
{
	// Discover simple geometry variables
	int var_count;
	nc_inq_nvars(ncid, &var_count);	
	std::vector<int> vidList;

	nccfdriver::scanForGeometryContainers(ncid, vidList);	

	// To still do: support multiple geometry containers

	for(size_t itr = 0; itr < vidList.size(); itr++)
	{
		try
		{
			LoadSGVarIntoLayer(ncid, vidList[itr]);

		}
		
		catch(nccfdriver::SG_Exception& e)
		{
			CPLError(CE_Warning, CPLE_AppDefined,
				"Translation of a simple geometry layer has been terminated prematurely due to an error.\n%s", e.get_err_msg());
		}
	}

	return CE_None;
}

CPLErr netCDFDataset::LoadSGVarIntoLayer(int ncid, int nc_basevarId)
{
	std::unique_ptr<nccfdriver::SGeometry> sg (new nccfdriver::SGeometry(ncid, nc_basevarId));
	int cont_id = sg->getContainerId();
	nccfdriver::SGeometry_PropertyScanner pr(ncid, cont_id);
	OGRwkbGeometryType owgt = nccfdriver::RawToOGR(sg->getGeometryType());

	// Geometry Type invalid, avoid further processing
	if(owgt == wkbNone)
	{
		throw nccfdriver::SG_Exception_BadFeature();
	}
	

	char baseName[NC_MAX_CHAR + 1];
	memset(baseName, 0, NC_MAX_CHAR + 1);
	nc_inq_varname(ncid, nc_basevarId, baseName);

	netCDFLayer * poL = new netCDFLayer(this, ncid, baseName, owgt, nullptr); 

	poL->EnableSGBypass();
	OGRFeatureDefn * defn = poL->GetLayerDefn();
	defn->SetGeomType(owgt);

	size_t shape_count = sg->get_geometry_count();

	// Add properties
	std::vector<int> props = pr.ids();
	for(size_t itr = 0; itr < props.size(); itr++)
	{
		poL->AddField(props[itr]);	
	}

	for(size_t featCt = 0; featCt < shape_count; featCt++)
	{
		OGRGeometry * geometry;

		try
		{
			switch(owgt)
			{		
				case wkbPoint:
					geometry = new OGRPoint;
					break;
				case wkbLineString:
					geometry = new OGRLineString;
					break;
				case wkbPolygon:
					geometry = new OGRPolygon;
					break;
				case wkbMultiPoint:
					geometry = new OGRMultiPoint;
					break;
				case wkbMultiLineString:
					geometry = new OGRMultiLineString;
					break;
				case wkbMultiPolygon:
					geometry = new OGRMultiPolygon;
					break;
				default:
					throw nccfdriver::SG_Exception_BadFeature();
					break;
			}
		}
		// This may be unncessary, given the check previously...
		catch(nccfdriver::SG_Exception&)
		{
			delete poL;
			throw;
		}

		int r_size = 0;
		std::unique_ptr<unsigned char> wkb_rep = std::unique_ptr<unsigned char>(sg->serializeToWKB(featCt, r_size));
		geometry->importFromWkb(static_cast<const unsigned char*>(wkb_rep.get()), r_size, wkbVariantIso);
		OGRFeature * feat = new OGRFeature(defn);
		feat -> SetGeometryDirectly(geometry);
			
		int dimId = sg -> getInstDim();	

		// If instance dim is not specified, try to assume instance dimension is the first dimension
		if(dimId == nccfdriver::INVALID_DIM_ID)
		{	
			if(props.size() > 0)
			{
				// All property values of a geometry container have the same inst. dim
				// So just, use one of them
				int dim_c;
				nc_inq_varndims(ncid, props[0], &dim_c);
				std::unique_ptr<int>dim_ids = std::unique_ptr<int>(new int[dim_c]);
				nc_inq_vardimid(ncid, props[0], dim_ids.get());
			
				// Take the first throwaway the rest
				dimId = *dim_ids;
			}
		}

		size_t dim_len = 0;

		if(nc_inq_dimlen(ncid, dimId, &dim_len) != NC_NOERR)
		{
			throw nccfdriver::SG_Exception_Existential(sg->getContainerName().c_str(), CF_SG_GEOMETRY_DIMENSION);
		}
		
		// Fill fields
		for(size_t itr = 0; itr < props.size() && itr < dim_len; itr++)
		{
			poL->FillFeatureFromVar(feat, dimId, featCt);
		}

		feat -> SetFID(featCt);
		poL->AddSimpleGeometryFeature(feat);
	}

	papoLayers = (netCDFLayer**)CPLRealloc(papoLayers, (nLayers + 1) * sizeof(netCDFLayer *)); 
	papoLayers[nLayers] = poL;
	nLayers++;

	return CE_None;
}
