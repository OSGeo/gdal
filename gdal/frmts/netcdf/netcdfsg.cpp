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
	 * A function which makes it a bit easier to fetch single text attribute values
	 * attrgets returns a char. seq. on success, nullptr on failure 
	 * attrgets takes in ncid and varId in which to look for the attribute with name attrName 
	 *
	 * The returned char. seq. must be free'd afterwards
	 */
	char* attrf(int ncid, int varId, const char * attrName)
	{
		size_t len = 0;
		nc_inq_attlen(ncid, varId, attrName, &len);
		
		if(len < 1)
		{
			return nullptr;
		}

		char * attr_vals = new char[len];
		// Now look through this variable for the attribute
		if(nc_get_att_text(ncid, varId, attrName, attr_vals) != NC_NOERR)
		{
			delete[] attr_vals;
			return nullptr;		
		}

		//char * ret = new char[len];
		//strcpy(ret, attr_vals[0]);
		//nc_free_string(len, attr_vals);
		return attr_vals;
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

		// Once found, go to open geometry_container variable	
		int geoVarId = 0;
		char * cart = nullptr;
		
		if(nc_inq_varid(ncId, attrVal, &geoVarId) != NC_NOERR) { delete[] attrVal; return; }
		if((cart = attrf(ncId, geoVarId, CF_SG_NODE_COORDINATES)) == nullptr) { delete[] attrVal; return; }

		// Find geometry type
		this->type = nccfdriver::getGeometryType(ncId, geoVarId); 
		delete[] attrVal;

		// Find a list of node counts and part node count
		char * nc_name = nullptr; char * pnc_name = nullptr; char * inter_name = nullptr;
		int pnc_vid = -2; int nc_vid = -2; int inter_vid = -2;
		size_t bound = 0; int buf;
		if((nc_name = attrf(ncId, geoVarId, CF_SG_NODE_COUNT)) != nullptr)
		{
			nc_inq_varid(ncId, nc_name, &nc_vid);
			while(nc_get_var1_int(ncId, nc_vid, &bound, &buf) == NC_NOERR)
			{
				this->node_counts.push_back(buf);
				bound++;	
			}	

		}	

		if((pnc_name = attrf(ncId, geoVarId, CF_SG_PART_NODE_COUNT)) != nullptr)
		{
			bound = 0;
			nc_inq_varid(ncId, pnc_name, &pnc_vid);
			while(nc_get_var1_int(ncId, pnc_vid, &bound, &buf) == NC_NOERR)
			{
				this->pnode_counts.push_back(buf);
				bound++;	
			}	
		}	
	
		if((inter_name = attrf(ncId, geoVarId, CF_SG_INTERIOR_RING)) != nullptr
						&&
					this->type == POLYGON	
		)
		{
			bound = 0;
			// to do: check for error cond and exit
			nc_inq_varid(ncId, inter_name, &inter_vid);
			while(nc_get_var1_int(ncId, inter_vid, &bound, &buf) == NC_NOERR)
			{
				bool has_ring = buf == 1 ? true : false;
				this->pnode_counts.push_back(has_ring);
				bound++;	
			}	
		}	

		delete[] nc_name;
		delete[] pnc_name;
		delete[] inter_name;

		// Create bound list
		int rc = 0;
		bound_list.push_back(0);// start with 0
		for(size_t i = 0; i < node_counts.size() - 1; i++)
		{
			rc = rc + node_counts[i];
			bound_list.push_back(rc);	
		}

		// Create parts count list and an offset list for parts indexing	
		if(this->node_counts.size() > 0)
		{
			// to do: check for out of bounds
			int ind = 0; int parts = 0; int prog = 0;
			for(size_t pcnt = 0; pcnt < pnode_counts.size() ; pcnt++)
			{
				if(prog == 0) pnc_bl.push_back(pcnt);
				prog = prog + pnode_counts[pcnt];
				parts++;

				if(prog == node_counts[ind])
				{
					ind++;
					this->parts_count.push_back(parts);
					prog = 0; parts = 0;
				}	
				else if(prog > node_counts[ind])
				{
					delete[] cart;
					return;
				}
			} 
		}

		// (1) the touple order for a single point
		// (2) the variable ids with the relevant coordinate values
		// (3) initialize the point buffer
		char * dim = strtok(cart,  " ");
		int axis_id = 0;
		
		while(dim != NULL)
		{
			if(nc_inq_varid(ncId, dim, &axis_id) == NC_NOERR)
			{ this->touple_order++;
				this->nodec_varIds.push_back(axis_id);
			}

			dim = strtok(NULL, " ");

		}

		delete[] cart;

		this->pt_buffer = new Point(this->touple_order);
		
		// Set other values accordingly
		this->base_varId = baseVarId;
		this->gc_varId = geoVarId; 
		this->current_vert_ind = 0;	
		this->ncid = ncId;
		
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
		if(!this->has_next_pt())
		{
			return nullptr;
		}

		// Fill pt
		// New pt now
		for(int order = 0; order < touple_order; order++)
		{
			Point& pt = *(this->pt_buffer);
			double data;
			size_t full_ind = bound_list[cur_geometry_ind] + current_vert_ind;

			// Read a single coord
			int err = nc_get_var1_double(ncid, nodec_varIds[order], &full_ind, &data);
			// To do: optimize through multiple reads at once, instead of one datum

			if(err != NC_NOERR)
			{
				return nullptr;
			}

			pt[order] = data;
		}	
		
		this->current_vert_ind++;
		return (this->pt_buffer);	
	}

	bool SGeometry::has_next_pt()
	{
		if(this->current_vert_ind < node_counts[cur_geometry_ind])
		{
			return true;
		}
	
		else return false;
	}

	void SGeometry::next_geometry()
	{
		// to do: maybe implement except. and such on error conds.

		this->cur_geometry_ind++;
		this->cur_part_ind = 0;
		this->current_vert_ind = 0;	
	}

	bool SGeometry::has_next_geometry()
	{
		if(this->cur_geometry_ind < node_counts.size())
		{
			return true;
		}
		else return false;
	}

	/* serializeToWKB(SGeometry * sg)
	 * Takes the geometry in SGeometry at a given index and converts it into WKB format.
	 * Converting SGeometry into WKB automatically allocates the required buffer space
	 * and returns a buffer that MUST be free'd
	 */
	void * SGeometry::serializeToWKB(int featureInd, size_t& wkbSize)
	{
		if(featureInd < 0) return nullptr;
		void * ret = nullptr;
		// Serialization occurs differently depending on geometry
		// The memory requirements also differ between geometries
		switch(this->getGeometryType())
		{
			case POLYGON:
				// A polygon has:
				// 1 byte header
				// 4 byte Type (=3 for polygon)
				// 4 byte ring count (1 (exterior) or 2 (exterior and interior)
				// For each ring:
				// 4 byte point count, 8 byte double x point count x 2 dimension
				// (This is equivalent to requesting enough for all points and a point count header for each point)
				// (Or 8 byte double x node count x 2 dimension + 4 byte point count x part_count)
				// At the end: EOF

				int8_t header = 1;
				int32_t t = wkbPolygon;
				int32_t rc = parts_count[featureInd];
				int pc = pnc_bl[featureInd]; // initialize with first part count, list of part counts is contiguous
				const int32_t end = EOF;
				
				// case: no part node counts, no interior rings
				if(this->pnode_counts.size() == 0)
				{
					
				}

				else
				{
					wkbSize = 1 + 4 + 4 + 8 * node_counts[featureInd] * 2 + 4 * rc + 4;
					ret = new int8_t[wkbSize];
					void * writer = ret;
					writer = mempcpy(writer, &header, 1);
					writer = mempcpy(writer, &t, 4);
					writer = mempcpy(writer, &rc, 4);
					
					int32_t parts = pnode_counts[pc];
					pc++;
					writer = mempcpy(writer, &parts, 4);	
						
					// Check for dimension error / touple order < 2 still to implement					

					for(int node = 0; node < parts; node++)
					{
						Point * pt= this->next_pt();
						double x = (*pt)[0]; double y = (*pt)[1];
						writer = mempcpy(writer, &x, 8);
						writer = mempcpy(writer, &y, 8);
					}

					next_geometry();
					memcpy(writer, &end, 4);
				}

				break;
		}

		return ret;
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
					delete[] attrVal;
					return minor_ver;
				}	

				if(parse[1] != '.')
				{
					delete[] attrVal;
					return minor_ver;	
				}

				char * minor = parse + sizeof(char) * 2;
				minor_ver = atoi(minor);				

				// check for "0" and potentially malformed, due to atoi return cond
				if(minor_ver == 0)
				{
					if(strlen(parse) != 3)
						if(parse[2] != '0')
					{
						delete[] attrVal;
						minor_ver = -1;
					}
				}
			}	

			else
			{
				minor_ver = -1;
				delete[] attrVal;
				return minor_ver;	
			}

			parse = strtok(NULL, "-");
		}
	
	
		delete []attrVal;
		return minor_ver;
	}

	geom_t getGeometryType(int ncid, int varid)
	{
		geom_t ret = NONE;
		char * gt_name = attrf(ncid, varid, CF_SG_GEOMETRY_TYPE);

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

		delete[] gt_name;
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
