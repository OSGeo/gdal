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
	/* Re-implementation of mempcpy
	 * but compatible with libraries which only implement memcpy
	 */
	static void* memcpy_jump(void *dest, const void *src, size_t n)
	{
		memcpy(dest, src, n);
		char * a = (char*)dest + n; // otherwise unneccesary casting to stop compiler warnings
		void * b = (void*)a;	
		return b;
	}

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
	SGeometry::SGeometry(int ncId, int geoVarId)
		: gc_varId(geoVarId), touple_order(0), current_vert_ind(0), cur_geometry_ind(0), cur_part_ind(0)
	{

		char * cart = nullptr;
		this->pt_buffer = nullptr;
		memset(this->container_name, 0, NC_MAX_NAME+1);

		// Get geometry container name
		if(nc_inq_varname(ncId, geoVarId, container_name) != NC_NOERR)
		{
			throw new SG_Exception_Existential((char*)"new geometry container:", "the variable of the given ID"); 	
		}

		// Find geometry type
		this->type = nccfdriver::getGeometryType(ncId, geoVarId); 

		// Get grid mapping variable, if it exists
		char * gm_name = nullptr;
		this->gm_varId = INVALID_VAR_ID;
		if((gm_name = attrf(ncId, geoVarId, CF_GRD_MAPPING)) != nullptr)
		{
			int gmVID;
			if(nc_inq_varid(ncId, gm_name, &gmVID) == NC_NOERR)
			{
				this->gm_varId = gmVID;	
			}
		}	
		
		delete[] gm_name;

		// Find a list of node counts and part node count
		char * nc_name = nullptr; char * pnc_name = nullptr; char * inter_name = nullptr; char * ir_name = nullptr;
		int pnc_vid = INVALID_VAR_ID; int nc_vid = INVALID_VAR_ID; int ir_vid = INVALID_VAR_ID;
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
	
		if((ir_name = attrf(ncId, geoVarId, CF_SG_INTERIOR_RING)) != nullptr)
		{
			bound = 0;
			nc_inq_varid(ncId, ir_name, &ir_vid);
			while(nc_get_var1_int(ncId, ir_vid, &bound, &buf) == NC_NOERR)
			{
				bool store = buf == 0 ? false : true;
				this->int_rings.push_back(store);
				bound++;
			}
		}

		delete[] nc_name;
		delete[] pnc_name;
		delete[] inter_name;
		delete[] ir_name;

		/* Enforcement of well formed CF files
		 * If these are not met then the dataset is malformed and will halt further processing of
		 * simple geometries. (Slightly permissive, letts other netCDF items to be processed)
		 */

		// part node count exists only when node count exists
		if(pnode_counts.size() > 0 && node_counts.size() == 0)
		{
			throw new SG_Exception_Dep(container_name, CF_SG_NODE_COUNT, CF_SG_PART_NODE_COUNT);
		}

		// interior rings only exist when part node counts exist
		if(int_rings.size() > 0 && pnode_counts.size() == 0)
		{
			throw new SG_Exception_Dep(container_name, CF_SG_INTERIOR_RING, CF_SG_PART_NODE_COUNT);
		}	

	
		// cardinality of part_node_counts == cardinality of interior_ring (if interior ring > 0)
		if(int_rings.size() > 0)
		{
			if(int_rings.size() != pnode_counts.size())
			{
				throw new SG_Exception_Dim_MM(container_name, CF_SG_INTERIOR_RING, CF_SG_PART_NODE_COUNT);
			}
		}

		// cardinality of node_counts == cardinality of x and y node_coordinates

		// lines and polygons require node_counts, multipolygons are checked with part_node_count
		if(this->type == POLYGON || this->type == LINE)
		{
			if(node_counts.size() < 1)
			{
				throw new SG_Exception_Existential(container_name, CF_SG_NODE_COUNT);
			}
		}

		/* Safety checks End
		 */

		// Create bound list
		int rc = 0;
		bound_list.push_back(0);// start with 0

		if(node_counts.size() > 0)
		{
			for(size_t i = 0; i < node_counts.size() - 1; i++)
			{
				rc = rc + node_counts[i];
				bound_list.push_back(rc);	
			}
		}

		// Node Coordinates
		if((cart = attrf(ncId, geoVarId, CF_SG_NODE_COORDINATES)) == nullptr)
		{
			throw new SG_Exception_Existential(container_name, CF_SG_NODE_COORDINATES);
		}	

		// Create parts count list and an offset list for parts indexing	
		if(this->node_counts.size() > 0)
		{
			// to do: check for out of bounds
			int ind = 0; int parts = 0; int prog = 0; int c = 0;
			for(size_t pcnt = 0; pcnt < pnode_counts.size() ; pcnt++)
			{
				if(prog == 0) pnc_bl.push_back(pcnt);

				if(int_rings.size() > 0) if(!int_rings[pcnt]) c++;

				prog = prog + pnode_counts[pcnt];
				parts++;

				if(prog == node_counts[ind])
				{
					ind++;
					this->parts_count.push_back(parts);
					if(int_rings.size() > 0) this->poly_count.push_back(c);
					c = 0;
					prog = 0; parts = 0;
				}	
				else if(prog > node_counts[ind])
				{
					delete[] cart;
					throw new SG_Exception_BadSum(container_name, CF_SG_PART_NODE_COUNT, CF_SG_NODE_COUNT);
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
		this->gc_varId = geoVarId; 
		this->current_vert_ind = 0;	
		this->ncid = ncId;
	}

	SGeometry::~SGeometry()
	{
		delete this->pt_buffer;
	}

	Point& SGeometry::next_pt()
	{
		if(!this->has_next_pt())
		{
			throw new SG_Exception_BadPoint();
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
				throw new SG_Exception_BadPoint();
			}

			pt[order] = data;
		}	
		
		this->current_vert_ind++;
		return *(this->pt_buffer);	
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

	Point& SGeometry::operator[](int index)
	{
		for(int order = 0; order < touple_order; order++)
		{
			Point& pt = *(this->pt_buffer);
			double data;
			size_t real_ind = index;

			// Read a single coord
			int err = nc_get_var1_double(ncid, nodec_varIds[order], &real_ind, &data);

			if(err != NC_NOERR)
			{
				throw new SG_Exception_BadPoint();
			}

			pt[order] = data;
		}	

		return *(this->pt_buffer);
	}

	size_t SGeometry::get_geometry_count()
	{
		if(type == POINT)
		{
			// If nodes global attribute is available, use that

			// Otherwise, don't fail- use dimension length of one of x

			if(this->nodec_varIds.size() < 1) return 0;

			// If more than one dim, then error. Otherwise inquire its length and return that
			int dims;
			if(nc_inq_varndims(this->ncid, nodec_varIds[0], &dims) != NC_NOERR) return 0;
			if(dims != 1) return 0;
			
			// Find which dimension is used for x
			int index;
			if(nc_inq_vardimid(this->ncid, nodec_varIds[0], &index) != NC_NOERR)
			{
				return 0;
			}

			// Finally find the length
			size_t len;
			if(nc_inq_dimlen(this->ncid, index, &len) != NC_NOERR)
			{
				return 0;
			}
			return len;	
		}

		else return this->node_counts.size();
	}

	/* serializeToWKB(SGeometry * sg)
	 * Takes the geometry in SGeometry at a given index and converts it into WKB format.
	 * Converting SGeometry into WKB automatically allocates the required buffer space
	 * and returns a buffer that MUST be free'd
	 */
	void * SGeometry::serializeToWKB(size_t featureInd, int& wkbSize)
	{		
		void * ret = nullptr;

		int nc = 0; int sb = 0;

		// Points don't have node_count entry... only inspect and set node_counts if not a point
		if(this->getGeometryType() != POINT)
		{
			nc = node_counts[featureInd];
			sb = bound_list[featureInd];
		}

		// Serialization occurs differently depending on geometry
		// The memory requirements also differ between geometries
		switch(this->getGeometryType())
		{
			case POINT:
				wkbSize = 1 + 4 + 16;
				ret = new int8_t[wkbSize];
				inPlaceSerialize_Point(this, featureInd, ret);
				break;

			case LINE:
				wkbSize = 1 + 4 + 4 + 16 * nc;
				ret = new uint8_t[wkbSize];
				inPlaceSerialize_LineString(this, nc, sb, ret);
				break;

			case POLYGON:
				// A polygon has:
				// 1 byte header
				// 4 byte Type (=3 for polygon)
				// 4 byte ring count (1 (exterior) or 2 (exterior and interior)
				// For each ring:
				// 4 byte point count, 8 byte double x point count x 2 dimension
				// (This is equivalent to requesting enough for all points and a point count header for each point)
				// (Or 8 byte double x node count x 2 dimension + 4 byte point count x part_count)

				// if interior ring, then assume that it will be a multipolygon (maybe future work?)
				wkbSize = 1 + 4 + 4 + 4 + 16 * nc;
				ret = new int8_t[wkbSize];
				inPlaceSerialize_PolygonExtOnly(this, nc, sb, ret);
				break;

			case MULTIPOINT:
				{
					wkbSize = 1 + 4 + 4 + nc * (1 + 4 + 16);
					ret = new int8_t[wkbSize];

					void * worker = ret;
					int8_t header = 1;
					int32_t t = wkbMultiPoint;

					// Add metadata
					worker = memcpy_jump(worker, &header, 1);
					worker = memcpy_jump(worker, &t, 4);
					worker = memcpy_jump(worker, &nc, 4);

					// Add points
					for(int pts = 0; pts < nc; pts++)
					{
						worker = inPlaceSerialize_Point(this, sb + pts, worker);								
					}
				}

				break;

			case MULTILINE:
				{
					int32_t header = 1;
					int32_t t = wkbMultiLineString;
					int32_t lc = parts_count[featureInd];
					int seek_begin = bound_list[featureInd];
					int pc_begin = pnc_bl[featureInd]; // initialize with first part count, list of part counts is contiguous	
					wkbSize = 1 + 4 + 4;
					std::vector<int> pnc;

					// Build sub vector for part_node_counts
					// + Calculate wkbSize
					for(int itr = 0; itr < lc; itr++)
					{
						pnc.push_back(pnode_counts[pc_begin + itr]);	
					 	wkbSize += 16 * pnc[itr] + 1 + 4 + 4;
					}

				
					int cur_point = seek_begin;
					size_t pcount = pnc.size();

					// Allocate and set pointers
					ret = new int8_t[wkbSize];
					void * worker = ret;

					// Begin Writing
					worker = memcpy_jump(worker, &header, 1);
					worker = memcpy_jump(worker, &t, 4);
					worker = memcpy_jump(worker, &pcount, 4);

					for(size_t itr = 0; itr < pcount; itr++)
					{
							worker = inPlaceSerialize_LineString(this, pnc[itr], cur_point, worker);
							cur_point = pnc[itr] + cur_point;
					}
				}

				break;

			case MULTIPOLYGON:
				{
					int32_t header = 1;
					int32_t t = wkbMultiPolygon;
					bool noInteriors = this->int_rings.size() == 0 ? true : false;
					int32_t rc = parts_count[featureInd];
					int seek_begin = bound_list[featureInd];
					int pc_begin = pnc_bl[featureInd]; // initialize with first part count, list of part counts is contiguous		
					wkbSize = 1 + 4 + 4;
					std::vector<int> pnc;

					// Build sub vector for part_node_counts
					for(int itr = 0; itr < rc; itr++)
					{
						pnc.push_back(pnode_counts[pc_begin + itr]);	
					}	

					// Figure out each Polygon's space requirements
					if(noInteriors)
					{
						for(int ss = 0; ss < rc; ss++)
						{
						 	wkbSize += 16 * pnc[ss] + 1 + 4 + 4 + 4;
						}
					}

					else
					{
						// Total space requirements for Polygons:
						// (1 + 4 + 4) * number of Polygons
						// 4 * number of Rings Total
						// 16 * number of Points
		

						// Each ring collection corresponds to a polygon
						wkbSize += (1 + 4 + 4) * poly_count[featureInd]; // (headers)

						// Add header requirements for rings
						wkbSize += 4 * parts_count[featureInd];

						// Add requirements for number of points
						wkbSize += 16 * nc;
					}

					// Now allocate and serialize
					ret = new uint8_t[wkbSize];
				
					// Create Multipolygon headers
					void * worker = (void*)ret;
					worker = memcpy_jump(worker, &header, 1);
					worker = memcpy_jump(worker, &t, 4);

					if(noInteriors)
					{
						int cur_point = seek_begin;
						size_t pcount = pnc.size();
						worker = memcpy_jump(worker, &pcount, 4);

						for(size_t itr = 0; itr < pcount; itr++)
						{
							worker = inPlaceSerialize_PolygonExtOnly(this, pnc[itr], cur_point, worker);
							cur_point = pnc[itr] + cur_point;
						}
					}

					else
					{
						int32_t polys = poly_count[featureInd];
						worker = memcpy_jump(worker, &polys, 4);
	
						int base = pnc_bl[featureInd]; // beginning of parts_count for this multigeometry
						int seek = seek_begin; // beginning of node range for this multigeometry
						size_t ir_base = base + 1;
						int rc_m = 1; 

						// has interior rings,
						for(int32_t itr = 0; itr < polys; itr++)
						{	
							rc_m = 1;

							// count how many parts belong to each Polygon		
							while(ir_base < int_rings.size() && int_rings[ir_base])
							{
								rc_m++;
								ir_base++;	
							}

							if(rc_m == 1) ir_base++;	// single polygon case

							std::vector<int> poly_parts;

							// Make part node count sub vector
							for(int itr_2 = 0; itr_2 < rc_m; itr_2++)
							{
								poly_parts.push_back(pnode_counts[base + itr_2]);
							}

							worker = inPlaceSerialize_Polygon(this, poly_parts, rc_m, seek, worker);

							// Update seek position
							for(size_t itr_3 = 0; itr_3 < poly_parts.size(); itr_3++)
							{
								seek += poly_parts[itr_3];
							}
						}
					}
				}	
				break;

				default:

					throw new SG_Exception_BadFeature();	
					break;
		}

		return ret;
	}

	void SGeometry_PropertyReader::open(int container_id)
	{
		std::vector<std::pair<int, std::string>> addition;

		// First check for container_id, if variable doesn't exist error out
		if(nc_inq_var(this->nc, container_id, NULL, NULL, NULL, NULL, NULL) != NC_NOERR)
		{
			return;	// change to exception
		}

		// Now exists, see what variables refer to this one
		// First get name of this container
		char contname[NC_MAX_NAME];
		if(nc_inq_varname(this->nc, container_id, contname) != NC_NOERR)
		{
			return;
		}

		// Then scan throughout the netcdfDataset if those variables geometry_container
		// atrribute matches the container
		int varCount = 0;
		if(nc_inq_nvars(this->nc, &varCount) != NC_NOERR)
		{
			return;
		}

		for(int curr = 0; curr < varCount; curr++)
		{
			size_t contname2_len = 0;
			char * buf = nullptr;
			
			// First find container length, and make buf that size in chars
			if(nc_inq_attlen(this->nc, curr, CF_SG_GEOMETRY, &contname2_len) != NC_NOERR)
			{
				// not a geometry variable, continue
				continue;
			}

			// Also if present but empty, go on
			if(contname2_len == 0) continue;

			// Otherwise, geometry: see what container it has
			buf = new char[contname2_len];
			if(nc_get_att_text(this->nc, curr, CF_SG_GEOMETRY, buf)!= NC_NOERR)
			{
				delete[] buf;
				continue;
			}

			// If matches, then establish a reference by placing this variable's {id, name} pair into the map
			if(!strcmp(contname, buf))
			{
				char property_name[NC_MAX_NAME];
				nc_inq_varname(this->nc, curr, property_name);
				
				std::string n(property_name);
				std::pair<int, std::string> p;
				p.first = curr;
				p.second = n;

				addition.push_back(p);	
			}
			
			delete[] buf;
		}

		// Finally, insert the list into m
		std::pair<int, std::vector<std::pair<int, std::string>>> m_add;
		m_add.first = container_id;
		m_add.second = addition;
		m.insert(m_add);
	}

	std::vector<std::string> SGeometry_PropertyReader::headers(int cont_lookup)
	{
		std::vector<std::string> ret;

		std::vector<std::pair<int, std::string>> props(m.at(cont_lookup));
		
		// Remove IDs
		for(size_t itr = 0; itr < props.size(); itr++)
		{
			ret.push_back(props[itr].second);
		}

		return ret;
	}

	std::vector<int> SGeometry_PropertyReader::ids(int cont_lookup)
	{
		std::vector<int> ret;

		std::vector<std::pair<int, std::string>> props(m.at(cont_lookup));
		
		// Remove Headers 
		for(size_t itr = 0; itr < props.size(); itr++)
		{
			ret.push_back(props[itr].first);
		}

		return ret;
	}

	// Exception Class Implementations
	SG_Exception_Dim_MM::SG_Exception_Dim_MM(char* container_name, const char* field_1, const char* field_2)
	{
		const char * format = "%s: Dimensions of \"%s\" and \"%s\" must match but do not match.";
		// This may allocate a couple more than needed, potential improvement: optimize a couple of bytes later
		size_t s = strlen(field_1) + strlen(field_2) + strlen(format) + strlen(container_name) + 1;
		err_msg = new char[s];
		snprintf(err_msg, s, format, container_name, field_1, field_2);
	}

	SG_Exception_Existential::SG_Exception_Existential(char* container_name, const char* missing_name)
	{
		const char * format = "%s: The property or the variable associated with property \"%s\" is missing.";

		// This may allocate a couple more than needed, potential improvement: optimize a couple of bytes later
		size_t s = strlen(missing_name) + strlen(format) + strlen(container_name) + 1;
		err_msg = new char[s];
		snprintf(err_msg, s, format, container_name, missing_name);
	}

	SG_Exception_Dep::SG_Exception_Dep(char* container_name, const char* arg1, const char* arg2)
	{
		const char * format = "%s: The attribute \"%s\" may not exist without the attribute \"%s\" existing.";
		
		// This may allocate a couple more than needed, potential improvement: optimize a couple of bytes later
		size_t s = strlen(arg1) + strlen(arg2) + strlen(format) + strlen(container_name) + 1;
		err_msg = new char[s];
		snprintf(err_msg, s, format, container_name, arg1, arg2);
	}
	
	SG_Exception_BadSum::SG_Exception_BadSum(char* container_name, const char* arg1, const char* arg2)
	{
		const char * format = "%s: The sum of all values in \"%s\" and the sum of all values in \"%s\" do not match.";
		
		// This may allocate a couple more than needed, potential improvement: optimize a couple of bytes later
		size_t s = strlen(arg1) + strlen(arg2) + strlen(format) + strlen(container_name) + 1;
		err_msg = new char[s];
		snprintf(err_msg, s, format, container_name, arg1, arg2);
	}

	// to get past linker
	SG_Exception::~SG_Exception()
	{

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
			// Node Count not present? Assume that it is a multipoint.
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

	void* inPlaceSerialize_Point(SGeometry * ge, int seek_pos, void * serializeBegin)
	{
		uint8_t order = 1;
		uint32_t t = wkbPoint;

		serializeBegin = memcpy_jump(serializeBegin, &order, 1);
		serializeBegin = memcpy_jump(serializeBegin, &t, 4);

		// Now get point data;
		Point & p = (*ge)[seek_pos];
		double x = p[0];
		double y = p[1];
		serializeBegin = memcpy_jump(serializeBegin, &x, 8);
		serializeBegin = memcpy_jump(serializeBegin, &y, 8);
		return serializeBegin;
	}

	void* inPlaceSerialize_LineString(SGeometry * ge, int node_count, int seek_begin, void * serializeBegin)
	{
		uint8_t order = 1;
		uint32_t t = wkbLineString;
		uint32_t nc = (uint32_t) node_count;
		
		serializeBegin = memcpy_jump(serializeBegin, &order, 1);
		serializeBegin = memcpy_jump(serializeBegin, &t, 4);
		serializeBegin = memcpy_jump(serializeBegin, &nc, 4);

		// Now serialize points
		for(int ind = 0; ind < node_count; ind++)
		{
			Point & p = (*ge)[seek_begin + ind];
			double x = p[0];
			double y = p[1];
			serializeBegin = memcpy_jump(serializeBegin, &x, 8);
			serializeBegin = memcpy_jump(serializeBegin, &y, 8);	
		}

		return serializeBegin;
	}

	void* inPlaceSerialize_PolygonExtOnly(SGeometry * ge, int node_count, int seek_begin, void * serializeBegin)
	{	
		int8_t header = 1;
		int32_t t = wkbPolygon;
		int32_t rc = 1;
				
		void * writer = serializeBegin;
		writer = memcpy_jump(writer, &header, 1);
		writer = memcpy_jump(writer, &t, 4);
		writer = memcpy_jump(writer, &rc, 4);
						
		int32_t nc = (int32_t)node_count;
		writer = memcpy_jump(writer, &node_count, 4);

		for(int pind = 0; pind < nc; pind++)
		{
			Point & pt= (*ge)[seek_begin + pind];
			double x = pt[0]; double y = pt[1];
			writer = memcpy_jump(writer, &x, 8);
			writer = memcpy_jump(writer, &y, 8);
		}

		return writer;
	}

	void* inPlaceSerialize_Polygon(SGeometry * ge, std::vector<int>& pnc, int ring_count, int seek_begin, void * serializeBegin)
	{
			
		int8_t header = 1;
		int32_t t = wkbPolygon;
		int32_t rc = (int32_t)ring_count;
				
		void * writer = serializeBegin;
		writer = memcpy_jump(writer, &header, 1);
		writer = memcpy_jump(writer, &t, 4);
		writer = memcpy_jump(writer, &rc, 4);
		int cmoffset = 0;
						
		// Check for dimension error / touple order < 2 still to implement					
		for(int ring_c = 0; ring_c < ring_count; ring_c++)
		{
			int32_t node_count = pnc[ring_c];
			writer = memcpy_jump(writer, &node_count, 4);

			int pind = 0;
			for(pind = 0; pind < pnc[ring_c]; pind++)
			{
				Point & pt= (*ge)[seek_begin + cmoffset + pind];
				double x = pt[0]; double y = pt[1];
				writer = memcpy_jump(writer, &x, 8);
				writer = memcpy_jump(writer, &y, 8);
			}

			cmoffset += pind;
		}

		return writer;
	}

	int scanForGeometryContainers(int ncid, std::vector<int> & r_ids)
	{
		int nvars;
		if(nc_inq_nvars(ncid, &nvars) != NC_NOERR)
		{
			return -1;
		}

		r_ids.clear();

		// For each variable check for geometry attribute
		// If has geometry attribute, then check the associated variable ID

		for(int itr = 0; itr < nvars; itr++)
		{
			char c[NC_MAX_CHAR];
			memset(c, 0, NC_MAX_CHAR);
			if(nc_get_att_text(ncid, itr, CF_SG_GEOMETRY, c) != NC_NOERR)
			{
				continue;
			}

			int varID;
			if(nc_inq_varid(ncid, c, &varID) != NC_NOERR)
			{
				continue;
			}

			// Now have variable ID. See if vector contains it, and if not
			// insert
			bool contains = false;
			for(size_t itr_1 = 0; itr_1 < r_ids.size(); itr_1++)
			{
				if(r_ids[itr_1] == varID) contains = true;	
			}

			if(!contains)
			{
				r_ids.push_back(varID);
			}
		}	

		return 0 ;
	}

	SGeometry* getGeometryRef(int ncid, const char * varName )
	{
		int varId = 0;
		nc_inq_varid(ncid, varName, &varId);
		return new SGeometry(ncid, varId);
	}

}
