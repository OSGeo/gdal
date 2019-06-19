#include "netcdfsgwriterutil.h"
#include "netcdfdataset.h"

namespace nccfdriver
{
	OGR_SGeometry_Scribe GeometryScribe;

	SGeometry_Feature::SGeometry_Feature(OGRFeature& ft)
	{
		OGRGeometry * defnGeometry = ft.GetGeometryRef();
		
		if (defnGeometry == nullptr)
		{
			// throw exception
		}
		
		OGRGeometry& r_defnGeometry = *defnGeometry;

		OGRwkbGeometryType ogwkt = defnGeometry->getGeometryType();
		this->type = OGRtoRaw(ogwkt);

		if (this->type == LINE)
		{		
			//OGRLineString r_defnGeometryLine = dynamic_cast<OGRLineString&>(r_defnGeometry);
			// to do: check for std::bad_cast somewhere?

			// Get node count
			//this->total_point_count = r_defnGeometryLine.getNumPoints();
			
			// Single line: 1 part count == node count
			//this->ppart_node_count.push_back(r_defnGeometryLine.getNumPoints());

			// One part
			this->total_part_count = 1;

			// One curve
			//this->curve_collection.push_back(r_defnGeometryLine);
		}
	}

	void OGR_SGeometry_Scribe::writeSGeometryFeature(SGeometry_Feature& ft)
	{
		if (ft.getType() == NONE)
			return;	// change to exception

		
		// Prepare variable names
		char node_coord_names[NC_MAX_CHAR + 1];
		memset(node_coord_names, 0, NC_MAX_CHAR + 1);

		char node_count_name[NC_MAX_CHAR + 1];
		memset(node_count_name, 0, NC_MAX_CHAR + 1);

		int err_code;
		err_code = nc_get_att_text(ncID, containerVarID, CF_SG_NODE_COORDINATES, node_coord_names);
		err_code = nc_get_att_text(ncID, containerVarID, CF_SG_NODE_COUNT, node_count_name);

		// Detect if variable already exists in dataset. If it doesn't, define it
		int varId;
		int ncount_err_code = nc_inq_varid(ncID, node_count_name, &varId);

		if (ncount_err_code == NC_ENOTVAR)
		{
			int err_code_def;
			err_code_def = nc_def_var(ncID, node_count_name, NC_INT, 1, &node_count_dimID, &varId);
		}

		else if (ncount_err_code != NC_NOERR)
		{
			// error
		}


		// Append from the end
		int ncount_add = static_cast<int>(ft.getTotalNodeCount());
		err_code = nc_put_var1_int(ncID, varId, &next_write_pos_node_count, &ncount_add);

	}

	OGR_SGeometry_Scribe::OGR_SGeometry_Scribe()
		: ncID(0),
		containerVarID(INVALID_VAR_ID),
		next_write_pos_node_coord(0),
		next_write_pos_node_count(0),
		next_write_pos_pnc(0)
	{

	}

	OGR_SGeometry_Scribe::OGR_SGeometry_Scribe(int ncID_in, int container_varID_in)
		: ncID(ncID_in),
		containerVarID(container_varID_in),
		next_write_pos_node_coord(0),
		next_write_pos_node_count(0),
		next_write_pos_pnc(0)
	{
		// Prepare variable names
		char node_coord_names[NC_MAX_CHAR + 1];
		memset(node_coord_names, 0, NC_MAX_CHAR + 1);

		char node_count_name[NC_MAX_CHAR + 1];
		memset(node_count_name, 0, NC_MAX_CHAR + 1);

		int err_code;
		err_code = nc_get_att_text(ncID, containerVarID, CF_SG_NODE_COORDINATES, node_coord_names);
		err_code = nc_get_att_text(ncID, containerVarID, CF_SG_NODE_COUNT, node_count_name);

		// Make dimensions for each of these
		err_code = nc_def_dim(ncID_in, node_count_name, 0, &node_count_dimID);
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