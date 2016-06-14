/******************************************************************************
 *
 * Purpose:  Declaration of the APMODEL segment.
 * 
 ******************************************************************************
 * Copyright (c) 2010
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
#ifndef INCLUDE_SEGMENT_CPCIDSKAPMODEL_H
#define INCLUDE_SEGMENT_CPCIDSKAPMODEL_H

#include "pcidsk_airphoto.h"
#include "segment/cpcidsksegment.h"

#include <string>
#include <vector>

namespace PCIDSK {

    class CPCIDSKAPModelSegment : virtual public CPCIDSKSegment,
                                  public PCIDSKAPModelSegment
    {
    public:
        CPCIDSKAPModelSegment(PCIDSKFile *file, int segment,
            const char *segment_pointer);
            
        ~CPCIDSKAPModelSegment();

        unsigned int GetWidth(void) const;
        unsigned int GetHeight(void) const;
        unsigned int GetDownsampleFactor(void) const;

        // Interior Orientation Parameters
        PCIDSKAPModelIOParams const& GetInteriorOrientationParams(void) const;
        
        // Exterior Orientation Parameters
        PCIDSKAPModelEOParams const& GetExteriorOrientationParams(void) const;

        // ProjInfo
        PCIDSKAPModelMiscParams const& GetAdditionalParams(void) const;
        
        std::string GetMapUnitsString(void) const;
        std::string GetUTMUnitsString(void) const;
        std::vector<double> const& GetProjParams(void) const;

    private:
        void UpdateFromDisk();
        
        PCIDSKBuffer buf;
        std::string map_units_, utm_units_;
        std::vector<double> proj_parms_;
        PCIDSKAPModelIOParams* io_params_;
        PCIDSKAPModelEOParams* eo_params_;
        PCIDSKAPModelMiscParams* misc_params_;
        unsigned int width_, height_, downsample_;
        bool filled_;
    };

} // end namespace PCIDSK

#endif // INCLUDE_SEGMENT_CPCIDSKAPMODEL_H

