#include "netcdfsgwriterutil.h"

namespace nccfdriver
{
	int nc_int_vector_to_var(int ncID, int dimID, std::string& name, std::vector<int>& data)
	{
		int nc_err_code = NC_NOERR;
		int write_var_id;

		// Define the variable / add it to the dataset
		
		nc_err_code = nc_def_var(ncID, name.c_str(), NC_INT, 1, &dimID, &write_var_id);

		if (nc_err_code != NC_NOERR)
		{
			return nc_err_code;
		}

		// Write each item to the variable
		for (size_t itr = 0; itr < data.size(); itr++)
		{
			nc_err_code = nc_put_var1_int(ncID, write_var_id, &itr, &data[itr]);
			if (nc_err_code != NC_NOERR)
				break;
		}
		return nc_err_code;
	}

	int write_Geometry_Container
		(int ncID, std::string name, geom_t geometry_type, std::vector<std::string> & node_coordinate_names)
	{

		int write_var_id;
		int err_code;
		
		// Define geometry container variable
		err_code = nc_def_var(ncID, name.c_str(), NC_FLOAT, 0, nullptr, &write_var_id);
		// todo: exception handling of err_code

		
		/* Geometry Type Attribute
		 * -
		 */

		// Next, go on to add attributes needed for each geometry type
		std::string geometry_str =
			(geometry_type == POINT || geometry_type == MULTIPOINT) ? CF_SG_TYPE_POINT :
			(geometry_type == LINE || geometry_type == MULTILINE) ? CF_SG_TYPE_LINE :
			(geometry_type == POLYGON || geometry_type == MULTIPOLYGON) ? CF_SG_TYPE_POLY :
			""; // obviously an error condition...

		// todo: error on "none"

		// Add the geometry type attribute
		err_code = nc_put_att_text(ncID, write_var_id, CF_SG_GEOMETRY_TYPE, geometry_str.size(), geometry_str.c_str());

		/* Node Coordinates Attribute
		 * -
		 */
		std::string ncoords_atr_str = "";

		for(size_t itr = 0; itr < node_coordinate_names.size(); itr++)
		{
			ncoords_atr_str += node_coordinate_names[itr];
			if (itr < node_coordinate_names.size() - 1)
			{
				ncoords_atr_str += " ";
			}
		}

		err_code = nc_put_att_text(ncID, write_var_id, CF_SG_NODE_COORDINATES, ncoords_atr_str.size(), ncoords_atr_str.c_str());

		// The previous two attributes are all that are required from POINT


		/* Node_Count Attribute
		 * (not needed for POINT)
		 */
		if (geometry_type != POINT)
		{
			std::string nodecount_atr_str = name + "_node_count";
			
			err_code = nc_put_att_text(ncID, write_var_id, CF_SG_NODE_COUNT, nodecount_atr_str.size(), nodecount_atr_str.c_str());
		}

		/* Part_Node_Count Attribute
		 * (only needed for MULTILINE, MULTIPOLYGON, and (potentially) POLYGON)
		 */
		if (geometry_type == MULTILINE || geometry_type == MULTIPOLYGON)
		{
			std::string pnc_atr_str = name + "_part_node_count";

			err_code = nc_put_att_text(ncID, write_var_id, CF_SG_PART_NODE_COUNT, pnc_atr_str.size(), pnc_atr_str.c_str());
		}

		return write_var_id;
	}

	void nc_write_x_y_CF_axis(int ncID, int Xaxis_ID, int Yaxis_ID)
	{
		int err;
		err = nc_put_att_text(ncID, Xaxis_ID, CF_AXIS, strlen(CF_SG_X_AXIS), CF_SG_X_AXIS);
		err = nc_put_att_text(ncID, Yaxis_ID, CF_AXIS, strlen(CF_SG_Y_AXIS), CF_SG_Y_AXIS);

		// to do: throw excepton
	}
}