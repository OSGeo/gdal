// -*- mode: c++; c-basic-offset: 4 -*-

// Copyright (c) 2003 OPeNDAP, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//
// Authors:
//          James Gallagher <jgallagher@opendap.org>
//
// You can contact OPeNDAP, Inc. at PO Box 112, Saunderstown, RI. 02874-0112.

#include <string>
#include <sstream>
#include <algorithm>
#include <exception>

// #define DODS_DEBUG 1
#include <debug.h>

#include <BaseType.h>		// DODS
#include <Byte.h>
#include <Int16.h>
#include <UInt16.h>
#include <Int32.h>
#include <UInt32.h>
#include <Float32.h>
#include <Float64.h>
#include <Str.h>
#include <Url.h>
#include <Array.h>
#include <Structure.h>
#include <Sequence.h>
#include <Grid.h>

#include <AISConnect.h>		
#include <DDS.h>
#include <DAS.h>
#include <Error.h>
#include <escaping.h>

#include <gdal_priv.h>		// GDAL
#include <gdal.h>
#include <ogr_spatialref.h>

CPL_CVSID("$Id$");

#include "dodsdataset.h"

static GDALDriver *poDODSDriver = NULL;

CPL_C_START
void GDALRegister_DODS(void);
CPL_C_END

/** Attribute names used to encode geo-referencing information. Note that
    these are not C++ objects to avoid problems with static global
    constructors. 

    @see get_geo_info

    @name Attribute names */
//@{
const char *nlat = "Northernmost_Latitude"; //<
const char *slat = "Southernmost_Latitude"; //<
const char *wlon = "Westernmost_Longitude"; //<
const char *elon = "Easternmost_Longitude"; //<
const char *gcs = "GeographicCS";	//<
const char *pcs = "ProjectionCS";	//<
const char *norm_proj_param = "Norm_Proj_Param"; //<
//@}

/** Register DODS' driver with the GDAL driver manager. This C function
    registers the DODS GDAL driver so that when the library is asked to open
    up a DODS data source, it can find the DODSDataset::Open function.

    @see DODSDataset::Open */
void 
GDALRegister_DODS()
{
    GDALDriver *poDriver;

    if( GDALGetDriverByName( "DODS" ) == NULL ) {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "DODS" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "DAP 3.x servers" );
#if 0
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#DODS" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "mem" );
#endif

        poDriver->pfnOpen = DODSDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

/** Find the variable in the DDS or DataDDS, given its name. This function
    first looks for the name as given. If that can't be found, it finds the
    leaf name of a fully qualified name and looks for that. The DAP supports
    searching for leaf names as a short cut. In this case we're using it
    because of an odd problem in the responses returned by some servers when
    they are asked for a single array variable from a Grid. Instead of
    returning GRID_NAME.ARRAY_NAME, they return just ARRAY_NAME. That's
    really a bug in the spec (we'll fix it soon). However, it means that if
    a CE says GRID_NAME.ARRAY_NAME and the code looks only for that, it may
    not be found because the nesting has been removed and only an array
    called ARRAY_NAME returned. 

    @param dds Look in this DDS object
    @param n Names the variable to find.
    @return A BaseType pointer to the object/variabel in the DDS \e dds. */
static BaseType *
get_variable(DDS &dds, const string &n)
{
    BaseType *poBT = dds.var(www2id(n));
    if (!poBT) {
	try {
	    string leaf = n.substr(n.find_last_of('.')+1);
	    poBT = dds.var(www2id(leaf));
	}
	catch (const std::exception &e) {
	    poBT = 0;
	}
    }

    return poBT;
}

/** Given the filename passed to Open(), parse the DAP server URL, variable
    name and band specification. Store the results in this instance. The
    format parsed is:
    
    <code>URL ? NAME BAND_SPEC</code>

    where the literal \c ? separates the URL and NAME and the opening
    bracket \c [ separates the NAME and BAND_SPEC.

    @param filename The pszFilename passed to Open() inside the GDALOpenInfo
    parameter. 
    @exception Error Thrown if any of the three pieces are missing. */
void
DODSDataset::parse_input(const string &filename) throw(Error)
{
    // look for the '?'
    string::size_type q_mark = filename.find('?');
    if (q_mark == string::npos)
	throw Error(
"Failed to find '?' delimiter in the DAP server/layer-specification.\n\
The specification given was: " + filename);

    d_oURL = filename.substr(0, q_mark);
    if (d_oURL.size() == 0)
	throw Error(
"Failed to find a DAP server URL in the DAP server/layer-specification.\n\
The specification given was: " + filename);

    string::size_type bracket = filename.find('[', q_mark+1);
    if (bracket == string::npos)
	throw Error(
"Failed to find '[' delimiter in the DAP server/layer-specification.\n\
The specification given was: " + filename);

    d_oVarName = filename.substr(q_mark+1, bracket-q_mark-1);
    if (d_oVarName.size() == 0)
	throw Error(
"Failed to find a variable name in the DAP server/layer-specification.\n\
The specification given was: " + filename);

    d_oBandExpr = filename.substr(bracket);
    if (d_oBandExpr.size() == 0)
	throw Error(
"Failed to find a Band Specification in the DAP server/layer-specification.\n\
The specification given was: " + filename);

    // Parse the band specification. The format is:
    // <dim spec><dim spec><dim spec>* where <dim spec> may be:
    // [<int>] or [<range>] or [lat] or [lon] and the last two are required.
    string::size_type beg = 0;
    string::size_type end = d_oBandExpr.find(']');
    while (beg != string::npos && end != string::npos) {
	dim_spec ods;
	ods.parse(d_oBandExpr.substr(beg, end-beg+1));
	d_oBandSpec.push_back(ods);

	// Advance to next
	beg = d_oBandExpr.find('[', end);
	end = d_oBandExpr.find(']', beg);
    }
}

/** Is the string in the d_oURL field a URL to a DAP 3 server? If so, return
    a valid AISConnect to the server. If it's not a valid DAP 3 server, throw
    an Error.

    @return A pointer to an instance of AISConnect.
    @exception Error Thrown if the URL parsed from the GDAL filename
    parameter (see parse_input()) is not a DAP 3 URL. */
AISConnect *
DODSDataset::connect_to_server() throw(Error)
{
    // does the string start with 'http?'
    if (d_oURL.find("http://") == string::npos
	&& d_oURL.find("https://") == string::npos)
	throw Error(
"The URL does not start with 'http' or 'https,' I won't try connecting.");

    // can we get version information from it?
    // for now all we care about is some response... 10/31/03 jhrg
    AISConnect *poConnection = new AISConnect(d_oURL);
    string version = poConnection->request_version();
    if (version.empty() || version.find("/3.") == string::npos)
	throw Error("I connected to the URL but could not get a DAP 3.x version string from the server");
    
    return poConnection;
}

/** Verify that the Layer Specification is valid. Without a valid layer
    specification we cannot access the data source. This method should be
    called once the layer specification has been parsed.

    @exception Error if the layer specification is no good. 
    @exception InternalErr if the layer specification has not been parsed. */
void
DODSDataset::verify_layer_spec() throw(Error, InternalErr)
{
    if (d_oBandSpec.size() == 0)
	throw InternalErr(__FILE__, __LINE__, "The Layer Specification has not been parsed but DODSDataset::verify_layer_spec() was called!");

    int lat_count = 0, lon_count = 0, index_count = 0, range_count = 0;
    vector<dim_spec>::iterator i;
    for(i = d_oBandSpec.begin(); i != d_oBandSpec.end(); ++i) {
	switch (i->type) {
	  case dim_spec::lat: ++lat_count; break;
	  case dim_spec::lon: ++lon_count; break;
	  case dim_spec::index: ++index_count; break;
	  case dim_spec::range: ++ range_count; break;
	  case dim_spec::unknown: 
	    throw Error(string("In the layer specification: ") 
			+ d_oBandExpr + " at least one of the\n\
bracket sub-expressions could not be parsed."); break;
	}
    }

    if (lat_count != 1)
	throw Error(string("Missing 'lat' in layer specification: ")
		    + d_oBandExpr);
	
    if (lon_count != 1)
	throw Error(string("Missing 'lon' in layer specification: ")
		    + d_oBandExpr);
    
    if (range_count > 1)
	throw Error(string("More than one range in layer specification: ")
		    + d_oBandExpr);
    
    if ((index_count + range_count + 2) != d_iVarRank)
	throw Error(string("Not all dimensions accounted for in '")
		    + d_oBandExpr
		    + string(",'\nGiven that the variable '") + d_oVarName 
		    + string("' has rank ") + long_to_string(d_iVarRank));
    
}

/** Record information about a variable. Sets the x/y (lon/lat) size of the
    raster (e.g., 512 x 512), its data type, ...

    This method requires that the d_oVarName and d_oBandSpec fields be set.
    It sets the d_iVarRank, d_oNumBands, nRasterYSize, nRasterXSize and
    d_eDatatype fields. It also calls verify_layer_spec() before using the
    layer specification to determine the X/Y (Lon/Lat) dimensions.

    @exception Error if anything goes wrong. */
void 
DODSDataset::get_var_info(DAS &das, DDS &dds) throw(Error)
{
    // get a pointer to the Array and Grid.
    BaseType *poBt = get_variable(dds, d_oVarName);
    if (!poBt)
	throw Error(string("The variable ") + d_oVarName 
		    + string(" could not be found in the data source."));
    Grid *poG = 0;
    Array *poA = 0;
    switch (poBt->type()) {
      case dods_grid_c:
	poG = dynamic_cast<Grid*>(poBt);
	poA = dynamic_cast<Array*>(poG->array_var());
	break;

      case dods_array_c:
	poA = dynamic_cast<Array *>(poBt);
	break;

      default:
	throw Error("The DODS GDAL driver only supports array variables.");
    }

    // What is the rank of the Array/Grid?
    d_iVarRank = poA->dimensions();

    // Verify that the layer specification is valid. This not only make sure
    // the layer spec matches the variable, it also simplifies processing for
    // the remaining code since it can assume the layer spec is valid. This
    // method throws Error if anything is wrong.
    verify_layer_spec();

    // Compute the size of the dimensions of the Array/Grid that correspond
    // to Lat and Lon. This uses the parsed layer specification.

    // First find the indexes of the lat and lon dimensions. Set
    // d_oNumBands.
    d_iNumBands = 1;
    vector<dim_spec>::iterator i;
    int index = 0, lat_index = -1, lon_index = -1;
    for (i = d_oBandSpec.begin(); i != d_oBandSpec.end(); ++i, ++index) {
	switch (i->type) {
	  case dim_spec::lat: lat_index = index; break;
	  case dim_spec::lon: lon_index = index; break;
	  case dim_spec::range: d_iNumBands = i->stop - i->start + 1; break;
	  default: break;
	}
    }

    // Use the Array to compute the sizes. X is Longitude, Y is Latitude
    Array::Dim_iter lon = poA->dim_begin() + lon_index;
    Array::Dim_iter lat = poA->dim_begin() + lat_index;

    nRasterYSize = poA->dimension_size(lat); // GDALDataset::nRasterYSize
    nRasterXSize = poA->dimension_size(lon);

    // Now grab the data type of the variable.
    switch (poA->var()->type()) {
      case dods_byte_c: d_eDatatype = GDT_Byte; break;
      case dods_int16_c: d_eDatatype = GDT_Int16; break;
      case dods_uint16_c: d_eDatatype = GDT_UInt16; break;
      case dods_int32_c: d_eDatatype = GDT_Int32; break;
      case dods_uint32_c: d_eDatatype = GDT_UInt32; break;
      case dods_float32_c: d_eDatatype = GDT_Float32; break;
      case dods_float64_c: d_eDatatype = GDT_Float64; break;
      default:
	throw Error("The DODS GDAL driver supports only numeric data types.");
    }
	
    DBG(cerr << "Finished recording values." << endl);
}

static inline void
get_geo_ref_error(const string &var_name, const string &param) throw(Error)
{
    throw Error(string("While reading geo-referencing information for '")
		+ var_name 
		+ "' the value for '" 
		+ param 
		+ "' was not found.");
}

/** Extract geo-referencing information from the layer. This uses a set of
    well-know attributes to determine the latitude and longitude of the top,
    bottom, left and right sides of the image. It also uses well-known
    attributes to determine the OGC/WKT string and projection name. If these
    attributes are missing from the dataset, they can be added using the
    DAP/AIS system.

    How attributes are found: The geo-location information for a particular
    variable must be held in attributes bound to that variable. Here a DAP
    variable corresponds to a GIS layer.

    The well-known attributes: 
        - "Northernmost_Latitude"
        - "Southernmost_Latitude"
        - "Westernmost_Longitude"
        - "Easternmost_Longitude"
	- "ProjectionCS"
        - "GeographicCS"
        - "Norm_Proj_Param"

    Note that the first four are often found in a MODIS Level 3 file (except
    that the underscore is a space); I'm not sure if they are part of the
    MODIS/HDF-EOS specification or just used by convention.

    @param das The DAS for this data source.
    @param dds The DDS for this data source.
    @exception Error if there's no way to read the geo-referencing
    information from the data source.

    @see DODSDataset::GetGeoTransform */

void
DODSDataset::get_geo_info(DAS &das, DDS &dds) throw(Error)
{
    // Get the variable/layer attribute container.
    AttrTable *at = das.find_container(d_oVarName);
    string value;

    if (!at || ((value = at->get_attr(nlat)) == "" || value == "None")) {
	// OK, lets check for the global 'opendap_gdal' container. Test for
	// this after looking for/in the variable's container because a
	// variable might have values that override the global values.
	// Note that we test 'at' for null because there are broken servers
	// out there; it should _never_ be null. 11/25/03 jhrg
	at = das.find_container("opendap_org_gdal");
	if (!at)
	    throw Error(string(
"Could not find the geo-referencing information for '") + d_oVarName + "'"
+ " and could not find default geo-referencing information in the '"
+ "opendap_gdal' attribute container.");
    }

    DBG2(das.print(stderr));
    DBG2(at->print(stderr));

    // Grab the lat/lon corner points
    value = at->get_attr(nlat);
    if (value == "" || value == "None")
	get_geo_ref_error(d_oVarName, nlat);
    d_dfURLat = strtod(value.c_str(), 0);

    value = at->get_attr(slat);
    if (value == "" || value == "None")
	get_geo_ref_error(d_oVarName, slat);
    d_dfLLLat = strtod(value.c_str(), 0);

    value = at->get_attr(elon);
    if (value == "" || value == "None")
	get_geo_ref_error(d_oVarName, elon);
    d_dfURLon = strtod(value.c_str(), 0);

    value = at->get_attr(wlon);
    if (value == "" || value == "None")
	get_geo_ref_error(d_oVarName, wlon);
    d_dfLLLon = strtod(value.c_str(), 0);

    // Now get the Geographic coordinate system, projection coordinate system
    // and normalized PCS parameters.
    OGRSpatialReference oSRS;
    d_oWKT = "";		// initialize in case this code fails...

    value = at->get_attr(pcs);
    if (value == "" || value == "None")
	get_geo_ref_error(d_oVarName, pcs);
    DBG(cerr << pcs << ": " << value << endl);
    oSRS.SetProjCS(value.c_str());

    value = at->get_attr(gcs);
    if (value == "" || value == "None")
	get_geo_ref_error(d_oVarName, gcs);
    DBG(cerr << gcs << ": " << value << endl);
    oSRS.SetWellKnownGeogCS(value.c_str());

    // Loop over params, if present.
    AttrTable *parm = at->find_container(norm_proj_param);
    if (parm) {
	DBG(cerr << "Found the Param attributes." << endl);
	AttrTable::Attr_iter i = parm->attr_begin();
	for (i = parm->attr_begin(); i != parm->attr_end(); ++i) {
	    DBG(cerr << "Attribute: " << parm->get_name(i)
		<< " = " << parm->get_attr(i) << endl);
	    oSRS.SetNormProjParm(parm->get_name(i).c_str(), 
				 strtod(parm->get_attr(i).c_str(), 0));
	}
    }

    char *pszWKT = NULL;
    oSRS.exportToWkt(&pszWKT);

    d_oWKT = pszWKT;
    CPLFree(pszWKT);
}

/** Build the constraint. Use the offset and size for the X/Y (Lon/Lat) plus
    the band number to build a constraint for the variable described in this
    instance of DODSDataset. 

    This assumes Band Numbers use zero-indexing. Also, note that DAP Array
    index constraints use the starting and ending \e index, so an X offset of
    4 and a X size of 4 produces a dimension constraint of [4:7], the four
    elements 4, 5, 6, and 7.

    @return The constraint expression.
    @param iXOffset Start at this X element
    @param iYOffset Start at this Y element
    @param iXSize Extract this many elements on the X axis
    @param iYSize Extract this many elements on the Y axis
    @param iBandNum Get this band (uses zero-based indexing).
    @exception Error thrown if this method is called and the instance lacks
    information needed to build the constraint or any of the actual
    parameters are outside the bounds of the values for the variable in this
    instance. */
string
DODSDataset::BuildConstraint(int iXOffset, int iYOffset, 
			     int iXSize, int iYSize,
			     int iBandNum) throw(Error, InternalErr)
{
    if (iXOffset + iXSize - 1 > nRasterXSize
	|| iYOffset + iYSize - 1 > nRasterYSize)
	throw Error(string("While processing a request for '")
		    + d_oVarName + "', band number "
		    + long_to_string(iBandNum)
	    + "The offset and/or size values exceed the size of the layer.");

    ostringstream oss;
    oss <<  d_oVarName;
    vector<dim_spec>::iterator i;
    for(i = d_oBandSpec.begin(); i != d_oBandSpec.end(); ++i) {
	switch (i->type) {
	  case dim_spec::lat: 
	    oss << "[" << iYOffset << ":" << iYSize+iYOffset-1 << "]";
	    break;
	  case dim_spec::lon:
	    oss << "[" << iXOffset << ":" << iXSize+iXOffset-1 << "]";
	    break;
	  case dim_spec::index: 
	    oss << "[" << i->start << "]";
	    break;
	  case dim_spec::range: 
	    oss << "[" << i->start + iBandNum << "]";
	    break;
	  case dim_spec::unknown: 
	    throw InternalErr(__FILE__, __LINE__,
			      string("In the layer specification: ") 
			      + d_oBandExpr + " at least one of the\n\
bracket sub-expressions could not be parsed."); break;
	}
    }

    return oss.str();
}

/** A simpler constraint builder. This is used by
    GDALRasterBand::IReadBlock() which reads the entire raster over in one
    shot, not matter how much the GDAL client actually needs. */
string
DODSDataset::BuildConstraint(int iBandNum) throw(Error, InternalErr)
{
    ostringstream oss;
    oss <<  d_oVarName;
    vector<dim_spec>::iterator i;
    for(i = d_oBandSpec.begin(); i != d_oBandSpec.end(); ++i) {
	switch (i->type) {
	  case dim_spec::lat: 
	    oss << "[" << 0 << ":" << nRasterYSize-1 << "]";
	    break;
	  case dim_spec::lon:
	    oss << "[" << 0 << ":" << nRasterXSize-1 << "]";
	    break;
	  case dim_spec::index: 
	    oss << "[" << i->start << "]";
	    break;
	  case dim_spec::range: 
	    oss << "[" << i->start + iBandNum << "]";
	    break;
	  case dim_spec::unknown: 
	    throw InternalErr(__FILE__, __LINE__,
			      string("In the layer specification: ") 
			      + d_oBandExpr + " at least one of the\n\
bracket sub-expressions could not be parsed."); break;
	}
    }

    return oss.str();
}

/** This method is the generic DODS driver's open routine. The GDALOpenInfo
    parameter contains the fully constrained URL for a DODS Data source.
    Currently the constraint associated with this URL must list exactly one
    array variable in the dataset. The class creates a virtual connection to
    a DODS dataset and reads that dataset's (constrained) DDS.

    To read in the data in a Grid, use the dot operator to access the Array
    component of the Grid. 

    @param poOpenInfo A pointer to a GDALOpenInfo object. See the definition
    in gdal_priv.h. Currently we use only the pszFilename, which holds the
    specially constrained URL for the dataset, and the eAccess field, which
    should be GA_ReadOnly (see gdal.h). */
GDALDataset *
DODSDataset::Open(GDALOpenInfo *poOpenInfo)
{
    if( !EQUALN(poOpenInfo->pszFilename,"http://",7) )
        return NULL;

    DBG(cerr << "Entering the DODS GDAL driver." << endl);

    DODSDataset *poDS = new DODSDataset();

    try {
	// Parse pszFilename from poOpenInfo.
	poDS->parse_input(string(poOpenInfo->pszFilename));

	// Get the AISConnect instance.
	poDS->d_poConnect = poDS->connect_to_server();

	DAS das;
	poDS->d_poConnect->request_das(das);
    
	DDS dds;
	poDS->d_poConnect->request_dds(dds);

	// Record the geo-referencing information.
	poDS->get_var_info(das, dds);
        try { 
            poDS->get_geo_info(das, dds);
        } catch( Error &ei ) {
            poDS->d_dfURLon = poDS->nRasterXSize;
            poDS->d_dfURLat = 0.0;
            poDS->d_dfLLLon = 0.0;
            poDS->d_dfLLLat = poDS->nRasterYSize;
        }
    }

    catch (Error &e) {
	string msg =
"An error occurred while creating a virtual connection to the DAP server:\n";
	msg += e.get_error_message();
        CPLError(CE_Failure, CPLE_AppDefined, msg.c_str());
	return 0;
    }

    // poDODSDriver is a file-scope variable init'd by GDAL_RegisterDODS.
    poDS->poDriver = poDODSDriver; 

    // d_iNumBands was set in get_var_info().
    for (int i = 0; i < poDS->d_iNumBands; i++) 
	poDS->SetBand(i+1, new DODSRasterBand(poDS, i+1));

    return poDS;
}

/** @see GDALDataset::GetGeoTransform() */
CPLErr 
DODSDataset::GetGeoTransform( double * padfTransform )
{
    padfTransform[0] = d_dfLLLon;
    padfTransform[3] = d_dfURLat;
    padfTransform[1] = (d_dfURLon - d_dfLLLon) / GetRasterXSize();
    padfTransform[2] = 0.0;
        
    padfTransform[4] = 0.0;
    padfTransform[5] = -1 * (d_dfURLat - d_dfLLLat) / GetRasterYSize();


    return CE_None;
}

/** @see GDALDataset::GetProjectionRef() */
const char *
DODSDataset::GetProjectionRef()
{
    return d_oWKT.c_str();
}

/** Build an instance for the gien band

    @param DS The DODSDataset instance for the collection of rasters
    @param band_num The band. Uses ones-based indexing. */
DODSRasterBand::DODSRasterBand(DODSDataset *DS, int band_num)
{
    poDS = DS;
    nBand = band_num;

    eDataType = dynamic_cast<DODSDataset*>(poDS)->GetDatatype();

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = poDS->GetRasterYSize();
}

/** We define a Block to be the entire raster; this method reads the entire
    raster over in one shot. One way to use the sub setting features of the
    DAP would be to implement GDALRasterBand::IRasterIO().

    This reads the data into pImage. If caching is turned on, then subsequent
    calls to this method for the same layer will be read from disk, not the
    network.
    
    @param nBlockXOff This must be zero for this driver
    @param nBlockYOff This must be zero for this driver
    @param pImage Dump the data here

    @return A CPLErr code. This implementation returns a CE_Failure if the
    block offsets are non-zero, if the raster is not actually a DODS/OPeNDAP
    Grid or Array variable or if the variable named in the DODSDataset object
    used to build the DODSRasterBand instance could not be found (see
    get_variable()). If successful, returns CE_None. */
CPLErr 
DODSRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff, void * pImage)
{
    DODSDataset *poDODS = dynamic_cast<DODSDataset *>(poDS);

    try {
	// If the x or y block offsets are ever non-zero, something is wrong.
	if (nBlockXOff != 0 || nBlockYOff != 0)
	    throw InternalErr("Got a non-zero block offset!");

	// Grab the DataDDS
	AISConnect *poUrl = poDODS->GetConnect();
	DataDDS data;
	poUrl->request_data(data, poDODS->BuildConstraint(nBand));

	// Get the Array from it. We know there's only one var, et c. already.
	BaseType *poBt = get_variable(data, poDODS->GetVarName());
	if (!poBt)
	    throw Error(string("I could not read the variable '")
			+ poDODS->GetVarName()
			+ string("' from the data source at:\n")
			+ poDODS->GetUrl());
	Array *poA;
	switch (poBt->type()) {
	  case dods_grid_c:
	    poA = dynamic_cast<Array*>(dynamic_cast<Grid*>(poBt)->array_var());
	    break;

	  case dods_array_c:
	    poA = dynamic_cast<Array *>(poBt);
	    break;

	  default:
	    throw InternalErr("Expected an Array variable!");
	}
	
	
	poA->buf2val(&pImage);	// !Suck the data out of the Array!
    }
    catch (Error &e) {
        CPLError(CE_Failure, CPLE_AppDefined, e.get_error_message().c_str());
        return CE_Failure;
    }
    
    return CE_None;
}

// $Log$
// Revision 1.3  2003/12/16 00:48:51  warmerda
// Don't attempt to open as DODS dataset unless it is prefixed by http://
//
// Revision 1.2  2003/12/16 00:34:40  warmerda
// Added call to get_geo_info, and a default case if it fails.
//
// Revision 1.1  2003/12/12 23:28:17  jimg
// Added.
//
// Revision 1.9  2003/12/12 22:52:20  jimg
// Added.
//
