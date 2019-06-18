#ifndef __NETCDFSGWRITERUTIL_H__
#define __NETCDFSGWRITERUTIL_H__
#include <vector>
#include "netcdfdataset.h"
#include "netcdfsg.h"

namespace nccfdriver
{
	const bool POLYGON_HAS_HOLES = true;
	const bool POLYGON_NO_HOLES = false;

	enum write_attr_role
	{
		W_NODE_COORDINATES,
		W_NODE_COUNT,
		W_PART_NODE_COUNT,
		W_INTERIOR_RING
	};

	// Functions that interface with netCDF, for writing

	/* int nc_int_vector_to_var(...)
	 * Writes a  std::vector<int> to a single dimensional variable
	 * Given the following args:
	 * int ncID - ncid as used in netcdf.h, group or file id
	 * int dimID - id of single dimension- all the data must fit the dimension length!
	 * std::string& name - what to name the variable
	 * std::vector<int>& data - the data of the variable
	 *
	 */
	int nc_int_vector_to_var(int ncID, int dimID, std::string& name, std::vector<int>& data);

	/* std::vector<..> writeGeometryContainer(...)
	 * Writes a geometry container of a given geometry type given the following arguments:
	 * int ncID - ncid as used in netcdf.h, group or file id
	 * std::string name - what to name this container
	 * geom_t geometry_type - the geometry type of the container
	 * std::vector<..> node_coordinate_names - variable names corresponding to each axis
	 * Only writes attributes that are for sure required. i.e. does NOT required interior ring for anything or part node count for Polygons
	 *
	 * Returns: geometry container variable ID
	 */
	int write_Geometry_Container
		(int ncID, std::string name, geom_t geometry_type, std::vector<std::string> & node_coordinate_names);

	/* Write X, Y axis information
	 * ncID - as used in the netCDF Library
	 * Xaxis_ID - the X axis variable
	 * Yaxis_ID - the Y axis variable
	 */
	void nc_write_x_y_CF_axis(int ncID, int Xaxis_ID, int Yaxis_ID);
}

#endif