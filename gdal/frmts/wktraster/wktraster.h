/******************************************************************************
 * File :    wktraster.h
 * Project:  WKT Raster driver
 * Purpose:  Main header file for PostGIS WKTRASTER/GDAL Driver 
 * Author:   Jorge Arevalo, jorgearevalo@gis4free.org
 * 
 * Last changes: $Id$
 *
 ******************************************************************************
 * Copyright (c) 2009, Jorge Arevalo, jorgearevalo@gis4free.org
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
 ******************************************************************************/
#include "gdal_priv.h"
#include "libpq-fe.h"
//#include "liblwgeom.h"

/* Constant values for boolean variables */
#define TRUE	1
#define FALSE	0

#define NDR     1   // Little endian
#define XDR     0   // Big endian


/* Working modes */
#define REGULARLY_TILED_MODE    "REGULARLY_TILED_MODE"
#define IRREGULARLY_TILED_MODE  "IRREGULARLY_TILED_MODE"
#define IMAGE_WAREHOUSE         "IMAGE_WAREHOUSE"

#define DEFAULT_SCHEMA          "public"

// Supported raster version
#define WKT_RASTER_VERSION (GUInt16)0



// Utility function
GByte GetMachineEndianess();


class WKTRasterRasterBand;
class WKTRasterWrapper;

/************************************************************************
 * ====================================================================
 *                            WKTRasterDataset
 * ====================================================================
 * A set of associated raster bands, normally for one PostGIS table.
 ************************************************************************/
class WKTRasterDataset : public GDALDataset {
    friend class WKTRasterRasterBand;

    // Private attributes and methods
    PGconn * hPGconn; 
    GBool bCloseConnection;
    GBool bTableHasGISTIndex;
    GBool bTableHasRegularBlocking;
    CPLString osProjection;
    char * pszSchemaName;
    char * pszTableName;
    char * pszRasterColumnName;
    char * pszWhereClause;
    int nVersion;
    int nBlockSizeX;
    int nBlockSizeY;
    double dfPixelSizeX;
    double dfPixelSizeY;
    double dfUpperLeftX;
    double dfUpperLeftY;
    double dfLowerRightX;
    double dfLowerRightY;
    double dfSkewX;
    double dfSkewY;
    int nSrid;
    char * pszWorkingMode;  
    int nOverviews;
    WKTRasterDataset ** papoWKTRasterOv;
    GDALDataset * poOutdbRasterDS;

    // To be delete
    WKTRasterWrapper ** papoBlocks;
    int nBlocks;
    



    /**
     * Populate the georeference information fields of dataset
     * Parameters: none
     * Returns:
     *      CPLErr: CE_None if the fields are populated, CE_Failure otherwise
     */
    CPLErr SetRasterProperties();


    /**
     * Explode a string representing an array into an array of strings. The input
     * string has this format: {element1,element2,element3,....,elementN}
     * Parameters:
     *  const char *: a string representing an array
     *  int *: pointer to an int that will contain the number of elements
     * Returns:
     *  char **: An array of strings, one per element
     */
    char ** ExplodeArrayString(const char *, int *);

    /**
     * Implode an array of strings, to transform it into a PostgreSQL-style
     * array, with this format: {element1,element2,element3,....,elementN}
     * Parameters:
     *  char **: An array of strings, one per element
     *  int: An int that contains the number of elements
     * Returns:
     *  const char *: a string representing an array
     */
    char * ImplodeStrings(char ** papszElements, int);


    /* Public attributes and methods */
public:

    /**
     * Constructor. Init the class properties with default values
     */
    WKTRasterDataset();

    /**
     * Destructor. Frees allocated memory.
     */
    ~WKTRasterDataset();

    /**
     * Method: Open
     * Open a connection wiht PostgreSQL. The connection string will have this
     * format:
     * 	PG:[host='<host>]' user='<user>' [password='<password>]'
     *      dbname='<dbname>' table='<raster_table>' [mode='working_mode'
     *      where='<where_clause>'].
     * All the connection string, apart from table='table_name',
     * where='sql_where' and mode ='working_mode' is PQconnectdb-valid.
     *
     * 	NOTE: The table name can't include the schema in the form
     *      <schema>.<table_name>,
     * 	and this is a TODO task.
     * Parameters:
     *  - GDALOpenInfo *: pointer to a GDALOpenInfo structure, with useful
     *                  information for Dataset
     * Returns:
     *  - GDALDataset *: pointer to a new GDALDataset instance on success,
     *                  NULL otherwise
     */
    static GDALDataset * Open(GDALOpenInfo *);


    /**
     * Method: GetGeoTransform
     * Fetches the coefficients for transforming between pixel/line (P,L) raster
     * space and projection coordinates (Xp, Yp) space
     * The affine transformation performed is:
     * 	Xp = padfTransform[0] + P*padfTransform[1] + L*padfTransform[2];
     * 	Yp = padfTransform[3] + P*padfTransform[4] + L*padfTransform[5];
     * Parameters:
     *  - double *: pointer to a double array, that will contains the affine
     * 	transformation matrix coefficients
     * Returns:
     *  - CPLErr: CE_None on success, or CE_Failure if no transform can be
     *            fetched.
     */
    CPLErr GetGeoTransform(double * padfTransform);

    /**
     * Fetch the projection definition string for this dataset in OpenGIS WKT
     * format. It should be suitable for use with the OGRSpatialReference class
     * Parameters: none
     * Returns:
     *  - const char *: a pointer to an internal projection reference string.
     *                  It should not be altered, freed or expected to last for
     *                  long.
     */
    const char * GetProjectionRef();

    /**
     * Set the projection reference string for this dataset. The string should
     * be in OGC WKT or PROJ.4 format
     * Parameters:
     *  - const char *: projection reference string.
     * Returns:
     *  - CE_Failure if an error occurs, otherwise CE_None.
     */
    CPLErr SetProjection(const char *);

    /**
     * Set the affine transformation coefficients.
     * Parameters:
     *  - double *:a six double buffer containing the transformation
     *             coefficients to be written with the dataset.
     *  Returns:
     *  - CE_None on success, or CE_Failure if this transform cannot be written.
     */
    CPLErr SetGeoTransform(double *);

};

/************************************************************************
 * ====================================================================
 *                            WKTRasterRasterBand
 * ====================================================================
 *
 * A single WKT Raster band
 ************************************************************************/
class WKTRasterRasterBand : public GDALRasterBand {
    // Friend class
    friend class WKTRasterDataset;

    // class attributes
    double dfNoDataValue;
    GBool bIsSignedByte;
    int nBitDepth;

    /**
     * Set the block data to the null value if it is set, or zero if there is
     * no null data value.
     * Parameters:
     *  - void *: the block data
     * Returns: nothing
     */
    void NullBlock(void *);

protected:

    /**
     * Read a block of image data
     * Parameters:
     *  int: horizontal block offset
     *  int: vertical block offset
     *  void *: The buffer into the data will be read
     * Returns:
     *  CE_None on success, CE_Failure on error
     */
    virtual CPLErr IReadBlock(int, int, void *);

    /**
     * Write a block of data to the raster band. First establish if a 
     * corresponding row to the block already exists with a SELECT. If so,
     * update the raster column contents.  If it does not exist create a new
     * row for the block.
     * Parameters:
     *  int: horizontal block offset
     *  int: vertical block offset
     *  void *: The buffer wit the data to be written
     * Returns:
     *  CE_None on success, CE_Failure on error
     */
    virtual CPLErr IWriteBlock(int, int, void *);

public:
    /**
     * Constructor.
     * Parameters:
     *  - WKTRasterDataset *: The Dataset this band belongs to
     *  - int: the band number
     *  - GDALDataType: The data type for this band
     *  - double: The nodata value.  Could be any kind of data (GByte, GUInt16,
     *          GInt32...) but the variable has the bigger type.
     *  - GBool: if the data type is signed byte or not. If yes, the SIGNEDBYTE
     *          metadata value is added to IMAGE_STRUCTURE domain
     *  - int: The bit depth, to add NBITS as metadata value in IMAGE_STRUCTURE
     *          domain.
     */
    WKTRasterRasterBand(WKTRasterDataset *, int, GDALDataType, double, GBool,
            int);

    /**
     * Set the no data value for this band.
     * Parameters:
     *  - double: The nodata value
     * Returns:
     *  - CE_None.
     */
    CPLErr SetNoDataValue(double);

    /**
     * Fetch the no data value for this band.
     * Parameters:
     *  - int *: pointer to a boolean to use to indicate if a value is actually
     *          associated with this layer. May be NULL (default).
     *  Returns:
     *  - double: the nodata value for this band.
     */
    virtual double GetNoDataValue(int *pbSuccess = NULL);

    /**
     * Returns the number of overview layers available.
     * Parameters: none
     * Returns:
     *  int: the number of overviews layers available
     */
    virtual int GetOverviewCount();

    /**
     * Fetch overview raster band object.
     * Parameters:
     *  - int: overview index between 0 and GetOverviewCount()-1
     * Returns:
     *  - GDALRasterBand *: overview GDALRasterBand.
     */
    virtual GDALRasterBand * GetOverview(int);


    /**
     * Get the natural block size for this band.
     * Parameters:
     *  - int *: pointer to int to store the natural X block size
     *  - int *: pointer to int to store the natural Y block size
     * Returns: nothing
     */
    virtual void GetBlockSize(int *, int *);

    /**
     * Says if the datatype for this band is signedbyte.
     * Parameters: none
     * Returns:
     *  - TRUE if the datatype for this band is signedbyte, FALSE otherwise
     */
    GBool IsSignedByteDataType();

    /**
     * Get the bit depth for this raster band
     * Parameters: none.
     * Returns: the bit depth
     */
    int GetNBitDepth();

};




/**************************************************************************
 * ======================================================================
 *                            WKTRasterBandWrapper
 * ======================================================================
 *
 * This class wrapps the HEXWKB representation of a PostGIS WKT Raster
 * Band, that is a part of the wrapper of a WKT Raster.
 *
 * TODO:
 *  - Allow changing the size of the nodatavalue, that implies modify the
 *  allocated memory for HexWkb representation of the WKT Raster. Now, you
 *  only can change the value, not the size.
 *  - Avoid the use of double instead of double, to ensure compatibility
 *  with WKTRasterRasterBand types. Discuss it.
 ***************************************************************************/

// To declare this class as friend class
class WKTRasterWrapper;

class WKTRasterBandWrapper {
    // Friend classes
    friend class WKTRasterWrapper;
    friend class WKTRasterRasterBand;

    // Class attributes
    GBool bIsOffline;
    GByte byReserved;
    GByte byPixelType;
    double dfNodata;
    GByte * pbyData;
    GUInt32 nDataSize;
    GUInt16 nBand;
    GByte nOutDbBandNumber;


    // Pointer to the raster wrapper
    WKTRasterWrapper * poRW;

public:
    /**
     * Constructor.
     * Parameters:
     *  - WKTRasterWrapper *: the WKT Raster wrapper this band belongs to
     *  - GUInt16: band number
     *  - GByte: The first byte of the band header (contains the value for
     *          other class properties).
     *  - double: The nodata value. Could be any kind of data (GByte,
     *          GUInt16, GInt32...) but the variable has the bigger type
     */
    WKTRasterBandWrapper(WKTRasterWrapper *, GUInt16, GByte, double);


    /**
     * Class destructor. Frees the memory and resources allocated.
     */
    ~WKTRasterBandWrapper();

    /**
     * Set the raster band data. This method updates the data of the raster
     * band. Then, when the HexWkb representation of the raster is
     * required (via WKTRasterWrapper::GetBinaryRepresentation or
     * WKTRasterWrapper::GetHexWkbRepresentation), the new data will
     * be packed instead the data of the original HexWkb representation
     * used to create the WKTRasterWrapper.
     * Parameters:
     *  - GByte *: The data to set
     *  - GUInt32: data size
     * Returns:
     *  - CE_None if the data is set, CE_Failure otherwise
     */
    CPLErr SetData(GByte *, GUInt32);


    /**
     * Get the raster band data.
     * Parameters: nothing
     * Returns:
     *  - GByte *: The raster band data
     */
    GByte * GetData();
};

/************************************************************************
 * ====================================================================
 *                            WKTRasterWrapper
 * ====================================================================
 *
 * This class wraps the HEXWKB representation of a PostGIS WKT Raster.
 *
 * It splits the hexwkb string into fields, and reconstructs this string
 * from the fields each time that the representation is required (see
 * GetBinaryRepresentation method).
 *
 * The best way to get the representation of the raster is by using the
 * GetBinaryRepresentation and GetHexWkbRepresentation methods. This
 * methods construct the representation based on the class properties.
 *
 * If you access the pszHexWkb or pbyHexWkb variables directly, you may
 * get a non-updated version of the raster. Anyway, you only can access
 * this variables from friend classes.
 ************************************************************************/
class WKTRasterWrapper {
    // Friend classes
    friend class WKTRasterBandWrapper;
    friend class WKTRasterRasterBand;

    // Attributes of a PostGIS WKT Raster
    GByte byEndianess;
    GUInt16 nVersion;
    GUInt16 nBands;
    double dfScaleX;
    double dfScaleY;
    double dfIpX;
    double dfIpY;
    double dfSkewX;
    double dfSkewY;
    GInt32 nSrid;
    GUInt16 nWidth;
    GUInt16 nHeight;
    WKTRasterBandWrapper ** papoBands;
    int nLengthHexWkbString;
    int nLengthByWkbString;

    // binary and string hexwkb representations
    char * pszHexWkb;
    GByte * pbyHexWkb;

    // WKT string (polygon)
    char * pszWktExtent;



public:

    /**
     * Class constructor
     */
    WKTRasterWrapper();

    /**
     * Class destructor. Frees the memory and resources allocated.
     */
    ~WKTRasterWrapper();

    /**
     * Fill all the raster properties with the string
     * hexwkb representation given as input.
     * This method swaps words if the raster endianess is distinct from
     * the machine endianess
     * Properties:
     *  const char *: the string hexwkb representation of the raster
     * Returns :
     *   - int : TRUE if all right
     */
     int Initialize(const char* pszHex);

    /**
     * Creates a polygon in WKT representation that wrapps all the extent
     * covered by the raster
     * Parameters: nothing
     * Returns:
     *  char *: The polygon in WKT format
     */
    const char * GetWktExtent();

    /**
     * Constructs the binary representation of the PostGIS WKT raster wrapped
     * by this class, based on all the class properties.
     * This method swaps words if the raster endianess is distinct from
     * the machine endianess.
     * Parameters: nothing
     * Returns:
     *  - GByte *: Binary representation of the hexwkb string
     */
    GByte * GetBinaryRepresentation();


    /**
     * Constructs the hexwkb representation of the PostGIS WKT raster wrapped
     * by this class, based on all the class properties.
     * This method swaps words if the raster endianess is distinct from
     * the machine endianess.
     * Parameters: nothing
     * Returns:
     *  - GByte *: Hexwkb string
     */
    char * GetHexWkbRepresentation();

    /**
     * Gets a wrapper of a RasterBand, as a WKTRasterBandWrapper object.
     * Properties:
     *  - GUInt16: the band number.
     * Returns:
     *  - WKTRasterWrapper *: an object that wrapps the RasterBand
     */
    WKTRasterBandWrapper * GetBand(GUInt16);
};
