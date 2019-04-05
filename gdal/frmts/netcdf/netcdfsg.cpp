// Implementations of netCDF functions used for 
// implementing CF-1.8 convention awareness
//
// Author: wchen329
#include <cstdio>
#include <cstring>
#include <vector>
#include "netcdf.h"
#include "netcdfdataset.h"
#include "netcdfsg.h"
namespace nccfdriver
{
	/* Attribute Fetch
	 * -
	 * A function which makes it a bit easier to fetch single string attribute values
	 * attrgets returns a char. seq. on success, nullptr on failure 
	 * attrgets takes in ncid and varId in which to look for the attribute with name attrName 
	 *
	 * The returned char. seq. must be free'd afterwards
	 */
	char* attrf(int ncid, int varId, const char * attrName)
	{
		size_t len = 0;
		nc_inq_attlen(ncid, varId, attrName, &len);
		
		// If not one value, error
		if(len != 1)
		{
			return nullptr;
		}	

		char ** attr_vals = new char*[1];
		// Now look through this variable for the attribute
		if(nc_get_att_string(ncid, varId, attrName, attr_vals) != NC_NOERR)
		{
			delete[] attr_vals;
			return nullptr;		
		}

		char * ret = new char[strlen(attr_vals[0])];
		strcpy(ret, attr_vals[0]);
		nc_free_string(len, attr_vals);
		delete[] attr_vals;
		return ret;
	}

	/* Point
	 * (implementations)
	 *
	 */
	Point::~Point()
	{
		delete[] this->values;
	}

	/* SGeometry 
	 * (implementations)
	 *
	 */
	SGeometry::SGeometry(int ncId, int baseVarId) : base_varId(baseVarId)
	{
		char * attrVal = nullptr;
		// Look through base variable, for geometry_container
		if((attrVal = attrf(ncId, baseVarId, CF_SG_GEOMETRY)) == nullptr)
		{
			return;	
		}

		// Find geometry type
		this->type = getGeometryType(ncId, attrVal); 
		delete attrVal;	

		// Once found, go to open geometry_container variable	
		int geoVarId = 0;
		char * cart = nullptr;
		
		if(nc_inq_varid(ncId, attrVal, &geoVarId) != NC_NOERR)
		   if((cart = attrf(ncId, geoVarId, CF_SG_NODE_COORDINATES)) == nullptr)
		{
			return;
		}

		// (1) the touple order for a single point
		// (2) the variable ids with the relevant coordinate values
		// (3) initialize the point buffer
		char * dim = strtok(cart,  " ");
		int axis_id = 0;
		
		while(dim != NULL)
		{
			if(nc_inq_varid(ncId, dim, &axis_id) == NC_NOERR) { this->touple_order++;
				this->nodec_varIds.push_back(axis_id);
			}

			dim = strtok(NULL, " ");

		}

		delete cart;

		this->pt_buffer = new Point(this->touple_order);
		
		// Set other values accordingly
		this->base_varId = baseVarId;
		this->gc_varId = geoVarId; 
		this->current_vert_ind = 0;	
		
		if(this->type == POLYGON)
		{
			// still to implement
			this->interior = false;
		}
		else this->interior = false;

		this -> valid = true;
	}

	SGeometry::~SGeometry()
	{
		delete this->pt_buffer;
	}

	Point* SGeometry::next_pt()
	{
		// Fill pt
		if(!this->has_next_pt())
		{
			return nullptr;
		}
		// New pt now
		for(int order; order < touple_order; order++)
		{
			Point& pt = *this->pt_buffer;
			pt[order] = 0;
		}	
		
		return (this->pt_buffer);	
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
	int getCFMinorVersion(int ncid)
	{
		bool is_CF_conv = false;
		int minor_ver = -1;
		char * attrVal = nullptr;

		// Fetch the CF attribute
		if((attrVal = attrf(ncid, NC_GLOBAL, NCDF_CONVENTIONS)) == nullptr)
		{
			return minor_ver;
		}

		// Fetched without errors, now traverse	
		char * parse = strtok(attrVal, "-");
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
					return minor_ver;
				}	

				if(parse[1] != ',')
				{
					return minor_ver;	
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
				return minor_ver;	
			}

			parse = strtok(NULL, "-");
		}
	
	
		delete attrVal;
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
