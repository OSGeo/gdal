/*
 * CompoundBase.cpp
 *
 *  Created on: Oct 16, 2018
 *      Author: nielson
 */

#include <sstream>
#include <iomanip>
#include <string>
#include <iostream>

#include <stdio.h>
#include <string.h>

#include "CompoundBase.h"

//******************************************************************************
// Constructor
//******************************************************************************
CompoundBase::CompoundBase( CompoundData_t* cdptr )
: compoundElementMap_(),
  compoundData_( cdptr )
{}

//******************************************************************************
// Destructor
//******************************************************************************
CompoundBase::~CompoundBase()
{
    delete compoundData_;
    compoundData_ = nullptr;
}

//******************************************************************************
// Set the contents from a string based on CompoundElement_t
//******************************************************************************
void CompoundBase::CompoundElement_t::setValue( const std::string& v,
                                                CompoundData_t* dataPtr )
{
    // pointer to the data item
    uint8_t* dptr = reinterpret_cast<uint8_t*>(dataPtr) + offset;

    // number of  data items, with 0==>1 for scalar item
    // (note strings are assumed null terminated so nItems is not used
    //   for strings)
    int nItems = (dimension == 0) ? 1 : dimension;

    // Each parser reads the next token from the stream, both the stream
    // and nItems (above) control number of tokens allowed
    std::istringstream iss( v );
    std::string token;

    // convert from string to type, use istream for array types
    switch (ptrType)
    {
    case PT_I32:
    {
        int32_t* i32ptr = reinterpret_cast<int32_t*>(dptr);
        while ((iss >> token) && (nItems-- > 0))
            *i32ptr++ = std::stoi( token );
    }
    break;
    case PT_U32:
    {
        uint32_t* u32ptr = reinterpret_cast<uint32_t*>(dptr);
        while ((iss >> token) && (nItems-- > 0))
            *u32ptr++ = std::stoul( token );
    }
    break;

    case PT_I64:
    {
        int64_t* i64ptr = reinterpret_cast<int64_t*>(dptr);
        while ((iss >> token) && (nItems-- > 0))
            *i64ptr++ = std::stoll( token );
    }
    break;

    case PT_U64:
    {
        uint64_t* u64ptr = reinterpret_cast<uint64_t*>(dptr);
        while ((iss >> token) && (nItems-- > 0))
            *u64ptr++ = std::stoull( token );
    }
    break;

    case PT_FLT:
    {
        float* fltptr = reinterpret_cast<float*>(dptr);
        while ((iss >> token) && (nItems-- > 0))
            *fltptr++ = std::stof( token );
    }
    break;

    case PT_DBL:
    {
        double* dblptr = reinterpret_cast<double*>(dptr);
        while ((iss >> token) && (nItems-- > 0))
            *dblptr++ = std::stod( token );
    }
    break;

    case PT_CSTR:
    {
        char* cstr = reinterpret_cast<char*>(dptr);
        memset( cstr, 0, dimension );
        strncpy( cstr, v.c_str(), dimension-1 );
    }
    break;

    default:
        break;
    }
}

//******************************************************************************
// Build a value string from the contents of a CompoundElement_t
//******************************************************************************
std::string CompoundBase::CompoundElement_t::toString( const CompoundData_t* dataPtr ) const
{
    // pointer to the data item
    const uint8_t* dptr = reinterpret_cast<const uint8_t*>(dataPtr) + offset;

    // string to return
    std::string result;

    // number of iterations over data items, with 0==>1 for scalar item
    // (note strings are assumed null terminated so nItems is not used
    //   for strings)
    unsigned nItems = (dimension == 0) ? 1 : dimension;

    // convert to string by type
    switch (ptrType)
    {
    case PT_I32:
    {
        const int32_t* i32ptr = reinterpret_cast<const int32_t*>(dptr);
        for (unsigned i=0; i<nItems; ++i)
        {
            result += std::to_string( *i32ptr++ );
            if (i<(nItems-1))
                result += " ";
        }
    }
    break;
    case PT_U32:
    {
        const uint32_t* u32ptr = reinterpret_cast<const uint32_t*>(dptr);
        for (unsigned i=0; i<nItems; ++i)
        {
            result += std::to_string( *u32ptr++ );
            if (i<(nItems-1))
                result += " ";
        }
    }
    break;

    case PT_I64:
    {
        const int64_t* i64ptr = reinterpret_cast<const int64_t*>(dptr);
        for (unsigned i=0; i<nItems; ++i)
        {
            result += std::to_string( *i64ptr++ );
            if (i<(nItems-1))
                result += " ";
        }
    }
    break;

    case PT_U64:
    {
        const uint64_t* u64ptr = reinterpret_cast<const uint64_t*>(dptr);
        for (unsigned i=0; i<nItems; ++i)
        {
            result += std::to_string( *u64ptr++ );
            if (i<(nItems-1))
                result += " ";
        }
    }
    break;

    case PT_FLT:
    {
        const float* fltptr = reinterpret_cast<const float*>(dptr);
        for (unsigned i=0; i<nItems; ++i)
        {
            result += std::to_string( *fltptr++ );
            if (i<(nItems-1))
                result += " ";
        }
    }
    break;

    case PT_DBL:
    {
        const double* dblptr = reinterpret_cast<const double*>(dptr);
        for (unsigned i=0; i<nItems; ++i)
        {
            std::ostringstream oss;
            oss << std::setprecision( 18 ) << *dblptr++;
            result += oss.str();
            if (i<(nItems-1))
                result += " ";
        }
    }
    break;

    case PT_CSTR:
    {
        char* wkbuff = new char[dimension + 1];
        memset( wkbuff, 0, dimension+1);
        strncpy( wkbuff, reinterpret_cast<const char*>(dptr), dimension );
        result = std::string( wkbuff );
        delete [] wkbuff;
    }
    break;

    default:
        result = "UNKNOWN: Bad Conversion";
    }
    return result;
}


//******************************************************************************
// Build formatted string of selected attributes in FrameData_t
//******************************************************************************
bool CompoundBase::modifyValue( const std::string& name,
                                const std::string& value )
{
    // The map key starts after the last '.'
    std::string::size_type dotPos = name.find_last_of( '.' );

    // if '.' not found then use whole string
    if (dotPos == std::string::npos)
        dotPos = 0;

    // otherwise move past the '.' unless already at end
    else
        ++dotPos;

    // process valid substrings (if '.' is last char, could now be past the end)
    if (dotPos < name.size())
    {
        std::string fname = name.substr( dotPos );

        // locate in map
        CompoundElementMap_t::iterator ifind = compoundElementMap_.find( fname );
        if (ifind != compoundElementMap_.end())
        {
            // found name ==> change value
            ifind->second.setValue( value, compoundData_ );
            return true;
        }
    }

    return false;
}
