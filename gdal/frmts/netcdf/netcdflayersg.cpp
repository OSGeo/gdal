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

	// To still do: support multiple geometry containers
	if(vidList.size() != 0)
	{
		LoadSGVarIntoLayer(ncid, vidList[0]);
	}
	return CE_None;
}

CPLErr netCDFDataset::LoadSGVarIntoLayer(int ncid, int nc_basevarId)
{
	nccfdriver::SGeometry * sg = new nccfdriver::SGeometry(ncid, nc_basevarId);
	OGRwkbGeometryType owgt = nccfdriver::RawToOGR(sg->getGeometryType());

	// Geometry Type invalid, avoid further processing
	if(owgt == wkbNone)
	{
		delete sg;
		return CE_None;	// change to appropriate error
	}
	

	char baseName[256];
	nc_inq_varname(ncid, nc_basevarId, baseName);

	OGRFeatureDefn * defn = new OGRFeatureDefn(baseName);
	defn->Reference();
	defn->SetGeomType(owgt);

	netCDFLayer * poL = new netCDFLayer(this, ncid, baseName, owgt, nullptr);
	size_t shape_count = sg->get_geometry_count();
	
	for(size_t featCt = 0; featCt < shape_count; featCt++)
	{
		OGRGeometry * featureDesc;

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
				// Unsupported feature type?
				// Still: to crash
				break;
		}

		int out; size_t r_size = 0;
		void * wkb_rep = sg->serializeToWKB(featCt, r_size);
		featureDesc->importFromWkb((const unsigned char*)wkb_rep, r_size, wkbVariantIso, out);
		OGRFeature * feat = new OGRFeature(defn);
		feat -> SetGeometryDirectly(featureDesc);
		feat -> SetFID(featCt);
		delete wkb_rep;
		poL->AddSimpleGeometryFeature(feat);
	}

	papoLayers = (netCDFLayer**)reallocarray(papoLayers, nLayers + 1, sizeof(netCDFLayer *)); 
	papoLayers[nLayers] = poL;
	nLayers++;

	delete sg;
	return CE_None;
}
