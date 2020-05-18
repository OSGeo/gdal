/******************************************************************************
 *
 * Project:  netCDF read/write Driver
 * Purpose:  GDAL bindings over netCDF library.
 * Author:   Winor Chen <wchen329 at wisc.edu>
 *
 ******************************************************************************
 * Copyright (c) 2019, Winor Chen <wchen329 at wisc.edu>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/
#ifndef __NETCDFSG_H__
#define __NETCDFSG_H__
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "netcdf.h"

// Interface used for netCDF functions
// implementing awareness for the CF-1.8 convention
//
// Author: wchen329
namespace nccfdriver
{    
    // Constants
    const int INVALID_VAR_ID = -2;    
    const int INVALID_DIM_ID = INVALID_VAR_ID;

    // Enum used for easily identifying Geometry types
    enum geom_t
    {
        NONE,        // no geometry found
        POLYGON,    // OGRPolygon
        MULTIPOLYGON,    // OGRMultipolygon
        LINE,        // OGRLineString
        MULTILINE,    // OGRMultiLineString
        POINT,        // OGRPoint
        MULTIPOINT,    // OGRMultiPoint
        UNSUPPORTED    // Unsupported feature type    
    };    

    // Concrete "Point" class, holds n dimensional double precision floating point value, defaults to all zero values
    class Point
    {
        int size;
        std::unique_ptr<double, std::default_delete<double[]>> values;
        Point(Point&);
        Point operator=(const Point &);

        public:
        explicit Point(int dim) : size(dim), values(std::unique_ptr<double, std::default_delete<double[]>>(new double[dim])) {}
        double& operator[](size_t i) { return this->values.get()[i]; }
        int getOrder() { return this->size; }
    };



    // Simple geometry - doesn't actually hold the points, rather serves
    // as a pseudo reference to a NC variable
    class SGeometry_Reader
    {
        std::string container_name_s;        // name of the underlying geometry container
        geom_t type;         // internal geometry type structure
        int ncid;        // ncid - as used in netcdf.h
        int gc_varId;        // the id of the underlying geometry_container variable
        std::string gm_name_s; // grid mapping variable name
        int gm_varId;        // id used for grid mapping
        int inst_dimId;        // dimension id for geometry instance dimension
        size_t inst_dimLen;    // value of instance dimension
        int touple_order;    // amount of "coordinates" in a point
        std::vector<int> nodec_varIds;    // varIds for each node_coordinate entry
        std::vector<int> node_counts;    // node counts of each geometry in a container
        std::vector<int> pnode_counts;    // part node counts of each geometry in a container
        std::vector<bool> int_rings;    // list of parts that are interior rings
        std::vector<size_t> bound_list;    // a quick list used to store the real beginning indices of shapes
        std::vector<size_t> pnc_bl;    // a quick list of indices for part counts corresponding to a geometry
        std::vector<int> parts_count;    // a count of total parts in a single geometry instance
        std::vector<int> poly_count;    // count of polygons, for use only when interior rings are present
        std::unique_ptr<Point> pt_buffer;    // holds the current point
        SGeometry_Reader(SGeometry_Reader&);
        SGeometry_Reader operator=(const SGeometry_Reader&);

        public:

        /* int SGeometry_Reader::get_ncID()
         * return the group/file ID that the SGeometry object is operating over
         */
        int get_ncID() { return ncid; }

        /* int SGeometry_Reader::get_axisCount()
         * Returns the count of axis (i.e. X, Y, Z)
         */
        int get_axisCount() { return this->touple_order; }

        /* int SGeometry_Reader::getInstDim()
         * Returns the geometry instance dimension ID of this geometry
         */
        int getInstDim() { return this->inst_dimId; }

        /* size_t SGeometry_Reader::getInstDimLen()
         * Returns the length of the instance dimension
         */
        size_t getInstDimLen() { return this->inst_dimLen; }

        /* std::string& getGridMappingName()
         * returns the variable name which holds grid mapping data
         */
        std::string& getGridMappingName() { return this->gm_name_s; }

        /* int SGeometry_Reader::getGridMappingVarID();
         * returns the varID of the associated grid mapping variable ID
         */    
        int getGridMappingVarID() { return this->gm_varId; }

        /* geom_t getGeometryType()
         * Retrieves the associated geometry type with this geometry
         */
        geom_t getGeometryType() { return this->type; }

        /* void SGeometry_Reader::get_geometry_count()
         * returns a size, indicating the amount of geometries
         * contained in the variable
         */
        size_t get_geometry_count();

        /* const char* SGeometry_Reader::getContainerName()
         * Returns the container name as a string
         */
        std::string& getContainerName() { return container_name_s; }

        /* int SGeometry_Reader::getContainerId()
         * Get the ncID of the geometry_container variable
         */
        int getContainerId() { return gc_varId; }

        /* std::vector<unsigned char> serializeToWKB
         * Returns a pre-allocated array which serves as the WKB reference to this geometry
         */
        std::vector<unsigned char> serializeToWKB(size_t featureInd);

        /* Return a point at a specific index specifically
         * this point should NOT be explicitly freed.
         *
         */
        Point& operator[](size_t ind);

        /* std::vector<int>& getNodeCoordVars
         * Returns a vector with the node coord vars in X, Y, Z (if present) order
         */
        std::vector<int>& getNodeCoordVars() { return this->nodec_varIds; }

        /* ncID - as used in netcdf.h
         * baseVarId - the id of a variable with a geometry container attribute 
         */
        SGeometry_Reader(int ncId, int baseVarId); 

    };

    /* SGeometry_PropertyScanner
     * Holds names of properties for geometry containers
     * Pass in the geometry_container ID, automatically scans the netcdf Dataset for properties associated
     *
     * to construct: pass in the ncid which the reader should work over
     */
    class SGeometry_PropertyScanner
    {
        std::vector<int> v_ids;
        std::vector<std::string> v_headers;
        int nc;
        
        void open(int container_id);    // opens and initializes a geometry_container into the scanner 
        
        public:
            std::vector<std::string>& headers() { return this->v_headers; }
            std::vector<int>& ids() { return this->v_ids; }
            SGeometry_PropertyScanner(int ncid, int cid) : nc(ncid) { this->open(cid); }
    };

    // General exception interface for Simple Geometries
    // Whatever pointer returned should NOT be freed- it will be deconstructed automatically, if needed
    class SG_Exception
    {
        public:
            virtual const char * get_err_msg() = 0;    
            virtual ~SG_Exception();
    };

    // Mismatched dimension exception
    class SG_Exception_Dim_MM : public SG_Exception
    {
        std::string err_msg;

        public:
            const char* get_err_msg() override { return err_msg.c_str(); }

        SG_Exception_Dim_MM(const char* geometry_container, const char* field_1, const char *field_2);
    };

    // Missing (existential) property error
    class SG_Exception_Existential: public SG_Exception
    {
        std::string err_msg;

        public:
            const char* get_err_msg() override { return err_msg.c_str(); }
        
        SG_Exception_Existential(const char* geometry_container, const char* missing_name);
    };

    // Missing dependent property (arg_1 is dependent on arg_2)
    class SG_Exception_Dep: public SG_Exception
    {
        std::string err_msg;

        public:
            const char* get_err_msg() override { return err_msg.c_str(); }
        
        SG_Exception_Dep(const char* geometry_container, const char* arg_1, const char* arg_2);
    };

    // The sum of all values in a variable does not match the sum of another variable
    class SG_Exception_BadSum : public SG_Exception
    {
        std::string err_msg;

        public:
            const char* get_err_msg() override { return err_msg.c_str(); }
        
        SG_Exception_BadSum(const char* geometry_container, const char* arg_1, const char* arg_2);
    };

    // Unsupported Feature Type
    class SG_Exception_BadFeature : public SG_Exception
    {
        std::string err_msg;

        public:
            const char* get_err_msg() override { return err_msg.c_str(); }
        
        SG_Exception_BadFeature() : err_msg("Unsupported or unrecognized feature type.") {}
    };
    
    // Failed Read
    class SG_Exception_BadPoint : public SG_Exception
    {
        std::string err_msg;

        public:
            const char* get_err_msg() override { return err_msg.c_str(); }
        
        SG_Exception_BadPoint() : err_msg("An attempt was made to read an invalid point (likely index out of bounds).") {}
    };

    // Too many dimensions on node coordinates variable
    class SG_Exception_Not1D : public SG_Exception
    {
        std::string err_msg;

        public:
            const char* get_err_msg() override { return err_msg.c_str(); }
        
        SG_Exception_Not1D() : err_msg("A node coordinates axis variable or node_counts is not one dimensional.") {}
    };

    // Too many empty dimension
    class SG_Exception_EmptyDim : public SG_Exception
    {
        std::string err_msg;

        public:
            const char* get_err_msg() override { return err_msg.c_str(); }
        
        SG_Exception_EmptyDim() : err_msg("A dimension has length <= 0, but it must have length > 0") {}
    };

    //general corruption or malformed error
    class SG_Exception_General_Malformed : public SG_Exception
    {
        std::string err_msg;

        public:
            const char* get_err_msg() override { return err_msg.c_str(); }
        
        explicit SG_Exception_General_Malformed(const char*); 
    };

    // Invalid value detected
    class SG_Exception_Value_Violation : public SG_Exception
    {
        std::string err_msg;

        public:
            const char* get_err_msg() override { return err_msg.c_str(); }
            SG_Exception_Value_Violation(const char* containername, const char* type, const char* badvalue) :
                err_msg( std::string("[") + std::string(containername) + std::string("] ") + std::string(type) +
                         std::string(" values may not be ") + std::string(badvalue)
                       ) {}
    };

    // Required value(s)
    class SG_Exception_Value_Required : public SG_Exception
    {
        std::string err_msg;

        public:
            const char* get_err_msg() override { return err_msg.c_str(); }
            SG_Exception_Value_Required(const char* containername, const char* type, const char* expvalue) :
                err_msg( std::string("[") + std::string(containername) + std::string("] ") + std::string(type) +
                         std::string(" values must be ") + std::string(expvalue)
                       ) {}
    };

    // Some helpers which simply call some netcdf library functions, unless otherwise mentioned, ncid, refers to its use in netcdf.h
    
    /* Retrieves the version from the value Conventions global attr
     * Returns: a double precision decimal corresponding to the conventions value
     *    if not CF-x.y then return negative value, -1
     */
    double getCFVersion(int ncid);

    /* Given a geometry_container varID, searches that variable for a geometry_type attribute
     * Returns: the equivalent geometry type
     */
    geom_t getGeometryType(int ncid, int varid); 
    
    void inPlaceSerialize_Point(SGeometry_Reader * ge, size_t seek_pos, std::vector<unsigned char>& buffer);
    void inPlaceSerialize_LineString(SGeometry_Reader * ge, int node_count, size_t seek_begin, std::vector<unsigned char>& buffer);
    void inPlaceSerialize_PolygonExtOnly(SGeometry_Reader * ge, int node_count, size_t seek_begin, std::vector<unsigned char>& buffer);
    void inPlaceSerialize_Polygon(SGeometry_Reader * ge, std::vector<int>& pnc, int ring_count, size_t seek_begin, std::vector<unsigned char>& buffer);

    /* scanForGeometryContainers
     * A simple function that scans a netCDF File for Geometry Containers
     * -
     * Scans the given ncid for geometry containers
     * The vector passed in will be overwritten with a vector of scan results
     */
    int scanForGeometryContainers(int ncid, std::set<int> & r_ids);

    /* Attribute Fetch
     * -
     * A function which makes it a bit easier to fetch single text attribute values
     * ncid: as used in netcdf.h
     * varID: variable id in which to look for the attribute
     * attrName: name of attribute to fine
     * alloc: a reference to a string that will be filled with the attribute (i.e. truncated and filled with the return value)
     * Returns: a reference to the string to fill (a.k.a. string pointed to by alloc reference)
     */
    std::string& attrf(int ncid, int varId, const char * attrName, std::string & alloc);
}

#endif
