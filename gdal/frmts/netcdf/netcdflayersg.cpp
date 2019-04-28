/* NetCDF Simple Geometry specific implementations
 * for layers filled from CF 1.8 simple geometry variables
 *
 */
#include "netcdfsg.h"
#include "netcdfdataset.h"

namespace nccfdriver
{
	OGRwkbGeometryType RawToOGR(geom_t type)
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
	
	for(int cur = 0; cur < var_count; cur++)
	{
		char * cont = nccfdriver::attrf(ncid, cur, CF_SG_GEOMETRY);		
		if(cont != nullptr)
		{
			vidList.push_back(cur);
		}
		delete cont;
	}

	for(size_t si = 0; si < vidList.size(); si++)
	{
		LoadSGVarIntoLayer(ncid, vidList[si]);
	}
	return CE_None;
}

CPLErr netCDFDataset::LoadSGVarIntoLayer(int ncid, int nc_basevarId)
{
	nccfdriver::SGeometry * sg = new nccfdriver::SGeometry(ncid, nc_basevarId);
	OGRwkbGeometryType owgt = nccfdriver::RawToOGR(sg->getGeometryType());

	// Geometry Type invalid, avoid further processing
	if(owgt == wkbUnknown)
	{
		return CE_None;	// change to appropriate error
	}

	bool unt = false;
	char baseName[256];
	nc_inq_varname(ncid, nc_basevarId, baseName);

	netCDFLayer * poL = new netCDFLayer(this, ncid, baseName, owgt, nullptr);

	papoLayers = (netCDFLayer**)reallocarray(papoLayers, nLayers + 1, sizeof(netCDFLayer *)); 
	papoLayers[nLayers] = poL;
	nLayers++;

	delete sg;
	return CE_None;
}
