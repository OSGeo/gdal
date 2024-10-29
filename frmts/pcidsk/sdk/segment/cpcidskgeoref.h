/******************************************************************************
 *
 * Purpose:  Declaration of the CPCIDSKGeoref class.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef INCLUDE_SEGMENT_PCIDSKGEOREF_H
#define INCLUDE_SEGMENT_PCIDSKGEOREF_H

#include "pcidsk_config.h"
#include "pcidsk_types.h"
#include "pcidsk_georef.h"
#include "pcidsk_buffer.h"
#include "segment/cpcidsksegment.h"

#include <string>

namespace PCIDSK
{
    class PCIDSKFile;

    /************************************************************************/
    /*                            CPCIDSKGeoref                             */
    /************************************************************************/

    class CPCIDSKGeoref : virtual public CPCIDSKSegment,
                          public PCIDSKGeoref
    {
    public:
        CPCIDSKGeoref( PCIDSKFile *file, int segment,const char *segment_pointer );

        virtual     ~CPCIDSKGeoref();

        // PCIDSKSegment

        void        Initialize() override;

        // PCIDSKGeoref

        void        GetTransform( double &a1, double &a2, double &xrot,
                                  double &b1, double &yrot, double &b3 ) override;
        std::string GetGeosys() override;

        std::vector<double> GetParameters() override;

        void        WriteSimple( std::string const& geosys,
                                 double a1, double a2, double xrot,
                                 double b1, double yrot, double b3 ) override;
        void        WriteParameters( std::vector<double> const& parameters ) override;

        // special interface just for testing.
        std::vector<double> GetUSGSParameters();

     private:
        bool         loaded;

        std::string  geosys;
        double       a1, a2, xrot, b1, yrot, b3;

        void         Load();
        void         PrepareGCTPFields();
        std::string  ReformatGeosys( std::string const& geosys );

        PCIDSKBuffer seg_data;
    };
} // end namespace PCIDSK

#endif // INCLUDE_SEGMENT_PCIDSKGEOREF_H
