/*
 * HDF5RDataSet.cc
 *
 *  Created on: May 2, 2018
 *      Author: nielson
 */

#define H5_USE_16_API

#include <sstream>
#include <iostream>
#include <limits>
#include <cstring>

#include "hdf5.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "gdal.h"
#include "gdal_frmts.h"
#include "gdal_priv.h"
#include "gdal_alg.h"

#include "HDF5RDataSet.h"
#include "Hdf5rFileAttributes.h"
#include "Hdf5rGeoLocAttributes.h"
#include "Hdf5rSummaryMetaData.h"
#include "HDF5RRasterBand.h"

CPL_C_START
void GDALRegister_HDF5R(void);
CPL_C_END


const char* HDF5RDataSet::OpenOptionsXML =
        "<OpenOptionList>"
        "   <Option name='GCP_MAX'"
        "             type='unsigned int' min='0' default='225'"
        "             description='Max GCPs from GeoLocationData, 0==no limit'/>"
        "   <Option name='NO_GCP'"
        "             type='unsigned int' min='0' max='1' default='0'"
        "             description='0==generate GCPs, 1==generate affine xform'/>"
        "   <Option name='ATTR_RW'"
        "             type='unsigned int' min='0' max='1' default='1'"
        "             description='0==use file values, 1==use single frame values' />"
        "   <Option name='BLANK_OFF_EARTH'"
        "             type='unsigned int' min='0' max='1' default='1'"
        "             description='0==do nothing, 1==set off-Earth pixels to the NODATA value' />"
        "   <Option name='SAT_LON'"
        "             type='float' min='-180.0' max='180.0'"
        "             description='Recalculate GEO grid using Geosync satellite at this longitude (degrees)' />"
        "</OpenOptionList>";

const char* HDF5RDataSet::CreationOptionsXML =
        "<CreationOptionList>"
        "   <Option name='GCP_REGRID'"
        "             type='unsigned int' min='0' max='1' default='0'"
        "             description='0==use source GCP grid, 1==always resample the grid'/>"
        "   <Option name='NO_GCP'"
        "             type='unsigned int' min='0' max='1' default='0'"
        "             description='0==use GCPs if available, 1==use affine xform'/>"
        "   <Option name='GCP_ORDER'"
        "             type='unsigned int' min='0'         default='0'"
        "             description='GCP polynomial transform order per GDAL [0,1,...N]'/>"
        "</CreationOptionList>";

//******************************************************************************
// GDAL Plugin Registration Function (not a class method)
//   declaration occurs in: gdal/gcore/gdal_frmts.h
//******************************************************************************
void GDALRegister_HDF5R()
{
    if( GDALGetDriverByName("HDF5R") != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("HDF5R");

#ifdef GDAL_DCAP_RASTER
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
#endif

    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");

    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Hierarchical Data Format Release 5 for OPIR Raster Data");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_hdf5r.html");
#ifdef GDAL_DMD_EXTENSIONS
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "h5 hdf5 h5r hdf5r");
#endif
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");

    // Creation and Open options expressed in XML
    //   - As of GDAL 2.3 they are validated with -co
    //   - gdalinfo --format hdf5r displays the XML
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
                               HDF5RDataSet::CreationOptionsXML );

    poDriver->pfnOpen = HDF5RDataSet::Open;
    poDriver->pfnIdentify = HDF5RDataSet::Identify;
    poDriver->pfnCreateCopy = HDF5RDataSet::CreateCopy;
    GetGDALDriverManager()->RegisterDriver(poDriver);

    // Create()
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
    poDriver->pfnCreate = HDF5RDataSet::Create;
}

//******************************************************************************
// Constructor
//******************************************************************************
HDF5RDataSet::HDF5RDataSet() :
        hdf5rReader_( nullptr ),
        hdf5rWriter_( nullptr ),
        subDataNameValueList_(nullptr),
        haveGcps_( false ),
        useAffineXform_( false ),
        ogcWktProjectionInfo_( "" ),
        gdalTransform_{0.0, 1.0, 0.0, 0.0, 0.0, 1.0},
        earth_(), // WGS-84
        creationOptions_( nullptr )
{
}

//******************************************************************************
// Destructor
//******************************************************************************
HDF5RDataSet::~HDF5RDataSet()
{
    // if in read-write mode and an hdf5rWriter is present, then we are
    // closing out after a Create() call to the driver
    // time to build and write all datasets except calRawRaster which was
    // written by HDF5RRasterBand::IWriteBlock()
    if (hdf5rWriter_ && (eAccess == GA_Update) )
        finalizeHdf5rWrite();

    // memory cleanup
    CSLDestroy( subDataNameValueList_ );
    subDataNameValueList_ = nullptr;

    delete hdf5rReader_;
    delete hdf5rWriter_;
    delete creationOptions_;
}

//******************************************************************************
// Static Open Method
//   - required of all plugins
//   - Returns a GDAL DataSet if file matches basic tests and loads
//     successfully, nullptr otherwise
//******************************************************************************
GDALDataset* HDF5RDataSet::Open(GDALOpenInfo* gdalInfo)
{
    std::string fileName = gdalInfo->pszFilename;

    CPLDebug( HDF5R_DEBUG_STR, "HDF5RDataSet::Open() called for: %s", fileName.c_str() );

    if (!Identify(gdalInfo))
        return nullptr;

    // Open the HDF-R file and internal components, also gets the number
    // of image frames
    Hdf5rReader* hdf5rReader = new Hdf5rReader;
    if (!hdf5rReader->open( fileName ))
    {
        delete hdf5rReader;
        return nullptr;
    }

    // Create DataSet
    HDF5RDataSet* poDS = new HDF5RDataSet();

    // Make sure we don't try to do any pam stuff with this dataset.
    poDS->nPamFlags |= GPF_NOSAVE;

    // set some attributes in the GDAL dataset class
    poDS->setHdf5rReader( hdf5rReader );
    poDS->SetDescription( gdalInfo->pszFilename );

    // Create SUBDATASET attribute for each frame
    // if 1 SUBDATSET, the open it now
    if (poDS->setSubDataSetAttributes() == 1)
    {
        const char* osDSName = CSLFetchNameValue( poDS->subDataNameValueList_,
                                                  "SUBDATASET_0_NAME" );

        // if nullptr returned (really should not happen -- revert to showing
        //    the original dataset)
        if (osDSName)
        {
            // need a copy of osDSName since it is in the GDAL data-set we
            // are about to delete
            std::string subDs = osDSName;

            std::cout << "HDF5-R INFO: Opening single SUBDATASET: "
                    <<  osDSName << std::endl;

            delete poDS;
            poDS = static_cast<HDF5RDataSet*>(GDALOpenEx( subDs.c_str(),
                                                          gdalInfo->eAccess,
                                                          nullptr,
                                                          gdalInfo->papszOpenOptions,
                                                          nullptr ));
        }
    }

    return poDS;
}

//******************************************************************************
//* CreateCopy writes and HDF5R file and returns a pointer to it
//******************************************************************************
GDALDataset* HDF5RDataSet::CreateCopy( const char* pszFilename,
                                       GDALDataset* poSrcDS,
                                       int bStrict,
                                       char** papszOptions,
                                       GDALProgressFunc pfnProgress,
                                       void* pProgressData )
{
    // -------------------------------------------------------------------------
    // Initialization
    // -------------------------------------------------------------------------
    CPLDebug( HDF5R_DEBUG_STR, "HDF5RDataSet::CreateCopy() called for: %s", pszFilename );

    // init return value
    GDALDataset* rc = nullptr;

    // report initial progress
    if (pfnProgress)
        pfnProgress( 0.0, "Starting copy...", pProgressData );

    // instantiate the default HDF5-R file and GeoLocation attributes
    Hdf5rFileAttributes* fileAttributes = new Hdf5rFileAttributes;
    Hdf5rGeoLocAttributes* geoLocAttributes = new Hdf5rGeoLocAttributes;

    // instantiate the FrameMetaData default values and pointer to the structure
    Hdf5rFrameData* hdf5rFrameData = new Hdf5rFrameData;
    Hdf5rFrameData::FrameData_t* frameData = hdf5rFrameData->getFrameDataPtr();

    // instantiate vectors for summary metadata (filled by setSingleFrameMetaData)
    std::vector<const CompoundBase*> errorInfoVect;
    std::vector<const CompoundBase*> seqInfoVect;

    // -------------------------------------------------------------------------
    // Load the command line creation option NAME=VALUE pairs
    // -------------------------------------------------------------------------
    CreationOptions* creationOptions = loadCreationOptions( papszOptions );

    int32_t gcpOrder = 0;      // 0==use highest reliable order polynomial fit
    int32_t gcpRegrid = FALSE; // do not resample the grid if GCPs form a valid grid already
                               // if TRUE, then force a resample using the polynomial fit
    int32_t noGcp = FALSE;     // FALSE == use GCP's if available, TRUE == do not use GCPs
    creationOptions->getValue( "GCP_ORDER", &gcpOrder );
    creationOptions->getValue( "GCP_REGRID", &gcpRegrid );
    creationOptions->getValue( "NO_GCP", &noGcp );

    // -------------------------------------------------------------------------
    // Load Attributes and Frame Meta data
    // -------------------------------------------------------------------------
    // Load source data set file attributes and FrameMetaData
    // (at this point only if the source GDAL data set is also HDF5-R will these
    //  attributes be present in the data set)
    std::vector<Hdf5rAttributeBase*> attrLists;
    attrLists.push_back( fileAttributes );
    attrLists.push_back( geoLocAttributes );
    loadMapsFromMetadataList( poSrcDS->GetMetadata(),
                              attrLists,
                              hdf5rFrameData,
                              "Source metadata",
                              "H5R." ); // required prefix

    // -------------------------------------------------------------------------
    // Build Geo Location Grid
    // -------------------------------------------------------------------------
    // Get geolocation step size from GDAL attributes
    int32_t xStepSize = 20;
    geoLocAttributes->getValue( "H5R.GEO.X_Stepsize_Pixels", &xStepSize );

    int32_t yStepSize = 20;
    geoLocAttributes->getValue( "H5R.GEO.Y_Stepsize_Pixels", &yStepSize );

    // build the LOS grid using GDAL provided transforms
    // note satPosECF HDF5-R attribute is modified if it has a
    // magnitude less than the Earth's radius
    // Default Earth model
    Earth earth;
    Hdf5rLosGrid_t* losGrid = buildLosGrid( poSrcDS,
                                            xStepSize, yStepSize,
                                            gcpOrder, noGcp, gcpRegrid,
                                            frameData->satPosECF,
                                            earth );

    // -------------------------------------------------------------------------
    // Get the source single band image
    // -------------------------------------------------------------------------
    // Get a copy of the GDAL raster image in HDF5-R format (int32_t)
    double* rasterMinMax = nullptr;
    GInt32* hdf5rImage = getGdalSingleRaster( poSrcDS, &rasterMinMax );

    if (rasterMinMax)
    {
        frameData->minCalIntensity = int32_t( rasterMinMax[0] );
        frameData->maxCalIntensity = int32_t( rasterMinMax[1] );
    }

    // -------------------------------------------------------------------------
    // Load single frame values into various HDF5-R meta data items
    // -------------------------------------------------------------------------
    setCreateAttributes( poSrcDS,
                         losGrid,
                         geoLocAttributes,
                         frameData );

    setSingleFrameMetaData( hdf5rFrameData,
                            losGrid,
                            fileAttributes,
                            &errorInfoVect,
                            &seqInfoVect );

    // -------------------------------------------------------------------------
    // Write the file
    //     -- minimum requirement is the image
    //     -- bstrict: image, losgrid, and satellite location
    // -------------------------------------------------------------------------
    if (!hdf5rImage)
        CPLError( CE_Failure, CPLE_AppDefined, "HDF5RDataSet::CreateCopy: "
                  " Cannot create HDF5-R output - image not available of invalid" );
    else if (bStrict && (!losGrid))
        CPLError( CE_Failure, CPLE_AppDefined, "HDF5RDataSet::CreateCopy: "
                  " Cannot create HDF5-R output - bStrict && GCPs or Affine transform invalid." );
    else if (bStrict && (m3d::Vector( frameData->satPosECF ).magnitude() <= 0.0 ))
        CPLError( CE_Failure, CPLE_AppDefined, "HDF5RDataSet::CreateCopy: "
                  " Cannot create HDF5-R output - bStrict and Satellite location not valid" );
    else // good to go
    {
        // instantiate the writer and open the new file for writing
        Hdf5rWriter* hdf5rWriter = new Hdf5rWriter();
        if (hdf5rWriter->open( pszFilename ))
        {
            // Write the HDF5-R image data set
            hdf5rWriter->writeImage( poSrcDS->GetRasterYSize(),
                                     poSrcDS->GetRasterXSize(),
                                     hdf5rImage );

            // Write the LOS grid data set
            hdf5rWriter->writeLosGrid( losGrid, geoLocAttributes );

            // write the frame attributes compound data set
            hdf5rWriter->setFrameDataFromMap( hdf5rFrameData );

            // write the file attributes
            hdf5rWriter->setFileAttrsFromMap( fileAttributes->getConstAttrMap() );

            // write the summary metadata
            hdf5rWriter->setSummaryDataFromMap( errorInfoVect, seqInfoVect );

            // close the output file
            hdf5rWriter->close();

            // open the file we just wrote and return the dataset as the copy
            std::string subDataSetName = std::string( "HDF5R:" ) + pszFilename + std::string( ":0" );
            CPLDebug( HDF5R_DEBUG_STR,
                      "HDF5RDataSet::CreateCopy() GDALOpen called for new copy: %s",
                      subDataSetName.c_str() );

            rc = static_cast<GDALDataset*>(GDALOpen( subDataSetName.c_str(), GA_ReadOnly ));
        }

        // cleanup
        delete losGrid;
        CPLFree( rasterMinMax );
        delete hdf5rWriter;
        CPLFree( hdf5rImage );
        delete hdf5rFrameData;
        delete fileAttributes;
        delete geoLocAttributes;
        delete creationOptions;

        for (unsigned i=0; i<errorInfoVect.size(); ++i)
            delete errorInfoVect[i];

        for (unsigned i=0; i<seqInfoVect.size(); ++i)
            delete seqInfoVect[i];
    }

    // report final progress
    if (pfnProgress)
        pfnProgress( 1.0, "Copy complete.", pProgressData );

    return rc;
}

//******************************************************************************
//* Create method: Build HDF5-R file from scratch
//******************************************************************************
GDALDataset* HDF5RDataSet::Create( const char* pszFileName,
                                   int nXSize,
                                   int nYSize,
                                   int nBands,
                                   GDALDataType etype,
                                   char** papszOptions )
{
    CPLDebug( HDF5R_DEBUG_STR, "HDF5RDataSet::Create() called for: %s\n"
              "\tnXSize: %d nYSize: %d nBands: %d etype: %d\n "
              "\tOptions: \n",
              pszFileName, nXSize, nYSize, nBands, etype );

    // debug print of the create options (-co)
    if (papszOptions)
    {
        char** optr = papszOptions;
        while (optr != nullptr)
            CPLDebug( HDF5R_DEBUG_STR, "\t%s", *optr );
    }

    // HDF5-R supports 1 band -- otherwise error
    if (nBands != 1)
    {
        CPLError( CE_Failure, CPLE_AppDefined, "HDF5R Create() supports a single band." );
        return nullptr;
    }

    // HDF5-R supports GDT_Int32 -- otherwise error
    if (etype != GDT_Int32)
    {
        CPLError( CE_Failure, CPLE_AppDefined, "HDF5R Create() supports a Int32 only." );
        return nullptr;
    }

    // instantiate the default HDF5-R file and GeoLocation attributes
    Hdf5rFileAttributes* fileAttributes = new Hdf5rFileAttributes;

    // instantiate the writer and open the new file for writing
    // with minimum attributes so that it will be recognized as HDF5-R
    Hdf5rWriter* hdf5rWriter = new Hdf5rWriter();
    if (hdf5rWriter->open( pszFileName ))
    {
        // write the file attributes
        hdf5rWriter->setFileAttrsFromMap( fileAttributes->getConstAttrMap() );

        // close the output file
        hdf5rWriter->close();
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "HDF5R Create() Unable to open dummy file for writing.\n" );
        return nullptr;
    }

    // cleanup
    delete fileAttributes;

    // open the data set just created in update mode
    HDF5RDataSet* hdf5rDS = static_cast<HDF5RDataSet*>(GDALOpen( pszFileName,
                                                                GA_Update ));

    // set the GDAL data-set values we know at this point
    if (hdf5rDS)
    {
        // close the temporary read-only test file and reopen it for
        // truncate and write
        hdf5rDS->hdf5rReader_->close();
        hdf5rWriter->open( pszFileName );
        hdf5rDS->hdf5rWriter_ = hdf5rWriter;

        hdf5rDS->nRasterXSize = nXSize;
        hdf5rDS->nRasterYSize = nYSize;
        hdf5rDS->nBands = 1;
        hdf5rDS->eAccess = GA_Update;

        // Load the command line creation option NAME=VALUE pairs
        //    for the finalizeHdf5rWrite(), deleted by destructor
        hdf5rDS->creationOptions_ = loadCreationOptions( papszOptions );

        // Create the GDAL RasterBand -- data is not loaded until the
        // IWriteBlock method() is called, so no status to check here
        hdf5rDS->SetBand( 1, new HDF5RRasterBand( hdf5rDS,          // pointer to the DataSet
                                                  1,                // band number starts at 1
                                                  0,                // frame index starts at 0
                                                  nYSize,           // rows a.k.a lines
                                                  nXSize,           // cols a.k.a detectors in line
                                                  GA_Update ) );    // R/W access
    }
    else
    {
        delete hdf5rWriter;
        CPLError( CE_Failure, CPLE_AppDefined,
                  "HDF5R Create() Unable to open dummy HDF5-R file that we just wrote.\n" );
    }

    return hdf5rDS;
}

//******************************************************************************
//* Load the name-value pairs passed on the -co flag for GDAL commands
//******************************************************************************
CreationOptions* HDF5RDataSet::loadCreationOptions( char** papszOptions )
{
    CreationOptions* creationOptions = new CreationOptions;

    std::vector<Hdf5rAttributeBase*> attrLists;
    attrLists.push_back( creationOptions );

    loadMapsFromMetadataList( papszOptions,
                              attrLists,
                              nullptr,
                              "Cmdline Create() option" );

    return creationOptions;
}

//******************************************************************************
//* Using the metadata name-value pairs, modify any of the matching map values
//******************************************************************************
void HDF5RDataSet::setCreateAttributes( GDALDataset* poSrcDS,
                                       Hdf5rLosGrid_t* losGrid,
                                       Hdf5rGeoLocAttributes* geoLocAttributes,
                                       Hdf5rFrameData::FrameData_t* frameData )
{
    // set frame attributes settings for image size (if source is an
    // HDF5-R values then settings should be unchanged)
    // GDAL pixels == X == channel == column
    frameData->beginChannel = 0;
    frameData->endChannel = poSrcDS->GetRasterXSize(); // index following last channel/pixel/col (like stl)
    frameData->numChannels = poSrcDS->GetRasterXSize();

    // GDAL lines == Y == lines == rows
    frameData->beginLine = 0;
    frameData->endLine = poSrcDS->GetRasterYSize();   // index following last line/row (like stl)
    frameData->numLines = poSrcDS->GetRasterYSize();

    if (losGrid)
    {
        // use actual step size for the grid to set attribute values
        geoLocAttributes->setValue( "H5R.GEO.X_Stepsize_Pixels",
                                    losGrid->getColStepSize() );

        geoLocAttributes->setValue( "H5R.GEO.Y_Stepsize_Pixels",
                                    losGrid->getRowStepSize() );

        // frame attributes for sizes and bounds that are available from the grid
        //    - ICD size does not extend beyond the left and bottom sides
        //    - when loaded the bounding row and column are extrapolated
        //        so all pixels locations may be interpolated
        //    - Dimensions reduced here so frame data matches what we will write
        int ncols = losGrid->getNcols() - 1;
        int nrows = losGrid->getNrows() - 1;
        frameData->numGeoPoints = ncols * nrows;

        // get references to each corner of the grid
        //   - note due to the reduction by one for nrows and ncols above,
        //      these are now the end indexes of the grid
        const Hdf5rLosGrid_t::Hdf5rLosData_t& UL = (*losGrid)( 0, 0 );
        frameData->UL_lat = UL.map_Y;  // Y ==> latitude
        frameData->UL_lon = UL.map_X;  // X ==> longitude

        const Hdf5rLosGrid_t::Hdf5rLosData_t& LL = (*losGrid)( nrows, 0 );
        frameData->LL_lat = LL.map_Y;  // Y ==> latitude
        frameData->LL_lon = LL.map_X;  // X ==> longitude

        const Hdf5rLosGrid_t::Hdf5rLosData_t& UR = (*losGrid)( 0, ncols );
        frameData->UR_lat = UR.map_Y;  // Y ==> latitude
        frameData->UR_lon = UR.map_X;  // X ==> longitude

        const Hdf5rLosGrid_t::Hdf5rLosData_t& LR = (*losGrid)( nrows, ncols );
        frameData->LR_lat = LR.map_Y;  // Y ==> latitude
        frameData->LR_lon = LR.map_X;  // X ==> longitude
    }
}

//******************************************************************************
//* Using the metadata name-value pairs, modify any of the matching map values
//******************************************************************************
int HDF5RDataSet::loadMapsFromMetadataList( char* const* cstrArray,
                                            std::vector<Hdf5rAttributeBase*> attributes,
                                            Hdf5rFrameData* frameData,
                                            const char* what,
                                            const char* prefix )
{
    int nLoaded = 0;

    if (cstrArray != nullptr)
    {
        // loop over all c-string pointers in the array
        const char* mdItem = *cstrArray++;
        while (mdItem != nullptr)
        {
            // Use the GDAL supplied name=value parser
            char* name = nullptr;
            const char* value = CPLParseNameValue( mdItem, &name );

            // good name and value pointers
            if (name && value)
            {
                CPLDebug( HDF5R_DEBUG_STR,
                          "HDF5RDataSet::loadMapsFromMetadataList() %s name: %s value: %s",
                          what, name, value );

                // only check maps if name starts with prefix (if defined)
                if ((prefix == nullptr) || (0 == strncmp( name, prefix, 4 )))
                {
                    // Test the name against the attribute and frame data maps
                    // this logic assumes that once a name matches it is done
                    bool valueUsed = false;

                    // test the name against all attributes lists (until matched)
                    for( unsigned i=0; i<attributes.size() && !valueUsed; ++i)
                        valueUsed = attributes[i]->modifyValue( name, value );

                    if (valueUsed)
                    {
                        CPLDebug( HDF5R_DEBUG_STR, "     attribute map value found and modified" );
                        ++nLoaded;
                    }
                    // iterate over the frame data list -- if not already matched
                    else if (frameData && (frameData->modifyValue( name, value )))
                    {
                        CPLDebug( HDF5R_DEBUG_STR, "     frame data map value found and modified" );
                        ++nLoaded;
                    }

                    // ignore Summary Metadata -- these attributes are for info
                    // only and are set to single frame values on write
                    else if (std::strncmp( name,
                                           ErrorInfoTable::ERROR_INFO_PREFIX,
                                           ErrorInfoTable::ERROR_INFO_PREFIX_SZ )
                    || std::strncmp( name,
                                     SeqInfoTable::SEQ_INFO_PREFIX,
                                     SeqInfoTable::SEQ_INFO_PREFIX_SZ ))
                    {
                        CPLDebug( HDF5R_DEBUG_STR, "     Summary metadata name ignored" );
                    }
                    else
                    {
                        CPLDebug( HDF5R_DEBUG_STR, "     map name not found in map" );
                        CPLError( CE_Warning, CPLE_AppDefined,
                                  "GDAL metadata NAME=%s does not match any "
                                  " known HDF5-R attribute name. Ignored.",
                                  name );
                    }
                }
            }
            else
                CPLDebug( HDF5R_DEBUG_STR,
                          "HDF5RDataSet::loadMapsFromMetadataList() %s failed parse: %s",
                          what, mdItem );

            if (name)
                VSIFree( name );

            // on to the next c-string pointer in the list
            mdItem = *cstrArray++;
        }
    }

    // return number loaded into maps
    return nLoaded;
}

//******************************************************************************
//* Get the GDAL image band -- after some basic checks
//*    -- need to use CPLFree on the returned result
//******************************************************************************
GInt32* HDF5RDataSet::getGdalSingleRaster(  GDALDataset* poSrcDS,
                                            double** minMaxPtr )
{
    // Get the GDAL image characteristics
    int nrows = poSrcDS->GetRasterYSize();
    int ncols = poSrcDS->GetRasterXSize();
    int nbands = poSrcDS->GetRasterCount();

    // Verify dimensions are positive, non-zero
    if ((nrows <= 0) || (ncols <= 0))
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "HDF5-R driver: Image to copy has non-positive dimension(s):"
                  "rows: %d  cols: %d", nrows, ncols );
        return nullptr;
    }

    // Verify 1 band
    if (nbands != 1)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "HDF5-R driver requires single grey scale band. "
                  "This one has %d bands.", nbands );
        return nullptr;
    }

    // allocate space for the HDF5-R image buffer as int32_t
    int imageSz = nrows * ncols;
    GInt32* hdf5rImage = static_cast<GInt32*>( CPLMalloc( imageSz * sizeof(GInt32) ) );

    // GDAL RasterIO method handles conversion (and more)
    GDALRasterBand* poBand = poSrcDS->GetRasterBand( 1 );
    CPLErr iorc = poBand->RasterIO( GF_Read,        // read from GDAL into local buffer
                                    0, 0,           // nXOff, nYOff (upper left corner)
                                    ncols, nrows,   // nXsize by nYsize to read from
                                    hdf5rImage,     // output buffer
                                    ncols, nrows,   // nBUfXSize, nBufYSize
                                    GDT_Int32,      // nPixelSpace for HDF5-R
                                    0, 0,           // nPixelSpace, nLineSpace
                                    nullptr );      // psExtraArg

    if (iorc != CE_None)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "HDF5-R driver failed to get GDAL image (RasterIO)." );
        CPLFree( hdf5rImage );
        hdf5rImage = nullptr;
    }
    else
    {
        // get Raster min/max values if requested (FALSE requests exact)
        if (minMaxPtr)
        {
            double* minMax = static_cast<double*>( CPLMalloc( 2 * sizeof(double) ) );
            if ( poBand->ComputeRasterMinMax( FALSE, minMax ) == CE_None)
                *minMaxPtr = minMax;
            else
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "HDF5-R driver: GDAL ComputeRasterMinMax failed." );
                CPLFree( minMax );
            }
        }
    }

    return hdf5rImage;
}

//******************************************************************************
// Static Identity Method
//   - required of all plugins
//   - Returns a 1 if file matches basic tests, 0 otherwise
//******************************************************************************
int HDF5RDataSet::Identify(GDALOpenInfo* gdalInfo)
{
    std::string fileName = gdalInfo->pszFilename;

    // is this an hdf5-r SUBDATASET?
    if (fileName.substr( 0, 6 ) == "HDF5R:")
    {
        CPLDebug( HDF5R_DEBUG_STR, "HDF5RDataSet::Identify() This looks like a SUBDATASET" );
        return GDAL_IDENTIFY_FALSE;
    }

    int rc = GDAL_IDENTIFY_FALSE;

    // If filename exists then use hdf5 library function to verify it is hdf5
    if ((gdalInfo->pszFilename != nullptr) && gdalInfo->bStatOK && (H5Fis_hdf5(gdalInfo->pszFilename) > 0))
    {
        // open the file and verify HDF5-R top level attributes are present
        hid_t h5Hid = H5Fopen(gdalInfo->pszFilename, H5F_ACC_RDONLY, H5P_DEFAULT );
        if (h5Hid >= 0)
        {
            hid_t rootGroupId = H5Gopen(h5Hid, "/" );
            if (rootGroupId >= 0)
            {
                // test for SCID and SCA attributes
                if (H5Aexists(rootGroupId, "SCID")
                        && H5Aexists(rootGroupId, "SCA"))
                {
                    float versionNum = 0.0f;
                    if (Hdf5rReader::getAttribute( rootGroupId, "repositoryVerNum", H5T_NATIVE_FLOAT, &versionNum ))
                    {
                        rc = GDAL_IDENTIFY_TRUE;
                        CPLDebug( HDF5R_DEBUG_STR, "HDF5RDataSet::Identify(): HDF5-R ICD Version: %f", versionNum );
                    }
                }
                H5Gclose( rootGroupId );
            }
            H5Fclose( h5Hid );
        }
    }

    CPLDebug( HDF5R_DEBUG_STR, "HDF5RDataSet::Identify() for: %s result=%d",
              fileName.c_str(), rc );

    return rc;
}

//******************************************************************************
//  Get Affine transform from raster coordinates to projected map coordinates
//******************************************************************************
CPLErr HDF5RDataSet::SetGeoTransform( double* padTransform )
{
    // debug only
    CPLAssert(padTransform != nullptr);

    if (padTransform)
    {
        for (int i=0; i<GDAL_XFORM_SZ; ++i)
            gdalTransform_[i] = padTransform[i];

        useAffineXform_ = true;
    }

    return CE_None;
}

//******************************************************************************
//  Get Affine transform from raster coordinates to projected map coordinates
//******************************************************************************
CPLErr HDF5RDataSet::GetGeoTransform( double* padTransform )
{
    // debug only
    CPLAssert(padTransform != nullptr);

    if (padTransform)
    {
       for (int i=0; i<GDAL_XFORM_SZ; ++i)
           padTransform[i] = gdalTransform_[i];
    }

    // return CE_Failure if transform never set locally
    return useAffineXform_ ? CE_None : CE_Failure;
}

//******************************************************************************
// Generate the SUBDATASET NAME and DESC attributes for each frame
//******************************************************************************
int HDF5RDataSet::setSubDataSetAttributes( void )
{
    hsize_t nSubDs = hdf5rReader_->getNumSubFrames();
    if (nSubDs <= 0)
        return false;

    // Generate the attribute pairs (NAME and DESC) for each frame
    for (hsize_t i=0; i < nSubDs; ++i)
    {
        // SUBDATASET_NN_NAME = "HDF5R:<file_name>:<frame_index>"
        std::string name = "SUBDATASET_" + std::to_string(i) + "_NAME";
        std::string value = "HDF5R:" + std::string( GetDescription() ) + ":" + std::to_string( i );
        subDataNameValueList_ = CSLSetNameValue( subDataNameValueList_,
                                                 name.c_str(), value.c_str());

        // SUBDATASET_NN_DESC
        name = "SUBDATASET_" + std::to_string(i) + "_DESC";
        value = "HDF5 raster format - V2.1 for frame index: " + std::to_string( i );
        subDataNameValueList_ = CSLSetNameValue( subDataNameValueList_,
                                                 name.c_str(), value.c_str() );

    }

    SetMetadata( subDataNameValueList_, "SUBDATASETS" );

    return int(nSubDs);
}

//******************************************************************************
// Static method to parse the SUBDATSETs fileName field
//******************************************************************************
 bool HDF5RDataSet::parseSubDataDescriptor( const std::string& descStr, HDF5RDataSet::Hdf5rSubDataDesc_t* sdesc )
{
    // caller should give us a fresh Hdf5rSubDataDesc_t, but just in case
    // reset the 'filled' status
    sdesc->filled = false;

    // parse using stringstream getline() with ':' as the separator
    std::stringstream ss( descStr );
    std::string frameIndexStr;
    if (std::getline(ss, sdesc->hdr, ':'))
        if (std::getline(ss, sdesc->fileName, ':'))
            if (std::getline(ss, frameIndexStr, ':'))
            {
                sdesc->frameIndex = atoi(frameIndexStr.c_str());
                sdesc->filled = true;
            }
    return sdesc->filled;
}

 //******************************************************************************
 // Get the transform from the source data set that converts to
 // WGS84 latitude and longitude in degrees
 //******************************************************************************
 OGRCoordinateTransformation* HDF5RDataSet::getLatLonTransform( GDALDataset* poSrcDS )
 {
     // return value
     OGRCoordinateTransformation* ogrXform = nullptr;

     // Get the Well Known Text projection reference string from the source
     const char* wktStr = poSrcDS->GetProjectionRef();

     if (wktStr)
     {
         // initialize the input spatial reference from the WKT string
         OGRSpatialReference* ogrIn = new OGRSpatialReference;
         ogrIn->importFromWkt( &wktStr );

         // Clone the output reference from the Geographic portion of the input
         // based on GDAL example at https://www.gdal.org/osr_tutorial.html
         // for 'conveniently' creating a transform to lat-lon
         OGRSpatialReference* ogrOut = ogrIn->CloneGeogCS();

         // get the transform
         ogrXform = OGRCreateCoordinateTransformation( ogrIn, ogrOut );

         delete ogrIn;
         delete ogrOut;
     }

     return ogrXform;
 }

 //*****************************************************************************
 // Get the transform from the source data set that converts to
 // WGS84 latitude and longitude in degrees
 //*****************************************************************************
 OGRCoordinateTransformation* HDF5RDataSet::getGCPLatLonTransform( GDALDataset* poSrcDS )
 {
     // return value
     OGRCoordinateTransformation* ogrXform = nullptr;

     // Get the Well Known Text projection reference string from the source
     // GCPs (if present)
     const char* wktStr = nullptr;
     if (poSrcDS->GetGCPCount() >= 4)
         wktStr = poSrcDS->GetGCPProjection();

     if (wktStr)
     {
         // initialize the input spatial reference from the WKT string
         OGRSpatialReference* ogrIn = new OGRSpatialReference;
         ogrIn->importFromWkt( &wktStr );

         // Clone the output reference from the Geographic portion of the input
         // based on GDAL example at https://www.gdal.org/osr_tutorial.html
         // for 'conveniently' creating a transform to lat-lon
         OGRSpatialReference* ogrOut = ogrIn->CloneGeogCS();

         // get the transform
         ogrXform = OGRCreateCoordinateTransformation( ogrIn, ogrOut );

         delete ogrIn;
         delete ogrOut;
     }

     return ogrXform;
 }

 //*****************************************************************************
 // Transform coordinate arrays from source data sets with GCPs from pixel and
 // line coordinates to latitude and longitude (in degrees).
 //*****************************************************************************
 bool HDF5RDataSet::gcpConvertToLatLong( GDALDataset* poSrcDS,
                                         int count,
                                         double* x,
                                         double* y,
                                         double* z,
                                         int* status,
                                         int gcpOrder )
 {
     bool rc = false;

     // get the GCP transform from source data set projection coordinates to
     // latitude and longitude
     OGRCoordinateTransformation* llXform = getGCPLatLonTransform( poSrcDS );
     if (llXform)
     {
         CPLDebug( HDF5R_DEBUG_STR, "GCP Lat-Lon projection Transform SUCCESS" );

         // this is for info only to show the affine transform derived
         // from this set of GCPs
         double transform[GDAL_XFORM_SZ];
         if (GDALGCPsToGeoTransform( poSrcDS->GetGCPCount(), poSrcDS->GetGCPs(), transform, TRUE ))
         {
             CPLDebug( HDF5R_DEBUG_STR, "Affine Transform %f %f %f %f %f %f",
                       transform[0], transform[1], transform[2],
                       transform[3], transform[4], transform[5] );
         }
         else
             CPLError( CE_Warning, CPLE_AppDefined,
                       "HDF5RDataSet::gcpConvertToLatLong GDALGCPsToGeoTransform"
                       " failed to return an affine transform from"
                       " GCPs (which came from the GEO Grid)" );

         // get the GCP transform from pixel and line coordinates to source
         // dataset projection coordinates
         void* pTransformArg = GDALCreateGCPTransformer( poSrcDS->GetGCPCount(),
                                                         poSrcDS->GetGCPs(),
                                                         gcpOrder,
                                                         FALSE );  // bReversed
         if (pTransformArg)
         {
             CPLDebug( HDF5R_DEBUG_STR, "GCP Polynomial Transform SUCCESS" );

             // do the transform (in place) from pixel-line to projection
             GDALGCPTransform( pTransformArg,
                               FALSE,     // bDstToSrc
                               count,
                               x, y, z,
                               status );

             CPLDebug( HDF5R_DEBUG_STR, "GCP Transform from fp to projection: %f %f %f %d",
                       x[0], y[0], z[0], status[0] );

             // do the transform (in place) from source projection to lat-lon
             rc = llXform->Transform( count, x, y, z, status );

             CPLDebug( HDF5R_DEBUG_STR, "GCP Transform from projection to lat-lon: %f %f %f %d",
                       x[0], y[0], z[0], status[0] );

             GDALDestroyGCPTransformer( pTransformArg );
         }
         else
             CPLDebug( HDF5R_DEBUG_STR, "GCP Polynomial Transform FAIL" );

         OCTDestroyCoordinateTransformation( llXform );
     }
     else
         CPLDebug( HDF5R_DEBUG_STR, "GCP Lat-Lon projection Transform FAIL" );

     return rc;
 }

 //*****************************************************************************
 // Transform coordinate arrays from source data sets without GCPs from pixel
 // and line coordinates to latitude and longitude (in degrees).
 //*****************************************************************************
 bool HDF5RDataSet::affineConvertToLatLong( GDALDataset* poSrcDS,
                                            int count,
                                            double* x,
                                            double* y,
                                            int* status )
 {
     bool rc = false;

     // get the transform from source data set projection coordinates to
     // latitude and longitude
     OGRCoordinateTransformation* llXform = getLatLonTransform( poSrcDS );
     if (llXform)
     {
         CPLDebug( HDF5R_DEBUG_STR, "Lat-Lon projection Transform SUCCESS" );

         // get the affine transform from pixel and line coordinates to source
         // dataset projection coordinates
         double xform[6];
         if (poSrcDS->GetGeoTransform(xform ) == CE_None)
         {
             CPLDebug( HDF5R_DEBUG_STR, "Affine Transform availability: SUCCESS" );

             // do the affine transform in place
             for (int i=0; i<count; ++i)
             {
                 double xtmp = xform[0] + x[i] * xform[1] + y[i] * xform[2];
                 y[i] = xform[3] + x[i] * xform[4] + y[i] * xform[5];
                 x[i] = xtmp;
             }

             CPLDebug( HDF5R_DEBUG_STR, "Transform from fp to projection: %f %f",
                       x[0], y[0] );

             // do the transform (in place) from source projection to lat-lon
             rc = llXform->Transform( count, x, y, nullptr, status );

             CPLDebug( HDF5R_DEBUG_STR, "Transform from projection to lat-lon: %f %f %d",
                       x[0], y[0], status[0] );
         }
         else
             CPLDebug( HDF5R_DEBUG_STR, "Affine Transform availability: FAIL" );

         OCTDestroyCoordinateTransformation( llXform );
     }
     else
         CPLDebug( HDF5R_DEBUG_STR, "Lat-Lon projection Transform FAIL" );

     return rc;
 }

 //*****************************************************************************
 // Transform coordinate arrays from source data sets without GCPs from pixel
 // and line coordinates to latitude and longitude (in degrees).
 //*****************************************************************************
 Hdf5rLosGrid_t* HDF5RDataSet::buildLosGrid( GDALDataset* poSrcDS,
                                             int xStepSize,
                                             int yStepSize,
                                             int gcpOrder,
                                             bool noGcp,
                                             bool reGrid,
                                             double satEcfMeters[3],
                                             const Earth& earth )
 {
     // pointer to LOS grid -- return value
     Hdf5rLosGrid_t* losGrid = nullptr;

     // Grid points and size data
     // Notes:
     //    - includes call parameters xStepSize and yStepSize
     //    - array pointers and status are required for GDAL transforms
     //    - linear arrays must map to 2D row-line-y-lat major order to match
     //          the addressing scheme used by Hdf5rLosGrid_t
     int xGridSzIn = 0;
     int yGridSzIn = 0;
     double* x = nullptr;
     double* y = nullptr;
     double* z = nullptr;
     int* status = nullptr;
     bool haveLatLonGrid = false;

     // if GCPs are allowed and regridding is not required, then
     //   see if they are on a complete grid already, if so, that grid is used
     //   by copying the projected XYZ location and then converting the
     //   the projected XYZ to lat-lon (which is presumably the identity
     //   transform if XYZ is lon-lat-alt
     if (!noGcp && !reGrid)
     {
         haveLatLonGrid = loadGcpGridDirect( poSrcDS,
                                             &xGridSzIn, &yGridSzIn,
                                             &xStepSize, &yStepSize,
                                             &x, &y, &z, &status );
     }

     // As defined by the HDF5-R ICD the LOS grid size is truncated so it
     // does not extend past the last row and column, however
     // here we allocate a grid that extends past the last row and
     // column unless last is on the grid so every pixel can be interpolated
     int xGridSz = (poSrcDS->GetRasterXSize() + 2*xStepSize - 1) / xStepSize;
     int yGridSz = (poSrcDS->GetRasterYSize() + 2*yStepSize - 1) / yStepSize;

     // otherwise build transforms and compute each lat-lon on the grid
     if (!haveLatLonGrid)
     {
         // ----------------------- Build GDAL pixel coordinate arrays ---------

         // As defined by the HDF5-R ICD the LOS grid size is truncated so it
         // does not extend past the last row and column, however
         // here we allocate a grid that extends past the last row and
         // column unless last is on the grid so every pixel can be interpolated
         xGridSzIn = xGridSz;
         yGridSzIn = yGridSz;

         // allocate coordinate in arrays for the GDAL transform functions
         int arraySz = xGridSzIn * yGridSzIn;
         x = new double[arraySz];
         y = new double[arraySz];
         z = new double[arraySz];
         status = new int[arraySz];

         // allocate and set pixel (x) and line (y) coordinates in arrays for the
         // GDAL transform functions
         // GDAL pixel coordinates are at the pixel upper left edge so
         // 0.5 moves the grid to the center of the pixels
         int outIndex = 0;
         for (int iy=0; iy<yGridSzIn; ++iy)
         {
             double ys = yStepSize * iy + 0.5;

             for (int ix=0; ix<xGridSzIn; ++ix)
             {
                 x[outIndex] = xStepSize * ix + 0.5;
                 y[outIndex] = ys;
                 z[outIndex] = 0.0;
                 status[outIndex] = 0;
                 ++outIndex;
             }
         }

         // ----------------------- Transform coordinates to lat-lon ---------------

         // Try the the GCP transform first, if that fails then try the
         // affine transform.  'didConvert' means at least one point converted
         // successfully
         haveLatLonGrid = ((!noGcp) && gcpConvertToLatLong( poSrcDS, arraySz, x, y, z, status, gcpOrder ))
                            || affineConvertToLatLong( poSrcDS, arraySz, x, y, status );
     }

     // ----------------------- Build HDF5-R LOS Grid --------------------------
     if (haveLatLonGrid)
     {
         // if satellite radius is less than that of Earth, then derive a
         // location from the data
         m3d::Vector satEcf( satEcfMeters );
         double satRadiusIn = satEcf.magnitude();
         if (satRadiusIn <= earth.getEquatorialRadius())
         {
             // find min-max longitudes
             double minLongitude = Hdf5rLosGrid_t::DMAX;
             double maxLongitude = -Hdf5rLosGrid_t::DMAX;
             int arraySz = xGridSzIn * yGridSzIn;
             for (int i=0; i<arraySz; ++i)
                 if (status[i])
                 {
                     if (x[i] > maxLongitude)
                         maxLongitude = x[i];
                     else if (x[i] < minLongitude)
                         minLongitude = x[i];
                 }

             // compute center longitude
             double longitude = (maxLongitude + minLongitude)/2.0;

             // set the new observer location
             satEcf = Earth::GEO_SYNC_RADIUS_METERS
                          * earth.toEcef( 0.0,
                                          Earth::degToRad * longitude,
                                          Earth::GEO_SYNC_ALTITUDE_METERS ).normalize();

             // return settings via parameter list pointer
             satEcfMeters[0] = satEcf.i();
             satEcfMeters[1] = satEcf.j();
             satEcfMeters[2] = satEcf.k();

             CPLError( CE_Warning, CPLE_AppDefined, "HDF5RDataSet::buildLosGrid:"
                       " Invalid satellite radius.\n"
                       "\t frameMetaData.satPosEcf radius=%f\n"
                       "\t less than Earth radius in meters=%f\n"
                       "\t Changing to geosync satellite longitude derived from the LOS grid.\n"
                       "\t latitude=0 longitude=%f (min %f max %f)",
                       satRadiusIn, earth.getEquatorialRadius(),
                       longitude, minLongitude, maxLongitude );
         }

         losGrid = new Hdf5rLosGrid_t( yGridSz, xGridSz,
                                       yStepSize, xStepSize,
                                       satEcf,
                                       earth );

         // populate the LOS grid (y==latitude  x==longitude)
         losGrid->buildGridfromGdalArrays( yGridSzIn, xGridSzIn, y, x, status );

         // extrapolate last row and/or column if needed
        if (yGridSz > yGridSzIn)
            losGrid->extrapLastRow();
        if (xGridSz > xGridSzIn)
            losGrid->extrapLastColumn();
     }

     // cleanup
     delete [] x;
     delete [] y;
     delete [] z;
     delete [] status;

     return losGrid;
 }

 //*****************************************************************************
 // Test GCPs to see if they are on a grid already and if so use them
 //*****************************************************************************
 bool HDF5RDataSet::loadGcpGridDirect( GDALDataset* poSrcDS,
                                       int* xGridSizeIn, int* yGridSizeIn,
                                       int* xStepSize, int* yStepSize,
                                       double** x, double** y, double** z,
                                       int** status )
 {
     bool rc = false;

     // first: verify that source DS has GCPs
     int gcpCount = poSrcDS->GetGCPCount();
     const GDAL_GCP* gcps = poSrcDS->GetGCPs();
     if ((gcpCount > 0) && gcps)
     {
         // get the GCP transform from source data set projection coordinates to
         // latitude and longitude
         OGRCoordinateTransformation* llXform = getGCPLatLonTransform( poSrcDS );
         if (llXform)
         {
             // find min and max non-zero integer Pixel and Line by scanning all GCPs
             int minPixel = INT_MAX;
             int minLine = INT_MAX;
             int maxPixel = 0;
             int maxLine = 0;
             const GDAL_GCP* gcpptr = gcps;
             for (int i=0; i<gcpCount; ++i, ++gcpptr)
             {
                 int line = int(gcpptr->dfGCPLine);
                 int pixel = int(gcpptr->dfGCPPixel);
                 if ((line != 0) && (line < minLine))
                     minLine = line;
                 if ((pixel != 0) && (pixel < minPixel))
                     minPixel = pixel;
                 if (line > maxLine)
                     maxLine = line;
                 if (pixel > maxPixel)
                     maxPixel = pixel;
             }

             // second test: min evenly divides max for both lines and pixels
             if (((maxPixel%minPixel) == 0) && ((maxLine%minLine) == 0))
             {
                 int xGridSz = (maxPixel/minPixel) + 1;
                 int yGridSz = (maxLine/minLine) + 1;
                 int arraySz = xGridSz * yGridSz;

                 // minimum complete grid coverage allows for extrapolation of
                 // last row and column
                 int xGridSzMin = (poSrcDS->GetRasterXSize() + minPixel - 1) / minPixel;
                 int yGridSzMin = (poSrcDS->GetRasterYSize() + minLine - 1) / minLine;

                 // test: grid provides adequate coverage of image
                 if ((xGridSz >= xGridSzMin) && (yGridSz >= yGridSzMin))
                 {
                     // third test: arraySz must be same as number of GCPs
                     if (arraySz == gcpCount)
                     {
                         // allocate the linear coordinate arrays
                         *x = new double[arraySz];
                         *y = new double[arraySz];
                         *z = new double[arraySz];
                         *status = new int[arraySz];
                         memset( *status, 0, arraySz );

                         // load the grid points, each is tested for divisibility
                         // by the min grid point
                         gcpptr = gcps;
                         rc = true;
                         for (int i=0; i<gcpCount && rc; ++i, ++gcpptr)
                         {
                             // integer line,pixel grid point
                             int line = int(gcpptr->dfGCPLine);
                             int pixel = int(gcpptr->dfGCPPixel);

                             // fourth test: each grid point evenly divisible
                             if (((pixel%minPixel) != 0) || ((line%minLine) != 0))
                             {
                                 rc = false;
                                 CPLDebug( HDF5R_DEBUG_STR, "HDF5RDataSet::loadGcpGridDirect fail: "
                                           " Line or pixel divisibility test." );
                             }

                             // index into grid array
                             int idx = line/minLine * xGridSz + pixel/minPixel;

                             // load the grid point
                             (*x)[idx] = gcpptr->dfGCPX;
                             (*y)[idx] = gcpptr->dfGCPY;
                             (*z)[idx] = gcpptr->dfGCPZ;
                             (*status)[idx] = 1;
                         }

                         // fifth test: fully populated grid
                         for (int i=0; i<gcpCount; ++i)
                         {
                             if ((*status)[i] == 0)
                             {
                                 rc = false;
                                 CPLDebug( HDF5R_DEBUG_STR, "HDF5RDataSet::loadGcpGridDirect fail: "
                                           " Missing grid point: %d", i );
                             }
                         }

                         // if all tests passed transform from XY to lat-lon using the
                         // GCP projection and set the remaining return values
                         if (rc)
                         {
                             // do the transform (in place) from source projection to lat-lon
                             rc = llXform->Transform( arraySz, *x, *y, *z, *status );

                             // size info returned
                             *xGridSizeIn = xGridSz;
                             *yGridSizeIn = yGridSz;
                             *xStepSize = minPixel;
                             *yStepSize = minLine;

                             CPLDebug( HDF5R_DEBUG_STR, "HDF5RDataSet::loadGcpGridDirect success: "
                                       " size(step) X: %d(%d) Y:%d(%d)",
                                       xGridSz, minPixel, yGridSz, minLine );
                         }
                     }
                     else
                         CPLDebug( HDF5R_DEBUG_STR, "HDF5RDataSet::loadGcpGridDirect fail: "
                                   " Array size %d not equal to num GCPs: %d",
                                   arraySz, gcpCount );
                 }
                 else
                     CPLDebug( HDF5R_DEBUG_STR, "HDF5RDataSet::loadGcpGridDirect fail: "
                               " GCP Grid does not cover image X: %d Xmin: %d Y: %d Ymin: %d",
                               xGridSz, xGridSzMin, yGridSz, yGridSzMin );
             }
             else
                 CPLDebug( HDF5R_DEBUG_STR, "HDF5RDataSet::loadGcpGridDirect fail: "
                           " Max/min pixel or line not evenly divisible." );

             OCTDestroyCoordinateTransformation( llXform );
         }
         else
             CPLDebug( HDF5R_DEBUG_STR, "HDF5RDataSet::loadGcpGridDirect fail: "
                       " GCP projection to lat-lon transform not available." );

     }
     else
         CPLDebug( HDF5R_DEBUG_STR, "HDF5RDataSet::loadGcpGridDirect fail: "
                   " There are no GCPs." );
     return rc;
 }

 //*****************************************************************************
 // finalizeHdf5rWrite
 //   - called by the destructor when an hdf5rWriter exists and the
 //     dataset is in R/W mode
 //   - builds and writes fileMetaData, geoLocationData, frameMetaData,
 //     and summaryMetaData
 //   - calRawMetaData was written by HDF5RRasterBand::IWriteBlock()
 //*****************************************************************************
 void HDF5RDataSet::finalizeHdf5rWrite()
 {
     // -------------------------------------------------------------------------
     // Set variables for command line creation options that were saved
     // as a class attribute by Create()
     // -------------------------------------------------------------------------
     int32_t gcpOrder = 0;      // 0==use highest reliable order polynomial fit
     int32_t gcpRegrid = FALSE; // do not resample the grid if GCPs form a valid grid already
                                // if TRUE, then force a resample using the polynomial fit
     int32_t noGcp = FALSE;     // FALSE == use GCP's if available, TRUE == do not use GCPs

     if (creationOptions_)
     {
         creationOptions_->getValue( "GCP_ORDER", &gcpOrder );
         creationOptions_->getValue( "GCP_REGRID", &gcpRegrid );
         creationOptions_->getValue( "NO_GCP", &noGcp );
     }

     // -------------------------------------------------------------------------
     // Initialization of internal format of HDF5-R contents (except image)
     // -------------------------------------------------------------------------
     // instantiate the default HDF5-R file and GeoLocation attributes
     Hdf5rFileAttributes* fileAttributes = new Hdf5rFileAttributes;
     Hdf5rGeoLocAttributes* geoLocAttributes = new Hdf5rGeoLocAttributes;

     // instantiate the FrameMetaData default values and pointer to the structure
     Hdf5rFrameData* hdf5rFrameData = new Hdf5rFrameData;
     Hdf5rFrameData::FrameData_t* frameData = hdf5rFrameData->getFrameDataPtr();

     // instantiate vectors for summary metadata (filled by setSingleFrameMetaData)
     std::vector<const CompoundBase*> errorInfoVect;
     std::vector<const CompoundBase*> seqInfoVect;

     // -------------------------------------------------------------------------
     // Load Attributes and Frame Meta data items from GDAL NAME=VALUE metadata
     // -------------------------------------------------------------------------
     // Load source data set file attributes and FrameMetaData
     // (at this point only if the source GDAL data set is also HDF5-R will these
     //  attributes be present in the data set)
     std::vector<Hdf5rAttributeBase*> attrLists;
     attrLists.push_back( fileAttributes );
     attrLists.push_back( geoLocAttributes );
     loadMapsFromMetadataList( GetMetadata(),
                               attrLists,
                               hdf5rFrameData,
                               "Source metadata",
                               "H5R." ); // required prefix

     // -------------------------------------------------------------------------
     // Build Geo Location Grid
     // -------------------------------------------------------------------------
     // Get geolocation step size from GDAL attributes
     int32_t xStepSize = 20;
     geoLocAttributes->getValue( "H5R.GEO.X_Stepsize_Pixels", &xStepSize );

     int32_t yStepSize = 20;
     geoLocAttributes->getValue( "H5R.GEO.Y_Stepsize_Pixels", &yStepSize );

     // build the LOS grid using GDAL provided transforms
     // note satPosECF HDF5-R attribute is modified if it has a
     // magnitude less than the Earth's radius
     Hdf5rLosGrid_t* losGrid = buildLosGrid( this,
                                             xStepSize, yStepSize,
                                             gcpOrder, noGcp, gcpRegrid,
                                             frameData->satPosECF,
                                             earth_ );

     // -------------------------------------------------------------------------
     // Get the raster min/max intensity values and save in frame data
     // -------------------------------------------------------------------------
     double rasterMinMax[2];
     GDALRasterBand *poBand = GetRasterBand( 1 );
     if (poBand && (poBand->ComputeRasterMinMax( FALSE, rasterMinMax ) == CE_None))
     {
         frameData->minCalIntensity = int32_t( rasterMinMax[0] );
         frameData->maxCalIntensity = int32_t( rasterMinMax[1] );
     }
     else
     {
         CPLError( CE_Warning, CPLE_AppDefined,
                   "HDF5-R driver: GDAL ComputeRasterMinMax failed." );
     }

     // -------------------------------------------------------------------------
     // Load single frame values into various HDF5-R meta data items
     // -------------------------------------------------------------------------
     setCreateAttributes( losGrid,
                          geoLocAttributes,
                          frameData );

     setSingleFrameMetaData( hdf5rFrameData,
                             losGrid,
                             fileAttributes,
                             &errorInfoVect,
                             &seqInfoVect );

     // -------------------------------------------------------------------------
     // Write the data sets to the already open HDF5-R file
     // -------------------------------------------------------------------------
     // write the frame attributes compound data set
     hdf5rWriter_->setFrameDataFromMap( hdf5rFrameData );

     // write the file attributes
     hdf5rWriter_->setFileAttrsFromMap( fileAttributes->getConstAttrMap() );

     // write the summary metadata
     hdf5rWriter_->setSummaryDataFromMap( errorInfoVect, seqInfoVect );

     // Write the LOS grid data set
     hdf5rWriter_->writeLosGrid( losGrid, geoLocAttributes );

     // -------------------------------------------------------------------------
     // Cleanup
     // -------------------------------------------------------------------------
     delete losGrid;
     delete hdf5rFrameData;
     delete fileAttributes;
     delete geoLocAttributes;

     for (unsigned i=0; i<errorInfoVect.size(); ++i)
         delete errorInfoVect[i];

     for (unsigned i=0; i<seqInfoVect.size(); ++i)
         delete seqInfoVect[i];
 }

 //*****************************************************************************
 // Set the fileMetaData internal values from single frame frameMetaData and
 // the LOS grid
 //*****************************************************************************
 void HDF5RDataSet::setSingleFrameMetaData( const Hdf5rFrameData* frameData,
                                            const Hdf5rLosGrid_t* losGrid,
                                            Hdf5rFileAttributes* fileAttributes,
                                            std::vector<const CompoundBase*>* errorInfoVect,
                                            std::vector<const CompoundBase*>* seqInfoVect )
 {
     // get pointer to the single frame data
     const Hdf5rFrameData::FrameData_t* sfdPtr = frameData->getFrameDataConstPtr();

     // NOTE: SCID, SCA, and REPOSITORY_VER_NUM are not modified

     // accumulated values
     int32_t errorCount = 0;
     std::string errorList;

     // Get frame time stamp and set man/max times in frameMetaData
     std::ostringstream oss;
     oss << sfdPtr->year << "_" << sfdPtr->day << "_" << sfdPtr->secondsOfDay;

     fileAttributes->modifyValue( "H5R.minTimeStamp", oss.str().c_str() );
     fileAttributes->setValue( "H5R.minYear", sfdPtr->year );
     fileAttributes->setValue( "H5R.minDay", sfdPtr->day );
     fileAttributes->setValue( "H5R.minSeconds", sfdPtr->secondsOfDay );
     fileAttributes->modifyValue( "H5R.maxTimeStamp", oss.str().c_str() );
     fileAttributes->setValue( "H5R.maxYear", sfdPtr->year );
     fileAttributes->setValue( "H5R.maxDay", sfdPtr->day );
     fileAttributes->setValue( "H5R.maxSeconds", sfdPtr->secondsOfDay );

     if (sfdPtr->year == 0)
     {
         const char* err = "TIME_NOT_AVAILABLE ";
         ++errorCount;
         errorList += err;
         errorInfoVect->push_back( new ErrorInfoTable( err ) );
     }

     // frameMetaData::numberOfFrames always 1
     fileAttributes->setValue( "H5R.numberOfFrames", 1 );

     // set min/max latitude and longitude from LOS grid data
     if (losGrid)
     {
         fileAttributes->setValue( "H5R.minLatitude", losGrid->getYmin() );
         fileAttributes->setValue( "H5R.maxLatitude", losGrid->getYmax() );
         fileAttributes->setValue( "H5R.minLongitude", losGrid->getXmin() );
         fileAttributes->setValue( "H5R.maxLongitude", losGrid->getXmax() );
     }

     // scan the image for min/max intensities
     fileAttributes->setValue( "H5R.minCalIntensity", sfdPtr->minCalIntensity );
     fileAttributes->setValue( "H5R.maxCalIntensity", sfdPtr->maxCalIntensity );

     // line/channel reversed from frame data
     fileAttributes->setValue( "H5R.linesReversed", sfdPtr->linesReversed );
     fileAttributes->setValue( "H5R.chansReversed", sfdPtr->chansReversed );

     // LOS_FAILED/DEGRADED based on grid completeness
     if (losGrid)
     {
         int32_t losDegraded = int32_t( !losGrid->isValid() );
         fileAttributes->setValue( "H5R.LOS_degraded", losDegraded );
         fileAttributes->setValue( "H5R.LOS_failed", 0 );
         errorCount += losDegraded;
         if (losDegraded)
         {
             const char* err = "LOS_DEGRADED ";
             errorList += err;
             errorInfoVect->push_back( new ErrorInfoTable( err ) );
         }
     }
     else
     {
         fileAttributes->setValue( "H5R.LOS_degraded", 0 );
         fileAttributes->setValue( "H5R.LOS_failed", 1 );
         ++errorCount;
         const char* err = "LOS_FAILED ";
         errorList += err;
         errorInfoVect->push_back( new ErrorInfoTable( err ) );
     }

     // single frame values for flow control and image status
     fileAttributes->setValue( "H5R.flowControlFrameCt", sfdPtr->flowControl );
     fileAttributes->setValue( "H5R.imageStatus", sfdPtr->imageStatus );

     if (sfdPtr->flowControl)
     {
         const char* err = "FLOW_CONTROL_DETECTED ";
         errorList += err;
         errorInfoVect->push_back( new ErrorInfoTable( err ) );
     }

     // GDAL driver uses int32_t for image data so fullRangeCalibration=1
     fileAttributes->setValue( "H5R.fullRangeCalibration", 1 );

     if (m3d::Vector( sfdPtr->satPosECF ).magnitude() < Earth::WGS84_RE_METERS )
     {
         ++errorCount;
         const char* err = "EPH_NOT_AVAILABLE ";
         errorList += err;
         errorInfoVect->push_back( new ErrorInfoTable( err ) );
     }

     // error counts and list
     if (errorList.empty())
     {
         const char* err = "NO_ERRORS ";
         errorList += err;
         errorInfoVect->push_back( new ErrorInfoTable( err ) );
     }
     fileAttributes->setValue( "H5R.errorsDetectedCt", errorCount );
     fileAttributes->setValue( "H5R.offEarthDiscardCt", 0 );
     fileAttributes->modifyValue( "H5R.errorsDetectedList", errorList );

     // Sequence Info Table
     SeqInfoTable* seqInfoTable = new SeqInfoTable;
     seqInfoVect->push_back( seqInfoTable );

     SeqInfoTable::SeqInfoTable_t* seqInfo = seqInfoTable->getSeqInfoPtr();
     seqInfo->numFrames = 1;
     seqInfo->seqIndex = sfdPtr->sosSeqIndex;
     seqInfo->maxLineNumber = sfdPtr->endLine - 1;
     seqInfo->minCalIntensity = sfdPtr->minCalIntensity;
     seqInfo->maxCalIntensity = sfdPtr->maxCalIntensity;

     if (losGrid)
     {
         seqInfo->minLat = float( losGrid->getYmin() );
         seqInfo->maxLat = float( losGrid->getYmax() );
         seqInfo->minLon = float( losGrid->getXmin() );
         seqInfo->maxLon = float( losGrid->getXmax() );
     }
 }
