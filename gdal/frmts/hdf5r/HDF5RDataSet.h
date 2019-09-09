/*
 * HDF5RDataSet.h
 *
 *  Created on: May 2, 2018
 *      Author: nielson
 */

#ifndef FRMTS_HDF5R_HDF5RDATASET_H_
#define FRMTS_HDF5R_HDF5RDATASET_H_

#include "hdf5.h"

#include "cpl_list.h"
#include "cpl_progress.h"
#include "gdal_pam.h"

#include "hdf5r.h"
#include "Earth.h"
#include "Hdf5rFrameData.h"
#include "Hdf5rLosGrid.h"
#include "Hdf5rReader.h"
#include "Hdf5rWriter.h"
#include "CreationOptions.h"

/**
 * @brief HDF5-R GDAL DataSet
 * This is the GDAL driver base class for HDF5-R (Raster files) used by OPIR
 * systems which contain images and line-of-sight data for a sequence of looks
 * from a single sensor. Typically, multiple files are required for full
 * field-of-view images.
 * The GDAL driver static methods Open() and Identify() for this class are
 * for processing the base file.  This class will create a SUBDATASET entry
 * for each image in the HDF5-R file.  Individual SUBDATASET images are loaded
 * by the HDF5RSubDataSet derived class.
 */
class HDF5RDataSet: public GDALPamDataset
{
public:
    /**
     * Default constructor
     */
    HDF5RDataSet();

    /**
     * Destructor
     */
    virtual ~HDF5RDataSet();

    friend class HDF5RRasterBand;

    static const char* OpenOptionsXML;
    static const char* CreationOptionsXML;

    /**
     * Required static Open method required for all GDAL drivers.  This
     * method opens the HDF5-R file and creates a SUBDATASET attribute
     * for each image in the file.
     * @param gdalInfo File information class for the file to open.
     * @return On success an HDF5RDataSet super-class of GDALDataSet.
     */
    static GDALDataset* Open( GDALOpenInfo* gdalInfo );

    /**
     * Required static method required for all GDAL drivers. The identity
     * decision for HDF5-R certifies that the file is valid hdf5 using the
     * hdf5 library function H5Fis_hdf5 and contains two specific file level
     * attributes in HDF5-R: "SCID" and "SCA".  Note that this driver must
     * precede the other GDAL HDF5 drivers because their Identify() methods
     * yield false positives for HDF5-R.
     * @param gdalInfo
     * @return 1 (true) on HDF5-R identity match, 0 otherwise.
     */
    static int Identify( GDALOpenInfo* gdalInfo );

    /**
     * Create a copy of the poSrcDS (which can be from any GDAL driver)
     * and write it out in HDF5-R format, then use the Open() method to read
     * this new data set as the return value.  While the raster
     * data in the copied GDAL data set is a true copy of the original,
     * the meta-data is not in general.  The gdal_translate command uses
     * this method to build the output dataset.
     *
     * @param pszFilename the name for the new dataset.  UTF-8 encoded.
     * @param poSrcDS the dataset being duplicated.
     * @param bStrict TRUE if the copy must be strictly equivalent, or more
     * normally FALSE indicating that the copy may adapt as needed for the
     * output format.
     * @param papszOptions additional format dependent options controlling
     * creation of the output file. The APPEND_SUBDATASET=YES option can be
     * specified to avoid prior destruction of existing dataset.
     * @param pfnProgress a function to be used to report progress of the copy.
     * @param pProgressData application data passed into progress function.
     *
     * @return a pointer to the newly created dataset (may be read-only access).
     */
    static GDALDataset* CreateCopy( const char * pszFilename,
                                    GDALDataset * poSrcDS,
                                    int bStrict, char ** papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void * pProgressData );

    /**
     * Create an an HDF5-R file from scratch. The caller uses the
     * HDF5RRasterBand::IWriteBlock() method to write image data.
     * The HDF5-R attributes are set from GDAL attributes, command line
     * attributes (using the -co option on most gdal commands), and
     * attributes derived from the image and Line-of-sight grid.     *
     * @param pszFileName the name for the new dataset.  UTF-8 encoded.
     * @param nXSize X dimension which corresponds to pixels, channels, or
     *               columns.
     * @param nYSize Y dimension which corresponds to scan lines or rows
     * @param nBandsv Number of bands.  Must be 1 for HDF5-R.
     * @param etype Data type, must be GDT_CInt32 for HDF5-R.
     * @param papszOptions Array of null-terminated pointers to NAME=VALUE
     *                     c-strings. Typically passed on gdal command lines
     *                     using the -co switch.
     * @return a pointer to the newly created dataset in R/W mode.
     */
    static GDALDataset* Create( const char* pszFileName,
                                int nXSize,
                                int nYSize,
                                int nBands,
                                GDALDataType etype,
                                char** papszOptions );

    /**
     * \fn GDALDataset::SetGeoTransform(double*)
     * \brief Set the affine transformation coefficients.
     * See GetGeoTransform() for details on the meaning of the padfTransform
     * coefficients.
     * @param padfTransform a six double buffer containing the transformation
     *                      coefficients to be written with the dataset.
     * @return CE_None on success, or CE_Failure if this transform cannot be
     *         written.
     */
    virtual CPLErr SetGeoTransform( double* padTransform ) override;

    /**
     * \brief Fetch the affine transformation coefficients.
     *
     * Fetches the coefficients for transforming between pixel/line (P,L) raster
     * space, and projection coordinates (Xp,Yp) space.
     *
     * \code
     *   Xp = padfTransform[0] + P*padfTransform[1] + L*padfTransform[2];
     *   Yp = padfTransform[3] + P*padfTransform[4] + L*padfTransform[5];
     * \endcode
     *
     * In a north up image, padfTransform[1] is the pixel width, and
     * padfTransform[5] is the pixel height.  The upper left corner of the
     * upper left pixel is at position (padfTransform[0],padfTransform[3]).
     *
     * The default transform is (0,1,0,0,0,1).
     *
     * @param padfTransform an existing six double buffer into which the
     * transformation will be placed.
     *
     * @return CE_None on success, or CE_Failure has not been explicitly set
     *         with SetGeoTransform.
     */
    virtual CPLErr GetGeoTransform( double* padTransform ) override;

protected: // methods

    Hdf5rReader* getHdf5rReader() {return hdf5rReader_;}

    /**
     * This method builds the GDAL metadata for SUBDATASET_NAME and
     * SUBDATASET_DESC for all frames in the HDF5-R file.  These name-value
     * pairs are then loaded into the SUBDATASETS domain.
     * @return Number of subdatasets (frames) in HDF5_R file
     */
    int setSubDataSetAttributes();

    const Earth& getEarthModel() const {return earth_;}

    // Structure for SUBDATASET file descriptor
    // (does not include ':' separators)
    struct Hdf5rSubDataDesc_t
    {
        std::string hdr;
        std::string fileName;
        int frameIndex;
        bool filled;

        /** Default constructor */
        Hdf5rSubDataDesc_t()
        : hdr(), fileName(), frameIndex(-1), filled(false)
        {}
    };

    void setHdf5rReader( Hdf5rReader* hdf5rReader ) {hdf5rReader_ = hdf5rReader;}

    /**
     * Iterate through the list of pointer to GDAL name=value strings
     * and insert new values in the File Attributes or Frame Data members that
     * with matching names.
     * @param cstrArray NULL terminated GDAL name=value strings
     * @param fileAttributes Pointer to an instance of Hdf5rFileAttributes
     * @param what A string to identify the caller in debug output
     * @return Number of values loaded into either map.
     */
    static int loadMapsFromMetadataList( char* const* cstrArray,
                                         std::vector<Hdf5rAttributeBase*> attributes,
                                         Hdf5rFrameData* frameData,
                                         const char* what,
                                         const char* prefix = nullptr );

    /**
     * Static method to parse the SUBDATASET descriptor into components
     * separated by ':'
     * - hdr: always 'HDF5R'
     * - fileName: Name of the base HDF5-R file, it is the same for all
     *             SUBDATASETs
     * - frameIndex: Index of an individual frame
     * @param descStr: Descriptor string
     * @param sdesc: A Hdf5rSubDataDesc_t to fill
     * @return Boolean true if all elements of the descriptor are found, also
     *         sets the 'filled' boolean of the structure.
     */
    static bool parseSubDataDescriptor( const std::string& descStr, Hdf5rSubDataDesc_t* sdesc );

    /**
     * Get a pointer to the image data for a single raster band.
     * @param poSrcDS Source dataset.
     * @param minMaxPtr pointer to a location to store the raster min/max array.
     *                  If nullptr, then the min/max is not computed. Destination
     *                  must be deleted with CPLFree.  Location is set to
     *                  nullptr if GDAL cannot compute the min/max.
     * @return On success, a pointer to the image array, otherwise nullptr.
     */
    static GInt32* getGdalSingleRaster( GDALDataset* poSrcDS,
                                        double** minMaxPtr = nullptr );

    static OGRCoordinateTransformation* getLatLonTransform( GDALDataset* poSrcDS );

    static OGRCoordinateTransformation* getGCPLatLonTransform( GDALDataset* poSrcDS );

    static Hdf5rLosGrid_t* buildLosGrid( GDALDataset* poSrcDS,
                                         int xStepSize,
                                         int yStepSize,
                                         int gcpOrder,
                                         bool noGcp,
                                         bool reGrid,
                                         double satEcfMeters[3],
                                         const Earth& earth );

    static bool gcpConvertToLatLong( GDALDataset* poSrcDS,
                                     int count,
                                     double* x,
                                     double* y,
                                     double* z,
                                     int* status,
                                     int gcpOrder );

    static bool affineConvertToLatLong( GDALDataset* poSrcDS, int count, double* x, double* y, int* status );

    static bool loadGcpGridDirect( GDALDataset* poSrcDS,
                                   int* xGridSizeIn, int* yGridSizeIn,
                                   int* xStepSize, int* yStepSize,
                                   double** x, double** y, double** z,
                                   int** status );

    static void setSingleFrameMetaData( const Hdf5rFrameData* frameData,
                                        const Hdf5rLosGrid_t* losGrid,
                                        Hdf5rFileAttributes* fileAttributes,
                                        std::vector<const CompoundBase*>* errorInfoVect,
                                        std::vector<const CompoundBase*>* seqInfoVect );

    // Load the command line -co options for both CreateCopy() and Create()
    static CreationOptions* loadCreationOptions( char** papszOptions );

    static void setCreateAttributes( GDALDataset* poSrcDS,
                                     Hdf5rLosGrid_t* losGrid,
                                     Hdf5rGeoLocAttributes* geoLocAttributes,
                                     Hdf5rFrameData::FrameData_t* frameData );

    void setCreateAttributes( Hdf5rLosGrid_t* losGrid,
                              Hdf5rGeoLocAttributes* geoLocAttributes,
                              Hdf5rFrameData::FrameData_t* frameData )
    {
        setCreateAttributes( this, losGrid, geoLocAttributes,frameData );
    }

    void finalizeHdf5rWrite();

protected: // variables

    Hdf5rReader* hdf5rReader_;
    Hdf5rWriter* hdf5rWriter_;

    // Pointer to name-value list using the CSLSetNameValue and friends
    char** subDataNameValueList_;

    // whether or not using GCPs instead of the affine transform
    bool haveGcps_;
    bool useAffineXform_;

    // OGC WKT Projection String set by setWgs84OgrSpatialRef or similar
    std::string ogcWktProjectionInfo_;

    // GDAL transform as linear 6 element array
    static const int GDAL_XFORM_SZ = 6;
    double gdalTransform_[GDAL_XFORM_SZ];

    // Earth model
    // TODO Add constructor parameters to set Earth model parameters
    Earth earth_;

    // Creation used by the Create() method
    CreationOptions* creationOptions_;
};

#endif /* FRMTS_HDF5R_HDF5RDATASET_H_ */
