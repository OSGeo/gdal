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

#ifndef _dodsdataset_h
#define _dodsdataset_h

#include <string>
#include <vector>

#include <AISConnect.h>

#include <gdal_priv.h>

vector<string> dods_extract_variable_names(const string &url);

class DODSRasterBand;

/** The GDAL Dataset class for DAP servers. Each instance of this class must
    be bound to a single array variable which can be read from a DAP server.
    The variable \e must be of at least rank two and exactly two of the
    dimensions \e must correspond to latitude and longitude. When the static
    method DODSDataset::Open() is called, it must be passed a string which
    names the DAP server using the pszFilename field of the GDALOpenInfo
    structure, the variable to access and a band specification. The syntax
    for this is:

    <code>
    <DAP server URL>?<variable name><band specification>
    </code>

    A band specification must contain two bracket expressions and may contain
    more. The first two indicate which dimensions of the Array correspond to
    latitude and longitude. If a third dimension is present it must either be
    constrained to a single index (slice) or used to indicate that there are
    multiple bands. In the latter case the bands will be numbered 1, 2, ...,
    N and are indicated using [0:N-1]. If there are more than three dimensions
    in the Array, those must be constrained to a single index. Here are some
    examples:

    <pre>
    z[lon][lat]
    u[0:10][lat][lon]
    v[7][lat][lon]
    t[0:11][3][lat]lon]
    </pre>

    @author James Gallagher */

class DODSDataset : public GDALDataset
{
private:
    /** This struct is used to parse the bracket expression which tells the
	driver how to interpret the raster's different dimensions. It is
	private to the DODSDataset class. Each of the bracketed
	sub-expressions has a corresponding instance of dim_spec. */
    struct dim_spec {
	/// What kind of sub-expr is this?
	enum { unknown, index, range, lat, lon } type;
	int start;		///< start band num, ones-based indexing
	int stop;		///< end band num, ones-based indexing

	dim_spec() : type(unknown), start(-1), stop(-1) {}

	/** parse a single bracket sub-expression. */
	void parse(const string &expr) {
	    // Remove the brackets...
	    string e = expr.substr(1, expr.length()-2);

	    // first test for 'lat' or 'lon'
	    if (e == "lat") {
		type = lat;
		DBG(cerr << "Found lat" << endl);
	    }
	    else if (e == "lon") {
		type = lon;
		DBG(cerr << "Found lon" << endl);
	    }
	    else if (e.find('-') != string::npos) {
		// A range expression?
		istringstream iss(e);
		iss >> start;
		char c;
		iss >> c;
		iss >> stop;
		if (!(iss.eof() && c == '-' && start > -1 && stop > start)) 
		    throw Error(string("Malformed range sub-expression: ") 
				+ e);
		type = range;
		DBG(cerr << "Found range: " << start << ", " << stop << endl);
	    }
	    else {
		istringstream iss(e);
		iss >> start;
		if (!(iss.eof() && start > -1)) 
		    throw Error(string("Malformed sub-expression: ") + e);
		type = index;
		DBG(cerr << "Found index: " << start << endl);
	    }
	}
    };

    AISConnect *d_poConnect; 	//< Virtual connection to the data source

    string d_oURL;		//< data source URL
    string d_oBandExpr;		//< band expression
    string d_oVarName;		//< variable

    GDALDataType d_eDatatype;	//< GDAL type for the variable
    int d_iVarRank;		//< Variable rank from DDS.
    vector<dim_spec> d_oBandSpec; //< The result of parsing the BandExpr
    int d_iNumBands;		//< Number of bands, from BandExpr
    int d_bNeedTranspose;       // do we need an x/y transpose?
    int d_bFlipX;    
    int d_bFlipY;              

    double d_dfLLLat, d_dfLLLon, d_dfURLat, d_dfURLon;
    string d_oWKT;		//< Constructed WKT string

    // Private methods
    /// Parse the filename
    void parse_input(const string &filename) throw(Error);
    /// Connect to the data source
    AISConnect *connect_to_server() throw(Error);
    /// Is the band expression valid
    void verify_layer_spec() throw(Error, InternalErr);
    /// Extract information about the variable
    void get_var_info(DAS &das, DDS &dds) throw(Error);
    /// Extract projection info and build WKT string
    void get_geo_info(DAS &das, DDS &dds) throw(Error);
    /// How many of the first bands are sequential?
    int contiguous_bands(int iStart, int nBandCount, int *panBandMap) 
	throw(InternalErr);

    /// Build a constraint for a DAP 3.x server.
    string build_constraint(int iXOffset, int iYOffset, int iXSize, int iYSize,
			   int iStartBandNum, int iEndBandNum) 
	throw(Error, InternalErr);

    /// Get raw rater data.
    void get_raster(int iXOffset, int iYOffset, int iXSize, int iYSize,
		   int iStartBandNum, int iEndBandNum, void *pImage) 
	throw(Error, InternalErr);

    /// Help function for the IRasterIO() method.
    void irasterio_helper(GDALRWFlag eRWFlag,
			 int nXOff, int nYOff, int nXSize, int nYSize,
			 GDALDataType eDataType,
			 void * pData, int nBufXSize, int nBufYSize,
			 GDALDataType eBufType,
			 int iStartBandNum, int iEndBandNum,
			 int nPixelSpace, int nLineSpace) throw(Error);

    // Simplify testing, make the unit tests a friend class.
    friend class DODSDatasetTest;
    friend class DODSRasterBand;

protected:
    /// Specialization; Read a raster.
    virtual CPLErr IRasterIO(GDALRWFlag eRWFlag,
			     int nXOff, int nYOff, int nXSize, int nYSize,
			     void * pData, int nBufXSize, int nBufYSize,
			     GDALDataType eBufType, 
			     int nBandCount, int *panBandMap,
			     int nPixelSpace, int nLineSpace, int nBandSpace);
    
public:
    /// Open is not a method in GDALDataset; it's the driver.
    static GDALDataset *Open(GDALOpenInfo *);

    /// Return the connection object
    AISConnect *GetConnect() { return d_poConnect; }

    /// How many bands are there in this variable?
    int GetNumBands() { return d_iNumBands; }
    /// Return the OPeNDAP data source variable name
    string GetVarName() { return d_oVarName; }
    /// Return the data source URL
    string GetUrl() { return d_oURL; }
    /// Return the GDAL data type for the variable
    GDALDataType GetDatatype() { return d_eDatatype; }

    /// Get the geographic transform info
    CPLErr GetGeoTransform(double *padfTransform);
    /// Return the OGC/WKT projection string
    const char *GetProjectionRef();
};

/** The GDAL RasterBand class for DODS.
    @author James Gallagher */
class DODSRasterBand : public GDALRasterBand {
private:
    string d_oCE;		// Holds the CE

    friend class DODSDatasetTest;
    friend class DODSDataset;

protected:
    /// Read a raster
    virtual CPLErr IRasterIO(GDALRWFlag eRWFlag,
			     int nXOff, int nYOff, int nXSize, int nYSize,
			     void * pData, int nBufXSize, int nBufYSize,
			     GDALDataType eBufType,
			     int nPixelSpace, int nLineSpace);
    /// Read the entire raster.
    virtual CPLErr IReadBlock(int, int, void *);

public:
    /// Build an instance
    DODSRasterBand(DODSDataset *, int);
};

// $Log$
// Revision 1.5  2004/06/17 18:08:26  warmerda
// Added support for transposing and flipping grids if needed.
// Added support for falling back to "whole image at once" block based
// mechanism if interleaving request is odd or a directive says to.
//
// Revision 1.4  2004/01/29 22:56:18  jimg
// Second major attempt to optimize the driver. This implementation provides a
// specialization of GDALDataset::IRasterIO() which recognizes when the caller
// has requests several band which are contiguous (or groups of bands which are
// themselves contiguous) and uses just one OPeNDAP request (or one per group)
// to get the data for those bands.
//
// Revision 1.3  2004/01/21 21:52:16  jimg
// Removed the unused method build_constraint(int). GDAL uses ones indexing
// for Bands while this driver was using zero-based indexing. I changed
// the code so the driver now also uses ones-based indexing for bands.
//
// Revision 1.2  2004/01/20 16:36:15  jimg
// This version of the OPeNDAP driver uses GDALRasterBand::IRasterIO() to
// read from the remote servers. Using this protected method, it is possible
// to read portions of an entire raster (the previous version used
// IReadBlock() which could read only the entire raster). Note that both
// IRasterIO() and IReadBlock() are specializations of the protected methods
// from in GDALRasterBand. A better version of this driver would move the
// implementation into the class DODSDataset so that it could read several
// bands with one remote access.
//
// Revision 1.1  2003/12/12 23:28:17  jimg
// Added.
//
// Revision 1.6  2003/12/12 22:52:20  jimg
// Added.
//

#endif // _dodsdataset_h

