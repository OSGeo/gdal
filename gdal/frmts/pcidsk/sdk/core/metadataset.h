/******************************************************************************
 *
 * Purpose:  Declaration of the MetadataSet class.
 * 
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 50 West Wilmot Street, Richmond Hill, Ont, Canada
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/
#ifndef INCLUDE_PRIV_METADATASET_H
#define INCLUDE_PRIV_METADATASET_H

#include "pcidsk_config.h"
#include "pcidsk_file.h"
#include <vector>
#include <string>
#include <map>

namespace PCIDSK
{
    class CPCIDSKFile;
    /************************************************************************/
    /*                             MetadataSet                              */
    /************************************************************************/

    class MetadataSet 
    {									
    public:
        MetadataSet();
        ~MetadataSet();

        void        Initialize( PCIDSKFile *file, const std::string& group, int id );
        std::string GetMetadataValue( const std::string& key );
        void        SetMetadataValue( const std::string& key, const std::string& value );
        std::vector<std::string> GetMetadataKeys();
    
    private:
        PCIDSKFile  *file;

        bool         loaded;
        std::map<std::string,std::string> md_set;

        std::string  group;
        int          id;

        void         Load();
    };
    
} // end namespace PCIDSK
#endif // INCLUDE_PRIV_METADATASET_H
