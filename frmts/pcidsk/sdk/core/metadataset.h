/******************************************************************************
 *
 * Purpose:  Declaration of the MetadataSet class.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
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
