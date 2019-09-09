#ifndef FRMTS_HDF5R_HDF5RFRAMEDATA_H_
#define FRMTS_HDF5R_HDF5RFRAMEDATA_H_

#include <string>
#include <cstring>
#include <map>
#include <cstdint>

#include "CompoundBase.h"

/**
 * @brief Frame Metadata map
 * The Hdf5rFrameData defines a constant map of the FrameMetaData compound type.
 * The map is keyed by the GDAL attribute name which is the same as the
 * element name in all caps with spaces separating words and prefixed with
 * H5R_F%04d, e.g. frameNumber ==> H5R.F0001. The %04d facilitates
 * substituting the frame number when the attribute name is generated.
 */
class Hdf5rFrameData : public CompoundBase
{
public:

    /**
     * @brief Default constructor
     * The default constructor builds the full map for all FrameMetaData
     * attributes.
     */
    Hdf5rFrameData();

    /**
     * Destructor
     */
    ~Hdf5rFrameData();

    static constexpr const char* FRAME_PREFIX = "H5R.F";
    static constexpr const char* FRAME_FMT_PREFIX = "H5R.F%04d.";

    /**
      * @brief Frame Metadata
      * This structure maps the contents for the FrameMetaData compound type.
      * The native type and array sizes, if applicable, should match the HDF5-R
      * ICD schema.
      */
     struct FrameData_t : CompoundBase::CompoundData_t
     {
         int32_t  frameNumber;
         uint64_t imageStatus;
         int32_t  beginChannel;
         int32_t  endChannel;
         int32_t  numChannels;
         int32_t  beginLine;
         int32_t  endLine;
         int32_t  numLines;
         char     AOI_name[32];
         int32_t  AOI_beginLine;
         int32_t  AOI_endLine;
         int32_t  AOI_beginChannel;
         int32_t  AOI_endChannel;
         int32_t  scanDir;
         int32_t  numGeoPoints;
         int32_t  year;
         int32_t  day;
         double   secondsOfDay;
         int32_t  calNoCalFlag;  // 0==Calibrated 1==Uncalibrated
         int32_t  imageId;
         double   satPosECF[3];
         double   satVelECF[3];
         double   lineDeltaTimeSecs;
         double   absoluteCalCoeff_kws;
         double   absoluteCalCoeff_wcmsq;
         double   sosCTCsecs;
         int32_t  sosSeqIndex;
         int32_t  sosStepIndex;
         int32_t  sosDirection;
         char     sosScaSelectStr[32];
         char     sosParentAimPtStr[32];
         double   sosScanRateMradUsecs;
         double   sosFrameTimeUsecs;
         double   sosBlankTimeUsecs;
         double   sosLongIntUsecs;
         double   sosShortIntUsecs;
         char     sosIntegMode[16];
         int32_t  minCalIntensity;
         int32_t  maxCalIntensity;
         int32_t  linesReversed;
         int32_t  chansReversed;
         float    UL_lat;
         float    UL_lon;
         float    UR_lat;
         float    UR_lon;
         float    LL_lat;
         float    LL_lon;
         float    LR_lat;
         float    LR_lon;
         int32_t  flowControl;
         char     imageScaSelectStr[32];

         /**
          * Default Constructor
          * Clears all fields to 0 using memset plain-old-data
          **/
         FrameData_t()
         {
             memset( this, 0, sizeof(FrameData_t) );
         }

         /**
          * Produces a string of some select fields in the structure, mostly for
          * debug
          * @return Formated string of names and values.
          */
         std::string toString() const;
     };

     virtual size_t getCompoundSize() const override  {return sizeof(FrameData_t);}

    /**
     * The HDF5 reader needs direct access to the FrameData_t structure so
     * it can load the data from the file.
     * @return R/W Pointer to FrameData_t.
     */
    FrameData_t* getFrameDataPtr() {return static_cast<FrameData_t*>(compoundData_);}

    /**
     * Read only access to the FrameData_t structure.
     * @return RO Pointer to FrameData_t.
     */
    const FrameData_t* getFrameDataConstPtr() const {return static_cast<const FrameData_t*>(compoundData_);}

    virtual std::string formatAttribute( const std::string& name,
                                         unsigned frameNumber ) const override;
};

#endif /* FRMTS_HDF5R_HDF5RFRAMEDATA_H_ */
