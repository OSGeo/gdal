// Implementations of netCDF functions used for 
// implementing CF-1.8 convention awareness
//
// Author: wchen329
#include <cstdio>
#include <cstring>
#include "netcdf.h"
#include "netcdfdataset.h"
#include "netcdfsg.h"
namespace nccfdriver
{
	// Private Helpers

	/* Attribute-Gets
	 * A function which makes it a bit easier to fetch single string attribute values
	 * attrgets returns 0 on success, some non-zero value on failure
	 * attrgets takes in ncid and varId in which to look for the attribute with name attrName 
	 * then it fills bufOut with the attribute value 
	 */
	static int attrgets(int ncid, int varId, size_t buffer_length, char * attrName, char * bufOut)
	{
		
	}

	// Point
	Point::~Point()
	{
		delete[] this->values;
	}

	// SGeometry
	SGeometry::SGeometry(int ncId, int baseVarId) : base_varId(baseVarId)
	{
		size_t len = 0;
		nc_inq_attlen(ncId, baseVarId, CF_SG_GEOMETRY, &len); 
		// If not one value, error 
		if(len != 1)
		{
			this -> valid = false;
			return;
		}
	
		char ** attr_vals = new char*[1];
		// Look through base variable, for geometry_container
		if(nc_get_att_string(ncId, baseVarId, CF_SG_GEOMETRY, attr_vals) != NC_NOERR) {
			this -> valid = false;
			return;	
			delete[] attr_vals;
		}

		// Find geometry type
		this->type = getGeometryType(ncId, attr_vals[0]);

		// Once found, go to open geometry_container variable	
		int geoVarId = 0;
		nc_inq_varid(ncId, attr_vals[0], &geoVarId);


		// Now geometry_container is open, look for node_coordinates, assign the following
		// (1) the touple order for a single point
		// (2) the variable ids with the relevant coordinate values
		// (3) initialize the point buffer

		// Set other values accordingly
		this->base_varId = baseVarId;
		this->gc_varId = geoVarId; 
		this->current_vert_ind = 0;	
		
		if(this->type == POLYGON)
		{
			// still to implement
			this->interior = false;	// stub
		}
		else this->interior = false;

		nc_free_string(len, attr_vals);
		delete[] attr_vals;
		this -> valid = true;
	}

	Point* SGeometry::next_pt()
	{
		// Fill pt

		// New pt now
		return this->pt_buffer;	
	}

	bool SGeometry::has_next_pt()
	{
		// Check dimensions of one (or perhaps each of the node coordinate) arrays
		// false if the current_vert_ind is equal to or exceeds length of one of those arrays
		// stub	
		return false;
	}

	void SGeometry::next_geometry()
	{
		// stub
	}

	bool SGeometry::has_next_geometry()
	{
		// stub
		return false;
	}

	// Helpers
	// following is a short hand for a clean up and exit, since goto isn't allowed
	#define getCFMinorVersionExit delete[] attr_vals; nc_free_string(len, attr_vals); return minor_ver;	
	int getCFMinorVersion(int ncid)
	{
		bool is_CF_conv = false;
		int minor_ver = -1;

		// Allocate a large enough buffer	
		size_t len = 0;
		nc_inq_attlen(ncid, NC_GLOBAL, NCDF_CONVENTIONS, &len);

		// If not one value, error 
		if(len != 1)
		{
			return -1;
		}	

		char ** attr_vals = new char*[1];

		// Fetch the CF attribute
		if(nc_get_att_string(ncid, NC_GLOBAL, NCDF_CONVENTIONS, attr_vals) != NC_NOERR)
		{
			delete[] attr_vals;
			return minor_ver;
		}

		// Fetched without errors, now traverse	
		char * parse = strtok(attr_vals[0], "-");
		while(parse != NULL)
		{
			// still todo, look for erroneous standards
			// Test for CF Conventions
			if(!strcmp(parse, "CF"))
			{
				is_CF_conv = true;		
			}

			// Test for Version to see if 
			else if(parse[0] == '1' && is_CF_conv)
			{
				// ensure correct formatting and only singly defined
				if(strlen(parse) < 3 || minor_ver >= 0)
				{
					getCFMinorVersionExit 
				}	

				if(parse[1] != ',')
				{
					getCFMinorVersionExit 
				}

				char * minor = parse + sizeof(char) * 2;
				minor_ver = atoi(minor);				

				// check for "0" and potentially malformed, due to atoi return cond
				if(minor_ver == 0)
				{
					if(strlen(parse) > 3 || parse[2] == '0')
					{
						minor_ver = -1;
					}
				}
			}	

			else
			{
				minor_ver = -1;
				getCFMinorVersionExit 
			}

			parse = strtok(NULL, "-");
		}
	
	
		nc_free_string(len, attr_vals);	
		delete[] attr_vals;
		return minor_ver;
	}

	geom_t getGeometryType(int ncid, const char * varName)
	{
		int varId = 0; size_t len = 0;
		nc_inq_varid(ncid, varName, &varId);
		nc_inq_attlen(ncid, varId, CF_SG_GEOMETRY_TYPE, &len);
		geom_t ret = NONE;

		if(len != 1)
		{
			return NONE;
		} 

		char ** attr_vals = new char*[1];	// have to allocate prehand, still to do! 


		if(nc_get_att_string(ncid, varId, CF_SG_GEOMETRY_TYPE, attr_vals) != NC_NOERR)
		{
			ret = NONE;	
		}

		char * gt_name = attr_vals[0];

		// Points	
		if(!strcmp(gt_name, CF_SG_TYPE_POINT))
		{
			// still to do, add detection for multipart geometry
			ret = POINT;	
		}

		// Lines
		else if(!strcmp(gt_name, CF_SG_TYPE_LINE))
		{
			ret = LINE;
		}

		// Polygons
		else if(!strcmp(gt_name, CF_SG_TYPE_POLY))
		{
			ret = POLYGON;
		}

		nc_free_string(len, attr_vals);	
		delete[] attr_vals;
		return ret;
	}

	SGeometry* getGeometryRef(int ncid, const char * varName )
	{
		int varId = 0;
		nc_inq_varid(ncid, varName, &varId);
		return new SGeometry(ncid, varId);
	}

	int putGeometryRef(int ncid, SGeometry * geometry)
	{
		
		// stub
		return -1;
	}

}
