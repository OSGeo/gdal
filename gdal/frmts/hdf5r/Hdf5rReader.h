/*
 * Hdf5rReader.h
 *
 *  Created on: Sep 4, 2018
 *      Author: nielson
 */

#ifndef FRMTS_HDF5R_HDF5RREADER_H_
#define FRMTS_HDF5R_HDF5RREADER_H_

#include <string>
#include <functional>

#define H5_USE_16_API
#include "hdf5.h"

#include "hdf5r.h"
#include "Hdf5rIObase.h"
#include "Hdf5rLosGrid.h"
#include "Hdf5rFrameData.h"
#include "Hdf5rFileAttributes.h"
#include "Hdf5rGeoLocAttributes.h"
#include "Hdf5rSummaryMetaData.h"

class Hdf5rReader : public Hdf5rIObase
{
public:
    Hdf5rReader();
    virtual ~Hdf5rReader();

    bool open( const std::string& filename,
               unsigned h5Flags = H5F_ACC_RDONLY );

    void close();

    const std::string& getFileName() const {return fileName_;}

    hsize_t getNumSubFrames() const {return nMetaDataFrames_;}

    /**
     * Retrieve specific frameMetaData fields from for a particular frame index
     * in the range [0..numFrames_]. Applies to a successfully opened file
     * i.e. hdf5rFileHid_ must be >= 0.
     * @param frameIndex Frame index to retrieve, in the range [0..numFrames_].
     * @return A single FrameData type from the heap.  Caller must delete the
     *         returned value when done.  nullptr returned if HDF5-R file is not
     *         open (hdf5rFileHid_ < 0), or frameIndex is out of range.
     */
    bool getFrameMetaData( unsigned frameIndex, Hdf5rFrameData* hdf5rFrameData );

    /**
     * Get the image dimensions for a selected frame index (from 0).
     * @param frameIndex Index starting at 0 of the selected frame.
     * @param rows Pointer to an integer to load with the number of rows
     *             or scanlines.  Corresponds to the Y dimension. Can be null.
     * @param cols Pointer to an integer to load with the number of columns
     *             or sensor cells.  Corresponds to the X dimension.
     *             Can be null.
     * @return true on success, false if HDF5 file does not have the dimensions
     *         set properly (number of elements != 3).
     *
     */
    bool getImageDimensions( unsigned frameIndex, int* rows, int* cols );

    /**
     * Set or change the number of rows which corresponds to the Y dimension.
     * @param rows New number of rows, must be a positive integer.
     */
    void setRows( int rows ) {nImageRows_ = rows;}

    /**
     * Set or change the number of columns which corresponds to the X dimension.
     * @param cols  New number of columns, must be a positive integer.
     */
    void setColumns( int cols ) {nImageColumns_ = cols;}

    /**
     * Retrieves the line-of-sight grid and associated attributes and checks for
     * completeness.  The getFrameMetaData() and getImageDimensions() methods
     * must be called before calling this method.
     * @param frameIndex Frame index to retrieve, in the range [0..numFrames_].
     * @return  True if LOS grid is complete for this image, false if
     *          incomplete or values set by getFrameMetaData() or
     *          getImageDimensions are incorrect or have not been set.
     */
    const Hdf5rLosGrid_t* getLosGrid( unsigned frameIndex,
                                      const Hdf5rGeoLocAttributes* geoLocAttributes,
                                      m3d::Vector& satEcfMeters,
                                      const Earth& earth );

    /**
     * Load each compound element of the Summary Metadata groups
     * ErrorInfo and SeqInfo into vectors.  The vector element pointers are of
     * the common super class, CompoundBase.
     * @param errorInfoVect A reference to a vector of pointers to ErrorInfoTable
     *                  type.
     * @param seqInfoVect A reference to a vector of pointers to SeqInfoTable
     *                type.
     * @return true on successful read, false otherwise.
     */
    bool getSummaryMetadata( std::vector<const CompoundBase*>& errorInfoVect,
                             std::vector<const CompoundBase*>& seqInfoVect );

    bool chkFileAttribute( hid_t h5Hid, const std::string& attrName ) const;

    /**
     * Open an attribute from an open group (must be open), read the value,
     * and close the attribute. Generate error messages if necessary.
     * @param groupHid An open hid_t for an HDF5 group
     * @param attrName HDF5 group attribute name.
     * @param h5TypeId The H5T_* type name of the parameter.
     * @param val void pointer to a data area that is at least as large as
     *            the type being fetched
     * @return true on success, false otherwise
     */
    static bool getAttribute( hid_t groupHid,
                              const std::string& attrName,
                              hid_t h5TypeId,
                              void* val );

    static bool getStrAttribute( hid_t groupHid,
                                 const std::string& attrName,
                                 char** val );

    int fillAttrMap( hid_t h5Hid,
    		         Hdf5rAttributeBase::H5AttrMap_t& attrMap,
					 bool warnMissing ) const;

    int fillFileAttrMap( Hdf5rFileAttributes::H5AttrMap_t& fileAttrMap,
    		             bool warnMissing ) const
    {
        return fillAttrMap( rootGroupHid_, fileAttrMap, warnMissing );
    }

    int fillGeoLocAttrMap( Hdf5rAttributeBase::H5AttrMap_t& geoLocAttrMap,
    		               bool warnMissing ) const
    {
        return fillAttrMap( geoLocationDataHid_, geoLocAttrMap, warnMissing );
    }

    /**
     * Open an attribute from the root group (must be open), read the value,
     * and close the attribute. Generate error messages if necessary.
     * @param attrName HDF5 Root group attribute name.
     * @param h5TypeId The H5T_* type name of the parameter.
     * @param val void pointer to a data area that is at least as large as
     *            the type being fetched
     * @return true on success, false otherwise
     */
    bool getAttribute( const std::string& attrName,
                       hid_t h5TypeId,
                       void* val ) const
    {
        return getAttribute( rootGroupHid_,
                             attrName,
                             h5TypeId,
                             val );
    }

    bool getStrAttribute( const std::string& attrName,
                          char** val ) const
    {
        return getStrAttribute( rootGroupHid_,
                                attrName,
                                val );
    }

    CPLErr readBlock( int frameIndex, int row, int col, int32_t noDataValue, void* pImage );

    bool haveGeoLocationData() const {return geoLocationDataHid_ > 0;}
    bool haveFrameMetaData() const {return frameMetaDataHid_ > 0;}
    bool haveCalRawData() const {return imageHid_ > 0;}

    /**
     * Recompute the LOS grid using an observer at 'satEcfMeters'
     * @param satEcfMeters Observing satellite vector.
     * @param offEarthValue The value used to flag off-Earth latitude and
     *                      longitude.
     * @param earth The Earth model in use.
     * @return number of on-Earth points in grid
     */
    int changeLosGridReference( const m3d::Vector& setEcfMeters,
                                double offEarthValue )
    {
        if (losGrid_)
            return losGrid_->changeObserverLocation( setEcfMeters, offEarthValue );
        else
            return 0;
    }

    void blankOffEarthOnRead( bool b ) {doBlankOffEarth_ = b;}


private:
    std::string fileName_;

    // H5 descriptors, open if >= 0, closed by the destructor
    hid_t hdf5rFileHid_;
    hid_t rootGroupHid_;
    hid_t geoLocationDataHid_;
    hid_t geoLocationSpaceHid_;
    hid_t frameMetaDataHid_;
    hid_t frameMetaDataSpaceHid_;
    hid_t imageHid_;
    hid_t imageSpaceHid_;

    // Number of frames in frameMetaData
    hsize_t nMetaDataFrames_;

    // Number of rows and columns in image (-1 if not set)
    int nImageRows_;
    int nImageColumns_;

    // Line-of-Sight grid loaded by getLosGrid()
    Hdf5rLosGrid_t* losGrid_;

    bool doBlankOffEarth_;

    /**
     * Open the H5 component descriptors for this file. Also gets the number of
     * image frames in the file.  Descriptors are closed by the destructor.
     * @param fileName Name of the HDF5-R file to open
     * @return Boolean true if H5 descriptors are opened
     */
    bool OpenHdf5Components(const std::string& fileName);

    bool h5ReadCompound( std::vector<const CompoundBase*>& compoundVect,
                         std::function<CompoundBase* ()> create,
                         hid_t groupHid,
                         const std::string& dsName );

    void offEarthBlanking( int32_t noDataValue, int32_t* pImage ) const;

    void blankGridTile( int gridRow, int gridCol,
                        int32_t noDataValue, int32_t* pImage ) const;

    void testAndBlankGridTile( const Hdf5rLosGrid_t::GridTile_t& gridTile,
                               int gridRow, int gridCol,
                               int32_t noDataValue, int32_t* pImage ) const;

    void interpolateUnitTest() const;
};

#endif /* FRMTS_HDF5R_HDF5RREADER_H_ */
