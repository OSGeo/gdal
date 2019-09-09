/*
 * CompoundBase.h
 *
 *  Created on: Oct 16, 2018
 *      Author: nielson
 */

#ifndef FRMTS_HDF5R_COMPOUNDBASE_H_
#define FRMTS_HDF5R_COMPOUNDBASE_H_

#include <string>
#include <map>

#include "hdf5.h"

class CompoundBase
{
public:

    /**
     * The CompoundData_t is the base structure for the actual compound type
     * defined in classes derived from CompoundBase.  The actual compound type
     * requires ": public CompoundBase::CompoundData_t".
     */
    struct CompoundData_t
    {
        std::string toString() const;
    };

    /**
     * Constructor.
     * @note: The constructor of the derived class must instantiate
     * an instance of the actual compound structure for the derived class
     * and populate the map with the full CompoundElement_t constructor
     * for each element in the compound.
     * @param cdptr A pointer to the compound data structure derived from
     *              CompoundBase::CompoundData_t allocated from the heap.
     *              (It is deleted by the destructor).
     */
    CompoundBase( CompoundData_t* cdptr );

    /**
     * Destructor
     */
    virtual ~CompoundBase();


    /**
     * @brief A map element in CompoundData_t
     * Each element contains the HDF5-R defined name, its offset in the
     * subclass of CompoundData_, the HDT_NATIVE_... type id, dimension for
     * string and vector types or 0 for scalar, and an enumeration for the
     * c-type (so we can convert it to a string).
     */
    struct CompoundElement_t
    {
        enum Ptr_t {PT_UNKNOWN, PT_I32, PT_U32, PT_I64, PT_U64, PT_FLT, PT_DBL, PT_CSTR };

        CompoundElement_t()
        : name(), offset( 0 ), h5TypeId( -1 ), dimension( 0 ), ptrType( PT_UNKNOWN )
        {}

        CompoundElement_t( const std::string& nm, size_t off, hid_t typeId, Ptr_t pt, hsize_t dim )
        : name( nm ), offset( off ), h5TypeId( typeId ), dimension( dim ), ptrType( pt )
        {}

        std::string name;      ///< Case sensitive HDF5-R Frame compound data element name
        size_t      offset;    ///< Element offset in compound data structure
        hid_t       h5TypeId;  ///< HDF5 type name (HDT_NATIVE_*)
        hsize_t     dimension; ///< 0==scalar; 1...== number of vector elements
        Ptr_t       ptrType;   ///< pointer type for string conversion

        void setValue( const std::string& v, CompoundData_t* cdataPtr );

        /**
         * @brief Convert element content to a string.
         * Using the address of the Hdf5rFrameData_t and the enumeration
         * defining the underlying type, convert the element contents to a string.
         * @param frmDataPtr Pointer to as Hdf5rFrameData_t for the conversion.
         * @return String representation of the element contents.
         */
        std::string toString( const CompoundData_t* cdataPtr ) const;
    };

    // typedef for the map of FrameElement_t's indexed by GDAL attribute names
    typedef std::map<std::string, CompoundElement_t> CompoundElementMap_t;

    /**
     * Modify the value for a given frame data attribute name.
     * @param name GDAL attribute used as the map key for a frame data element.
     * @param value The new value for the element as a std::string.
     * @return Boolean true if name is found and the value modified.
     */
    bool modifyValue( const std::string& name,
                      const std::string& value );

    /**
     * This method searches the map for an attribute name that includes the
     * prefix (something like H5R.F0001. see FRAME_FMT_PREFIX for the actual
     * prefix). Thus for a fullName of H5R.F0001.SOME_ATTRIBUTE, this method
     * will search the map for SOME_ATTRIBUTE and return the string
     * representation of the value if found.
     * @param fullName Fully qualified attribute name
     * @return Value associated with the attribute suffix name.
     */
    const std::string& getValue( const std::string& fullName ) const;

    const CompoundElementMap_t& getAttrMap() const {return compoundElementMap_;}

    /**
     * This method adds the reference number to the attribute name which is
     * of the form "<prefix>%04d.name".
     * @param name An attribute base name.
     * @param refNumber An unsigned reference number
     * @return Formatted string
     */
    virtual std::string formatAttribute( const std::string& name,
                                         unsigned refNumber ) const = 0;

    virtual CompoundData_t* getCompoundDataPtr() {return compoundData_;}

    virtual const CompoundData_t* getConstCompoundDataPtr() const {return compoundData_;}

    virtual size_t getCompoundSize() const = 0;

protected:
    // map populated by the constructor key is GDAL attribute name, value
    // is CompoundElement_t
    CompoundElementMap_t compoundElementMap_;

    // instance of the derived compound data structure
    CompoundData_t* compoundData_;
};

#endif /* FRMTS_HDF5R_COMPOUNDBASE_H_ */
