/* NetCDF Simple Geometry specific implementations
 * for layers filled from CF 1.8 simple geometry variables
 *
 */
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
		throw new nccfdriver::SG_Exception_BadFeature();
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
		//std::unique_ptr<OGRGeometry> geometry;
		OGRGeometry * geometry;

		try
		{
			switch(owgt)
			{		
				case wkbPoint:
					geometry = new OGRPoint;
					//geometry = std::unique_ptr<OGRGeometry>(new OGRPoint);
					break;
				case wkbLineString:
					geometry = new OGRLineString;
					//geometry = std::unique_ptr<OGRGeometry>(new OGRLineString);	
					break;
				case wkbPolygon:
					geometry = new OGRPolygon;
					//geometry = std::unique_ptr<OGRGeometry>(new OGRPolygon);
					break;
				case wkbMultiPoint:
					geometry = new OGRMultiPoint;
					//geometry = std::unique_ptr<OGRGeometry>(new OGRMultiPoint);
					break;
				case wkbMultiLineString:
					geometry = new OGRMultiLineString;
					//geometry = std::unique_ptr<OGRGeometry>(new OGRMultiLineString);
					break;
				case wkbMultiPolygon:
					geometry = new OGRMultiPolygon;
					//geometry = std::unique_ptr<OGRGeometry>(new OGRMultiPolygon);
					break;
				default:
					throw new nccfdriver::SG_Exception_BadFeature();
					break;
			}
		}
		// This may be unncessary, given the check previously...
		catch(nccfdriver::SG_Exception& e)
		{
			delete poL;
			throw;
		}

		int r_size = 0;
		char * wkb_rep = (char*)sg->serializeToWKB(featCt, r_size);
		geometry->importFromWkb((unsigned const char*) wkb_rep, r_size, wkbVariantIso);
		OGRFeature * feat = new OGRFeature(defn);
		feat -> SetGeometryDirectly(geometry);
		delete[] wkb_rep;
			
		int dimId = -1;	

		// Find all dims, assume instance dimension is the first dimension
		/* Update to CF-1.8 standard likely to change
		 * use "geometry_dimension" instead
		 */
		if(props.size() > 0)
		{
			// All property values of a geometry container have the same inst. dim
			// So just, use one of them
			int dim_c;
			nc_inq_varndims(ncid, props[0], &dim_c);
			int * dim_ids = new int[dim_c];
			nc_inq_vardimid(ncid, props[0], dim_ids);
			
			// Take the first delete the rest
			dimId = dim_ids[0];
			delete[] dim_ids;
		}
		
		// Fill fields
		for(size_t itr = 0; itr < props.size(); itr++)
		{
			// to do: add check to fill only as fields are defined	
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
