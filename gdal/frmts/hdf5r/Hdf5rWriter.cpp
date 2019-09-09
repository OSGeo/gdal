/*
 * Hdf5rWriter.cpp
 *
 *  Created on: Sep 7, 2018
 *      Author: nielson
 */

#include <exception>

#include "cpl_error.h"

#include "Hdf5rWriter.h"

//**********************************************************************
// constructor
//**********************************************************************
Hdf5rWriter::Hdf5rWriter()
:  filename_(),
   hdf5rFileHid_( -1 ),
   rootGroupHid_( -1 )
{
}

//**********************************************************************
// destructor
//**********************************************************************
Hdf5rWriter::~Hdf5rWriter()
{
    close();
}

//**********************************************************************
// Open the file for writing
//   - open top level groups
//**********************************************************************
bool Hdf5rWriter::open( const std::string& filename )
{
    // Open the dataset for write -- it is kept open until destructor call
    hdf5rFileHid_ = H5Fcreate( filename.c_str(),
                               H5F_ACC_TRUNC | H5F_ACC_DEBUG,
                               H5P_DEFAULT, H5P_DEFAULT );
    if (hdf5rFileHid_ < 0)
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "HDF5RDataSet::open H5Fopen failed for %s.",
                  filename.c_str() );
        return false;
    }

    // Open the root group "/" -- it is kept open until destructor call
    rootGroupHid_ = H5Gopen( hdf5rFileHid_, "/" );
    if (rootGroupHid_ < 0)
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "HDF5RDataSet::OpenHdf5Components H5Gopen failed / of %s.",
                  filename.c_str() );
        return false;
    }

    filename_ = filename;

    return true;
}

//**********************************************************************
// Close the hdf5r file
//**********************************************************************
void Hdf5rWriter::close()
{
    filename_.clear();

    if (rootGroupHid_ >= 0)
        H5Gclose(rootGroupHid_);
    if (hdf5rFileHid_ >= 0)
        H5Fclose(hdf5rFileHid_);

    rootGroupHid_ = hdf5rFileHid_ = -1;
}

//******************************************************************************
// Set all HDF5-R file attributes from the file attribute map
//******************************************************************************
int Hdf5rWriter::setAttrsFromMap( hid_t h5GroupHid,
                                  const Hdf5rAttributeBase::H5AttrMap_t& attrMap )
{
    int nAttrSet = 0;

    Hdf5rAttributeBase::H5AttrMap_t::const_iterator i = attrMap.begin();
    while (i != attrMap.end())
    {
        // get a reference to the map element contents
        const Hdf5rAttributeBase::H5Attr_t& attr = i->second;

        // if type is a C-string then need to modify the type to include length info
        hid_t strHid = -1;
        if (attr.h5TypeId == H5T_C_S1)
        {
            strHid = H5Tcopy( H5T_C_S1 );
            if (strHid < 0)
                throw std::runtime_error( "HDF5RDataset::CreateCopy() Call to H5Tcopy( H5T_C_S1 ) failed!" );

            // if string size attribute is -1 then use H5 variable string
            herr_t status = H5Tset_size( strHid, attr.h5StrSz < 0 ?  H5T_VARIABLE : attr.h5StrSz );
            if (status < 0)
                throw std::runtime_error( "HDF5RDataset::CreateCopy() Call to H5Tset_size( h5VarCstrTypeId_, 16 ) failed!" );

            H5Tset_strpad( strHid, H5T_STR_NULLTERM );
        }

        // set the attribute value
        if ((strHid > 0) && (attr.h5StrSz >= 0))
            setAttribute( h5GroupHid,
                          attr.name,
                          (strHid > 0) ? strHid : attr.h5TypeId,
                          attr.h5SpaceId,
                          attr.value.cstr );
        else
            setAttribute( h5GroupHid,
                          attr.name,
                          (strHid > 0) ? strHid : attr.h5TypeId,
                          attr.h5SpaceId,
                          &attr.value );

        ++nAttrSet;

        if (strHid >= 0)
            H5Tclose( strHid );

        ++i;
    }

    return nAttrSet;
}

//******************************************************************************
// Build and write a compound data set to an open group hid_t
//******************************************************************************
int Hdf5rWriter::h5WriteCompoundFromMap( const CompoundBase::CompoundElementMap_t& elementMap,
                                         const void* structPtr,
                                         size_t structSize,
                                         hid_t groupHid,
                                         const std::string& dsName )
{
    // Create HDF5 memory Type and Space for the frameMetaData Compound
    // type with space for 1 element
    hid_t memHid = H5Tcreate( H5T_COMPOUND, structSize );
    hsize_t memDims[1] = { 1 };
    hid_t memSzHid = H5Screate_simple(1, memDims, nullptr);

    if ((memHid >= 0) && (memSzHid >= 0))
    {
        // do an H5Insert for each element in the compound data map
        h5InsertFromMap( elementMap, memHid, dsName );

        hid_t frameMetaDataHid = H5Dcreate2( groupHid, dsName.c_str(),
                                             memHid, memSzHid,
                                             H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT );

        // Set the element selector on the space (0 based)
        hsize_t fileDims[1] = { 1 };
        hid_t fileSzHid = H5Screate_simple(1, fileDims, nullptr);
        hsize_t offset[1] = { 0 };
        H5Sselect_elements( fileSzHid, H5S_SELECT_SET, 1, offset );

        // Write the frameData to the selected frameMetaData element
        if (H5Dwrite( frameMetaDataHid, memHid, memSzHid,
                      fileSzHid, H5P_DEFAULT,
                      structPtr ) >= 0)
        {
            CPLDebug( HDF5R_DEBUG_STR, "Hdf5rWriter::h5WriteCompoundFromMap write success." );
        }
        else
            CPLError( CE_Failure, CPLE_FileIO, "Hdf5rWriter::h5WriteCompoundFromMap write FAILED for %s!",
                      dsName.c_str() );

        H5Tclose( memHid );
        H5Sclose( memSzHid );
        H5Sclose( fileSzHid );
    }
    return 0;
}

//******************************************************************************
// Build and write a compound data set to an open group hid_t
//******************************************************************************
int Hdf5rWriter::h5WriteCompound( const std::vector<const CompoundBase*>& compoundVect,
                                  size_t structSize,
                                  hid_t groupHid,
                                  const std::string& dsName )
{
    // Create HDF5 memory Type and Space for the frameMetaData Compound
    // which contains 1 element
    hid_t memHid = H5Tcreate( H5T_COMPOUND, structSize );
    hsize_t memDims[1] = { 1 };
    hid_t memSzHid = H5Screate_simple( 1, memDims, nullptr );

    // File space must has a dimension equal to the number of compounds input
    hsize_t fileDims[1] = { compoundVect.size() };
    hid_t fileSzHid = H5Screate_simple( 1, fileDims, nullptr );

    if ((memHid >= 0) && (memSzHid >= 0))
    {
        // do an H5Insert for each element in the compound data map
        h5InsertFromMap( compoundVect[0]->getAttrMap(), memHid, dsName );

        hid_t frameMetaDataHid = H5Dcreate2( groupHid, dsName.c_str(),
                                             memHid, fileSzHid,
                                             H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT );

        for( unsigned i = 0; i<compoundVect.size(); ++i)
        {
            hsize_t offset[1] = { i };
            H5Sselect_elements( fileSzHid, H5S_SELECT_SET, 1, offset );

            // Write the frameData to the selected frameMetaData element

            if (H5Dwrite( frameMetaDataHid, memHid, memSzHid,
                          fileSzHid, H5P_DEFAULT,
                          compoundVect[i]->getConstCompoundDataPtr() ) >= 0)
            {
                CPLDebug( HDF5R_DEBUG_STR, "Hdf5rWriter::h5WriteCompound write success." );
            }
            else
                CPLError( CE_Failure, CPLE_FileIO, "Hdf5rWriter::h5WriteCompound write FAILED for %s!",
                          dsName.c_str() );

        }

        H5Tclose( memHid );
        H5Sclose( memSzHid );
        H5Sclose( fileSzHid );
    }
    return 0;
}

//******************************************************************************
// Build and write the FrameMetaData compound data set
//******************************************************************************
int Hdf5rWriter::setFrameDataFromMap( const Hdf5rFrameData* frameData )
{
    const Hdf5rFrameData::CompoundElementMap_t& frameElementMap = frameData->getAttrMap();

    return h5WriteCompoundFromMap( frameElementMap,
                                   frameData->getFrameDataConstPtr(),
                                   sizeof(Hdf5rFrameData::FrameData_t),
                                   rootGroupHid_,
                                   "frameMetaData" );
}

//******************************************************************************
// Set an HDF5 attribute with error handling
//******************************************************************************
bool Hdf5rWriter::setAttribute( hid_t groupHid,
                                const std::string& attrName,
                                hid_t h5TypeId,
                                hid_t h5SpaceId,
                                const void* val )
{
    bool rc = true;

    hid_t aHid = H5Acreate2( groupHid, attrName.c_str(),
                             h5TypeId, h5SpaceId, H5P_DEFAULT, H5P_DEFAULT );
    if (aHid >= 0)
    {
        if (H5Awrite( aHid, h5TypeId, val ) < 0)
        {
            CPLError(CE_Failure, CPLE_IllegalArg, "  HDF5-R Attribute write failed for: %s" , attrName.c_str() );
        }
        H5Aclose( aHid );
    }
    else
        CPLError(CE_Failure, CPLE_IllegalArg, "  HDF5-R Attribute open failed for: %s" , attrName.c_str() );

    return rc;
}

//******************************************************************************
// Write the image buffer to the HDF5-R file
//******************************************************************************
bool Hdf5rWriter::writeImage( unsigned nrows, unsigned ncols, const int32_t* pImage )
{
    // create the h5 data space
    hsize_t dims[3] = {1, nrows, ncols};
    hid_t imageSpaceHid = H5Screate_simple( 3, dims, nullptr );
    if (imageSpaceHid < 0)
    {
        throw std::runtime_error( "Hdf5rWriter::writeImage() H5Screate_simple for image failed." );
    }

    // use a hyperslab space to select (the) 1 frame
    // set up hyper-slab selection arrays
    //   [0]: selected image frame
    //   [1]: row == scan line
    //   [2]: column == pixels per scan line
    hsize_t offset[3] = { 0, 0, 0 };
    hsize_t count[3] = { 1, nrows, ncols };
    herr_t err = H5Sselect_hyperslab( imageSpaceHid,
                                      H5S_SELECT_SET,
                                      offset, nullptr,
                                      count, nullptr );
    if (err < 0)
    {
        H5Sclose( imageSpaceHid );
        throw std::runtime_error( "Hdf5rWriter::writeImage() H5Sselect_hyperslab for image failed." );
    }

    // Open the CalRawData DATASET and Space
    hid_t imageHid = H5Dcreate2( rootGroupHid_,
                                 "CalRawData",
                                 H5T_NATIVE_INT32,
                                 imageSpaceHid,
                                 H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT );
    if (imageHid < 0)
    {
        H5Sclose( imageSpaceHid );
        throw std::runtime_error( "Hdf5rWriter::writeImage() H5Dcreate2 for image failed." );
    }

    // the memory space that contains the source image frame
    hsize_t memDims[2] = {nrows, ncols};
    hid_t memSpace = H5Screate_simple( 2, memDims, nullptr );

    // Transfer from memory to the hdf5-r file
    bool rc = true;
    if (H5Dwrite( imageHid, H5T_NATIVE_INT32, memSpace, imageSpaceHid, H5P_DEFAULT, pImage ) < 0)
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "Hdf5rWriter::writeImage()  H5Dwrite failed." );
        rc = false;
    }
    else
        CPLDebug( HDF5R_DEBUG_STR, "Hdf5rWriter::writeImage() wrote image to: %s",
                  filename_.c_str() );

    H5Sclose( imageSpaceHid );
    H5Sclose( memSpace );
    H5Dclose( imageHid );
    return rc;
}

//******************************************************************************
// Write the GeoLocationData (LOS grid) to the HDF5-R file
//******************************************************************************
bool Hdf5rWriter::writeLosGrid( const Hdf5rLosGrid_t* losGrid,
                                const Hdf5rGeoLocAttributes* geoLocAttributes )
{
    // require both input pointers to proceed
    if ((!losGrid) || (!geoLocAttributes))
        return false;

    // Create HDF5 memory Type and Space for the GeoLocationData Compound
    // type with space for 1 element
    hid_t memHid = H5Tcreate( H5T_COMPOUND, sizeof(Hdf5rLosGrid_t::Hdf5rLosData_t) );
    hsize_t memDims[2] = { losGrid->getNrows(), losGrid->getNcols() };
    hid_t memSzHid = H5Screate_simple( 2, memDims, nullptr);
    if (memSzHid < 0)
    {
        throw std::runtime_error( "Hdf5rWriter::writeLosGrid() H5Screate_simple for image failed." );
    }

    // identify the compound data components
    H5Tinsert( memHid, "ecf_X",  HOFFSET(Hdf5rLosGrid_t::Hdf5rLosData_t, ecf_X),  H5T_NATIVE_FLOAT );
    H5Tinsert( memHid, "ecf_Y",  HOFFSET(Hdf5rLosGrid_t::Hdf5rLosData_t, ecf_Y),  H5T_NATIVE_FLOAT );
    H5Tinsert( memHid, "ecf_Z",  HOFFSET(Hdf5rLosGrid_t::Hdf5rLosData_t, ecf_Z),  H5T_NATIVE_FLOAT );
    H5Tinsert( memHid, "lat",    HOFFSET(Hdf5rLosGrid_t::Hdf5rLosData_t, map_Y),  H5T_NATIVE_FLOAT );
    H5Tinsert( memHid, "lon",    HOFFSET(Hdf5rLosGrid_t::Hdf5rLosData_t, map_X),  H5T_NATIVE_FLOAT );

    // Create the file data type
    // Per the HDF5R-ICD the grids do not overhang the rows or columns
    hsize_t fileDims[3] = { 1, losGrid->getNrows()-1, losGrid->getNcols()-1 };
    hid_t fileSzHid = H5Screate_simple( 3, fileDims, nullptr);
    hid_t geoLocationDataHid = H5Dcreate2( rootGroupHid_, "GeoLocationData",
                                           memHid, fileSzHid,
                                           H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT );

    // Select the grid region to transfer to the file
    hsize_t offsetOut[2] = { 0, 0 };
    hsize_t countOut[2] = { fileDims[1], fileDims[2] };
    if (H5Sselect_hyperslab( memSzHid, H5S_SELECT_SET, offsetOut, nullptr,
                                 countOut, nullptr ) >= 0)
    {
        // Write the GeoLocationData
        if (H5Dwrite( geoLocationDataHid, memHid, memSzHid, fileSzHid, H5P_DEFAULT,
                      losGrid->getConstLosDataArray() ) >= 0)
        {
            CPLDebug( HDF5R_DEBUG_STR,
                      "Hdf5rWriter::writeLosGrid write success." );
        }
        else
            CPLError( CE_Failure, CPLE_FileIO,
                      "Hdf5rWriter::writeLosGrid write FAILED!" );
    }
    else // selection failed
    {
        CPLError(
                CE_Failure,
                CPLE_IllegalArg,
                "Hdf5rWriter::writeLosGridd H5Sselect_hyperslab of output grid failed." );
    }

    // Set the geoLocation attributes from the map
    setAttrsFromMap( geoLocationDataHid, geoLocAttributes->getConstAttrMap() );

    H5Sclose( fileSzHid );
    H5Dclose( geoLocationDataHid );
    H5Sclose( memSzHid );
    H5Tclose( memHid );

    return true;
}

//******************************************************************************
// Write summary metadata, which includes both the Error Info Table
// and the Sequence Info Table for one frame, to the HDF5-R file
//******************************************************************************
int Hdf5rWriter::setSummaryDataFromMap( std::vector<const CompoundBase*>& errorInfoVect,
                                        std::vector<const CompoundBase*>& seqInfoVect )
{
    int rc = 0;

    hid_t summaryMetaDataGroupHid = H5Gcreate2( rootGroupHid_, "summaryMetaData",
                                                H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT );

    if (summaryMetaDataGroupHid >= 0)
    {
        rc += h5WriteCompound( errorInfoVect,
                               sizeof(ErrorInfoTable::ErrorInfoTable_t),
                               summaryMetaDataGroupHid,
                               "errorInfoTable" );

        rc += h5WriteCompound( seqInfoVect,
                               sizeof(SeqInfoTable::SeqInfoTable_t),
                               summaryMetaDataGroupHid,
                               "seqInfoTable" );

        H5Gclose( summaryMetaDataGroupHid );
    }

    return rc;
}
