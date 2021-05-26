/******************************************************************************
 *
 * Purpose: Implementation of access to a PCIDSK GCP2 Segment
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
#include "segment/cpcidskgcp2segment.h"

#include "pcidsk_gcp.h"
#include "pcidsk_exception.h"
#include "pcidsk_file.h"
#include "core/pcidsk_utils.h"

#include <cstring>
#include <iostream>
#include <vector>
#include <string>

using namespace PCIDSK;

struct CPCIDSKGCP2Segment::PCIDSKGCP2SegInfo
{
    std::vector<PCIDSK::GCP> gcps;
    unsigned int num_gcps;
    PCIDSKBuffer seg_data;

    std::string map_units;   ///< PCI mapunits string
    std::string proj_parms;  ///< Additional projection parameters
    unsigned int num_proj;
    bool changed;
};

CPCIDSKGCP2Segment::CPCIDSKGCP2Segment(PCIDSKFile *fileIn, int segmentIn, const char *segment_pointer)
    : CPCIDSKSegment(fileIn, segmentIn, segment_pointer), loaded_(false)
{
    pimpl_ = new PCIDSKGCP2SegInfo;
    pimpl_->gcps.clear();
    pimpl_->changed = false;
    try
    {
        Load();
    }
    catch( const PCIDSKException& )
    {
        delete pimpl_;
        pimpl_ = new PCIDSKGCP2SegInfo;
        pimpl_->gcps.clear();
        pimpl_->num_gcps = 0;
        pimpl_->changed = false;
        this->loaded_ = true;
    }
}

CPCIDSKGCP2Segment::~CPCIDSKGCP2Segment()
{
    try
    {
        RebuildSegmentData();
    }
    catch( const PCIDSKException& )
    {
        // TODO ?
    }
    delete pimpl_;
}

void CPCIDSKGCP2Segment::Load()
{
    if (loaded_) {
        return;
    }

    // Read the segment in. The first block has information about
    // the structure of the GCP segment (how many, the projection, etc.)
    pimpl_->seg_data.SetSize(static_cast<int>(data_size) - 1024);
    ReadFromFile(pimpl_->seg_data.buffer, 0, data_size - 1024);

    // check for 'GCP2    ' in the first 8 bytes
    if (!STARTS_WITH(pimpl_->seg_data.buffer, "GCP2    ")) {
        // Assume it is an empty segment, so we can mark loaded_ = true,
        // write it out and return
        pimpl_->changed = true;
        pimpl_->map_units = "LAT/LONG D000";
        pimpl_->proj_parms = "";
        pimpl_->num_gcps = 0;
        loaded_ = true;
        return;
    }

    // Check the number of blocks field's validity
    unsigned int num_blocks = pimpl_->seg_data.GetInt(8, 8);

    if (((data_size - 1024 - 512) / 512) != num_blocks) {
        //ThrowPCIDSKException("Calculated number of blocks (%d) does not match "
        //    "the value encoded in the GCP2 segment (%d).", ((data_size - 1024 - 512)/512),
        //    num_blocks);
        // Something is messed up with how GDB generates these segments... nice.
    }

    pimpl_->num_gcps = pimpl_->seg_data.GetInt(16, 8);

    // Extract the map units string:
    pimpl_->map_units = std::string(pimpl_->seg_data.buffer + 24, 16);

    // Extract the projection parameters string
    pimpl_->proj_parms = std::string(pimpl_->seg_data.buffer + 256, 256);

    // Get the number of alternative projections (should be 0!)
    pimpl_->num_proj = pimpl_->seg_data.GetInt(40, 8);
    if (pimpl_->num_proj != 0) {
        return ThrowPCIDSKException("There are alternative projections contained in this "
            "GCP2 segment. This functionality is not supported in libpcidsk.");
    }

    // Load the GCPs into the vector of PCIDSK::GCPs
    for (unsigned int i = 0; i < pimpl_->num_gcps; i++)
    {
        unsigned int offset = 512 + i * 256;
        bool is_cp = pimpl_->seg_data.buffer[offset] == 'C';
        bool is_active = pimpl_->seg_data.buffer[offset] != 'I';
        double pixel = pimpl_->seg_data.GetDouble(offset + 6, 14);
        double line = pimpl_->seg_data.GetDouble(offset + 20, 14);

        double elev = pimpl_->seg_data.GetDouble(offset + 34, 12);
        double x = pimpl_->seg_data.GetDouble(offset + 48, 22);
        double y = pimpl_->seg_data.GetDouble(offset + 70, 22);

        char cElevDatum = (char)toupper(pimpl_->seg_data.buffer[offset + 47]);
        PCIDSK::GCP::EElevationDatum elev_datum = cElevDatum != 'M' ?
            GCP::EEllipsoidal : GCP::EMeanSeaLevel;

        char elev_unit_c = (char)toupper(pimpl_->seg_data.buffer[offset + 46]);
        PCIDSK::GCP::EElevationUnit elev_unit = elev_unit_c == 'M' ? GCP::EMetres :
            elev_unit_c == 'F' ? GCP::EInternationalFeet :
            elev_unit_c == 'A' ? GCP::EAmericanFeet : GCP::EUnknown;

        double pix_err = pimpl_->seg_data.GetDouble(offset + 92, 10);
        double line_err = pimpl_->seg_data.GetDouble(offset + 102, 10);
        double elev_err = pimpl_->seg_data.GetDouble(offset + 112, 10);

        double x_err = pimpl_->seg_data.GetDouble(offset + 122, 14);
        double y_err = pimpl_->seg_data.GetDouble(offset + 136, 14);

        std::string gcp_id(pimpl_->seg_data.buffer + offset + 192, 64);

        PCIDSK::GCP gcp(x, y, elev,
                        line, pixel, gcp_id, pimpl_->map_units,
                        pimpl_->proj_parms,
                        x_err, y_err, elev_err,
                        line_err, pix_err);
        gcp.SetElevationUnit(elev_unit);
        gcp.SetElevationDatum(elev_datum);
        gcp.SetActive(is_active);
        gcp.SetCheckpoint(is_cp);

        pimpl_->gcps.push_back(gcp);
    }

    loaded_ = true;
}

 // Return all GCPs in the segment
std::vector<PCIDSK::GCP> const& CPCIDSKGCP2Segment::GetGCPs(void) const
{
    return pimpl_->gcps;
}

// Write the given GCPs to the segment. If the segment already
// exists, it will be replaced with this one.
void CPCIDSKGCP2Segment::SetGCPs(std::vector<PCIDSK::GCP> const& gcps)
{
    pimpl_->num_gcps = static_cast<unsigned int>(gcps.size());
    pimpl_->gcps = gcps; // copy them in
    pimpl_->changed = true;

    RebuildSegmentData();
}

// Return the count of GCPs in the segment
unsigned int  CPCIDSKGCP2Segment::GetGCPCount(void) const
{
    return pimpl_->num_gcps;
}

void CPCIDSKGCP2Segment::Synchronize()
{
    if( pimpl_ != nullptr )
    {
        RebuildSegmentData();
    }
}

void CPCIDSKGCP2Segment::RebuildSegmentData(void)
{
    if (pimpl_->changed == false || !this->file->GetUpdatable()) {
        return;
    }
    pimpl_->changed = false;

    // Rebuild the segment data based on the contents of the struct
    int num_blocks = (pimpl_->num_gcps + 1) / 2;

    // This will have to change when we have proper projections support

    if (!pimpl_->gcps.empty())
    {
        pimpl_->gcps[0].GetMapUnits(pimpl_->map_units,
            pimpl_->proj_parms);
    }

    pimpl_->seg_data.SetSize(num_blocks * 512 + 512);

    // Write out the first few fields
    pimpl_->seg_data.Put("GCP2    ", 0, 8);
    pimpl_->seg_data.Put(num_blocks, 8, 8);
    pimpl_->seg_data.Put((int)pimpl_->gcps.size(), 16, 8);
    pimpl_->seg_data.Put(pimpl_->map_units.c_str(), 24, 16);
    pimpl_->seg_data.Put((int)0, 40, 8);
    pimpl_->seg_data.Put(pimpl_->proj_parms.c_str(), 256, 256);

    // Time to write GCPs out:
    std::vector<PCIDSK::GCP>::const_iterator iter =
        pimpl_->gcps.begin();

    int id = 0;
    while (iter != pimpl_->gcps.end()) {
        int offset = 512 + id * 256;

        if ((*iter).IsCheckPoint()) {
            pimpl_->seg_data.Put("C", offset, 1);
        }
        else if ((*iter).IsActive())
        {
            pimpl_->seg_data.Put("G", offset, 1);
        }
        else
        {
            pimpl_->seg_data.Put("I", offset, 1);
        }

        pimpl_->seg_data.Put("0", offset + 1, 5);

        // Start writing out the GCP values
        pimpl_->seg_data.Put((*iter).GetPixel(), offset + 6, 14, "%14.4f");
        pimpl_->seg_data.Put((*iter).GetLine(), offset + 20, 14, "%14.4f");
        pimpl_->seg_data.Put((*iter).GetZ(), offset + 34, 12, "%12.4f");

        GCP::EElevationUnit unit;
        GCP::EElevationDatum datum;
        (*iter).GetElevationInfo(datum, unit);

        char unit_c[2];

        switch (unit)
        {
        case GCP::EMetres:
        case GCP::EUnknown:
            unit_c[0] = 'M';
            break;
        case GCP::EAmericanFeet:
            unit_c[0] = 'A';
            break;
        case GCP::EInternationalFeet:
            unit_c[0] = 'F';
            break;
        }

        char datum_c[2];

        switch(datum)
        {
        case GCP::EEllipsoidal:
            datum_c[0] = 'E';
            break;
        case GCP::EMeanSeaLevel:
            datum_c[0] = 'M';
            break;
        }

        unit_c[1] = '\0';
        datum_c[1] = '\0';

        // Write out elevation information
        pimpl_->seg_data.Put(unit_c, offset + 46, 1);
        pimpl_->seg_data.Put(datum_c, offset + 47, 1);

        pimpl_->seg_data.Put((*iter).GetX(), offset + 48, 22, "%22.14e");
        pimpl_->seg_data.Put((*iter).GetY(), offset + 70, 22, "%22.14e");
        pimpl_->seg_data.Put((*iter).GetPixelErr(), offset + 92, 10, "%10.4f");
        pimpl_->seg_data.Put((*iter).GetLineErr(), offset + 102, 10, "%10.4f");
        pimpl_->seg_data.Put((*iter).GetZErr(), offset + 112, 10, "%10.4f");
        pimpl_->seg_data.Put((*iter).GetXErr(), offset + 122, 14, "%14.4e");
        pimpl_->seg_data.Put((*iter).GetYErr(), offset + 136, 14, "%14.4e");
        pimpl_->seg_data.Put((*iter).GetIDString(), offset + 192, 64, true );

        ++id;
        ++iter;
    }

    WriteToFile(pimpl_->seg_data.buffer, 0, pimpl_->seg_data.buffer_size);

    pimpl_->changed = false;
}

// Clear a GCP Segment
void  CPCIDSKGCP2Segment::ClearGCPs(void)
{
    pimpl_->num_gcps = 0;
    pimpl_->gcps.clear();
    pimpl_->changed = true;

    RebuildSegmentData();
}
