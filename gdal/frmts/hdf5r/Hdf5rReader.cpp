/*
 * Hdf5rReader.cpp
 *
 *  Created on: Sep 4, 2018
 *      Author: nielson
 */

#include <iostream>

#include "cpl_error.h"

#include "Hdf5rReader.h"

//**********************************************************************
// constructor
//**********************************************************************
Hdf5rReader::Hdf5rReader()
: fileName_(),
  hdf5rFileHid_(-1),
  rootGroupHid_(-1),
  geoLocationDataHid_( -1 ),
  geoLocationSpaceHid_( -1 ),
  frameMetaDataHid_(-1),
  frameMetaDataSpaceHid_(-1),
  imageHid_(-1),
  imageSpaceHid_(-1),
  nMetaDataFrames_( 0 ),
  nImageRows_( -1 ),
  nImageColumns_( -1 ),
  losGrid_( nullptr ),
  doBlankOffEarth_( true )
{}

//**********************************************************************
// destructor
//**********************************************************************
Hdf5rReader::~Hdf5rReader()
{
    close();
}

//**********************************************************************
// Open the file, groups, and spaces
//**********************************************************************
bool Hdf5rReader::open( const std::string& filename,
                        unsigned h5Flags )
{
    bool rc = false;

    // Open the dataset -- it is kept open until destructor call
    hdf5rFileHid_ = H5Fopen( filename.c_str(), h5Flags, H5P_DEFAULT );
    if (hdf5rFileHid_ < 0)
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Hdf5rReader::open H5Fopen failed for %s.", filename.c_str() );
    }
    else
    {
        rc = OpenHdf5Components( filename );
    }

    // on success, save the file name
    // on failure ensure that all hid_t's are closed
    if (rc)
        fileName_ = filename;
    else
        close();

    return rc;
}

//**********************************************************************
// Close the file, groups, and spaces and cleanup memory
//**********************************************************************
void Hdf5rReader::close()
{
    fileName_.clear();

    // close all persistent H5 descriptors (reverse of open order)
    if (imageSpaceHid_ >= 0)
        H5Sclose(imageSpaceHid_);
    imageSpaceHid_ = -1;

    if (imageHid_ >= 0)
        H5Dclose(imageHid_);
    imageHid_ = -1;

    if (frameMetaDataSpaceHid_ >= 0)
        H5Sclose(frameMetaDataSpaceHid_);
    frameMetaDataSpaceHid_ = -1;

    if (frameMetaDataHid_ >= 0)
        H5Dclose(frameMetaDataHid_);
    frameMetaDataHid_ = -1;

    if (geoLocationSpaceHid_ >= 0)
        H5Sclose( geoLocationSpaceHid_ );
    geoLocationSpaceHid_ = -1;

    if (geoLocationDataHid_ >= 0)
        H5Dclose( geoLocationDataHid_ );
    geoLocationDataHid_ = -1;

    if (rootGroupHid_ >= 0)
        H5Gclose(rootGroupHid_);
    rootGroupHid_ = -1;

    if (hdf5rFileHid_ >= 0)
        H5Fclose(hdf5rFileHid_);
     hdf5rFileHid_ = -1;

    nImageRows_ = nImageColumns_ = -1;

    delete losGrid_;
    losGrid_ = nullptr;
}

//******************************************************************************
// Load attributes from the HDF5-R file into the map
//******************************************************************************
int Hdf5rReader::fillAttrMap( hid_t h5Hid,
                              Hdf5rAttributeBase::H5AttrMap_t& fileAttrMap,
							  bool warnMissing ) const
{
    int nAttrLoaded = 0;
    Hdf5rAttributeBase::H5AttrMap_t::iterator i = fileAttrMap.begin();
    while (i != fileAttrMap.end())
    {
        Hdf5rAttributeBase::H5Attr_t& fileAttr = i->second;

        if (chkFileAttribute( h5Hid, fileAttr.name ))
        {
            if (fileAttr.h5TypeId == H5T_C_S1)
                getStrAttribute( h5Hid, fileAttr.name, &fileAttr.value.cstr );
            else
                getAttribute( h5Hid, fileAttr.name, fileAttr.h5TypeId, &fileAttr.value );

            ++nAttrLoaded;
        }
        else if (warnMissing)
            CPLError( CE_Warning, CPLE_AppDefined, "HDF5-R Attribute not present and ignored: %s",
                      fileAttr.name.c_str() );

        ++i;
    }

    return nAttrLoaded;

}

//******************************************************************************
// Test for file attribute existence
//******************************************************************************
bool Hdf5rReader::chkFileAttribute( hid_t h5Hid, const std::string& attrName ) const
{
    return (h5Hid > 0) && H5Aexists( h5Hid, attrName.c_str() );
}

//******************************************************************************
// Get an HDF5 attribute, except for strings, with error handling
//******************************************************************************
bool Hdf5rReader::getAttribute( hid_t groupHid,
                                const std::string& attrName,
                                hid_t h5TypeId,
                                void* val )
{
    bool rc = false;
    hid_t aHid = H5Aopen( groupHid, attrName.c_str(), H5P_DEFAULT );
    if (aHid >= 0)
    {
        if (H5Aread( aHid, h5TypeId, val ) < 0)
            CPLError(CE_Failure, CPLE_IllegalArg, "  HDF5-R Attribute read failed for: %s" , attrName.c_str() );
        else
            rc = true;
        H5Aclose( aHid );
    }
    else
        CPLError(CE_Failure, CPLE_IllegalArg, "  HDF5-R Attribute open failed for: %s" , attrName.c_str() );

    return rc;
}

//******************************************************************************
// Get an variable or fixed length HDF5 string attribute
//******************************************************************************
bool Hdf5rReader::getStrAttribute( hid_t groupHid,
                                   const std::string& attrName,
                                   char** val )
{
    bool rc = false;

    // open the attribute
    hid_t aHid = H5Aopen( groupHid, attrName.c_str(), H5P_DEFAULT );
    if (aHid >= 0)
    {
        // get the stored type and class from the HDF5-R file
        hid_t atype = H5Aget_type( aHid );
        H5T_class_t type_class = H5Tget_class( atype );

        // should be a string -- otherwise caller called the wrong method
        if (type_class == H5T_STRING)
        {
            // need the native type for the read
            hid_t atype_mem = H5Tget_native_type(atype, H5T_DIR_ASCEND);

            // variable and fixed length strings are handled differently
            //    - fixed: we need to allocate space for the string
            //    - variable: string is returned on the heap
            htri_t varStrResult = H5Tis_variable_str( atype );
            if (varStrResult > 0)
            {
                // this is a variable string
                char* attrPtr = nullptr;

                // get the attribute
                if (H5Aread( aHid, atype_mem, &attrPtr ) < 0)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg, "  HDF5-R Attribute read failed for: %s" , attrName.c_str() );
                }
                else
                {
                    // delete the old guy
                    free( *val );

                    // assign the new guy
                    *val = attrPtr;
//                    CPLDebug( HDF5R_DEBUG_STR, "String attribute: %s, value =   %s\n",
//                              attrName.c_str(), *val );
                    rc = true;
                }
            }
            else if (varStrResult == 0)
            {
                // this is a fixed length string -- discover and allocate space
                size_t strSz = H5Tget_size( atype );
                char* attrPtr = static_cast<char*>( calloc( strSz + 1, 1 ) );

                if (H5Aread( aHid, atype_mem, attrPtr ) < 0)
                {
                    CPLError(CE_Failure, CPLE_IllegalArg, "  HDF5-R Attribute read failed for: %s" , attrName.c_str() );
                    delete [] attrPtr;
                    attrPtr = nullptr;
                }
                else
                {
                    // delete the old guy
                    free( *val );

                    // assign the new guy
                    *val = attrPtr;
//                    CPLDebug( HDF5R_DEBUG_STR, "String attribute: %s; sz %ld , value =   %s\n",
//                              attrName.c_str(), strSz, attrPtr );
                    rc = true;
                }
            }
            else
                CPLDebug( HDF5R_DEBUG_STR, "H5Tis_variable_str call failed for: %s", attrName.c_str() );

            H5Tclose( atype_mem );
        }
        else
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Hdf5rReader::getStrAttribute() called for non-string attribute"
                      " Check attribute table. Attribute: %s.",
                      attrName.c_str() );

        H5Tclose( atype );
        H5Aclose( aHid );
    }
    else
        CPLError(CE_Failure, CPLE_IllegalArg, "  HDF5-R Attribute open failed for: %s" , attrName.c_str() );

    return rc;
}

//******************************************************************************
// Opens core components of the HDF5-R file which remain open until destruction
//******************************************************************************
bool Hdf5rReader::OpenHdf5Components( const std::string& fileName )
{
    //-------------------------------------------------------------------------
    // Open the root group "/" -- it is kept open until destructor call
    rootGroupHid_ = H5Gopen(hdf5rFileHid_, "/");
    if (rootGroupHid_ < 0)
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "HDF5RDataSet::OpenHdf5Components H5Gopen failed for / of %s.",
                  fileName.c_str() );
        return false;
    }

    //-------------------------------------------------------------------------
    // check for existence of the GeoLocationData dataset
    // (0 == does not exist) (<0  == error)
    if (H5Lexists( rootGroupHid_, "GeoLocationData", H5P_DEFAULT ) > 0)
    {
        // Open the GeoLocationData dataset
        geoLocationDataHid_ = H5Dopen(rootGroupHid_, "GeoLocationData");
        if (geoLocationDataHid_ < 0)
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Hdf5rReader::OpenHdf5Components H5Dopen of GeoLocationData component failed for %s.",
                      fileName_.c_str() );
            return false;
        }

        // Open the GeoLocationData Space
        geoLocationSpaceHid_ = H5Dget_space(geoLocationDataHid_);
        if (geoLocationSpaceHid_ < 0)
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Hdf5rReader::OpenHdf5Components H5Sopen of GeoLocationData component failed for %s.",
                      fileName_.c_str() );
            return false;
        }
    }

    //-------------------------------------------------------------------------
    // Open the frameMetaData dataset  -- it is kept open until destructor call
    if  (H5Lexists( rootGroupHid_, "frameMetaData", H5P_DEFAULT ) > 0)
    {
        frameMetaDataHid_ = H5Dopen(rootGroupHid_, "frameMetaData");
        if (frameMetaDataHid_ < 0)
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "HDF5RDataSet::OpenHdf5Components H5Dopen of frameMetaData component failed for %s.",
                      fileName.c_str() );
            return false;
        }

        // Open the frameMetaData Space
        frameMetaDataSpaceHid_ = H5Dget_space(frameMetaDataHid_);
        if (frameMetaDataHid_ < 0)
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "HDF5RDataSet::OpenHdf5Components H5Sopen of frameMetaData component failed for %s.",
                     fileName.c_str() );
            return false;
        }

        // get the number of records (one per frame) in frameMetaData
        int fmdRank = H5Sget_simple_extent_ndims(frameMetaDataSpaceHid_);

        hsize_t* dimensions = new hsize_t[fmdRank];
        H5Sget_simple_extent_dims(frameMetaDataSpaceHid_, dimensions, nullptr);
        nMetaDataFrames_ = dimensions[0];
        delete[] dimensions;

        CPLDebug( HDF5R_DEBUG_STR, "Number dimensions in frameMetaData=%d dim[0]=%lld",
                  fmdRank, nMetaDataFrames_ );
    }

    //-------------------------------------------------------------------------
    // Open the CalRawData DATASET and Space
    // Open the frameMetaData dataset  -- it is kept open until destructor call
    if  (H5Lexists( rootGroupHid_, "CalRawData", H5P_DEFAULT ) > 0)
    {
        imageHid_ = H5Dopen(rootGroupHid_, "CalRawData");
        if (imageHid_ < 0)
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "HDF5RDataSet::OpenHdf5Components H5Dopen of CalRawData component failed for %s.",
                     fileName.c_str());
            return false;
        }
        imageSpaceHid_ = H5Dget_space(imageHid_);
        if (imageSpaceHid_ < 0)
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "HDF5RDataSet::OpenHdf5Components H5Sopen of CalRawData component failed for %s.",
                     fileName.c_str());
            return false;
        }
    }

    return true;
}

//******************************************************************************
// Retrieves components of a selected index frameMetaData
//     - Delete when done to prevent resource leak
//******************************************************************************
bool Hdf5rReader::getFrameMetaData( unsigned frameIndex, Hdf5rFrameData* hdf5rFrameData )
{
    bool rc = false;

    // Frame data structure to fill from the file (with H5Read)
    Hdf5rFrameData::FrameData_t* frameData = hdf5rFrameData->getFrameDataPtr();

    hid_t dataTypeHid = H5Dget_type(frameMetaDataHid_);
    if (dataTypeHid >= 0)
    {
        int nFrames = H5Tget_nmembers(dataTypeHid);
        CPLDebug( HDF5R_DEBUG_STR, "Number of frameMetaData members = %d", nFrames );

        int frameIdx = H5Tget_member_index(dataTypeHid, "frameNumber");
        CPLDebug( HDF5R_DEBUG_STR, "frameNumber index: %d", frameIdx );

        // Create HDF5 memory Type and Space for the frameMetaData Compound
        // type with space for 1 element
        hid_t memHid = H5Tcreate( H5T_COMPOUND, sizeof(Hdf5rFrameData::FrameData_t) );
        hsize_t memDims[1] = { 1 };
        hid_t memSzHid = H5Screate_simple(1, memDims, nullptr);

        if ((memHid >= 0) && (memSzHid >= 0))
        {
            // Map of all frame data elements and characteristics
            Hdf5rFrameData::CompoundElementMap_t frameElementMap = hdf5rFrameData->getAttrMap();

            Hdf5rFrameData::CompoundElementMap_t::const_iterator i = frameElementMap.begin();
            while( i != frameElementMap.end())
            {
                // reference to the element
                const Hdf5rFrameData::CompoundElement_t& frameEl = i->second;

                // scalar types
                if (frameEl.dimension == 0)
                {
                    CPLDebug( HDF5R_DEBUG_STR, "FrameData inserting: %s", frameEl.name.c_str() );
                    if (H5Tinsert( memHid, frameEl.name.c_str(), frameEl.offset, frameEl.h5TypeId ) < 0)
                        CPLError( CE_Warning, CPLE_AppDefined, "HDF5-R Frame attribute insert failed for: %s",
                                  frameEl.name.c_str() );
                }

                // strings
                else if (frameEl.h5TypeId == H5T_C_S1)
                {
                    hid_t strHid = H5Tcopy( H5T_C_S1 );
                    H5Tset_size( strHid, frameEl.dimension );
                    CPLDebug( HDF5R_DEBUG_STR, "FrameData inserting: %s", frameEl.name.c_str() );
                    if (H5Tinsert( memHid, frameEl.name.c_str(), frameEl.offset, strHid ) < 0)
                        CPLError( CE_Warning, CPLE_AppDefined, "HDF5-R Frame attribute insert failed for: %s",
                                  frameEl.name.c_str() );
                    H5Tclose( strHid );
                }

                // rank 1 arrays (vectors)
                else
                {
                    hsize_t dims[1] = {frameEl.dimension};
                    hid_t vectTypeId = H5Tarray_create( frameEl.h5TypeId, 1, dims, nullptr );
                    CPLDebug( HDF5R_DEBUG_STR, "FrameData inserting: %s", frameEl.name.c_str() );
                    if (H5Tinsert( memHid, frameEl.name.c_str(), frameEl.offset, vectTypeId ) < 0)
                        CPLError( CE_Warning, CPLE_AppDefined, "HDF5-R Frame attribute insert failed for: %s",
                                  frameEl.name.c_str() );
                    H5Tclose( vectTypeId );
                }

                ++i;
            }

            // Set the element selector on the space (0 based)
            hsize_t offset[1] = { frameIndex };
            H5Sselect_elements(frameMetaDataSpaceHid_, H5S_SELECT_SET, 1, offset);

            // Read the selected frameMetaData element into the frameData
            if (H5Dread( frameMetaDataHid_, memHid, memSzHid,
                         frameMetaDataSpaceHid_, H5P_DEFAULT, frameData) >= 0)
            {
                CPLDebug( HDF5R_DEBUG_STR, "getSingleFrameMetaData: read frameNumber at index: %d = %d",
                          frameIndex, frameData->frameNumber );
                rc = true;
            }
            else
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "HDF5RDataSet::getFrameMetaData getSingleFrameMetaData H5Dread failed for index: %d.",
                          frameIndex );

            H5Tclose( memHid );
            H5Sclose( memSzHid );
        }

        H5Tclose( dataTypeHid );
    }
    else
      CPLError( CE_Failure, CPLE_OpenFailed,
                "HDF5RDataSet::getFrameMetaData getSingleFrameMetaData: H5Dget_type failed" );

    return rc;
}

//******************************************************************************
// Retrieve the HDF5-R image as a 2-D array
//******************************************************************************
bool Hdf5rReader::getImageDimensions( unsigned frameIndex,
                                      int* rows,
                                      int* cols )
{
    bool rc = false;

    // verify that the rank of CalRawData is 3
    int imgRank = H5Sget_simple_extent_ndims(imageSpaceHid_);
    CPLDebug( HDF5R_DEBUG_STR, "Number dimensions in CalRawData=%d", imgRank );
    if (imgRank == 3)
    {
        // Set the element selector on the space (0 based)
        hsize_t offset[1] = { frameIndex };
        H5Sselect_elements(frameMetaDataSpaceHid_, H5S_SELECT_SET, 1, offset);

        hsize_t dimensions[3];
        H5Sget_simple_extent_dims(imageSpaceHid_, dimensions, nullptr);

        for (int i = 0; i < imgRank; ++i)
            CPLDebug( HDF5R_DEBUG_STR, " Image: dim[%d] = %lld", i, dimensions[i] );

        // check that frame index is in range [0..(dimension[0]-1)]
        if (frameIndex < dimensions[0])
        {
            nImageRows_ = dimensions[1];
            nImageColumns_ = dimensions[2];

            if (rows)
                *rows = dimensions[1];

            if (cols)
                *cols = dimensions[2];

            rc = true;
        }
        else
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "Hdf5rReader::getImageDimensions Requested out of "
                      "range index %d must be less than %lld",
                      frameIndex, dimensions[0] );
    }
    else // rank not 3
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                "Hdf5rReader::getImageDimensions CalRawData rank is %d, but expected 3!",
                imgRank);
    }

    return rc;
}

//******************************************************************************
// Retrieve the HDF5-R LOS grid as a 2-D array
//******************************************************************************
const Hdf5rLosGrid_t* Hdf5rReader::getLosGrid( unsigned frameIndex,
                                               const Hdf5rGeoLocAttributes* geoLocAttributes,
                                               m3d::Vector& satEcfMeters,
                                               const Earth& earth )
{
    // if 'H5Exists' test failed then the geoLocationData hid's will be -1
    // warnings and/or errors have already been generated for that case
    if (geoLocationDataHid_ >= 0)
    {
        // get attributes for the X and Y LOS grid step-size
        int32_t xStepSize, yStepSize;
        if (geoLocAttributes->getValue( "H5R.GEO.X_Stepsize_Pixels", &xStepSize )
                && geoLocAttributes->getValue( "H5R.GEO.Y_Stepsize_Pixels", &yStepSize ))
        {
            // As defined by the HDF5-R ICD the LOS grid size is truncated so it
            // does not extend past the last row and column, however
            // here we allocate a grid that extends past the last row and
            // column unless last is on the grid so every pixel can be interpolated
            int xGridSz = (nImageColumns_ + 2*xStepSize - 1) / xStepSize;
            int yGridSz = (nImageRows_ + 2*yStepSize - 1) / yStepSize;

            CPLDebug( HDF5R_DEBUG_STR, "LOS grid step size X: %d Y: %d"
                                       " grid size X: %d Y: %d",
                      xStepSize, yStepSize, xGridSz, yGridSz );

            // verify the rank of the GeoLocation Data space is 3
            int losRank = H5Sget_simple_extent_ndims( geoLocationSpaceHid_ );
            CPLDebug( HDF5R_DEBUG_STR, "Number dimensions in GeoLocationData=%d", losRank );
            if (losRank == 3)
            {
                hsize_t dimensions[3];
                H5Sget_simple_extent_dims( geoLocationSpaceHid_, dimensions, nullptr );

                for (int i = 0; i < losRank; ++i)
                    CPLDebug( HDF5R_DEBUG_STR, " LOS: dim[%d] = %lld", i, dimensions[i] );

                // verify that the grid size matches, or is larger than the required size
                if (((yGridSz-1) <= int(dimensions[1])) && ((xGridSz-1) <= int(dimensions[2])))
                {
                    // set up input selection arrays
                    //   [0]: selected image frame
                    //   [1]: row == scan line
                    //   [2]: column == pixels per scan line
                    hsize_t offsetIn[3] = { unsigned(frameIndex), 0, 0 };
                    hsize_t countIn[3] = { 1, hsize_t(yGridSz-1), hsize_t(xGridSz-1) };
                    CPLDebug( HDF5R_DEBUG_STR, "LOS hyperslab select dimensions: %lld %lld %lld",
                              countIn[0], countIn[1], countIn[2] );
                    if (H5Sselect_hyperslab( geoLocationSpaceHid_, H5S_SELECT_SET, offsetIn, nullptr, countIn, nullptr ) >= 0)
                    {
                        // Allocate the LOS in-memory structure to fill
                        losGrid_ = new Hdf5rLosGrid_t( yGridSz, xGridSz,
                                                       yStepSize, xStepSize,
                                                       satEcfMeters,
                                                       earth );

                        // Create HDF5 memory Type for one frame of the LosData Compound
                        hid_t memHid = H5Tcreate( H5T_COMPOUND, sizeof(Hdf5rLosGrid_t::Hdf5rLosData_t) );

                        // Insert each element we want to retrieve from LosData
                        H5Tinsert( memHid, "ecf_X",  HOFFSET(Hdf5rLosGrid_t::Hdf5rLosData_t, ecf_X),  H5T_NATIVE_FLOAT );
                        H5Tinsert( memHid, "ecf_Y",  HOFFSET(Hdf5rLosGrid_t::Hdf5rLosData_t, ecf_Y),  H5T_NATIVE_FLOAT );
                        H5Tinsert( memHid, "ecf_Z",  HOFFSET(Hdf5rLosGrid_t::Hdf5rLosData_t, ecf_Z),  H5T_NATIVE_FLOAT );
                        H5Tinsert( memHid, "lat",    HOFFSET(Hdf5rLosGrid_t::Hdf5rLosData_t, map_Y),  H5T_NATIVE_FLOAT );
                        H5Tinsert( memHid, "lon",    HOFFSET(Hdf5rLosGrid_t::Hdf5rLosData_t, map_X),  H5T_NATIVE_FLOAT );

                        // set up output selection arrays
                        //   [0]: row == scan line (same as 2D input)
                        //   [1]: column == pixels per scan line (same as 2D input)
                        hsize_t memDims[2] = { hsize_t(yGridSz), hsize_t(xGridSz) };
                        hid_t memSpaceHid = H5Screate_simple( 2, memDims, nullptr );
                        hsize_t offsetOut[2] = { 0, 0 };
                        hsize_t countOut[2] = { hsize_t(yGridSz-1), hsize_t(xGridSz-1) };
                        if (H5Sselect_hyperslab( memSpaceHid, H5S_SELECT_SET, offsetOut, nullptr, countOut, nullptr ) >= 0)
                        {
                            // read the LOS data for a single frame into the memory array
                            if (H5Dread( geoLocationDataHid_, memHid, memSpaceHid,
                                         geoLocationSpaceHid_, H5P_DEFAULT,
                                         losGrid_->getLosDataArray() ) < 0)
                            {
                                CPLError( CE_Failure, CPLE_IllegalArg,
                                          "HDF5RDataSet::getLosGrid hyper-slab read failed for %s.",
                                          fileName_.c_str());
                                delete losGrid_;
                                losGrid_ = nullptr;
                            }
                            else
                            {
                                // extrapolate last row which is not present (test in case HDF5-R ICD changes)
                                if (yGridSz > int(dimensions[1]))
                                    losGrid_->extrapLastRow();
                                if (xGridSz > int(dimensions[2]))
                                    losGrid_->extrapLastColumn();

                                // scan and set the grid min/max values, does not
                                // include OTH points by default
                                losGrid_->summarize();
                            }

                            // debug info
                            if (losGrid_ && losGrid_->isValid())
                            {
                                int endRow = yGridSz - 1;
                                int endCol = xGridSz - 1;
                                Hdf5rLosGrid_t& losg = *losGrid_;
                                Hdf5rLosGrid_t::Hdf5rLosData_t ckData = losg( 0, 0 );
                                CPLDebug( HDF5R_DEBUG_STR, "LOS grid (0, 0): %f %f %f %f %f",
                                          ckData.ecf_X, ckData.ecf_Y, ckData.ecf_Z, ckData.map_Y, ckData.map_X );
                                ckData = losg( 0, endCol );
                                CPLDebug( HDF5R_DEBUG_STR, "LOS grid (0, %d): %f %f %f %f %f",
                                          endCol,
                                          ckData.ecf_X, ckData.ecf_Y, ckData.ecf_Z, ckData.map_Y, ckData.map_X );
                                ckData = losg( endRow, 0 );
                                CPLDebug( HDF5R_DEBUG_STR, "LOS grid (%d, 0): %f %f %f %f %f",
                                          endRow,
                                          ckData.ecf_X, ckData.ecf_Y, ckData.ecf_Z, ckData.map_Y, ckData.map_X );
                                ckData = losg( endRow, endCol );
                                CPLDebug( HDF5R_DEBUG_STR, "LOS grid (%d, %d): %f %f %f %f %f",
                                          endRow, endCol,
                                          ckData.ecf_X, ckData.ecf_Y, ckData.ecf_Z, ckData.map_Y, ckData.map_X );
                            }
                        }
                        else //
                        {
                            CPLError( CE_Failure, CPLE_IllegalArg,
                                      "HDF5RDataSet::getLosGrid H5Sselect_hyperslab of memory grid failed for %s.",
                                      fileName_.c_str());
                        }
                    }
                    else // selection failed
                    {
                        CPLError( CE_Failure, CPLE_IllegalArg,
                                  "HDF5RDataSet::getLosGrid H5Sselect_hyperslab of input grid failed for %s.",
                                  fileName_.c_str());
                    }
                }
                else // grid size does not match image
                {
                    if ((yGridSz-1) > int(dimensions[1]))
                        CPLError( CE_Failure, CPLE_IllegalArg,
                                  "HDF5RDataSet::getLosGrid grid size too small for image."
                                  " image rows: %d rowStepSize: %d gridRows: %lld needs to be at least: %d",
                                  nImageRows_, yStepSize, dimensions[1], yGridSz-1 );
                    if ((xGridSz-1) > int(dimensions[2]))
                        CPLError( CE_Failure, CPLE_IllegalArg,
                                  "HDF5RDataSet::getLosGrid grid size too small for image."
                                  " image cols: %d colStepSize: %d gridColumnss: %lld needs to be at least: %d",
                                  nImageColumns_, xStepSize, dimensions[2], xGridSz-1 );
                }
            }
            else // rank not 3
            {
                CPLError( CE_Failure, CPLE_IllegalArg,
                          "HDF5RDataSet::getLosGrid GeoLocationData rank is %d, but expected 3!",
                          losRank);
            }
        }
        else
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "HDF5RDataSet::getLosGrid X_Stepsize_Pixels and/or "
                      "Y_Stepsize_Pixels attribute not found in GeoLocationData" );
        }
    }

    return losGrid_;
}

//******************************************************************************
// readBlock
// This method reads a full frame of HDF5-R imagery
//    - Optionally blanks off-Earth pixels if requested
//******************************************************************************
CPLErr Hdf5rReader::readBlock( int frameIndex,
                               int row,
                               int col,
                               int32_t noDataValue,
                               void* pImage )
{
    CPLErr rc = CE_None;

    CPLDebug( HDF5R_DEBUG_STR,
              "Hdf5rReader::readBlock called. row=%d col%d", row, col );

    // Driver currently supports full frame read, so (row,col) must be (0,0)
    if ((row == 0) || (col == 0))
    {
        // NOTE: At this time the HDF5-R file is open as are the ImageMetaData,
        //       CalRawData, and TDB datasets and spaces

        // set up hyper-slab selection arrays
        //   [0]: selected image frame
        //   [1]: row == scan line
        //   [2]: column == pixels per scan line
        hsize_t offset[3] = { unsigned(frameIndex), 0, 0 };
        hsize_t count[3] = { 1, unsigned(nImageRows_), unsigned(nImageColumns_) };
        CPLDebug( HDF5R_DEBUG_STR, "Image hyperslab select dimensions: %lld %lld %lld",
                  count[0], count[1], count[2] );
        if (H5Sselect_hyperslab( imageSpaceHid_, H5S_SELECT_SET, offset, nullptr, count, nullptr ) >= 0)
        {
            // define the memory space to receive the data
            //int* memImage = new int[dimensions[1] * dimensions[2]];

            hsize_t count_out[2] = {unsigned(nImageRows_), unsigned(nImageColumns_)};
            hid_t memSpace = H5Screate_simple( 2, count_out, nullptr );

            // read the full frame raster image into the GDAL array
            if (H5Dread( imageHid_, H5T_NATIVE_INT, memSpace,
                         imageSpaceHid_, H5P_DEFAULT, pImage ) < 0)
            {
                CPLError( CE_Failure, CPLE_IllegalArg,
                          "HDF5RDataSet::getImageArray hyper-slab read failed for %s.",
                          fileName_.c_str());
                rc = CE_Failure;
            }
            else
            {
                int32_t *pImageI32 = static_cast<int32_t*>( pImage );
                CPLDebug( HDF5R_DEBUG_STR, "Sample first row raster data: %d %d",
                          pImageI32[0], pImageI32[1] );

                // to blank off-Earth portions of the image, must have an
                // LOS grid
                if (doBlankOffEarth_ && losGrid_)
                {
                    offEarthBlanking( noDataValue, pImageI32 );
                }
            }
        }
        else // hyper-slab selection failed
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "HDF5RDataSet::readBlock hyper-slab selection failed for %s.",
                      fileName_.c_str());
            rc = CE_Failure;
        }
    }
    else // image tiling not supported
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "HDF5RDataSet::readBlock Image tiling is not supported by the HDF5-R driver. file=%s",
                  fileName_.c_str());
        rc = CE_Failure;
    }

    return rc;
}

//******************************************************************************
//  Unit test code to print one complete LOS grid area
//   change roow0 and col0 to select the area
//******************************************************************************
void Hdf5rReader::interpolateUnitTest() const
{
    int32_t row0 = losGrid_->getRowStepSize() * (losGrid_->getNrows() - 2);
    int32_t col0 = losGrid_->getColStepSize() * (losGrid_->getNcols() - 2);

    // iterate over rows
    for (int irow=0; irow < (losGrid_->getRowStepSize()+1); ++irow)
    {
        // dummy call for one-time debug print of corner points
        Hdf5rLosGrid_t::GeoMapXY_t tmp;
        losGrid_->interpolate( row0, col0, &tmp );

        std::cout << "interp row(y)=" << std::setw( 2 ) << irow <<":";

        // iterate over columns
        for (int icol=0; icol < (losGrid_->getColStepSize()+1); ++icol)
        {
            Hdf5rLosGrid_t::GeoMapXY_t mapxy;
            losGrid_->interpolate( row0+irow, col0+icol, &mapxy );

            std::cout << std::setw( 8 )<< " " << mapxy.second << " " << mapxy.first;
        }
        std::cout << std::endl;
    }
}

//******************************************************************************
// Blank off-earth pixels using the GDAL no-data-value. Image and LOS grid
// must be available.
//******************************************************************************
void Hdf5rReader::offEarthBlanking( int32_t noDataValue, int32_t* pImage ) const
{
    //interpolateUnitTest();

    if (!pImage)
        return;

    // basic LOS grid tests
    if (losGrid_ && losGrid_->isValid())
    {
        // If all on-Earth, then nothing to do
        if (losGrid_->hasAllOnEarth())
        {
            CPLDebug( HDF5R_DEBUG_STR, "Hdf5rReader::offEarthBlanking "
                      "All on-Earth == no blanking required." );
        }

        // if all off-Earth then blank the entire image
        else if (losGrid_->hasAllOffEarth())
        {
            for (int i=0; i<(nImageRows_ * nImageColumns_); ++i)
                pImage[i] = noDataValue;

            CPLError( CE_Warning, CPLE_AppDefined,
                      "HDF5RDataSet::offEarthBlanking: Entire image "
                      "is off-Earth and set to NODATA value.\nUse GDAL open "
                      "option (-oo) BLANK_OFF_EARTH=0 to inhibit blanking." );
        }

        else // partial blanking required
        {
            // Iterate over each LOS grid tile, which means iterations stop
            // 1 short of the end
            for (size_t gRow=0; gRow < losGrid_->getNrows()-1; ++gRow)
            {
                for (size_t gCol=0; gCol < losGrid_->getNcols()-1; ++gCol)
                {
                    // Instantiate a LOS grid tile
                    Hdf5rLosGrid_t::GridTile_t gridTile( losGrid_, gRow, gCol );

                    // if all on-Earth, then skip this tile
                    if (gridTile.getStatus() == Hdf5rLosGrid_t::ALL_ON_EARTH)
                    {
                        ;
                    }

                    // if 0 or 1 corner on-Earth, then blank the tile
                    else if (gridTile.getNumOnEarth() <= 1)
                    {
                        blankGridTile( gRow, gCol, noDataValue, pImage );
                    }

                    // otherwise must test each pixel in the tile
                    else if (gridTile.getStatus() == Hdf5rLosGrid_t::PARTIAL_ON_EARTH)
                    {
                        // FIXME interpolation is not working well for OTH
                        //       could be a bug or that linear interpolation
                        //       just doesn't work for Earth limb tangent area
                        //
                        //testAndBlankGridTile( gridTile, gRow, gCol, noDataValue, pImage );

                        // fall-back is to blank any tile that is partial
                        blankGridTile( gRow, gCol, noDataValue, pImage );
                    }
                    else // error
                    {
                        CPLError( CE_Warning, CPLE_AppDefined,
                                  "HDF5RDataSet::offEarthBlanking: bad grid pt row=%d col=%d",
                                  int(gRow), int(gCol) );
                    }

                }
            }

        }
    }
}

//******************************************************************************
// Blank all pixels in the given grid tile (upper-left grid corner)
// using the GDAL no-data-value.  Special case for the right-most and
// bottom most tiles, the edge is also blanked if there are pixels at that
// location. Left and top edges as well as all internal pixels are always
// blanked.
//******************************************************************************
void Hdf5rReader::blankGridTile( int gridRow, int gridCol,
                                 int32_t noDataValue,
                                 int32_t* pImage ) const
{
    // starting pixel row,col index for this tile
    int pRow0 = gridRow * losGrid_->getRowStepSize();
    int pCol0 = gridCol * losGrid_->getColStepSize();

    // end of iteration is tile size for all tile row
    // except the last, which can end early or extend to
    // the end boundary of the tile
    int jEnd = losGrid_->getRowStepSize();
    int pRow1 = (gridRow + 1) * losGrid_->getRowStepSize();
    if ((pRow1+1) == nImageRows_)
        ++jEnd;
    else if (pRow1 > nImageRows_)
        jEnd -= (pRow1 - nImageRows_);

    // ditto for end column tile index
    int iEnd = losGrid_->getColStepSize();
    int pCol1 = (gridCol + 1) * losGrid_->getColStepSize();
    if ((pCol1+1) == nImageColumns_)
        ++iEnd;
    else if (pCol1 > nImageColumns_)
        iEnd -= (pCol1 - nImageColumns_);

    // iterate over the tile rows
    for (int j=0; j < jEnd; ++j, ++pRow0)
    {
        // starting pixel address for this tile row,col
        int32_t* pAddr = &pImage[pRow0*nImageColumns_ + pCol0];

        // iterate over the tile columns blanking pixels
        for (int i=0; i < iEnd; ++i)
        {
            *pAddr++ = noDataValue;
        }
    }
}

//******************************************************************************
// Like the tile blanking above, except each pixel is test if on or off
// Earth.  Only off-Earth pixels are blanked.
//******************************************************************************
void Hdf5rReader::testAndBlankGridTile( const Hdf5rLosGrid_t::GridTile_t& gridTile,
                                        int gridRow, int gridCol,
                                        int32_t noDataValue, int32_t* pImage ) const
{
    // starting pixel row,col index for this tile
    int pRow0 = gridRow * losGrid_->getRowStepSize();
    int pCol0 = gridCol * losGrid_->getColStepSize();

    // end of iteration is tile size for all tile row
    // except the last, which can end early or extend to
    // the end boundary of the tile
    int jEnd = losGrid_->getRowStepSize();
    int pRow1 = (gridRow + 1) * losGrid_->getRowStepSize();
    if ((pRow1+1) == nImageRows_)
        ++jEnd;
    else if (pRow1 > nImageRows_)
        jEnd -= (pRow1 - nImageRows_);

    // ditto for end column tile index
    int iEnd = losGrid_->getColStepSize();
    int pCol1 = (gridCol + 1) * losGrid_->getColStepSize();
    if ((pCol1+1) == nImageColumns_)
        ++iEnd;
    else if (pCol1 > nImageColumns_)
        iEnd -= (pCol1 - nImageColumns_);

    // iterate over the tile rows
    for (int j=0; j < jEnd; ++j)
    {
        // starting pixel address for this tile row,col
        int32_t* pAddr = &pImage[(pRow0 + j)*nImageColumns_ + pCol0];

        // iterate over the tile columns blanking pixels that are OTH
        for (int i=0; i < iEnd; ++i, ++pAddr)
        {
            // unit test using full interpolation vs tile for OTH status
//            bool othTile = !gridTile.testPixelOnEarth( j, i );
//            Earth::MapXY_t drillLonLat;
//            bool othInterp = (1 == losGrid_->interpolate( pRow0 + j, pCol0 + i, &drillLonLat ));
//
//            if (othTile != othInterp)
//                CPLError( CE_Warning, CPLE_AppDefined,
//                          "Hdf5rReader::testAndBlankGridTile failed for Row: %d Col: %d",
//                          int(pRow0 + j), int(pCol0 + i) );

            if (!gridTile.testPixelOnEarth( j, i ))
                *pAddr = noDataValue;

        }
    }
}

//******************************************************************************
// getSummaryMetadata
// Reads the compound elements from Summary Metadata groups and loads into
// a vector of pointers to each.
//******************************************************************************
bool Hdf5rReader::getSummaryMetadata( std::vector<const CompoundBase*>& errorInfoVect,
                                      std::vector<const CompoundBase*>& seqInfoVect )
{
    bool rc = true;

    hid_t summaryMetaDataGroupHid = H5Gopen( rootGroupHid_, "summaryMetaData" );

    if (summaryMetaDataGroupHid >= 0)
    {
        rc &= h5ReadCompound( errorInfoVect,
                              []() {return new ErrorInfoTable;},
                              summaryMetaDataGroupHid,
                              "errorInfoTable" );

        rc &= h5ReadCompound( seqInfoVect,
                              []() {return new SeqInfoTable;},
                              summaryMetaDataGroupHid,
                              "seqInfoTable" );

        H5Gclose( summaryMetaDataGroupHid );
    }
    else
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                          "Hdf5rReader::getSummaryMetadata H5Gopen of "
                          " the summaryMetaData group failed." );
        rc = false;
    }

    return rc;
}

//******************************************************************************
// Read H5 Compound dataset into memory -- on vector element at a time
//******************************************************************************
bool Hdf5rReader::h5ReadCompound( std::vector<const CompoundBase*>& compoundVect,
                                  std::function<CompoundBase* ()> create,
                                  hid_t groupHid,
                                  const std::string& dsName )
{
    bool rc = true;

    // open the dataset
    hid_t dsHid = H5Dopen( groupHid, dsName.c_str() );
    if (dsHid < 0)
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Hdf5rReader::h5ReadCompound H5Dopen of "
                  " %s of the summaryMetaData group failed.",
                  dsName.c_str() );
        return false;
    }

    // open the space
    hid_t dsSpaceHid = H5Dget_space( dsHid );
    if (dsSpaceHid < 0)
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Hdf5rReader::h5ReadCompound H5Dget_space of "
                  " %s of the summaryMetaData group failed.",
                  dsName.c_str() );
        H5Dclose( dsHid );
        return false;
    }

    // number of dimensions (rank) of the space must be 1
    int rank = H5Sget_simple_extent_ndims( dsSpaceHid );
    if (rank == 1)
    {
        // create a member of the CompoundBase class using the Lambda
        // function passed as the second parameter
        // (need to do it now to get the size)
        CompoundBase* compoundBase = create();

        // Create HDF5 memory Type for one frame of the Compound
        hid_t memHid = H5Tcreate( H5T_COMPOUND, compoundBase->getCompoundSize() );

        // do an H5Insert for each element in the compound data map
        h5InsertFromMap( compoundBase->getAttrMap(), memHid, dsName );

        // get the number of elements in the data set
        hsize_t dimensions[1];
        H5Sget_simple_extent_dims( dsSpaceHid, dimensions, nullptr );

        // memory space allocates one element at a time
        hsize_t memDims[1] = {1};
        hid_t memSpaceHid = H5Screate_simple( 1, memDims, nullptr );

        // iterate over the number of elements in the data set
        for (unsigned i=0; i<dimensions[0]; ++i)
        {
            hsize_t selectSet[1] = {i};
            if (H5Sselect_elements( dsSpaceHid, H5S_SELECT_SET, 1, selectSet ) >= 0)
            {
                // read the element data into the memory structure
                if (H5Dread( dsHid, memHid, memSpaceHid,
                             dsSpaceHid, H5P_DEFAULT,
                             compoundBase->getCompoundDataPtr() ) < 0)
                {
                    CPLError( CE_Failure, CPLE_IllegalArg,
                              "Hdf5rReader::h5ReadCompound H5Dread"
                              " of summaryMetaData::%s failed for element: %d",
                              dsName.c_str(), i );
                }
                else // good read
                {
//                    if (dsName == "errorInfoTable")
//                    {
//                        const ErrorInfoTable::ErrorInfoTable_t* iet = reinterpret_cast<const ErrorInfoTable::ErrorInfoTable_t*>(compoundBase->getConstCompoundDataPtr());
//                        std::cerr << "errorTypeStr: " << iet->errorTypeStr << " i:" << i << std::endl;
//                    }

                    // push the element on the vector and get a new element
                    // for the next pass
                    compoundVect.push_back( compoundBase );
                    compoundBase = create();
                }
            }
            else
            {
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "Hdf5rReader::h5ReadCompound H5Sselect_elements"
                          " of summaryMetaData::%s failed for element %d.",
                          dsName.c_str(), i );
            }
        }

        H5Sclose( memSpaceHid );
        H5Tclose( memHid );

        // delete the last (unused) compound element
        delete compoundBase;
    }
    else // dataset has unexpected rank
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Hdf5rReader::h5ReadCompound H5Sget_simple_extent_ndims"
                  " of summaryMetaData::%s not 1!.",
                  dsName.c_str() );
        rc = false;
    }

    H5Sclose( dsSpaceHid );
    H5Dclose( dsHid );

    return rc;
}
