/*
 * Hdf5rWriter.h
 *
 *  Created on: Sep 7, 2018
 *      Author: nielson
 */

#include <string>

#define H5_USE_16_API
#include "hdf5.h"
#include "Hdf5rIObase.h"
#include "Hdf5rFileAttributes.h"
#include "Hdf5rGeoLocAttributes.h"
#include "Hdf5rFrameData.h"
#include "Hdf5rLosGrid.h"
#include "Hdf5rSummaryMetaData.h"

#ifndef FRMTS_HDF5R_HDF5RWRITER_H_
#define FRMTS_HDF5R_HDF5RWRITER_H_

class Hdf5rWriter : public Hdf5rIObase
{
public:
    /**
     * Default constructor performs minimal intialization.
     * @note Use the open() method to open the file and top level group.
     */
    Hdf5rWriter();

    /**
     * Destructor.
     */
    virtual ~Hdf5rWriter();

    /**
     * Open the specified file and top level groups and datasets.
     * @param filename HDF5-R file to open.
     * @return true on success
     */
    bool open( const std::string& filename );

    /**
     * Close the HDF5-R file from the open() method and clean up.
     */
    void close();

    int setAttrsFromMap( hid_t h5GroupHid,
                         const Hdf5rAttributeBase::H5AttrMap_t& attrMap );

    /**
     * Using the File Attribute Map, set all file level attributes on the
     * root group referenced in the map.
     * @param fileAttrMap Reference to a File Attribute Map.
     * @return Number of attributes set.
     */
    int setFileAttrsFromMap( const Hdf5rAttributeBase::H5AttrMap_t& fileAttrMap )
    {
        return setAttrsFromMap( rootGroupHid_, fileAttrMap );
    }

    int setSummaryDataFromMap( std::vector<const CompoundBase*>& errorInfoVect,
                               std::vector<const CompoundBase*>& seqInfoVect );

    /**
     * Using the Frame Element Map, set one FrameData dataset
     * referenced in the map.
     * @param frameDataMap Reference to a Frame Element Map.
     * @return Number of
     */
    int setFrameDataFromMap( const Hdf5rFrameData* frameData );

    /**
     * This static function sets an attribute on the specified Group ID.
     * @param groupHid An open h5 group ID.
     * @param attrName The name of the attribute to set.
     * @param h5TypeId The H5T type for this attribute.
     * @param h5SpaceId The H5S space for this attribute.
     * @param val A void pointer to the value for this attribute. The type of
     *            value pointed to must conform to the h5TypeId.
     * @return true on success
     */
    static bool setAttribute( hid_t groupHid,
                              const std::string& attrName,
                              hid_t h5TypeId,
                              hid_t h5SpaceId,
                              const void* val );

    /**
     * This class method sets an attribute on the root Group ID for
     * the open HDF5-R file.
     * @param attrName The name of the attribute to set.
     * @param h5TypeId The H5T type for this attribute.
     * @param h5SpaceId The H5S space for this attribute.
     * @param val A void pointer to the value for this attribute. The type of
     *            value pointed to must conform to the h5TypeId.
     * @return true on success
     */
    bool setRootAttribute( const std::string& attrName,
                           hid_t h5TypeId,
                           hid_t h5SpaceId,
                           const void* val ) const
    {
        return setAttribute( rootGroupHid_,
                             attrName,
                             h5TypeId,
                             h5SpaceId,
                             val );
    }

    bool writeImage( unsigned nrows, unsigned ncols, const int32_t* pImage );

    bool writeLosGrid( const Hdf5rLosGrid_t* losGrid,
                       const Hdf5rGeoLocAttributes* geoLocAttributes );

private:
    std::string filename_;

    // HDF5-R file and top level components
    hid_t hdf5rFileHid_;
    hid_t rootGroupHid_;

    int h5WriteCompoundFromMap( const CompoundBase::CompoundElementMap_t& elementMap,
                                const void* structPtr,
                                size_t structSize,
                                hid_t groupHid,
                                const std::string& dsName );

    int h5WriteCompound( const std::vector<const CompoundBase*>& compoundVect,
                         size_t structSize,
                         hid_t groupHid,
                         const std::string& dsName );

};

#endif /* FRMTS_HDF5R_HDF5RWRITER_H_ */
