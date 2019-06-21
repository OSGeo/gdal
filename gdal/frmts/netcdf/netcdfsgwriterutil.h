#ifndef __NETCDFSGWRITERUTIL_H__
#define __NETCDFSGWRITERUTIL_H__
#include <vector>
#include "ogr_core.h"
#include "ogrsf_frmts.h"
#include "netcdfsg.h"
#include "netcdflayersg.h"

namespace nccfdriver
{

	/* OGR_SGeometry_Feature
	 * Constructs over a OGRFeature
	 * gives some basic information about that SGeometry Feature such as
	 * Hold references... limited to scope of its references
	 * - what's its geometry type
	 * - how much total points it has
	 * - how many parts it has
	 * - a vector of counts of points for each part
	 */
	class SGeometry_Feature
	{
		bool hasInteriorRing;
		OGRGeometry * geometry_ref;
		geom_t type;
		size_t total_point_count;
		size_t total_part_count;
		std::vector<size_t> ppart_node_count;
		std::vector<bool> part_at_ind_interior; // for use with Multipolygons ONLY
		OGRPoint pt_buffer;

		public:
			geom_t getType() { return this->type; }
			size_t getTotalNodeCount() { return this->total_point_count; }
			size_t getTotalPartCount() { return this->total_part_count;  }
			std::vector<size_t> & getPerPartNodeCount() { return this->ppart_node_count; }
			OGRPoint& getPoint(size_t part_no, int point_index);
			SGeometry_Feature(OGRFeature&);
			bool getHasInteriorRing() { return this->hasInteriorRing; }
			bool IsPartAtIndInteriorRing(size_t ind) { return this->part_at_ind_interior[ind]; } // ONLY used for Multipolygon
	};

	/* OGR_SGeometry_Scribe
	 * Takes a SGeometry_Feature and given a target geometry container ID it will write the feature 
	 * to a given netCDF dataset in CF-1.8 compliant formatting.
	 * Any needed variables will automatically be defined, and dimensions will be automatically grown corresponding with need
	 *
	 */
	class OGR_SGeometry_Scribe
	{
		int ncID;
		int containerVarID;
		bool interiorRingDetected; // flips on when an interior ring polygon has been detected
		std::vector<int> node_coordinates_varIDs;// ids in X, Y (and then possibly Z) order
		int node_coordinates_dimID; // dim of all node_coordinates
		int node_count_dimID;	// node count dim
		int node_count_varID;
		int pnc_dimID;			// part node count dim AND interior ring dim
		int pnc_varID;
		int intring_varID;
		size_t next_write_pos_node_coord;
		size_t next_write_pos_node_count;
		size_t next_write_pos_pnc;

		public:
			void writeSGeometryFeature(SGeometry_Feature& ft);
			OGR_SGeometry_Scribe(int ncID, int containerVarID);
			OGR_SGeometry_Scribe();
			int get_node_count_dimID() { return this->node_count_dimID; }
			int get_node_coord_dimID() { return this->node_coordinates_dimID; }
			int get_pnc_dimID() { return this->pnc_dimID; }
			size_t get_next_write_pos_node_coord() { return this->next_write_pos_node_coord; }
			size_t get_next_write_pos_node_count() { return this->next_write_pos_node_count; }
			size_t get_next_write_pos_pnc() { return this->next_write_pos_pnc; }
			void redef_interior_ring(); // adds an interior ring attribute and to the target geometry container and corresponding variable 
			void redef_pnc(); // adds a part node count attribute to the target geometry container and corresponding variable
			void turnOnInteriorRingDetect() { this->interiorRingDetected = true; }
			bool getInteriorRingDetected() { return this->interiorRingDetected; }
	};

	// Namespace global, since strange VS2015 (specific!) heap corruption issue when declaring in netCDFLayer
	// will look at later
	extern OGR_SGeometry_Scribe GeometryScribe;

	// Exception Classes
	class SGWriter_Exception : public SG_Exception
	{
		public:
			const char * get_err_msg() override { return "A general error occurred when writing a netCDF dataset"; }
	};

	class SGWriter_Exception_NCWriteFailure : public SGWriter_Exception
	{
		std::string msg;

		public:
			const char * get_err_msg() override { return this->msg.c_str(); }
			SGWriter_Exception_NCWriteFailure(const char * layer_name, const char * failure_name,
				const char * failure_type);
	};

	class SGWriter_Exception_NCInqFailure : public SGWriter_Exception
	{
		std::string msg;

		public:
			const char * get_err_msg() override { return this->msg.c_str(); }
			SGWriter_Exception_NCInqFailure(const char * layer_name, const char * failure_name,
				const char * failure_type);
	};

	class SGWriter_Exception_NCDefFailure : public SGWriter_Exception
	{
		std::string msg;

		public:
			const char * get_err_msg() override { return this->msg.c_str(); }
			SGWriter_Exception_NCDefFailure(const char * layer_name, const char * failure_name,
				const char * failure_type);
	};

	class SGWriter_Exception_EmptyGeometry : public SGWriter_Exception
	{
		std::string msg;

		public:
			const char * get_err_msg() override { return this->msg.c_str(); } 
			SGWriter_Exception_EmptyGeometry()
			{
				msg = std::string("An empty geometry was detected when writing a netCDF file. ") +
					std::string("Empty geometries are not allowed.");
			}
	};

	class SGWriter_Exception_GeometryTMismatch : public SGWriter_Exception
	{
		std::string msg;

		public:
			const char * get_err_msg() override { return this->msg.c_str(); }
			SGWriter_Exception_GeometryTMismatch(const char * layer_type, const char * wrong_type);
	};

	// Functions that interface with netCDF, for writing

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
