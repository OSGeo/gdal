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
			OGRLineString& r_defnGeometryLine = dynamic_cast<OGRLineString&>(r_defnGeometry);
			// to do: check for std::bad_cast somewhere?

			// Get node count
			this->total_point_count = r_defnGeometryLine.getNumPoints();
			
			// Single line: 1 part count == node count
			this->ppart_node_count.push_back(r_defnGeometryLine.getNumPoints());

			// One part
			this->total_part_count = 1;
			
			// One curve
			this->geometry_ref = ft.GetGeometryRef();
		}
	}

	OGRPoint& SGeometry_Feature::getPoint(size_t part_no, int point_index)
	{
		if (this->type == LINE)
		{
			OGRLineString* as_line_ref = dynamic_cast<OGRLineString*>(this->geometry_ref);
			as_line_ref->getPoint(point_index, &pt_buffer);
		}

		return pt_buffer;
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

		if (ncount_err_code != NC_NOERR)
		{
			// error
		}


		// Append from the end
		int ncount_add = static_cast<int>(ft.getTotalNodeCount());
		size_t ind[1] = { next_write_pos_node_count };
		err_code = nc_put_var1_int(ncID, varId, ind, &ncount_add);
		next_write_pos_node_count++;

		// Write each point from each part in node coordinates
		for(size_t part_no = 0; part_no < ft.getTotalPartCount(); part_no++)
		{
			for(size_t pt_ind = 0; pt_ind < ft.getTotalNodeCount(); pt_ind++)
			{
				int pt_ind_int = static_cast<int>(pt_ind);
				OGRPoint& write_pt = ft.getPoint(part_no, pt_ind_int);
			
				// Write each node coordinate
				double x = write_pt.getX();
				nc_put_var1_double(ncID, node_coordinates_varIDs[0], &next_write_pos_node_coord, &x);
				double y = write_pt.getY();
				nc_put_var1_double(ncID, node_coordinates_varIDs[1], &next_write_pos_node_coord, &y);
				if(this->node_coordinates_varIDs.size() > 2)
				{
					double z = write_pt.getZ();
					nc_put_var1_double(ncID, node_coordinates_varIDs[2], &next_write_pos_node_coord, &z);
				}

				// Step the position
				this->next_write_pos_node_coord++;		
			}
		}
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
		char container_name[NC_MAX_CHAR + 1];
		memset(container_name, 0, NC_MAX_CHAR + 1);

		// Prepare variable names
		char node_coord_names[NC_MAX_CHAR + 1];
		memset(node_coord_names, 0, NC_MAX_CHAR + 1);

		char node_count_name[NC_MAX_CHAR + 1];
		memset(node_count_name, 0, NC_MAX_CHAR + 1);

		int err_code;
		err_code = nc_get_att_text(ncID, containerVarID, CF_SG_NODE_COORDINATES, node_coord_names);
		err_code = nc_get_att_text(ncID, containerVarID, CF_SG_NODE_COUNT, node_count_name);
		err_code = nc_inq_varname(ncID, containerVarID, container_name);

		// Make dimensions for each of these
		err_code = nc_def_dim(ncID_in, node_count_name, 1, &node_count_dimID);
		err_code = nc_def_dim(ncID_in, container_name, 1, &node_coordinates_dimID);

		// Define variables for each of those
		err_code = nc_def_var(ncID_in, node_count_name, NC_INT, 1, &node_count_dimID, &node_count_varID);

		// Node coordinates
		int new_varID;
		char * pszNcoord = strtok(node_coord_names, " ");
		int count = 0;

		while (pszNcoord != nullptr)
		{
			// Define a new variable with that name
			err_code = nc_def_var(ncID, pszNcoord, NC_DOUBLE, 1, &node_coordinates_dimID, &new_varID);

			// to do: check for error

			// Add it to the coordinate varID list
			this->node_coordinates_varIDs.push_back(new_varID);

			// Add the mandatory "axis" attribute
			switch(count)
			{
				case 0:
					// first it's X
					nc_put_att_text(ncID, new_varID, CF_AXIS, strlen(CF_SG_X_AXIS), CF_SG_X_AXIS);
					break;
				case 1:
					// second it's Y
					nc_put_att_text(ncID, new_varID, CF_AXIS, strlen(CF_SG_Y_AXIS), CF_SG_Y_AXIS);
					break;
				case 2: 
					// third it's Z
					nc_put_att_text(ncID, new_varID, CF_AXIS, strlen(CF_SG_Z_AXIS), CF_SG_Z_AXIS);
					break;
			}	

			pszNcoord = strtok(nullptr, " ");
			count++;
		}
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
