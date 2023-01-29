/******************************************************************************
 * $Id$
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Header file for HDF5 HDFEOS parser
 * Author:   Even Rouault
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef HDF5EOSPARSER_H_INCLUDED
#define HDF5EOSPARSER_H_INCLUDED

#include "hdf5_api.h"

#include <map>
#include <string>
#include <vector>

/************************************************************************/
/*                             HDF5EOSParser                            */
/************************************************************************/

class HDF5EOSParser
{
  public:
    HDF5EOSParser() = default;

    struct Dimension
    {
        std::string osName;
        int nDimIndex;
        int nSize;
    };

    struct GridMetadata
    {
        std::vector<Dimension> aoDimensions;
        std::string osProjection{};  // e.g HE5_GCTP_SNSOID
        std::string osGridOrigin{};  // e.g HE5_HDFE_GD_UL
        std::vector<double>
            adfProjParams{};  // e.g (6371007.181000,0,0,0,0,0,0,0,0,0,0,0,0)
        std::vector<double>
            adfUpperLeftPointMeters{};  // e.g (-1111950.519667,5559752.598333)
        std::vector<double>
            adfLowerRightPointMeters{};  // e.g (0.000000,4447802.078667)
    };

    static bool HasHDFEOS(hid_t hRoot);
    bool Parse(hid_t hRoot);

    bool GetMetadata(const char *pszSubdatasetName,
                     GridMetadata &gridMetadataOut) const;

  private:
    std::map<std::string, GridMetadata> m_oMapSubdatasetNameToMetadata{};
};

#endif  // HDF5EOSPARSER_H_INCLUDED
