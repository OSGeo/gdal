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

	Point* SGeometry::operator[](int index)
	{
	/*	if(index >= this->node_counts[cur_geometry_ind])
		{
			return nullptr;
		}
*/
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

		return (this->pt_buffer);
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
		// to do: revise for single point case
		int nc = node_counts[featureInd];
		int sb = bound_list[featureInd];
		// Serialization occurs differently depending on geometry
		// The memory requirements also differ between geometries
		switch(this->getGeometryType())
		{
			case POINT:
				{
					wkbSize = 16;
					ret = new int8_t[wkbSize];
					Point * single = (*this)[featureInd];
					int32_t x = (*single)[0]; int32_t y = (*single)[1];
					void * worker = (void*)ret;
					worker = mempcpy(worker, &x, 8);
					worker = mempcpy(worker, &y, 8);
				}
				break;

			case LINE:
				wkbSize = 1 + 4 + 4 + 16 * nc;
				ret = new uint8_t[wkbSize];
				inPlaceSerialize_LineString(this, nc, sb, ret);
				break;

			case POLYGON:
				/*// A polygon has:
				// 1 byte header
				// 4 byte Type (=3 for polygon)
				// 4 byte ring count (1 (exterior) or 2 (exterior and interior)
				// For each ring:
				// 4 byte point count, 8 byte double x point count x 2 dimension
				// (This is equivalent to requesting enough for all points and a point count header for each point)
				// (Or 8 byte double x node count x 2 dimension + 4 byte point count x part_count)

				int32_t rc = parts_count[featureInd];
				int pc_begin = pnc_bl[featureInd]; // initialize with first part count, list of part counts is contiguous	
				size_t wkbSize = 1 + 4 + 4 + 8 * node_counts[featureInd] * 2 + 4 * rc;
				int serial_start = bound_list[featureInd];

				std::vector<int> pnc;

				// Build sub vector for part_node_counts
				for(int itr = 0; itr < rc; itr++)
				{
					pnc.push_back(parts_count[pc_begin + itr]);	
				}
	

				// Now allocate and serialize
				ret = new[wkbSize];
				inPlaceSerialize_Polygon(this, pnc, rc, seek_begin, ret);
				*/
				break;

			case MULTIPOINT:
				wkbSize = 1 + 4 + 4 + 16 * nc;
				ret = new uint8_t[wkbSize];
				inPlaceSerialize_MultiPoint(this, nc, sb, ret); 
				break;

			case MULTILINE:
				// Calculate wkbSize
				wkbSize = 0;
				break;

			case MULTIPOLYGON:
				// A multipolygon has:
				// 1 byte header
				// 4 byte Type (=6 for multipolygon)
				// 4 byte polygon count
				// For each Polygon:
				// 1 + 4 + 4 + 8 * 2 * node_count

				int32_t header = 1;
				int32_t t = wkbMultiPolygon;
				bool noInteriors = this->int_rings.size() == 0 ? true : false;
				int32_t rc = parts_count[featureInd];
				int first_pt = bound_list[featureInd];
				int pc_begin = pnc_bl[featureInd]; // initialize with first part count, list of part counts is contiguous	
				
				wkbSize = 1 + 4 + 4;
					

				int seek_begin = bound_list[featureInd];

				std::vector<int> pnc;

				// Build sub vector for part_node_counts
				for(int itr = 0; itr < rc; itr++)
				{
					pnc.push_back(parts_count[pc_begin + itr]);	
				}	

				// Figure out each Polygon's space requirements
				if(noInteriors)
				{
					for(int ss = 0; ss < rc; ss++)
					{
					 	wkbSize += 16 * pnc[ss] + 1 + 4 + 4;
					}
				}

				// Now allocate and serialize
				ret = new uint8_t[wkbSize];
				
				// Create Multipolygon headers
				void * worker = (void*)ret;
				worker = mempcpy(worker, &header, 1);
				worker = mempcpy(worker, &t, 4);

				if(noInteriors)
				{
					int cur_point = first_pt;
					int32_t pcount = pnc.size();
					worker = mempcpy(worker, &pcount, 4);

					for(int32_t itr = 0; itr < pcount; itr++)
					{
						worker = inPlaceSerialize_PolygonExtOnly(this, pnc[itr], cur_point, worker);
						cur_point = pnc[itr] + cur_point;
					}
				}

				else
				{
					inPlaceSerialize_Polygon(this, pnc, rc, seek_begin, ret);
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
			// Node Count present? Assume that it is a multipoint.
			if(nc_inq_att(ncid, varid, CF_SG_NODE_COUNT, NULL, NULL) == NC_ENOTATT)
			{
				ret = POINT;	
			}
			else ret = MULTIPOINT;
		}

		// Lines
		else if(!strcmp(gt_name, CF_SG_TYPE_LINE))
		{
			// Part Node Count present? Assume multiline
			if(nc_inq_att(ncid, varid, CF_SG_PART_NODE_COUNT, NULL, NULL) == NC_ENOTATT)
			{
				ret = LINE;
			}
			else ret = MULTILINE;
		}

		// Polygons
		else if(!strcmp(gt_name, CF_SG_TYPE_POLY))
		{
			/* Polygons versus MultiPolygons, slightly ambiguous
			 * Part Node Count & no Interior Ring - MultiPolygon
			 * no Part Node Count & no Interior Ring - Polygon
			 * Part Node Count & Interior Ring - assume that it is a MultiPolygon
			 */
			int pnc_present = nc_inq_att(ncid, varid, CF_SG_PART_NODE_COUNT, NULL, NULL);
			int ir_present = nc_inq_att(ncid, varid, CF_SG_INTERIOR_RING, NULL, NULL);

			if(pnc_present == NC_ENOTATT && ir_present == NC_ENOTATT)
			{
				ret = POLYGON;
			}
			else ret = MULTIPOLYGON;
		}

		delete[] gt_name;
		return ret;
	}

	void* inPlaceSerialize_PointLocusGeneric(SGeometry * ge, int node_count, int seek_begin, void * serializeBegin, uint32_t type)
	{
		uint8_t order = 1;
		uint32_t t = type;
		uint32_t nc = (uint32_t) node_count;
		
		serializeBegin = mempcpy(serializeBegin, &order, 1);
		serializeBegin = mempcpy(serializeBegin, &t, 4);
		serializeBegin = mempcpy(serializeBegin, &nc, 4);

		// Now serialize points
		for(int ind = 0; ind < node_count; ind++)
		{
			Point * p = (*ge)[seek_begin + ind];
			double x = (*p)[0];
			double y = (*p)[1];
			serializeBegin = mempcpy(serializeBegin, &x, 8);
			serializeBegin = mempcpy(serializeBegin, &y, 8);	
		}

		return serializeBegin;
	}
	
	void* inPlaceSerialize_MultiPoint(SGeometry * ge, int node_count, int seek_begin, void * serializeBegin)
	{
		return inPlaceSerialize_PointLocusGeneric(ge, node_count, seek_begin, serializeBegin, wkbMultiPoint);
	}

	void* inPlaceSerialize_LineString(SGeometry * ge, int node_count, int seek_begin, void * serializeBegin)
	{
		return inPlaceSerialize_PointLocusGeneric(ge, node_count, seek_begin, serializeBegin, wkbLineString);
	}

	void* inPlaceSerialize_PolygonExtOnly(SGeometry * ge, int node_count, int seek_begin, void * serializeBegin)
	{	
		int8_t header = 1;
		int32_t t = wkbPolygon;
		int32_t rc = 1;
				
		void * writer = serializeBegin;
		writer = mempcpy(writer, &header, 1);
		writer = mempcpy(writer, &t, 4);
		writer = mempcpy(writer, &rc, 4);
						
		int32_t nc = (int32_t)node_count;
		writer = mempcpy(writer, &node_count, 4);

		for(int pind = 0; pind < nc; pind++)
		{
			Point * pt= (*ge)[seek_begin + pind];
			double x = (*pt)[0]; double y = (*pt)[1];
			writer = mempcpy(writer, &x, 8);
			writer = mempcpy(writer, &y, 8);
		}

		return writer;
	}

	void* inPlaceSerialize_Polygon(SGeometry * ge, std::vector<int>& pnc, int ring_count, int seek_begin, void * serializeBegin)
	{
			
		int8_t header = 1;
		int32_t t = wkbPolygon;
		int32_t rc = (int32_t)ring_count;
				
		void * writer = serializeBegin;
		writer = mempcpy(writer, &header, 1);
		writer = mempcpy(writer, &t, 4);
		writer = mempcpy(writer, &rc, 4);
		int cmoffset = 0;
						
		// Check for dimension error / touple order < 2 still to implement					
		for(int ring_c = 0; ring_c < ring_count; ring_c++)
		{
			int32_t node_count = pnc[ring_c];
			writer = mempcpy(writer, &node_count, 4);

			int pind = 0;
			for(pind = 0; pind < pnc[ring_c]; pind++)
			{
				Point * pt= (*ge)[seek_begin + cmoffset + pind];
				double x = (*pt)[0]; double y = (*pt)[1];
				writer = mempcpy(writer, &x, 8);
				writer = mempcpy(writer, &y, 8);
			}

			cmoffset += pind;
		}

		return writer;
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
