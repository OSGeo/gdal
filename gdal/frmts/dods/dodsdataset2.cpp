/******************************************************************************
 * $Id$
 *
 * Project:  OPeNDAP Raster Driver
 * Purpose:  Implements DODSDataset and DODSRasterBand classes.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2003 OPeNDAP, Inc.
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

#ifdef LIBDAP_310
/* AISConnect.h/AISConnect class was renamed to Connect.h/Connect in libdap 3.10 */
#include <Connect.h>
#define AISConnect Connect
#else
#include <AISConnect.h>
#endif

#include <DDS.h>
#include <DAS.h>
#include <BaseTypeFactory.h>
#include <Error.h>
#include <escaping.h>

#include "gdal_priv.h"		// GDAL
#include "ogr_spatialref.h"
#include "cpl_string.h"

using namespace libdap;

CPL_CVSID("$Id$");

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

/************************************************************************/
/*                            get_variable()                            */
/************************************************************************/

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

/************************************************************************/
/*                            StripQuotes()                             */
/*                                                                      */
/*      Strip the quotes off a string value and remove internal         */
/*      quote escaping.                                                 */
/************************************************************************/

static string StripQuotes( string oInput )

{
    char *pszResult;

    if( oInput.length() < 2 )
        return oInput;

    oInput = oInput.substr(1,oInput.length()-2);

    pszResult = CPLUnescapeString( oInput.c_str(), NULL, 
                                   CPLES_BackslashQuotable );

    oInput = pszResult;
    
    CPLFree( pszResult );

    return oInput;
}

/************************************************************************/
/*                            GetDimension()                            */
/*                                                                      */
/*      Get the dimension for the named constrain dimension, -1 is      */
/*      returned if not found.  We would pass in a CE like              */
/*      "[band][x][y]" or "[1][x][y]" and a dimension name like "y"     */
/*      and get back the dimension index (2 if it is the 3rd            */
/*      dimension).                                                     */
/*                                                                      */
/*      eg. GetDimension("[1][y][x]","y") -> 1                          */
/************************************************************************/

static int GetDimension( string oCE, const char *pszDimName,
                         int *pnDirection )

{
    int iCount = 0, i;
    const char *pszCE = oCE.c_str();

    if( pnDirection != NULL )
        *pnDirection = 1;

    for( i = 0; pszCE[i] != '\0'; i++ )
    {
        if( pszCE[i] == '[' && pszCE[i+1] == pszDimName[0] )
            return iCount;

        else if( pszCE[i] == '[' 
                 && pszCE[i+1] == '-'
                 && pszCE[i+2] == pszDimName[0] )
        {
            if( pnDirection != NULL )
                *pnDirection = -1;

            return iCount;
        }
        else if( pszCE[i] == '[' )
            iCount++;
    }

    return -1;
}

/************************************************************************/
/* ==================================================================== */
/*                              DODSDataset                             */
/* ==================================================================== */
/************************************************************************/

class DODSDataset : public GDALDataset
{
private:
    AISConnect *poConnect; 	//< Virtual connection to the data source

    string oURL;		//< data source URL
    double adfGeoTransform[6];
    int    bGotGeoTransform;
    string oWKT;		//< Constructed WKT string

    DAS    oDAS;
    DDS   *poDDS;
    BaseTypeFactory *poBaseTypeFactory;

    AISConnect *connect_to_server() throw(Error);

    static string      SubConstraint( string raw_constraint, 
                                      string x_constraint, 
                                      string y_constraint );

    char            **CollectBandsFromDDS();
    char            **CollectBandsFromDDSVar( string, char ** );
    char            **ParseBandsFromURL( string );

    void            HarvestDAS();
    static void     HarvestMetadata( GDALMajorObject *, AttrTable * );
    void            HarvestMaps( string oVarName, string oCE );
    
    friend class DODSRasterBand;

public:
                    DODSDataset();
    virtual        ~DODSDataset();

    // Overridden GDALDataset methods
    CPLErr GetGeoTransform(double *padfTransform);
    const char *GetProjectionRef();

    /// Open is not a method in GDALDataset; it's the driver.
    static GDALDataset *Open(GDALOpenInfo *);

    /// Return the connection object
    AISConnect *GetConnect() { return poConnect; }

    /// Return the data source URL
    string GetUrl() { return oURL; }
    DAS &GetDAS() { return oDAS; }
    DDS &GetDDS() { return *poDDS; }
};

/************************************************************************/
/* ==================================================================== */
/*                            DODSRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class DODSRasterBand : public GDALRasterBand 
{
private:
    string oVarName;          
    string oCE;		        // Holds the CE (with [x] and [y] still there

    friend class DODSDataset;

    GDALColorInterp eColorInterp;
    GDALColorTable  *poCT;

    int		   nOverviewCount;
    DODSRasterBand **papoOverviewBand;

    int    nOverviewFactor;     // 1 for base, or 2/4/8 for overviews.
    int    bTranspose;
    int    bFlipX;
    int    bFlipY;
    int    bNoDataSet;
    double dfNoDataValue;

    void            HarvestDAS();

public:
    DODSRasterBand( DODSDataset *poDS, string oVarName, string oCE, 
                    int nOverviewFactor );
    virtual ~DODSRasterBand();

    virtual int    GetOverviewCount();
    virtual GDALRasterBand *GetOverview( int );
    virtual CPLErr IReadBlock(int, int, void *);
    virtual GDALColorInterp GetColorInterpretation();
    virtual GDALColorTable *GetColorTable();
		virtual CPLErr          SetNoDataValue( double );
    virtual double          GetNoDataValue( int * );
};

/************************************************************************/
/* ==================================================================== */
/*                              DODSDataset                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            DODSDataset()                             */
/************************************************************************/

DODSDataset::DODSDataset()

{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    bGotGeoTransform = FALSE;

    poConnect = NULL;
    poBaseTypeFactory = new BaseTypeFactory();
    poDDS = new DDS( poBaseTypeFactory );
}

/************************************************************************/
/*                            ~DODSDataset()                            */
/************************************************************************/

DODSDataset::~DODSDataset()

{
    if( poConnect )
        delete poConnect;

    if( poDDS )
        delete poDDS;
    if( poBaseTypeFactory )
        delete poBaseTypeFactory;
}

/************************************************************************/
/*                         connect_to_server()                          */
/************************************************************************/

AISConnect *
DODSDataset::connect_to_server() throw(Error)
{
    // does the string start with 'http?'
    if (oURL.find("http://") == string::npos
	&& oURL.find("https://") == string::npos)
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

/* -------------------------------------------------------------------- */
/*      Connect, and fetch version information.                         */
/* -------------------------------------------------------------------- */
    AISConnect *poConnection = new AISConnect(oURL);
    string version = poConnection->request_version();
    /*    if (version.empty() || version.find("/3.") == string::npos)
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "I connected to the URL but could not get a DAP 3.x version string\n"
                  "from the server.  I will continue to connect but access may fail.");
    }
    */
    return poConnection;
}

/************************************************************************/
/*                           SubConstraint()                            */
/*                                                                      */
/*      Substitute into x and y constraint expressions in template      */
/*      constraint string for the [x] and [y] parts.                    */
/************************************************************************/

string DODSDataset::SubConstraint( string raw_constraint, 
                                   string x_constraint, 
                                   string y_constraint )

{
    string::size_type x_off, y_off, x_len=3, y_len=3;
    string final_constraint;

    x_off = raw_constraint.find( "[x]" );
    if( x_off == string::npos )
    {
        x_off = raw_constraint.find( "[-x]" );
        x_len = 4;
    }

    y_off = raw_constraint.find( "[y]" );
    if( y_off == string::npos )
    {
        y_off = raw_constraint.find( "[-y]" );
        y_len = 4;
    }

    CPLAssert( x_off != string::npos && y_off != string::npos );

    if( x_off < y_off )
        final_constraint = 
            raw_constraint.substr( 0, x_off )
            + x_constraint
            + raw_constraint.substr( x_off + x_len, y_off - x_off - x_len )
            + y_constraint
            + raw_constraint.substr( y_off + y_len );
    else
        final_constraint = 
            raw_constraint.substr( 0, y_off )
            + y_constraint
            + raw_constraint.substr( y_off + y_len, x_off - y_off - y_len )
            + x_constraint
            + raw_constraint.substr( x_off + x_len );

    return final_constraint;
}

/************************************************************************/
/*                        CollectBandsFromDDS()                         */
/*                                                                      */
/*      If no constraint/variable list is provided we will scan the     */
/*      DDS output for arrays or grids that look like bands and         */
/*      return the list of them with "guessed" [y][x] constraint        */
/*      strings.                                                        */
/*                                                                      */
/*      We pick arrays or grids with at least two dimensions as         */
/*      candidates.  After the first we only accept additional          */
/*      objects as bands if they match the size of the original.        */
/*                                                                      */
/*      Auto-recognision rules will presumably evolve over time to      */
/*      recognise different common configurations and to support        */
/*      more variations.                                                */
/************************************************************************/

char **DODSDataset::CollectBandsFromDDS()

{
    DDS::Vars_iter v_i;
    char **papszResultList = NULL;

    for( v_i = poDDS->var_begin(); v_i != poDDS->var_end(); v_i++ )
    {
        BaseType *poVar = *v_i;
        papszResultList = CollectBandsFromDDSVar( poVar->name(), 
                                                  papszResultList );
    }

    return papszResultList;
}

/************************************************************************/
/*                       CollectBandsFromDDSVar()                       */
/*                                                                      */
/*      Collect zero or more band definitions (varname + CE) for the    */
/*      passed variable.  If it is inappropriate then nothing is        */
/*      added to the list.  This method is shared by                    */
/*      CollectBandsFromDDS(), and by ParseBandsFromURL() when it       */
/*      needs a default constraint expression generated.                */
/************************************************************************/

char **DODSDataset::CollectBandsFromDDSVar( string oVarName, 
                                            char **papszResultList )

{
    Array *poArray;
    Grid *poGrid = NULL;
    
/* -------------------------------------------------------------------- */
/*      Is this a grid or array?                                        */
/* -------------------------------------------------------------------- */
    BaseType *poVar = get_variable( GetDDS(), oVarName );

    if( poVar->type() == dods_array_c )
    {
        poGrid = NULL;
        poArray = dynamic_cast<Array *>( poVar );
    }
    else if( poVar->type() == dods_grid_c )
    {
        poGrid = dynamic_cast<Grid *>( poVar );
        poArray = dynamic_cast<Array *>( poGrid->array_var() );
    }
    else
        return papszResultList;

/* -------------------------------------------------------------------- */
/*      Eventually we will want to support arrays with more than two    */
/*      dimensions ... but not quite yet.                               */
/* -------------------------------------------------------------------- */
    if( poArray->dimensions() != 2 )
        return papszResultList;

/* -------------------------------------------------------------------- */
/*      Get the dimension information for this variable.                */
/* -------------------------------------------------------------------- */
    Array::Dim_iter dim1 = poArray->dim_begin() + 0;
    Array::Dim_iter dim2 = poArray->dim_begin() + 1;
    
    int nDim1Size = poArray->dimension_size( dim1 );
    int nDim2Size = poArray->dimension_size( dim2 );
    
    if( nDim1Size == 1 || nDim2Size == 1 )
        return papszResultList;

/* -------------------------------------------------------------------- */
/*      Try to guess which is x and y.                                  */
/* -------------------------------------------------------------------- */
    string dim1_name = poArray->dimension_name( dim1 );
    string dim2_name = poArray->dimension_name( dim2 );
    int iXDim=-1, iYDim=-1;

    if( dim1_name == "easting" && dim2_name == "northing" )
    {
        iXDim = 0;
        iYDim = 1;
    }
    else if( dim1_name == "easting" && dim2_name == "northing" )
    {
        iXDim = 1;
        iYDim = 0;
    }
    else if( EQUALN(dim1_name.c_str(),"lat",3) 
             && EQUALN(dim2_name.c_str(),"lon",3) )
    {
        iXDim = 0;
        iYDim = 1;
    }
    else if( EQUALN(dim1_name.c_str(),"lon",3) 
             && EQUALN(dim2_name.c_str(),"lat",3) )
    {
        iXDim = 1;
        iYDim = 0;
    }
    else 
    {
        iYDim = 0;
        iXDim = 1;
    }

/* -------------------------------------------------------------------- */
/*      Does this match the established dimension?                      */
/* -------------------------------------------------------------------- */
    Array::Dim_iter dimx = poArray->dim_begin() + iXDim;
    Array::Dim_iter dimy = poArray->dim_begin() + iYDim;

    if( nRasterXSize == 0 && nRasterYSize == 0 )
    {
        nRasterXSize = poArray->dimension_size( dimx );
        nRasterYSize = poArray->dimension_size( dimy );
    }

    if( nRasterXSize != poArray->dimension_size( dimx )
        || nRasterYSize != poArray->dimension_size( dimy ) )
        return papszResultList;

/* -------------------------------------------------------------------- */
/*      OK, we have an acceptable candidate!                            */
/* -------------------------------------------------------------------- */
    string oConstraint;

    if( iXDim == 0 && iYDim == 1 )
        oConstraint = "[x][y]";
    else if( iXDim == 1 && iYDim == 0 )
        oConstraint = "[y][x]";
    else
        return papszResultList;

    papszResultList = CSLAddString( papszResultList, 
                                    poVar->name().c_str() );
    papszResultList = CSLAddString( papszResultList, 
                                    oConstraint.c_str() );

    return papszResultList;
}

/************************************************************************/
/*                         ParseBandsFromURL()                          */
/************************************************************************/

char **DODSDataset::ParseBandsFromURL( string oVarList )

{
    char **papszResultList = NULL;
    char **papszVars = CSLTokenizeString2( oVarList.c_str(), ",", 0 );
    int i;

    for( i = 0; papszVars != NULL && papszVars[i] != NULL; i++ )
    {
        string oVarName;
        string oCE;

/* -------------------------------------------------------------------- */
/*      Split into a varname and constraint equation.                   */
/* -------------------------------------------------------------------- */
        char *pszCEStart = strstr(papszVars[i],"[");

        // If we have no constraints we will have to try to guess
        // reasonable values from the DDS.  In fact, we might end up
        // deriving multiple bands from one variable in this case.
        if( pszCEStart == NULL )
        {
            oVarName = papszVars[i];

            papszResultList = 
                CollectBandsFromDDSVar( oVarName, papszResultList );
        }
        else
        {
            oCE = pszCEStart;
            *pszCEStart = '\0';
            oVarName = papszVars[i];

            // Eventually we should consider supporting a [band] keyword
            // to select a constraint variable that should be used to
            // identify a band dimension ... but not for now. 

            papszResultList = CSLAddString( papszResultList,
                                            oVarName.c_str() );
            papszResultList = CSLAddString( papszResultList, 
                                            oCE.c_str() );
        }
    }

    CSLDestroy(papszVars);

    return papszResultList;
}

/************************************************************************/
/*                          HarvestMetadata()                           */
/*                                                                      */
/*      Capture metadata items from an AttrTable, and assign as         */
/*      metadata to the target object.                                  */
/************************************************************************/

void DODSDataset::HarvestMetadata( GDALMajorObject *poTarget, 
                                   AttrTable *poSrcTable )

{
    if( poSrcTable == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Find Metadata container.                                        */
/* -------------------------------------------------------------------- */
    AttrTable *poMDTable = poSrcTable->find_container("Metadata");
    
    if( poMDTable == NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Collect each data item from it.                                 */
/* -------------------------------------------------------------------- */
    AttrTable::Attr_iter dv_i;

    for( dv_i=poMDTable->attr_begin(); dv_i != poMDTable->attr_end(); dv_i++ )
    {
        if( poMDTable->get_attr_type( dv_i ) != Attr_string )
            continue;

        poTarget->SetMetadataItem( 
            poMDTable->get_name( dv_i ).c_str(), 
            StripQuotes( poMDTable->get_attr(dv_i) ).c_str() );
    }
}

/************************************************************************/
/*                             HarvestDAS()                             */
/************************************************************************/

void DODSDataset::HarvestDAS()

{
/* -------------------------------------------------------------------- */
/*      Try and fetch the corresponding DAS subtree if it exists.       */
/* -------------------------------------------------------------------- */
#ifdef LIBDAP_39
    AttrTable *poFileInfo = oDAS.get_table( "GLOBAL" );

    if( poFileInfo == NULL )
    {
        poFileInfo = oDAS.get_table( "NC_GLOBAL" );

	if( poFileInfo == NULL )
	{
	    poFileInfo = oDAS.get_table( "HDF_GLOBAL" );

	    if( poFileInfo == NULL )
	    {
	        CPLDebug( "DODS", "No GLOBAL DAS info." );
	        return;
	    }
	}
    }
#else
    AttrTable *poFileInfo = oDAS.find_container( "GLOBAL" );

    if( poFileInfo == NULL )
    {
        poFileInfo = oDAS.find_container( "NC_GLOBAL" );

	if( poFileInfo == NULL )
	{
	    poFileInfo = oDAS.find_container( "HDF_GLOBAL" );

	    if( poFileInfo == NULL )
	    {
	        CPLDebug( "DODS", "No GLOBAL DAS info." );
	        return;
	    }
	}
    }
#endif

/* -------------------------------------------------------------------- */
/*      Try and fetch the bounds                                        */
/* -------------------------------------------------------------------- */
    string oNorth, oSouth, oEast, oWest;

    oNorth = poFileInfo->get_attr( "Northernmost_Northing" );
    oSouth = poFileInfo->get_attr( "Southernmost_Northing" );
    oEast = poFileInfo->get_attr( "Easternmost_Easting" );
    oWest = poFileInfo->get_attr( "Westernmost_Easting" );

    if( oNorth != "" && oSouth != "" && oEast != "" && oWest != "" )
    {
        adfGeoTransform[0] = atof(oWest.c_str());
        adfGeoTransform[1] = 
            (atof(oEast.c_str()) - atof(oWest.c_str())) / nRasterXSize;
        adfGeoTransform[2] = 0.0;
        adfGeoTransform[3] = atof(oNorth.c_str());
        adfGeoTransform[4] = 0.0;
        adfGeoTransform[5] = 
            (atof(oSouth.c_str()) - atof(oNorth.c_str())) / nRasterYSize;

        bGotGeoTransform = TRUE;
    }

/* -------------------------------------------------------------------- */
/*      Try and fetch a GeoTransform.  The result will override the     */
/*      geotransform derived from the bounds if it is present.  This    */
/*      allows us to represent rotated and sheared images.              */
/* -------------------------------------------------------------------- */
    string oValue;

    oValue = StripQuotes(poFileInfo->get_attr( "GeoTransform" ));
    if( oValue != "" )
    {
        char **papszItems = CSLTokenizeString( oValue.c_str() );
        if( CSLCount(papszItems) == 6 )
        {
            adfGeoTransform[0] = atof(papszItems[0]);
            adfGeoTransform[1] = atof(papszItems[1]);
            adfGeoTransform[2] = atof(papszItems[2]);
            adfGeoTransform[3] = atof(papszItems[3]);
            adfGeoTransform[4] = atof(papszItems[4]);
            adfGeoTransform[5] = atof(papszItems[5]);
            bGotGeoTransform = TRUE;
        }
        else
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Failed to parse GeoTransform DAS value: %s",
                      oValue.c_str() );
        }
        CSLDestroy( papszItems );
    }

/* -------------------------------------------------------------------- */
/*      Get the Projection.  If it doesn't look like "pure" WKT then    */
/*      try to process it through SetFromUserInput().  This expands     */
/*      stuff like "WGS84".                                             */
/* -------------------------------------------------------------------- */
    oWKT = StripQuotes(poFileInfo->get_attr( "spatial_ref" ));
    // strip remaining backslashes (2007-04-26, gaffigan@sfos.uaf.edu)
    oWKT.erase(std::remove(oWKT.begin(), oWKT.end(), '\\'), oWKT.end());
    if( oWKT.length() > 0 
        && !EQUALN(oWKT.c_str(),"GEOGCS",6)
        && !EQUALN(oWKT.c_str(),"PROJCS",6)
        && !EQUALN(oWKT.c_str(),"LOCAL_CS",8) )
    {
        OGRSpatialReference oSRS;

        if( oSRS.SetFromUserInput( oWKT.c_str() ) != OGRERR_NONE )
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Failed to recognise 'spatial_ref' value of: %s", 
                      oWKT.c_str() );
            oWKT = "";
        }
        else
        {
            char *pszProcessedWKT = NULL;
            oSRS.exportToWkt( &pszProcessedWKT );
            oWKT = pszProcessedWKT;
            CPLFree( pszProcessedWKT );
        }
    }

/* -------------------------------------------------------------------- */
/*      Collect Metadata.                                               */
/* -------------------------------------------------------------------- */
    HarvestMetadata( this, poFileInfo );
}

/************************************************************************/
/*                            HarvestMaps()                             */
/************************************************************************/

void DODSDataset::HarvestMaps( string oVarName, string oCE )

{
    BaseType *poDDSDef = get_variable( GetDDS(), oVarName );
    if( poDDSDef == NULL || poDDSDef->type() != dods_grid_c )
        return;

/* -------------------------------------------------------------------- */
/*      Get the grid.                                                   */
/* -------------------------------------------------------------------- */
    Grid  *poGrid = NULL;
    poGrid = dynamic_cast<Grid *>( poDDSDef );

/* -------------------------------------------------------------------- */
/*      Get the map arrays for x and y.                                 */
/* -------------------------------------------------------------------- */
    Array *poXMap = NULL, *poYMap = NULL;
    int iXDim = GetDimension( oCE, "x", NULL );
    int iYDim = GetDimension( oCE, "y", NULL );
    int iMap;
    Grid::Map_iter iterMap;
    
    for( iterMap = poGrid->map_begin(), iMap = 0;
         iterMap != poGrid->map_end(); 
         iterMap++, iMap++ )
    {
        if( iMap == iXDim )
            poXMap = dynamic_cast<Array *>(*iterMap);
        else if( iMap == iYDim )
            poYMap = dynamic_cast<Array *>(*iterMap);
    }

    if( poXMap == NULL || poYMap == NULL )
        return;

    if( poXMap->var()->type() != dods_float64_c 
        || poYMap->var()->type() != dods_float64_c )
    {
        CPLDebug( "DODS", "Ignoring Grid Map - not a supported data type." );
        return;							       
    }

/* -------------------------------------------------------------------- */
/*      TODO: We ought to validate the dimension of the map against our */
/*      expected size.                                                  */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- 
 *  Fetch maps.  We need to construct a seperate request like:     
 *    http://dods.gso.uri.edu/cgi-bin/nph-dods/MCSST/Northwest_Atlantic/5km/raw/1982/9/m82258070000.pvu.Z?dsp_band_1.lat,dsp_band_1.lon
 *
 *  to fetch just the maps, and not the actual dataset. 
 * -------------------------------------------------------------------- */
/* -------------------------------------------------------------------- */
/*      Build constraint expression.                                    */
/* -------------------------------------------------------------------- */
    string constraint;
    string xdimname = "lon";
    string ydimname = "lat";
    

    constraint = oVarName + "." + xdimname + "," 
        + oVarName + "." + ydimname;

/* -------------------------------------------------------------------- */
/*      Request data from server.                                       */
/* -------------------------------------------------------------------- */
    DataDDS data( poBaseTypeFactory );
    
    GetConnect()->request_data(data, constraint );

/* -------------------------------------------------------------------- */
/*      Get the DataDDS Array object from the response.                 */
/* -------------------------------------------------------------------- */
    BaseType *poBtX = get_variable(data, oVarName + "." + xdimname );
    BaseType *poBtY = get_variable(data, oVarName + "." + ydimname );
    if (!poBtX || !poBtY 
        || poBtX->type() != dods_array_c 
        || poBtY->type() != dods_array_c )
        return;

    Array *poAX = dynamic_cast<Array *>(poBtX);
    Array *poAY = dynamic_cast<Array *>(poBtY);

/* -------------------------------------------------------------------- */
/*      Pre-initialize the output buffer to zero.                       */
/* -------------------------------------------------------------------- */
    double *padfXMap, *padfYMap;

    padfXMap = (double *) CPLCalloc(sizeof(double),nRasterXSize );
    padfYMap = (double *) CPLCalloc(sizeof(double),nRasterYSize );

/* -------------------------------------------------------------------- */
/*      Dump the contents of the Array data into our output image       */
/*      buffer.                                                         */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
/* The cast below is the correct way to handle the problem.             */
/* The (void *) cast is to avoid a GCC warning like:                    */
/* "warning: dereferencing type-punned pointer will break \             */
/*  strict-aliasing rules"                                              */
/*                                                                      */
/*  (void *) introduces a compatible intermediate type in the cast list.*/
/* -------------------------------------------------------------------- */
    poAX->buf2val( (void **) (void *) &padfXMap );
    poAY->buf2val( (void **) (void *) &padfYMap );

/* -------------------------------------------------------------------- */
/*      Compute a geotransform from the maps.  We are implicitly        */
/*      assuming the maps are linear and refer to the center of the     */
/*      pixels.                                                         */
/* -------------------------------------------------------------------- */
    bGotGeoTransform = TRUE;

    // pixel size. 
    adfGeoTransform[1] = (padfXMap[nRasterXSize-1] - padfXMap[0])
        / (nRasterXSize-1);
    adfGeoTransform[5] = (padfYMap[nRasterYSize-1] - padfYMap[0])
        / (nRasterYSize-1);

    // rotational coefficients. 
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[4] = 0.0;

    // origin/ 
    adfGeoTransform[0] = padfXMap[0] - adfGeoTransform[1] * 0.5;
    adfGeoTransform[3] = padfYMap[0] - adfGeoTransform[5] * 0.5;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *
DODSDataset::Open(GDALOpenInfo *poOpenInfo)
{
    if( !EQUALN(poOpenInfo->pszFilename,"http://",7) 
        && !EQUALN(poOpenInfo->pszFilename,"https://",8) )
        return NULL;

    
    DODSDataset *poDS = new DODSDataset();
    char **papszVarConstraintList = NULL;

    poDS->nRasterXSize = 0;
    poDS->nRasterYSize = 0;

    try {
/* -------------------------------------------------------------------- */
/*      Split the URL from the projection/CE portion of the name.       */
/* -------------------------------------------------------------------- */
        string oWholeName( poOpenInfo->pszFilename );
        string oVarList;
        string::size_type t_char;

        t_char = oWholeName.find('?');
        if( t_char == string::npos )
        {
            oVarList = "";
            poDS->oURL = oWholeName;
        }
        else
        {
            poDS->oURL = oWholeName.substr(0,t_char);
            oVarList = oWholeName.substr(t_char+1,oWholeName.length());
        }

/* -------------------------------------------------------------------- */
/*      Get the AISConnect instance and the DAS and DDS for this        */
/*      server.                                                         */
/* -------------------------------------------------------------------- */
	poDS->poConnect = poDS->connect_to_server();
	poDS->poConnect->request_das(poDS->oDAS);
	poDS->poConnect->request_dds(*(poDS->poDDS));

/* -------------------------------------------------------------------- */
/*      If we are given a constraint/projection list, then parse it     */
/*      into a list of varname/constraint pairs.  Otherwise walk the    */
/*      DDS and try to identify grids or arrays that are good           */
/*      targets and return them in the same format.                     */
/* -------------------------------------------------------------------- */

        if( oVarList.length() == 0 )
            papszVarConstraintList = poDS->CollectBandsFromDDS();
        else
            papszVarConstraintList = poDS->ParseBandsFromURL( oVarList );

/* -------------------------------------------------------------------- */
/*      Did we get any target variables?                                */
/* -------------------------------------------------------------------- */
        if( CSLCount(papszVarConstraintList) == 0 )
            throw Error( "No apparent raster grids or arrays found in DDS.");

/* -------------------------------------------------------------------- */
/*      For now we support only a single band.                          */
/* -------------------------------------------------------------------- */
        DODSRasterBand *poBaseBand = 
            new DODSRasterBand(poDS, 
                               string(papszVarConstraintList[0]),
                               string(papszVarConstraintList[1]), 
                               1 );

        poDS->nRasterXSize = poBaseBand->GetXSize();
        poDS->nRasterYSize = poBaseBand->GetYSize();
            
	poDS->SetBand(1, poBaseBand );

        for( int iBand = 1; papszVarConstraintList[iBand*2] != NULL; iBand++ )
        {
            poDS->SetBand( iBand+1, 
                           new DODSRasterBand(poDS, 
                                   string(papszVarConstraintList[iBand*2+0]),
                                   string(papszVarConstraintList[iBand*2+1]), 
                                              1 ) );
        }

/* -------------------------------------------------------------------- */
/*      Harvest DAS dataset level information including                 */
/*      georeferencing, and metadata.                                   */
/* -------------------------------------------------------------------- */
        poDS->HarvestDAS();

/* -------------------------------------------------------------------- */
/*      If we don't have georeferencing, look for "map" information     */
/*      for a grid.                                                     */
/* -------------------------------------------------------------------- */
        if( !poDS->bGotGeoTransform )
        {
            poDS->HarvestMaps( string(papszVarConstraintList[0]),
                               string(papszVarConstraintList[1]) );
        }
    }

    catch (Error &e) {
        string msg =
"An error occurred while creating a virtual connection to the DAP server:\n";
        msg += e.get_error_message();
        CPLError(CE_Failure, CPLE_AppDefined, "%s", msg.c_str());
        delete poDS;
        poDS = NULL;
    }

    CSLDestroy(papszVarConstraintList);
    
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poDS != NULL && poOpenInfo->eAccess == GA_Update )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The DODS driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }
    
    return poDS;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr 
DODSDataset::GetGeoTransform( double * padfTransform )
{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );

    return bGotGeoTransform ? CE_None : CE_Failure;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *
DODSDataset::GetProjectionRef()
{
    return oWKT.c_str();
}

/************************************************************************/
/* ==================================================================== */
/*                            DODSRasterBand                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           DODSRasterBand()                           */
/************************************************************************/

DODSRasterBand::DODSRasterBand(DODSDataset *poDSIn, string oVarNameIn, 
                               string oCEIn, int nOverviewFactorIn )
{
    poDS = poDSIn;

    bTranspose = FALSE;
    bFlipX = FALSE;
    bFlipY = FALSE;

    oVarName = oVarNameIn;
    oCE = oCEIn;
    nOverviewFactor = nOverviewFactorIn;
    eColorInterp = GCI_Undefined;
    poCT = NULL;

    nOverviewCount = 0;
    papoOverviewBand = NULL;

/* -------------------------------------------------------------------- */
/*      Fetch the DDS definition, and isolate the Array.                */
/* -------------------------------------------------------------------- */
    BaseType *poDDSDef = get_variable( poDSIn->GetDDS(), oVarNameIn );
    if( poDDSDef == NULL )
    {
        throw InternalErr(
            CPLSPrintf( "Could not find DDS definition for variable %s.", 
                        oVarNameIn.c_str() ) );
        return;
    }

    Array *poArray = NULL;
    Grid  *poGrid = NULL;

    if( poDDSDef->type() == dods_grid_c )
    {
        poGrid = dynamic_cast<Grid *>( poDDSDef );
        poArray = dynamic_cast<Array *>( poGrid->array_var() );
    }
    else if( poDDSDef->type() == dods_array_c )
    {
        poArray = dynamic_cast<Array *>( poDDSDef );
    }
    else
    {
        throw InternalErr(
            CPLSPrintf( "Variable %s is not a grid or an array.",
                        oVarNameIn.c_str() ) );
    }

/* -------------------------------------------------------------------- */
/*      Determine the datatype.                                         */
/* -------------------------------------------------------------------- */
    
    // Now grab the data type of the variable.
    switch (poArray->var()->type()) {
      case dods_byte_c: eDataType = GDT_Byte; break;
      case dods_int16_c: eDataType = GDT_Int16; break;
      case dods_uint16_c: eDataType = GDT_UInt16; break;
      case dods_int32_c: eDataType = GDT_Int32; break;
      case dods_uint32_c: eDataType = GDT_UInt32; break;
      case dods_float32_c: eDataType = GDT_Float32; break;
      case dods_float64_c: eDataType = GDT_Float64; break;
      default:
	throw Error("The DODS GDAL driver supports only numeric data types.");
    }

/* -------------------------------------------------------------------- */
/*      For now we hard code to assume that the two dimensions are      */
/*      ysize and xsize.                                                */
/* -------------------------------------------------------------------- */
    if( poArray->dimensions() < 2 )
    {
        throw Error("Variable does not have even 2 dimensions.  For now this is required." );
    }

    int nXDir = 1, nYDir = 1;
    int iXDim = GetDimension( oCE, "x", &nXDir );
    int iYDim = GetDimension( oCE, "y", &nYDir );

    if( iXDim == -1 || iYDim == -1 )
    {
        throw Error("Missing [x] or [y] in constraint." );
    }

    Array::Dim_iter x_dim = poArray->dim_begin() + iXDim;
    Array::Dim_iter y_dim = poArray->dim_begin() + iYDim;

    nRasterXSize = poArray->dimension_size( x_dim ) / nOverviewFactor;
    nRasterYSize = poArray->dimension_size( y_dim ) / nOverviewFactor;

    bTranspose = iXDim < iYDim;

    bFlipX = (nXDir == -1);
    bFlipY = (nYDir == -1);

/* -------------------------------------------------------------------- */
/*      Decide on a block size.  We aim for a block size of roughly     */
/*      256K.  This should be a big enough chunk to justify a           */
/*      roundtrip to get the data, but small enough to avoid reading    */
/*      too much data.                                                  */
/* -------------------------------------------------------------------- */
    int nBytesPerPixel = GDALGetDataTypeSize( eDataType ) / 8;

    if( nBytesPerPixel == 1 )
    {
        nBlockXSize = 1024;
        nBlockYSize= 256;
    }
    else if( nBytesPerPixel == 2 )
    {
        nBlockXSize = 512;
        nBlockYSize= 256;
    }
    else if( nBytesPerPixel == 4 )
    {
        nBlockXSize = 512;
        nBlockYSize= 128;
    }
    else
    {
        nBlockXSize = 256;
        nBlockYSize= 128;
    }

    if( nRasterXSize < nBlockXSize * 2 )
        nBlockXSize = nRasterXSize;
    
    if( nRasterYSize < nBlockYSize * 2 )
        nBlockYSize = nRasterYSize;

/* -------------------------------------------------------------------- */
/*      Get other information from the DAS for this band.               */
/* -------------------------------------------------------------------- */
    if( nOverviewFactorIn == 1 )
        HarvestDAS();

/* -------------------------------------------------------------------- */
/*      Create overview band objects.                                   */
/* -------------------------------------------------------------------- */
    if( nOverviewFactorIn == 1 )
    {
        int iOverview;

        nOverviewCount = 0;
        papoOverviewBand = (DODSRasterBand **) 
            CPLCalloc( sizeof(void*), 8 );

        for( iOverview = 1; iOverview < 8; iOverview++ )
        {
            int nThisFactor = 1 << iOverview;
            
            if( nRasterXSize / nThisFactor < 128
                && nRasterYSize / nThisFactor < 128 )
                break;

            papoOverviewBand[nOverviewCount++] = 
                new DODSRasterBand( poDSIn, oVarNameIn, oCEIn, 
                                    nThisFactor );

            papoOverviewBand[nOverviewCount-1]->bFlipX = bFlipX;
            papoOverviewBand[nOverviewCount-1]->bFlipY = bFlipY;
        }
    }
}

/************************************************************************/
/*                          ~DODSRasterBand()                           */
/************************************************************************/

DODSRasterBand::~DODSRasterBand()

{
    for( int iOverview = 0; iOverview < nOverviewCount; iOverview++ )
        delete papoOverviewBand[iOverview];
    CPLFree( papoOverviewBand );

    if( poCT )
        delete poCT;
}

/************************************************************************/
/*                             HarvestDAS()                             */
/************************************************************************/

void DODSRasterBand::HarvestDAS()

{
    DODSDataset *poDODS = dynamic_cast<DODSDataset *>(poDS);
    
/* -------------------------------------------------------------------- */
/*      Try and fetch the corresponding DAS subtree if it exists.       */
/* -------------------------------------------------------------------- */
#ifdef LIBDAP_39
    AttrTable *poBandInfo = poDODS->GetDAS().get_table( oVarName );
#else
    AttrTable *poBandInfo = poDODS->GetDAS().find_container( oVarName );
#endif

    if( poBandInfo == NULL )
    {
        CPLDebug( "DODS", "No band DAS info for %s.", oVarName.c_str() );
        return;
    }

/* -------------------------------------------------------------------- */
/*      collect metadata.                                               */
/* -------------------------------------------------------------------- */
    poDODS->HarvestMetadata( this, poBandInfo );

/* -------------------------------------------------------------------- */
/*      Get photometric interpretation.                                 */
/* -------------------------------------------------------------------- */
    string oValue;
    int  iInterp;

    oValue = StripQuotes(poBandInfo->get_attr( "PhotometricInterpretation" ));
    for( iInterp = 0; iInterp < (int) GCI_Max; iInterp++ )
    {
        if( oValue == GDALGetColorInterpretationName( 
                (GDALColorInterp) iInterp ) )
            eColorInterp = (GDALColorInterp) iInterp;
    }
    
/* -------------------------------------------------------------------- */
/*      Get band description.                                           */
/* -------------------------------------------------------------------- */
    oValue = StripQuotes(poBandInfo->get_attr( "Description" ));
    if( oValue != "" )
        SetDescription( oValue.c_str() );

/* -------------------------------------------------------------------- */
/* Try missing_value                                                    */
/* -------------------------------------------------------------------- */
    oValue = poBandInfo->get_attr( "missing_value" );
    bNoDataSet=FALSE;
    if( oValue != "" ) {
        SetNoDataValue( atof(oValue.c_str()) );
    } else {

/* -------------------------------------------------------------------- */
/* Try _FillValue                                                       */
/* -------------------------------------------------------------------- */
	oValue = poBandInfo->get_attr( "_FillValue" );
	if( oValue != "" ) {
	    SetNoDataValue( atof(oValue.c_str()) );
	}
    }



/* -------------------------------------------------------------------- */
/*      Collect color table                                             */
/* -------------------------------------------------------------------- */
    AttrTable *poCTable = poBandInfo->find_container("Colormap");
    if( poCTable != NULL )
    {
        AttrTable::Attr_iter dv_i;

        poCT = new GDALColorTable();

        for( dv_i = poCTable->attr_begin(); 
             dv_i != poCTable->attr_end(); 
             dv_i++ )
        {
            if( !poCTable->is_container( dv_i ) )
                continue;

            AttrTable *poColor = poCTable->get_attr_table( dv_i );
            GDALColorEntry sEntry;

            if( poColor == NULL )
                continue;

            sEntry.c1 = atoi(poColor->get_attr( "red" ).c_str());
            sEntry.c2 = atoi(poColor->get_attr( "green" ).c_str());
            sEntry.c3 = atoi(poColor->get_attr( "blue" ).c_str());
            if( poColor->get_attr( "alpha" ) == "" )
                sEntry.c4 = 255;
            else
                sEntry.c4 = atoi(poColor->get_attr( "alpha" ).c_str());
            
            poCT->SetColorEntry( poCT->GetColorEntryCount(), &sEntry );
        }
    }

/* -------------------------------------------------------------------- */
/*      Check for flipping instructions.                                */
/* -------------------------------------------------------------------- */
    oValue = StripQuotes(poBandInfo->get_attr( "FlipX" ));
    if( oValue != "no" && oValue != "NO" && oValue != "" )
        bFlipX = TRUE;

    oValue = StripQuotes(poBandInfo->get_attr( "FlipY" ));
    if( oValue != "no" && oValue != "NO" && oValue != "" )
        bFlipY = TRUE;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr 
DODSRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage)
{
    DODSDataset *poDODS = dynamic_cast<DODSDataset *>(poDS);
    int nBytesPerPixel = GDALGetDataTypeSize(eDataType) / 8;

/* -------------------------------------------------------------------- */
/*      What is the actual rectangle we want to read?  We can't read    */
/*      full blocks that go off the edge of the original data.          */
/* -------------------------------------------------------------------- */
    int nXOff, nXSize, nYOff, nYSize;

    nXOff = nBlockXOff * nBlockXSize;
    nYOff = nBlockYOff * nBlockYSize;
    nXSize = nBlockXSize;
    nYSize = nBlockYSize;

    if( nXOff + nXSize > nRasterXSize )
        nXSize = nRasterXSize - nXOff;
    if( nYOff + nYSize > nRasterYSize )
        nYSize = nRasterYSize - nYOff;

/* -------------------------------------------------------------------- */
/*      If we are working with a flipped image, we need to transform    */
/*      the requested window accordingly.                               */
/* -------------------------------------------------------------------- */
    if( bFlipY )
        nYOff = (nRasterYSize - nYOff - nYSize);

    if( bFlipX )
        nXOff = (nRasterXSize - nXOff - nXSize);

/* -------------------------------------------------------------------- */
/*      Prepare constraint expression for this request.                 */
/* -------------------------------------------------------------------- */
    string x_constraint, y_constraint, raw_constraint, final_constraint;

    x_constraint = 
        CPLSPrintf( "[%d:%d:%d]", 
                    nXOff * nOverviewFactor, 
                    nOverviewFactor, 
                    (nXOff + nXSize - 1) * nOverviewFactor );
    y_constraint = 
        CPLSPrintf( "[%d:%d:%d]", 
                    nYOff * nOverviewFactor,
                    nOverviewFactor,
                    (nYOff + nYSize - 1) * nOverviewFactor );

    raw_constraint = oVarName + oCE;

    final_constraint = poDODS->SubConstraint( raw_constraint, 
                                              x_constraint, 
                                              y_constraint );

    CPLDebug( "DODS", "constraint = %s", final_constraint.c_str() );

/* -------------------------------------------------------------------- */
/*      Request data from server.                                       */
/* -------------------------------------------------------------------- */
    try {
        DataDDS data( poDODS->poBaseTypeFactory );

        poDODS->GetConnect()->request_data(data, final_constraint );

/* -------------------------------------------------------------------- */
/*      Get the DataDDS Array object from the response.                 */
/* -------------------------------------------------------------------- */
        BaseType *poBt = get_variable(data, oVarName );
        if (!poBt)
            throw Error(string("I could not read the variable '")
		    + oVarName		    + string("' from the data source at:\n")
		    + poDODS->GetUrl() );

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

/* -------------------------------------------------------------------- */
/*      Pre-initialize the output buffer to zero.                       */
/* -------------------------------------------------------------------- */
        if( nXSize < nBlockXSize || nYSize < nBlockYSize )
            memset( pImage, 0, 
                    nBlockXSize * nBlockYSize 
                    * GDALGetDataTypeSize(eDataType) / 8 );

/* -------------------------------------------------------------------- */
/*      Dump the contents of the Array data into our output image buffer.*/
/*                                                                      */
/* -------------------------------------------------------------------- */
        poA->buf2val(&pImage);	// !Suck the data out of the Array!

/* -------------------------------------------------------------------- */
/*      If the [x] dimension comes before [y], we need to transpose     */
/*      the data we just got back.                                      */
/* -------------------------------------------------------------------- */
        if( bTranspose )
        {
            GByte *pabyDataCopy;
            int iY;
            
            CPLDebug( "DODS", "Applying transposition" );
            
            // make a copy of the original
            pabyDataCopy = (GByte *) 
                CPLMalloc(nBytesPerPixel * nXSize * nYSize);
            memcpy( pabyDataCopy, pImage, nBytesPerPixel * nXSize * nYSize );
            memset( pImage, 0, nBytesPerPixel * nXSize * nYSize );
            
            for( iY = 0; iY < nYSize; iY++ )
            {
                GDALCopyWords( pabyDataCopy + iY * nBytesPerPixel, 
                               eDataType, nBytesPerPixel * nYSize,
                               ((GByte *) pImage) + iY * nXSize * nBytesPerPixel, 
                               eDataType, nBytesPerPixel, nXSize );
            }
            
            // cleanup
            CPLFree( pabyDataCopy );
        }
        
/* -------------------------------------------------------------------- */
/*      Do we need "x" flipping?                                        */
/* -------------------------------------------------------------------- */
        if( bFlipX )
        {
            GByte *pabyDataCopy;
            int iY;
            
            CPLDebug( "DODS", "Applying X flip." );
            
            // make a copy of the original
            pabyDataCopy = (GByte *) 
                CPLMalloc(nBytesPerPixel * nXSize * nYSize);
            memcpy( pabyDataCopy, pImage, nBytesPerPixel * nXSize * nYSize );
            memset( pImage, 0, nBytesPerPixel * nXSize * nYSize );
            
            for( iY = 0; iY < nYSize; iY++ )
            {
                GDALCopyWords( pabyDataCopy + iY*nXSize*nBytesPerPixel,
                               eDataType, nBytesPerPixel,
                               ((GByte *) pImage) + ((iY+1)*nXSize-1)*nBytesPerPixel, 
                               eDataType, -nBytesPerPixel, nXSize );
            }
            
            // cleanup
            CPLFree( pabyDataCopy );
        }

/* -------------------------------------------------------------------- */
/*      Do we need "y" flipping?                                        */
/* -------------------------------------------------------------------- */
        if( bFlipY )
        {
            GByte *pabyDataCopy;
            int iY;
            
            CPLDebug( "DODS", "Applying Y flip." );
            
            // make a copy of the original
            pabyDataCopy = (GByte *) 
                CPLMalloc(nBytesPerPixel * nXSize * nYSize);
            memcpy( pabyDataCopy, pImage, nBytesPerPixel * nXSize * nYSize );
            
            for( iY = 0; iY < nYSize; iY++ )
            {
                GDALCopyWords( pabyDataCopy + iY*nXSize*nBytesPerPixel,
                               eDataType, nBytesPerPixel,
                               ((GByte *) pImage) + (nYSize-iY-1)*nXSize*nBytesPerPixel,
                               eDataType, nBytesPerPixel,
                               nXSize );
            }
            
            // cleanup
            CPLFree( pabyDataCopy );
        }

/* -------------------------------------------------------------------- */
/*      If we only read a partial block we need to re-organize the      */
/*      data.                                                           */
/* -------------------------------------------------------------------- */
        if( nXSize < nBlockXSize )
        {
            int iLine;

            for( iLine = nYSize-1; iLine >= 0; iLine-- )
            {
                memmove( ((GByte *) pImage) + iLine*nBlockXSize*nBytesPerPixel,
                         ((GByte *) pImage) + iLine * nXSize * nBytesPerPixel,
                         nBytesPerPixel * nXSize );
                memset( ((GByte *) pImage) 
                        + (iLine*nBlockXSize + nXSize)*nBytesPerPixel,
                        0, nBytesPerPixel * (nBlockXSize - nXSize) );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Catch exceptions                                                */
/* -------------------------------------------------------------------- */
    catch (Error &e) {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.get_error_message().c_str());
        return CE_Failure;
    }
    
    return CE_None;
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int DODSRasterBand::GetOverviewCount()

{
    return nOverviewCount;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *DODSRasterBand::GetOverview( int iOverview )

{
    if( iOverview < 0 || iOverview >= nOverviewCount )
        return NULL;
    else
        return papoOverviewBand[iOverview];
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp DODSRasterBand::GetColorInterpretation()

{
    return eColorInterp;
}

/************************************************************************/
/*                           GetColorTable()                            */
/************************************************************************/

GDALColorTable *DODSRasterBand::GetColorTable()

{
    return poCT;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr DODSRasterBand::SetNoDataValue( double dfNoData )

{
    bNoDataSet = TRUE;
    dfNoDataValue = dfNoData;

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double DODSRasterBand::GetNoDataValue( int * pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = bNoDataSet;

    return dfNoDataValue;
}

/************************************************************************/
/*                         GDALRegister_DODS()                          */
/************************************************************************/

void 
GDALRegister_DODS()
{
    GDALDriver *poDriver;
    
    if (! GDAL_CHECK_VERSION("GDAL/DODS driver"))
        return;

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

