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
		
		catch(nccfdriver::SG_Exception * e)
		{
			CPLError(CE_Warning, CPLE_AppDefined, "Translation of a simple geometry layer has been terminated due to an error.");
			CPLError(CE_Warning, CPLE_AppDefined, "ERROR: %s", e->get_err_msg());
			delete e;
		}
	}

	return CE_None;
}

CPLErr netCDFDataset::LoadSGVarIntoLayer(int ncid, int nc_basevarId)
{
	nccfdriver::SGeometry * sg = new nccfdriver::SGeometry(ncid, nc_basevarId);
	nccfdriver::SGeometry_PropertyReader pr(ncid);
	int cont_id = sg->getContainerId();
	pr.open(cont_id);
	OGRwkbGeometryType owgt = nccfdriver::RawToOGR(sg->getGeometryType());

	// Geometry Type invalid, avoid further processing
	if(owgt == wkbNone)
	{
		delete sg;
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
	std::vector<int> props = pr.ids(cont_id);
	for(size_t itr = 0; itr < props.size(); itr++)
	{
		poL->AddField(props[itr]);	
	}

	for(size_t featCt = 0; featCt < shape_count; featCt++)
	{
		OGRGeometry * featureDesc;

		try
		{
			switch(owgt)
			{		
				case wkbPoint:
					featureDesc = new OGRPoint;
					break;
				case wkbLineString:
					featureDesc = new OGRLineString;	
					break;
				case wkbPolygon:
					featureDesc = new OGRPolygon;
					break;
				case wkbMultiPoint:
					featureDesc = new OGRMultiPoint;
					break;
				case wkbMultiLineString:
					featureDesc = new OGRMultiLineString;
					break;
				case wkbMultiPolygon:
					featureDesc = new OGRMultiPolygon;
					break;
				default:
					throw new nccfdriver::SG_Exception_BadFeature();
					break;
			}
		}
		// This may be unncessary, given the check previously...
		catch(nccfdriver::SG_Exception * e)
		{
			delete poL;
			throw;
		}

		int out; size_t r_size = 0;
		void * wkb_rep = sg->serializeToWKB(featCt, r_size);
		featureDesc->importFromWkb((const unsigned char*)wkb_rep, r_size, wkbVariantIso, out);
		OGRFeature * feat = new OGRFeature(defn);
		feat -> SetGeometryDirectly(featureDesc);
			
		int dimId = -1;	

		// Find all dims, assume instance dimension is the first dimension
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
		char * wkb_rep_del = (char*)wkb_rep; // no compiler warning then
		delete wkb_rep_del;
		poL->AddSimpleGeometryFeature(feat);
	}

	papoLayers = (netCDFLayer**)reallocarray(papoLayers, nLayers + 1, sizeof(netCDFLayer *)); 
	papoLayers[nLayers] = poL;
	nLayers++;

	delete sg;
	return CE_None;
}
