/******************************************************************************
 *
 * Purpose: Declaration of the PCIDSK::GCP class.
 *
 ******************************************************************************
 * Copyright (c) 2009
 * PCI Geomatics, 90 Allstate Parkway, Markham, Ontario, Canada.
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

#ifndef INCLUDE_PCIDSK_SRC_GCP_H
#define INCLUDE_PCIDSK_SRC_GCP_H

#include "pcidsk_config.h"

#include <string>
#include <cstring>

namespace PCIDSK {
    /**
     * \brief PCIDSK Generic GCP Structure
     *
     * The PCIDSK::GCP class encompases all the possible field
     * combinations in the last two revisions of PCI's GCP segment
     * type.
     *
     * If a legacy GCP type is used, the additional information fields
     * will return empty values.
     */
    class PCIDSK_DLL GCP {
    public:
        GCP(double x, double y, double z,
            double line, double pix,
            std::string const& gcp_id,
            std::string const& map_units,
            std::string const& proj_parms = "",
            double xerr = 0.0, double yerr = 0.0, double zerr = 0.0,
            double line_err = 0.0, double pix_err = 0.0)
        {
            ground_point_[0] = x;
            ground_point_[1] = y;
            ground_point_[2] = z;

            ground_error_[0] = xerr;
            ground_error_[1] = yerr;
            ground_error_[2] = zerr;

            raster_point_[1] = line;
            raster_point_[0] = pix;

            raster_error_[1] = line_err;
            raster_error_[0] = pix_err;

            std::memset(gcp_id_, ' ', 64);

            std::strncpy(gcp_id_, gcp_id.c_str(),
                         gcp_id.size() > 64 ? 64 : gcp_id.size());
            gcp_id_[gcp_id.size() > 64 ? 64 : gcp_id.size()] = '\0';

            this->map_units_ = map_units;
            this->proj_parms_ = proj_parms;

            elevation_unit_ = EMetres;
            elevation_datum_ = EEllipsoidal;
            iscp_ = false; // default to GCPs
            isactive_ = true;
        }

        GCP(GCP const& gcp)
        {
            Copy(gcp);
        }

        GCP& operator=(GCP const& gcp)
        {
            Copy(gcp);
            return *this;
        }

        enum EElevationDatum
        {
            EMeanSeaLevel = 0,
            EEllipsoidal
        };

        enum EElevationUnit
        {
            EMetres = 0,
            EAmericanFeet,
            EInternationalFeet,
            EUnknown
        };

        void SetElevationUnit(EElevationUnit unit)
        {
            elevation_unit_ = unit;
        }

        void SetElevationDatum(EElevationDatum datum)
        {
            elevation_datum_ = datum;
        }

        void GetElevationInfo(EElevationDatum& datum, EElevationUnit& unit) const
        {
            unit = elevation_unit_;
            datum = elevation_datum_;
        }

        void SetCheckpoint(bool is_checkpoint)
        {
            if (is_checkpoint)
                isactive_ = true;  // active doesn't apply to check points
            iscp_ = is_checkpoint;
        }

        bool IsCheckPoint(void) const
        {
            return iscp_;
        }

        void SetActive(bool is_active)
        {
            isactive_ = is_active;
            iscp_ = false; // active doesn't apply to check points
        }

        bool IsActive(void) const
        {
            return isactive_;
        }

        double GetX() const { return ground_point_[0]; }
        double GetXErr() const { return ground_error_[0]; }
        double GetY() const { return ground_point_[1]; }
        double GetYErr() const { return ground_error_[1]; }
        double GetZ() const { return ground_point_[2]; }
        double GetZErr() const { return ground_error_[2]; }

        double GetPixel() const { return raster_point_[0]; }
        double GetPixelErr() const { return raster_error_[0]; }
        double GetLine() const { return raster_point_[1]; }
        double GetLineErr() const { return raster_error_[1]; }

        void GetMapUnits(std::string& map_units, std::string& proj_parms) const
        { map_units = map_units_; proj_parms = proj_parms_;}
        void SetMapUnits(std::string const& map_units,
            std::string const& proj_parms) { map_units_ = map_units;
                                             proj_parms_ = proj_parms;}

        const char* GetIDString(void) const { return gcp_id_; }
    private:
        void Copy(GCP const& gcp)
        {
            ground_point_[0] = gcp.ground_point_[0];
            ground_point_[1] = gcp.ground_point_[1];
            ground_point_[2] = gcp.ground_point_[2];

            ground_error_[0] = gcp.ground_error_[0];
            ground_error_[1] = gcp.ground_error_[1];
            ground_error_[2] = gcp.ground_error_[2];

            raster_point_[0] = gcp.raster_point_[0];
            raster_point_[1] = gcp.raster_point_[1];

            raster_error_[0] = gcp.raster_error_[0];
            raster_error_[1] = gcp.raster_error_[1];

            this->map_units_ = gcp.map_units_;
            this->proj_parms_ = gcp.proj_parms_;
            this->iscp_ = gcp.iscp_;
            this->isactive_ = gcp.isactive_;

            std::strncpy(this->gcp_id_, gcp.gcp_id_, 64);

            this->gcp_id_[64] = '\0';

            this->elevation_unit_ = gcp.elevation_unit_;
            this->elevation_datum_ = gcp.elevation_datum_;
        }

        bool iscp_; // true = checkpoint, false = GCP
        bool isactive_; // true = active, false = inactive

        EElevationUnit elevation_unit_;
        EElevationDatum elevation_datum_;

        // Point information
        double ground_point_[3];
        double ground_error_[3]; // variances

        double raster_point_[2];
        double raster_error_[2];

        char gcp_id_[65];

        std::string map_units_; ///< PCI mapunits string
        std::string proj_parms_;  ///< PCI projection parameters string
    };
} // end namespace PCIDSK

#endif // INCLUDE_PCIDSK_SRC_GCP_H

