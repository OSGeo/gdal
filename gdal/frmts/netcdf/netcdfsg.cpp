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
	Point::~Point()
	{
		delete[] this->values;
	}

	// short hand for a clean up and exit, since goto isn't allowed
	#define getCFMinorVersionExit delete[] attr_vals; return minor_ver;
	int getCFMinorVersion(int ncid)
	{
		bool is_CF_conv = false;
		int minor_ver = -1;

		// Allocate a large enough buffer	
		size_t len = 0;
		char ** attr_vals = new char*[1];
		nc_inq_attlen(ncid, NC_GLOBAL, NCDF_CONVENTIONS, &len);
		
		// If not one value, error 
		if(len != 1)
		{
			getCFMinorVersionExit 
		}	


		// Fetch the CF attribute
		if(nc_get_att_string(ncid, NC_GLOBAL, NCDF_CONVENTIONS, attr_vals) == NC_ERANGE)
		{
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
				// check for "0" and malformed, differentiate still to do
			}	

			else
			{
				minor_ver = -1;
				getCFMinorVersionExit 
			}

			parse = strtok(NULL, "-");
		}
	
	
		delete[] attr_vals;
		return minor_ver;
	}

	geom_t getGeometryType(int ncid, const char * varName)
	{
		int varId = 0;
		nc_inq_varid(ncid, varName, &varId);
		char ** attr_vals;	// have to allocate prehand, still to do! 
		if(nc_get_att_string(ncid, varId, CF_SG_GEOMETRY_TYPE, attr_vals) == NC_ERANGE)
		{
			return NONE;
		}

		char * gt_name = attr_vals[0];

		// Points	
		if(!strcmp(gt_name, CF_SG_TYPE_POINT))
		{
			// still to do, add detection for multipart geometry
			return POINT;	
		}

		// Lines
		if(!strcmp(gt_name, CF_SG_TYPE_LINE))
		{
			return LINE;
		}

		// Polygons
		if(!strcmp(gt_name, CF_SG_TYPE_POLY))
		{
			return POLYGON;
		}

		return NONE;
	}
}
