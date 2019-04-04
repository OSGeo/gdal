#ifndef __NETCDFSG_H__
#define __NETCDFSG_H__
#include <cstring>

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
		Point(int dim) : size(dim) { this->values = new double[dim]; memset(this->values, 0, dim*sizeof(double)); }	
	};

	// Simple geometry - doesn't actually hold the points, rather serves
	// as a pseudo reference to a NC variable
	class SGeometry
	{
		geom_t type;	 // internal geometry type structure

		public:
		// might make the following two virtual
		Point next_pt(); // returns the next pt coordinate
		bool has_next_pt(); // returns whether or not the geometry has another point
	
		// also to do, add something to retrieve a "next" geometry
	};

	// Some helpers which simply call some netcdf library functions, unless otherwise mentioned, ncid, refers to its use in netcdf.h
	
	/* Retrieves the minor version from the value Conventions global attr
	 * Returns: a positive integer corresponding to the conventions value
	 *	if not CF-1.x then return negative value, -1
	 */
	int getCFMinorVersion(int ncid);

	/* Given a var name, searches that variable for a geometry attribute
	 * Returns: the equivalent geometry type
	 */
	geom_t getGeometryType(int ncid, const char * varName );

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
}

#endif
