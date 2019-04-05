#ifndef __NETCDFSG_H__
#define __NETCDFSG_H__
#include <cstring>
#include <vector>

// Interface used for netCDF functions
// implementing awareness for the CF-1.8 convention
//
// Author: wchen329
namespace nccfdriver
{
	// Enum used for easily identifying Geometry types
	enum geom_t
	{
		NONE,		// no geometry found
		POLYGON,	// OGRPolygon
		MULTIPOLYGON,	// OGRMultipolygon
		LINE,		// OGRLineString
		MULTILINE,	// OGRMultiLineString
		POINT,		// OGRPoint
		MULTIPOINT	// OGRMultiPoint
	};	

	// Concrete "Point" class, holds n dimensional double precision floating point value, defaults to all zero values
	class Point
	{
		int size;
		double * values;	
		Point(Point &);
		Point operator=(const Point &);

		public:
		~Point();	
		Point(int dim = 0) : size(dim)
			{ this->values = new double[dim]; } // memset(this->values, 0, dim*sizeof(double)); }	
		double& operator[](int i) { return this->values[i]; }
	};



	// Simple geometry - doesn't actually hold the points, rather serves
	// as a pseudo reference to a NC variable
	class SGeometry
	{
		geom_t type;	 	// internal geometry type structure
		int ncid;		// ncid - as used in netcdf.h
		int base_varId;		// var with geometry_container attribute
		int gc_varId;		// the name of the underlying geometry_container variable
		int touple_order;	// amount of "coordinates" in a point
		std::vector<int> nodec_varIds;	// varIds for each node_coordinate entry
		std::vector<int> node_counts;	// node counts of each geometry in a container
		std::vector<int> pnode_counts;	// part node counts of each geometry in a container
		std::vector<int> bound_list;	// a quick list used to store the real beginning indicies of shapes
		size_t current_vert_ind;	// used to keep track of current point being used
		size_t cur_geometry_ind;	// used to keep track of current geometry index
		size_t cur_part_ind;		// used to keep track of current part index
		bool interior;		// interior ring = true. only meaningful for polygons
		bool valid;		// true if geometry is valid, or false if construction failed, improve by using exception
		Point * pt_buffer;	// holds the current point
		SGeometry(SGeometry &);
		SGeometry operator=(const SGeometry &);

		public:

		/* Point* SGeometry::next_pt()
		 * returns a pointer to the next pt in sequence, if any. If none, returns a nullptr
		 * calling next_pt does not have additional space requirements
		 */
		Point* next_pt(); 
		bool has_next_pt(); // returns whether or not the geometry has another point
			
		/* void SGeometry::next_geometry()
		 * does not return anything. Rather, the SGeometry for which next_geometry() was called
		 * essentially gets replaced by the new geometry. So to access the next geometry,
		 * no additional space is required, just use the reference to the previous geometry, now
		 * pointing to the new geometry.
		 */
		void next_geometry(); // simply reuses the host structure
		bool has_next_geometry();

		/* void SGeometry::next_part()
		 * retrieves the next part of a single geometry (i.e. for multigeometries)
		 * similar to next geometry() but for multipart geometries
		 */
		void next_part();
		bool has_next_part();

		/* void SGeometry::get_geometry_count()
		 * returns a size, indicating the amount of geometries
		 * contained in the variable
		 */
		size_t get_geometry_count() { return this->node_counts.size(); }

		/* ncID - as used in netcdf.h
		 * baseVarId - the id of a variable with a geometry container attribute 
		 */
		SGeometry(int ncId, int baseVarId); 
		~SGeometry();
	};

	// Some helpers which simply call some netcdf library functions, unless otherwise mentioned, ncid, refers to its use in netcdf.h
	
	/* Retrieves the minor version from the value Conventions global attr
	 * Returns: a positive integer corresponding to the conventions value
	 *	if not CF-1.x then return negative value, -1
	 */
	int getCFMinorVersion(int ncid);

	/* Given a geometry_container varID, searches that variable for a geometry_type attribute
	 * Returns: the equivalent geometry type
	 */
	geom_t getGeometryType(int ncid, int varid); 

	/* Given a variable name, and the ncid, returns a SGeometry reference object, which acts more of an iterator of a geometry container
	 *
	 * Returns: a NEW geometry "reference" object, that can be used to quickly access information about the object, nullptr if invalid or an error occurs
	 */
	SGeometry* getGeometryRef(int ncid, const char * varName );	

	/* Writes a geometry to the specified NC file.
	 * The write uses the following guidelines
	 * - the geometry variable name is geometryX where X is some natural number
	 * - the geometry container is called geometry_containerX, corresponding to some geometryX
	 *
	 * Returns: an error code if failure, 0 on success
	 */
	int putGeometryRef(int ncid, SGeometry* geometry);

	/* Fetches a one dimensional string attribute
	 * using the given ncid, variable ID (varID), and attribute key (attrName)
	 *
	 * Returns: a NEW cstring of the attribute value, that must be separately deleted 
	 */
	char * attrf(int ncid, int varId, const char * attrName);
}

#endif
