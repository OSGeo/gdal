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

#include "gdal_priv.h"		// GDAL
#include "ogr_spatialref.h"
#include "cpl_string.h"

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
const char *nlat = "Northernmost_Latitude"; ///<
const char *slat = "Southernmost_Latitude"; ///<
const char *wlon = "Westernmost_Longitude"; ///<
const char *elon = "Easternmost_Longitude"; ///<
const char *gcs = "GeographicCS";	    ///<
const char *pcs = "ProjectionCS";	    ///<
const char *norm_proj_param = "Norm_Proj_Param"; ///<
const char *spatial_ref = "spatial_ref";    ///<
//@}

/** Register OPeNDAP' driver with the GDAL driver manager. This C function
    registers the OPeNDAP GDAL driver so that when the library is asked to
    open up a DODS data source, it can find the DODSDataset::Open function.

    @see DODSDataset::Open */
void 
GDALRegister_DODS()
{
    GDALDriver *poDriver;

    if( GDALGetDriverByName( "DODS" ) == NULL ) {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "DODS" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "DAP 3.x servers" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#DODS" );

        poDriver->pfnOpen = DODSDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

/** Find the variable in the DDS or DataDDS, given its name. This function
    first looks for the name as given. If that can't be found, it determines
    the leaf name of a fully qualified name and looks for that (the DAP
    supports searching for leaf names as a short cut). In this case the
    driver is using that feature because of an odd problem in the responses
    returned by some servers when they are asked for a single array variable
    from a Grid. Instead of returning GRID_NAME.ARRAY_NAME, they return just
    ARRAY_NAME. That's really a bug in the spec. However, it means that if a
    CE says GRID_NAME.ARRAY_NAME and the code looks only for that, it may not
    be found because the nesting has been removed and only an array called
    ARRAY_NAME returned.

    @param dds Look in this DDS object
    @param n Names the variable to find.
    @return A BaseType pointer to the object/variable in the DDS \e dds. */
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

    // Special check to see if we need to transpose x and y. We do [lon]
    // comes before [lat]. 
    if( strstr( filename.c_str(), "[lon]") != NULL 
        && strstr( strstr( filename.c_str(), "[lon]"), "[lat]" ) != NULL )
        d_bNeedTranspose = TRUE;
    else
        d_bNeedTranspose = FALSE;

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

/* -------------------------------------------------------------------- */
/*      Do we want to override the .dodsrc file setting?  Only do       */
/*      the putenv() if there isn't already a DODS_CONF in the          */
/*      environment.                                                    */
/* -------------------------------------------------------------------- */
    if( CPLGetConfigOption( "DODS_CONF", NULL ) != NULL 
        && getenv("DODS_CONF") == NULL )
    {
        static char szDODS_CONF[1000];
            
        sprintf( szDODS_CONF, "DODS_CONF=%.980s", 
                 CPLGetConfigOption( "DODS_CONF", "" ) );
        putenv( szDODS_CONF );
    }

/* -------------------------------------------------------------------- */
/*      If we have a overridding AIS file location, apply it now.       */
/* -------------------------------------------------------------------- */
    if( CPLGetConfigOption( "DODS_AIS_FILE", NULL ) != NULL )
    {
        string oAISFile = CPLGetConfigOption( "DODS_AIS_FILE", "" );
        RCReader::instance()->set_ais_database( oAISFile );
    }

    // can we get version information from it?
    // for now all we care about is some response... 10/31/03 jhrg
    AISConnect *poConnection = new AISConnect(d_oURL);
    string version = poConnection->request_version();
    if (version.empty() || version.find("/3.") == string::npos)
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "I connected to the URL but could not get a DAP 3.x version string\n"
                  "from the server.  I will continue to connect but access may fail.");
    }

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
	throw Error(
"The DODS GDAL driver only supports Array and Grid variables.");
    }

    // What is the rank of the Array/Grid?
    d_iVarRank = poA->dimensions();

    // Verify that the layer specification is valid. This not only makes sure
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
    well-known attributes to determine the latitude and longitude of the top,
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

    // Check for flipx/flipy requests.
    value = at->get_attr("FlipX");
    if( value == "" || value == "no" || value == "NO" )
        d_bFlipX = FALSE;
    else
        d_bFlipX = TRUE;
	
    value = at->get_attr("FlipY");
    if( value == "" || value == "no" || value == "NO" )
        d_bFlipY = FALSE;
    else
        d_bFlipY = TRUE;
	
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
    if (value != "" && value != "None")
    {
        DBG(cerr << pcs << ": " << value << endl);
        oSRS.SetProjCS(value.c_str());

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
    }

    // Set the GCS.
    value = at->get_attr(gcs);
    if (value == "" || value == "None")
	get_geo_ref_error(d_oVarName, gcs);
    DBG(cerr << gcs << ": " << value << endl);
    oSRS.SetWellKnownGeogCS(value.c_str());

    //Set a full coordinate system from "spatial_ref" in WKT or any user input.
    value = at->get_attr(spatial_ref);
    if( value != "" value != "None" )
    {
        DBG(cerr << spatial_ref << ": " << value << endl);
        
        oSRS.SetFromUserInput( value );
    }

    char *pszWKT = NULL;
    oSRS.exportToWkt(&pszWKT);

    d_oWKT = pszWKT;
    CPLFree(pszWKT);
}

/** Return the index of \e panBandMap of the last band which is contiguous
    with the group of bands that start with panBandMap[iStart]. Bands are
    contiguous if they are sequentially numbered and the same data type.

    @note The DAP can be used to read multiple bands if they are sequential.
    It \e is also possible to read non-sequential bands if they are evenly
    'spaced' but this implementation doesn't look for that (yet).

    @param iStart Start scanning the \e panBandMap array at this element
    (panBandMap uses zero-based indexing).
    @param nBandCount The number of elements in the \e panBandMap array.
    @param panBandMap  An array of ints which holds the band numbers.
    panBandMap[0] holds the index of the first band, ...,
    panBandMap[nBandCount-1] holds the index of the last band. 
    @return The index of the last band in the sequence of contiguous bands
    that start with panBandMap[iStart]. returns \e nBandCount-1
    if all the bands are sequential. */
int
DODSDataset::contiguous_bands(int iStart, int nBandCount, int *panBandMap) 
    throw(InternalErr)
{
    if (nBandCount < 1)
	throw InternalErr(__FILE__, __LINE__, "nBandCount < 1");
    if (!(iStart < nBandCount))
	throw InternalErr(__FILE__, __LINE__, "!(iStart < nBandCount)");

    // panBandMap uses zero-based indexing, GetRasterBand() uses one-based...
    int band = iStart;
    while (band+1 < nBandCount 
	   && panBandMap[band] == panBandMap[band+1]-1
	   && GetRasterBand(band+1)->GetRasterDataType() == GetRasterBand(band+2)->GetRasterDataType())
	band++;
	
    return band;
}

/** Build the constraint. Use the offset and size for the X/Y (Lon/Lat) plus
    the bands to build a constraint for the variable described in this
    instance of DODSDataset. 

    This assumes Band Numbers use ones-indexing. Also, note that DAP Array
    index constraints use the starting and ending index numbers and that DAP
    arrays use zero-based indexing. An X offset of 4 and a X size of 4
    produces a DAP dimension constraint of [4:7], the four elements 4, 5, 6,
    7.

    @return The constraint expression.
    @param iXOffset Start at this X element (zero-based indexing).
    @param iYOffset Start at this Y element (zero-based indexing).
    @param iXSize Extract this many elements on the X axis.
    @param iYSize Extract this many elements on the Y axis.
    @param iStartBandNum Starting band number (ones-based).
    @param iEndBandNum Ending band number (ones-based).
    @exception Error thrown if this method is called and the instance lacks
    information needed to build the constraint or any of the actual
    parameters are outside the bounds of the values for the variable in this
    instance. */
string
DODSDataset::build_constraint(int iXOffset, int iYOffset, 
			      int iXSize, int iYSize,
			      int iStartBandNum, int iEndBandNum) 
    throw(Error, InternalErr)
{
    // DAP indexing is zero-based, but Raster{X,Y}Size is the number of
    // elements, not the maximum index value.
    if (iXOffset + iXSize - 1 > nRasterXSize
	|| iYOffset + iYSize - 1 > nRasterYSize)
	throw Error(string("While processing a request for '")
		    + d_oVarName + "', band numbers "
		    + long_to_string(iStartBandNum) + " to "
		    + long_to_string(iEndBandNum)
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
	    // The index from the band spec is ones-based
	  case dim_spec::index: 
	    oss << "[" << i->start << "]";
	    break;
	    // The start and stop values from the band spec are ones-based
	  case dim_spec::range:
	    if (iEndBandNum > i->stop)
		throw Error(
"The ending band number is greater than the value given\n\
in the OPeNDAP URL.");

	    if (iStartBandNum == iEndBandNum)
		oss << "[" << i->start-1 + iStartBandNum-1 << "]";
	    else
		oss << "[" << i->start-1 + iStartBandNum-1 << ":" 
		    << i->start-1 + iEndBandNum-1  << "]";
	    break;

	  case dim_spec::unknown: 
	    throw InternalErr(__FILE__, __LINE__,
			      string("In the layer specification: ") 
			      + d_oBandExpr + " at least one of the\n\
bracket sub-expressions could not be parsed.");
	    break;
	}
    }

    CPLDebug( "DODS", "constraint = %s", oss.str().c_str() );

    return oss.str();
}

/** Read the raster associated with a specific band number into a buffer.
    This method reads an Array or Grid variable from an OPeNDAP server and
    dumps the bytes into the buffer <tt>pImage</tt>. It knows how to read
    just a portion of the image using the offset and size parameters.

    @note The caller must allocate sufficient memory to <tt>pImage</tt>
    before calling this method.

    @param iXOffset Use this as the starting index for the horizontal
    dimension. 
    @param iYOffset Use that as the starting index for the vertical
    dimension. 
    @param iXSize number of pixels to read.
    @param iYSize number of pixels to read.
    @param iStartBandNum Start access with this band. Uses one-based indexing. 
    @param iEndBandNum End access with this band number. Uses ones-based 
    indexing.
    @param pImage Dump the bytes here. 
    @see GetDatatype() to determine how many bytes each pixel will hold.
    <tt>pImage</tt> should reference <tt>iXSize</tt> * <tt>iYSize</tt> * the
    width of the type returned by GetDatatype(). */
void
DODSDataset::get_raster(int iXOffset, int iYOffset, int iXSize, int iYSize,
			int iStartBandNum, int iEndBandNum, void *pImage)
    throw(Error, InternalErr)
{
    // Grab the DataDDS
    AISConnect *poUrl = GetConnect();
    DataDDS data;
    CPLDebug( "DODS", "get_raster(%d,%d,%d,%d,bands=%d-%d)",
              iXOffset, iYOffset, 
              iXSize, iYSize,
              iStartBandNum, iEndBandNum );

/* -------------------------------------------------------------------- */
/*      If flipping is enabled, transform the request window now.       */
/* -------------------------------------------------------------------- */
    if( d_bFlipY )
        iYOffset = GetRasterYSize() - iYOffset - iYSize;

    if( d_bFlipX )
        iXOffset = GetRasterXSize() - iXOffset - iXSize;

              
/* -------------------------------------------------------------------- */
/*      Request the raw data with an appropriate constraint.            */
/* -------------------------------------------------------------------- */
    poUrl->request_data(data, build_constraint(iXOffset, iYOffset, 
					       iXSize, iYSize,
					       iStartBandNum, iEndBandNum));

    // Get the Array from it. We know there's only one var, et c., already.
    BaseType *poBt = get_variable(data, GetVarName());
    if (!poBt)
	throw Error(string("I could not read the variable '")
		    + GetVarName()
		    + string("' from the data source at:\n")
		    + GetUrl());
    Array *poA;
    switch (poBt->type()) {
      case dods_grid_c:
	poA = dynamic_cast<Array*>(dynamic_cast<Grid*>(poBt)->array_var());
	break;

      case dods_array_c:
	poA = dynamic_cast<Array *>(poBt);
	break;

      default:
	throw InternalErr("Expected an Array or Grid variable!");
    }


    poA->buf2val(&pImage);	// !Suck the data out of the Array!

/* -------------------------------------------------------------------- */
/*      Do we need to apply a transposition?  If so, do it now.         */
/* -------------------------------------------------------------------- */
    if( d_bNeedTranspose )
    {
        GByte *pabyDataCopy;
        int nItemSize = GDALGetDataTypeSize( d_eDatatype ) / 8;
        int iY;

        CPLAssert( iStartBandNum == iEndBandNum );

        CPLDebug( "DODS", "Applying transposition" );

        // make a copy of the original
        pabyDataCopy = (GByte *) CPLMalloc(nItemSize * iXSize * iYSize);
        memcpy( pabyDataCopy, pImage, nItemSize * iXSize * iYSize );
        memset( pImage, 0, nItemSize * iXSize * iYSize );

        for( iY = 0; iY < iYSize; iY++ )
        {
            GDALCopyWords( pabyDataCopy + iY * nItemSize, 
                           d_eDatatype, nItemSize * iYSize,
                           ((GByte *) pImage) + iY * iXSize * nItemSize, 
                           d_eDatatype, nItemSize, 
                           iXSize );
        }

        // cleanup
        CPLFree( pabyDataCopy );
    }

/* -------------------------------------------------------------------- */
/*      Do we need "y" flipping?                                        */
/* -------------------------------------------------------------------- */
    if( d_bFlipY )
    {
        GByte *pabyDataCopy;
        int nItemSize = GDALGetDataTypeSize( d_eDatatype ) / 8;
        int iY;

        CPLAssert( iStartBandNum == iEndBandNum );

        CPLDebug( "DODS", "Applying Y flip." );

        // make a copy of the original
        pabyDataCopy = (GByte *) CPLMalloc(nItemSize * iXSize * iYSize);
        memcpy( pabyDataCopy, pImage, nItemSize * iXSize * iYSize );
        memset( pImage, 0, nItemSize * iXSize * iYSize );

        for( iY = 0; iY < iYSize; iY++ )
        {
            int iYNew = GetRasterYSize() - iY - 1;

            GDALCopyWords( pabyDataCopy + iY * nItemSize * iXSize,
                           d_eDatatype, nItemSize,
                           ((GByte *) pImage) + iYNew * nItemSize * iXSize, 
                           d_eDatatype, nItemSize,
                           iXSize );
        }

        // cleanup
        CPLFree( pabyDataCopy );
    }

/* -------------------------------------------------------------------- */
/*      Do we need "x" flipping?                                        */
/* -------------------------------------------------------------------- */
    if( d_bFlipX )
    {
        GByte *pabyDataCopy;
        int nItemSize = GDALGetDataTypeSize( d_eDatatype ) / 8;
        int iY;

        CPLAssert( iStartBandNum == iEndBandNum );

        CPLDebug( "DODS", "Applying X flip." );

        // make a copy of the original
        pabyDataCopy = (GByte *) CPLMalloc(nItemSize * iXSize * iYSize);
        memcpy( pabyDataCopy, pImage, nItemSize * iXSize * iYSize );
        memset( pImage, 0, nItemSize * iXSize * iYSize );

        for( iY = 0; iY < iYSize; iY++ )
        {
            GByte *pabySrc, *pabyDst;

            pabySrc = pabyDataCopy + iY * nItemSize * iXSize;
            pabyDst = ((GByte *) pImage) + iY * nItemSize * iXSize
                + (iXSize-1) * nItemSize;

            GDALCopyWords( pabySrc, d_eDatatype, nItemSize, 
                           pabyDst, d_eDatatype, -nItemSize,
                           iXSize );
        }

        // cleanup
        CPLFree( pabyDataCopy );
    }

}


/** This private method uses build_constraint() and get_raster() to read 1 to N
    adjacent rasters from an OPeNDAP server. This implementation reads all of
    the bands using one network access. The DAP does provide a way to read
    server non-adjacent bands at once, but only if they are equally spaced.
    This implementation cannot recognize that yet... 

    Because the DAP does not support writing to remote data sources, this
    method returns an error if the caller asks it to write data.

    @note all of the bands \e must be of the same data type.

    @note This is implementation is based on Frank's version of the method
    from the source file gdalrasterband.cpp.

    @param eRWFlag Read/Write flag. Since the DAP does not support writing,
    using any value other than GF_Read is an error. 
    @param nXOff The pixel offset to the top left corner of the region of the
    band to be accessed. This would be zero to start from the left side.
    @param nYOff The line offset to the top left corner of the region of the
    band to be accessed. This would be zero to start from the top. 
    @param nXSize The width of the region of the band to be accessed in pixels.
    @param nYSize The height of the region of the band to be accessed in lines.
    @param eDataType What is the data type of the bands. The bands must all be
    of the same data type.
    @param pData  The buffer into which the data should be read, or from
    which it should be written. This buffer must contain at least nBufXSize *
    nBufYSize words of type eBufType. It is organized in left to right, top
    to bottom pixel order. Spacing is controlled by the nPixelSpace, and
    nLineSpace parameters. 
    @param nBufXSize The width of the buffer image into which the desired
    region is to be read, or from which it is to be written. 
    @param nBufYSize The height of the buffer image into which the desired
    region is to be read, or from which it is to be written. 
    @param eBufType The type of the pixel values in the pData data buffer.
    The pixel values will automatically be translated to/from the
    GDALRasterBand data type as needed. 
    @param iStartBandNum The starting band number.
    @param iEndBandNum The ending band number.
    @param nPixelSpace The byte offset from the start of one pixel value in
    pData to the start of the next pixel value within a scan line. If
    defaulted (0) the size of the data type eBufType is used. 
    @param nLineSpace The byte offset from the start of one scan line in pData
    to the start of the next. If defaulted the size of the data type eBufType
    * nBufXSize is used.

    @return Returns CE_Failure if the access fails, otherwise CE_None. */ 
void
DODSDataset::irasterio_helper(GDALRWFlag eRWFlag,
			      int nXOff, int nYOff, int nXSize, int nYSize,
			      GDALDataType eDataType,
			      void * pData, int nBufXSize, int nBufYSize,
			      GDALDataType eBufType,
			      int iStartBandNum, int iEndBandNum,
			      int nPixelSpace, int nLineSpace)
    throw(Error)
{
    CPLDebug( "DODS", "irasterio_helper(%d,%d,%d,%d)", 
              nXOff, nYOff, nXSize, nYSize );

    // NB: The parameters nPixelSpace and nLineSpace are for data that are
    // pixel- and line-interlaced. The DAP does not normally externalize data
    // that way, even if it is stored like that.

    int nBufDataSize = GDALGetDataTypeSize( eBufType ) / 8;
    if (nPixelSpace != nBufDataSize || nLineSpace != nPixelSpace * nBufXSize) {
	throw Error(
"nPixelSpace or nLineSpace are invalid for an OPeNDAP data\n\
source. These data sources should never provide pixel- or\n\
band-interleaved rasters. Reading data using values anything\n\
other than the defaults for these parameters is not supported\n\
by this driver.");
    }

    // If the nXOff and nXSize are too big, it's an error. Same for the Y
    // dimension.

    if (nXOff + nXSize > GetRasterXSize() 
	|| nYOff + nYSize > GetRasterYSize()) {
	throw Error(
"The values given for either the X or Y Size and/or Offset exceeded\n\
the raster size.");
    }

    // If the buffer and the raster are the same data type and the size of
    // the buffer matches the size of the request, read directly into the
    // buffer (pData).

    if (eDataType == eBufType
	&& nXSize == nBufXSize
	&& nYSize == nBufYSize) {
	DBG(cerr << "get_raster(" << nXOff << ", " << nYOff << ", " 
	    << nXSize << ", " << nYSize << ", " << nBand << "...)" 
	    << endl);
	get_raster(nXOff, nYOff, nXSize, nYSize,
		   iStartBandNum, iEndBandNum, pData);
	return;
    }

    // OK, what if the request and the buffer sizes are the same but the
    // types differ?

    int nBandDataSize = GDALGetDataTypeSize( eDataType ) / 8;
    if (nXSize == nBufXSize
	&& nYSize == nBufYSize) {
	char *pabLocal = new char[nBandDataSize * nXSize * nYSize];
	try {
	    get_raster(nXOff, nYOff, nXSize, nYSize, 
		       iStartBandNum, iEndBandNum,
		       pabLocal);

	    GDALCopyWords(static_cast<void*>(pabLocal), eDataType, 
			  nBandDataSize,
			  pData, eBufType, nBufDataSize,
			  nBufXSize * nBufYSize);
	    delete[] pabLocal;
	}
	catch (Error &e) {
	    delete[] pabLocal;
	    throw e;
	}

	return;
    }

    // This is the most general implementation. This is just about verbatim
    // from Frank's code.

    // Compute stepping increment.
    double dfSrcX, dfSrcY, dfSrcXInc, dfSrcYInc;
    
    dfSrcXInc = nXSize / (double) nBufXSize;
    dfSrcYInc = nYSize / (double) nBufYSize;
    DBG(cerr << "dfSrcXInc: " << dfSrcXInc << endl);
    DBG(cerr << "dfSrcYInc: " << dfSrcYInc << endl);

    // Read the data. OPTIMIZE ME! Use the Increments above to build a
    // constraint!. 01/16/04 jhrg
    char *pabLocal = new char[nBandDataSize * nXSize * nYSize];
    try {
	get_raster(nXOff, nYOff, nXSize, nYSize, iStartBandNum, iEndBandNum,
		   pabLocal);

	// Loop over buffer computing source locations.
	int  iSrcY, iBufYOff;
	for( iBufYOff = 0; iBufYOff < nBufYSize; iBufYOff++ ) {
	    dfSrcY = (iBufYOff+0.5) * dfSrcYInc;
	    iSrcY = (int) dfSrcY;
	    DBG2(cerr << "iSrcY: " << iSrcY << endl);

	    int iBufOffset = iBufYOff * nLineSpace;

	    int iSrcX, iBufXOff;
	    for( iBufXOff = 0; iBufXOff < nBufXSize; iBufXOff++ ) {
		dfSrcX = (iBufXOff+0.5) * dfSrcXInc;
            
		iSrcX = (int) dfSrcX;
		DBG2(cerr << "iSrcX: " << iSrcX << endl);
	    
		// Copy over this pixel of data.
		int iSrcOffset = (iSrcX + (iSrcY * nXSize)) * nBandDataSize;

		DBG(cerr << "iSrcOffset: " << iSrcOffset << endl);
		DBG(cerr << "iBufOffset: " << iBufOffset << endl);

		if( eDataType == eBufType ) {
		    memcpy( ((GByte *) pData) + iBufOffset,
			    pabLocal + iSrcOffset, nBandDataSize );
		}
		else {
		    /* type to type conversion ... ouch, this is an expensive
		       way of handling single words */
		    GDALCopyWords(pabLocal + iSrcOffset, eDataType, 
				  nBandDataSize,
				  ((GByte *) pData) + iBufOffset, 
				  eBufType, nBufDataSize,
				  1);
		}

		iBufOffset += nPixelSpace;
	    }
	}

	delete[] pabLocal;
    }
    catch (Error &e) {
	delete[] pabLocal;
	throw e;
    }

    return;
}

/** This method is the generic OPeNDAP driver's open routine. The
    GDALOpenInfo parameter contains the fully constrained URL for a OPeNDAP
    data source. Currently the constraint associated with this URL must list
    exactly one array variable in the dataset. The class creates a virtual
    connection to a OPeNDAP data source and reads that data source's
    (constrained) DDS.

    @param poOpenInfo A pointer to a GDALOpenInfo object. See the definition
    in gdal_priv.h. Currently the driver uses only the pszFilename, which
    holds a specially constrained URL for the dataset, and the eAccess field,
    which should be GA_ReadOnly (see gdal.h). @see The documentation that
    comes with the driver for information about OPeNDAP URLs, data sources
    and configuration of data sources for use with this driver. */
GDALDataset *
DODSDataset::Open(GDALOpenInfo *poOpenInfo)
{
    if( !EQUALN(poOpenInfo->pszFilename,"http://",7) 
        && !EQUALN(poOpenInfo->pszFilename,"https://",8) )
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

/** This specialization of GDALDataset::IRasterIO() reads rasters from an
    OPeNDAP server. This implementation detects when the caller is asking for
    several contiguous bands and groups the requests for those bands into a
    single OPeNDAP data request. Thus if the caller asks for N bands and they
    happen to be contiguous, this specialization will make only one remote
    access to the OPeNDAP server to get all of those bands. The method
    detects when panBandMap holds several groups of bands which are
    themselves contiguous, and makes the smallest number of network requests
    possible. The bands are stored in pData in the order they are listed in
    panBandMap; it's possible that reordering the bands would allow for
    further optimization, but that is the caller's responsibility.

    @note DODSRasterBand::IRasterIO() is implemented to call \e this method;
    the opposite of the default versions of the two methods.

    @note nBandSpace must be the default value of nLineSapce*nBufYSize.

    @todo Optimize to perform read-ahead when callers access data using
    scan line access mode.

    @see GDALDataset::IRasterIO() */
CPLErr 
DODSDataset::IRasterIO(GDALRWFlag eRWFlag,
		       int nXOff, int nYOff, int nXSize, int nYSize,
		       void * pData, int nBufXSize, int nBufYSize,
		       GDALDataType eBufType, 
		       int nBandCount, int *panBandMap,
		       int nPixelSpace, int nLineSpace, int nBandSpace)
{
    try {
        int bUseBlockOriented = FALSE;
        const char *pszConfig;

	if( nBandSpace != nLineSpace * nBufYSize && nBandCount > 1 )
            bUseBlockOriented = TRUE;

        if( nXSize == 1 || nYSize == 1 )
            bUseBlockOriented = TRUE;

        if( d_bNeedTranspose )
            bUseBlockOriented = TRUE;

        pszConfig = CPLGetConfigOption( "DODS_BLOCK_ORIENTED", NULL );
        if( pszConfig )
            bUseBlockOriented = CSLTestBoolean(pszConfig);

        // Just use the block oriented approach if the request has 
        // unusual interleaving, or if it seems small (and thus likely to 
        // be part of a long sequence of small requests). 
        if( bUseBlockOriented ) {
            return GDALDataset::IRasterIO( 
                eRWFlag, nXOff, nYOff, nXSize, nYSize, 
                pData, nBufXSize, nBufYSize, eBufType, 
                nBandCount, panBandMap, 
                nPixelSpace, nLineSpace, nBandSpace );
	}
	
	// This loop iterates over the bands in the panBandMap array using
	// irasterio_helper to read groups of contiguous bands. Each iteration
	// of the loop reads another group of bands. Each group is loaded
	// into pData one after the other. Thus requesting bands 1,2,3,5,6,7
	// would result in first bands 1..3 and then 5..7 being read from the
	// OPeNDAP server. Two accesses would be used (one for bands 1..3 and
	// one for bands 5..7). In pData the bands would be present in that
	// order.

	int iStart = 0;
	do {
	    // get the index of the last band in this contiguous bunch
	    int iEnd = contiguous_bands(iStart, nBandCount, panBandMap);

	    // point to the correct place
	    GByte *pabyData = static_cast<GByte *>(pData) + iStart*nBandSpace;

	    // get the data; throws on error
	    irasterio_helper(eRWFlag,
			     nXOff, nYOff, nXSize, nYSize,
			     GetRasterBand(panBandMap[iStart])->GetRasterDataType(),
			     static_cast<void*>(pabyData), nBufXSize, nBufYSize,
			     eBufType,
			     panBandMap[iStart], panBandMap[iEnd],
			     nPixelSpace, nLineSpace);

	    // increment iStart
	    iStart = iEnd+1;

	} while(iStart < nBandCount);

    }
    catch (Error &e) {
        CPLError(CE_Failure, CPLE_AppDefined, e.get_error_message().c_str());
        return CE_Failure;
    }

    return CE_None;
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

////////////////////////////////////////////////////////////////////////////
//
// DODSRasterBand implementation follows.

/** Build an instance for the give band

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

/** A specialization of the protected method GDALRasterBand::IRasterIO().
    This version knows how to read from OPeNDAP servers. It can efficiently
    read parts of rasters (Arrays and Grids) and can use the DAP's
    sub-sampling capabilities.

    Because the DAP does not support writing to remote data sources, this
    implementation of IRaserIO() returns an error if the caller asks it to
    write data.

    @note See GDALRasterBand::IRasterIO() or GDALRasterBand::RasterIO() for
    parameter definitions.

    @see GDALRasterBand::IRasterIO()
    @see GDALRasterBand::RasterIO()
    @return Returns CE_Failure if the access fails, otherwise CE_None. */ 
CPLErr 
DODSRasterBand::IRasterIO(GDALRWFlag eRWFlag,
			  int nXOff, int nYOff, int nXSize, int nYSize,
			  void *pData, int nBufXSize, int nBufYSize,
			  GDALDataType eBufType,
			  int nPixelSpace, int nLineSpace)
{
    DODSDataset *poDODS = dynamic_cast<DODSDataset *>(poDS);

    try {
        int bUseBlockOriented = FALSE;
        int nBufDataSize = GDALGetDataTypeSize( eBufType ) / 8;
        const char *pszConfig;

        if (nPixelSpace != nBufDataSize 
            || nLineSpace != nPixelSpace * nBufXSize)
            bUseBlockOriented = TRUE;

        if( nXSize == 1 || nYSize == 1 )
            bUseBlockOriented = TRUE;

        if( poDODS->d_bNeedTranspose )
            bUseBlockOriented = TRUE;

        pszConfig = CPLGetConfigOption( "DODS_BLOCK_ORIENTED", NULL );
        if( pszConfig )
            bUseBlockOriented = CSLTestBoolean(pszConfig);

        // Just use the block oriented approach if the request has 
        // unusual interleaving, or if it seems small (and thus likely to 
        // be part of a long sequence of small requests). 
        if( bUseBlockOriented ) {
            return GDALRasterBand::IRasterIO( 
                eRWFlag, nXOff, nYOff, nXSize, nYSize, 
                pData, nBufXSize, nBufYSize, eBufType, 
                nPixelSpace, nLineSpace );
	}
	
	poDODS->irasterio_helper(eRWFlag, nXOff, nYOff, nXSize, nYSize,
				 GetRasterDataType(),
				 pData, nBufXSize, nBufYSize, eBufType,
				 GetBand(), GetBand(),
				 nPixelSpace, nLineSpace);
    }
    catch (Error &e) {
        CPLError(CE_Failure, CPLE_AppDefined, e.get_error_message().c_str());
        return CE_Failure;
    }

    return CE_None;
}

/** This driver defines a Block to be the entire raster; this method reads
    the entire raster over in one shot. One way to use the sub setting
    features of the DAP would be to implement GDALRasterBand::IRasterIO().

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
DODSRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage)
{
    DODSDataset *poDODS = dynamic_cast<DODSDataset *>(poDS);

    CPLDebug( "DODS", "Read whole band as block." );

    try {
	// If the x or y block offsets are ever non-zero, something is wrong.
	if (nBlockXOff != 0 || nBlockYOff != 0)
	    throw InternalErr("Got a non-zero block offset!");

	poDODS->get_raster(0, 0, nBlockXSize, nBlockYSize, nBand, nBand, 
			   pImage);
    }
    catch (Error &e) {
        CPLError(CE_Failure, CPLE_AppDefined, e.get_error_message().c_str());
        return CE_Failure;
    }
    
    return CE_None;
}

// $Log$
// Revision 1.11  2004/09/08 15:33:55  warmerda
// added support for spatial_ref (WKT)
//
// Revision 1.10  2004/06/17 18:08:26  warmerda
// Added support for transposing and flipping grids if needed.
// Added support for falling back to "whole image at once" block based
// mechanism if interleaving request is odd or a directive says to.
//
// Revision 1.9  2004/03/24 18:36:56  warmerda
// Ensure that geographic coordiante systems are supported without requiring
// the projected coordinate system.
//
// Revision 1.8  2004/03/23 18:53:11  warmerda
// Treat a version string that is not apparently DAP 3.x as a warning.
// Don't worry about nBandSpace in IRasterIO() if there is only one
// band being operated on.
//
// Revision 1.7  2004/01/29 22:56:07  jimg
// Second major attempt to optimize the driver. This implementation provides a
// specialization of GDALDataset::IRasterIO() which recognizes when the caller
// has requests several band which are contiguous (or groups of bands which are
// themselves contiguous) and uses just one OPeNDAP request (or one per group)
// to get the data for those bands.
//
// Revision 1.6  2004/01/21 21:52:16  jimg
// Removed the unused method build_constraint(int). GDAL uses ones indexing
// for Bands while this driver was using zero-based indexing. I changed
// the code so the driver now also uses ones-based indexing for bands.
//
// Revision 1.5  2004/01/20 16:35:54  jimg
// This version of the OPeNDAP driver uses GDALRasterBand::IRasterIO() to read
// from the remote servers. Using this protected method, it is possible to read
// portions of an entire raster (the previous version used IReadBlock() which
// could read only the entire raster). Note that both IRasterIO() and
// IReadBlock() are specializations of the protected methods from in
// GDALRasterBand. A better version of this driver would move the
// implementation into the class DODSDataset so that it could read several
// bands with one remote access.
//
// Revision 1.4  2004/01/08 20:23:28  warmerda
// allow https datasets as well.
//
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
