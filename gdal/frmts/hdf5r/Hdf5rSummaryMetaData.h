/*
 * ErrorInfoTable.h
 *
 *  Created on: Oct 18, 2018
 *      Author: nielson
 */

#ifndef FRMTS_HDF5R_ERRORINFOTABLE_H_
#define FRMTS_HDF5R_ERRORINFOTABLE_H_

#include <cstring>
#include <string>

#include "CompoundBase.h"

class ErrorInfoTable: public CompoundBase
{
public:

    static constexpr const char* ERROR_INFO_PREFIX = "H5R.EI";
    static constexpr int ERROR_INFO_PREFIX_SZ = sizeof(ERROR_INFO_PREFIX) / sizeof(char*);
    static constexpr const char* ERROR_INFO_FMT_PREFIX = "H5R.EI%03d.";

    ErrorInfoTable( const char* errCstr = "NO_ERRORS",
                    int count = 1,
                    float percent = 100.0 )
    : CompoundBase( new ErrorInfoTable_t( errCstr, count, percent ) )
    {
        compoundElementMap_["errorTypeStr"]            = CompoundElement_t( "errorTypeStr",            HOFFSET(ErrorInfoTable_t, errorTypeStr),             H5T_C_S1,         CompoundElement_t::PT_CSTR, sizeof(ErrorInfoTable_t::errorTypeStr) );
        compoundElementMap_["affectedFrameCt"]         = CompoundElement_t( "affectedFrameCt",         HOFFSET(ErrorInfoTable_t, affectedFrameCt),          H5T_NATIVE_INT32, CompoundElement_t::PT_I32,  0 );
        compoundElementMap_["percentOfFramesAffected"] = CompoundElement_t( "percentOfFramesAffected", HOFFSET(ErrorInfoTable_t, percentOfFramesAffected),  H5T_NATIVE_FLOAT, CompoundElement_t::PT_FLT,  0 );
    }

    virtual ~ErrorInfoTable() {}

    struct ErrorInfoTable_t  : CompoundBase::CompoundData_t
    {
        char    errorTypeStr[24];
        int32_t affectedFrameCt;
        float   percentOfFramesAffected;

        /**
         * Default Constructor
         * Sets the "NO_ERRORS" condition for a single generated frame.
         **/
        ErrorInfoTable_t()
        : errorTypeStr{ "NO_ERRORS" },
          affectedFrameCt( 1 ),
          percentOfFramesAffected( 100.0 )
        {}

        ErrorInfoTable_t( const char* errCstr,
                          int count,
                          float percent )
        : errorTypeStr{ "" },
          affectedFrameCt( count ),
          percentOfFramesAffected( percent )
        {
            std::strncpy( errorTypeStr, errCstr, sizeof(errorTypeStr)-1 );
        }
    };

    virtual size_t getCompoundSize() const override  {return sizeof(ErrorInfoTable_t);}

    /**
     * The HDF5 reader needs direct access to the ErrorInfoTable structure so
     * it can load the data from the file.
     * @return R/W Pointer to ErrorInfoTable.
     */
    ErrorInfoTable_t* getErrorInfoPtr() {return static_cast<ErrorInfoTable_t*>(compoundData_);}

    /**
     * Read only access to the ErrorInfoTable structure.
     * @return RO Pointer to ErrorInfoTable.
     */
    const ErrorInfoTable_t* getErrorInfoConstPtr() const {return static_cast<const ErrorInfoTable_t*>(compoundData_);}

    virtual std::string formatAttribute( const std::string& name,
                                         unsigned indexNumber ) const override
    {
        // prepend the the frame prefix to the attribute name
        std::string fmtString = ERROR_INFO_FMT_PREFIX + name;

        // use snprintf to format the frame number into the string.
        char wkbuff[256];
        std::snprintf( wkbuff, 255, fmtString.c_str(), indexNumber );

        return std::string( wkbuff );
    }
};

class SeqInfoTable: public CompoundBase
{
public:

    static constexpr const char* SEQ_INFO_PREFIX = "H5R.SI";
    static constexpr int SEQ_INFO_PREFIX_SZ = sizeof(SEQ_INFO_PREFIX) / sizeof(char*);
    static constexpr const char* SEQ_INFO_FMT_PREFIX = "H5R.SI%03d.";

    SeqInfoTable()
    : CompoundBase( new SeqInfoTable_t )
    {
        compoundElementMap_["seqIndex"]        = CompoundElement_t( "seqIndex",        HOFFSET(SeqInfoTable_t, seqIndex),        H5T_NATIVE_INT32, CompoundElement_t::PT_I32,  0 );
        compoundElementMap_["minLat"]          = CompoundElement_t( "minLat",          HOFFSET(SeqInfoTable_t, minLat),          H5T_NATIVE_FLOAT, CompoundElement_t::PT_FLT,  0 );
        compoundElementMap_["maxLat"]          = CompoundElement_t( "maxLat",          HOFFSET(SeqInfoTable_t, maxLat),          H5T_NATIVE_FLOAT, CompoundElement_t::PT_FLT,  0 );
        compoundElementMap_["minLon"]          = CompoundElement_t( "minLon",          HOFFSET(SeqInfoTable_t, minLon),          H5T_NATIVE_FLOAT, CompoundElement_t::PT_FLT,  0 );
        compoundElementMap_["maxLon"]          = CompoundElement_t( "maxLon",          HOFFSET(SeqInfoTable_t, maxLon),          H5T_NATIVE_FLOAT, CompoundElement_t::PT_FLT,  0 );
        compoundElementMap_["minCalIntensity"] = CompoundElement_t( "minCalIntensity", HOFFSET(SeqInfoTable_t, minCalIntensity), H5T_NATIVE_INT32, CompoundElement_t::PT_I32,  0 );
        compoundElementMap_["maxCalIntensity"] = CompoundElement_t( "maxCalIntensity", HOFFSET(SeqInfoTable_t, maxCalIntensity), H5T_NATIVE_INT32, CompoundElement_t::PT_I32,  0 );
        compoundElementMap_["maxLineNumber"]   = CompoundElement_t( "maxLineNumber",   HOFFSET(SeqInfoTable_t, maxLineNumber),   H5T_NATIVE_INT32, CompoundElement_t::PT_I32,  0 );
        compoundElementMap_["numFrames"]       = CompoundElement_t( "numFrames",       HOFFSET(SeqInfoTable_t, numFrames),       H5T_NATIVE_INT32, CompoundElement_t::PT_I32,  0 );
    }

    virtual ~SeqInfoTable() {}

    struct SeqInfoTable_t: CompoundBase::CompoundData_t
    {
        int32_t seqIndex;
        float   minLat;
        float   maxLat;
        float   minLon;
        float   maxLon;
        int32_t minCalIntensity;
        int32_t maxCalIntensity;
        int32_t maxLineNumber;
        int32_t numFrames;

        /**
         * Default constructor
         */
        SeqInfoTable_t()
        : seqIndex( 0 ),
          minLat( 0.0 ),
          maxLat( 0.0 ),
          minLon( 0.0 ),
          maxLon( 0.0 ),
          minCalIntensity( 0 ),
          maxCalIntensity( 0 ),
          maxLineNumber( 0 ),
          numFrames( 1 )
        {}
    };

    virtual size_t getCompoundSize() const override  {return sizeof(SeqInfoTable_t);}

    /**
     * The HDF5 reader needs direct access to the SeqInfoTable_t structure so
     * it can load the data from the file.
     * @return R/W Pointer to SeqInfoTable_t.
     */
    SeqInfoTable_t* getSeqInfoPtr() {return static_cast<SeqInfoTable_t*>(compoundData_);}

    /**
     * Read only access to the SeqInfoTable_t structure.
     * @return RO Pointer to SeqInfoTable_t.
     */
    const SeqInfoTable_t* getSeqInfoConstPtr() const {return static_cast<const SeqInfoTable_t*>(compoundData_);}

    virtual std::string formatAttribute( const std::string& name,
                                         unsigned indexNumber ) const override
    {
        // prepend the the frame prefix to the attribute name
        std::string fmtString = SEQ_INFO_FMT_PREFIX + name;

        // use snprintf to format the frame number into the string.
        char wkbuff[256];
        std::snprintf( wkbuff, 255, fmtString.c_str(), indexNumber );

        return std::string( wkbuff );
    }
};

#endif /* FRMTS_HDF5R_ERRORINFOTABLE_H_ */
