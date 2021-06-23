/*
 */

// #ifdef FRMT_vrc

#include "VRC.h"

extern short VRGetShort( void* base, int byteOffset )
{
    unsigned char * buf = static_cast<unsigned char*>(base)+byteOffset;
    short vv = buf[0];
    vv |= (buf[1] << 8);

    return(vv);
}

signed int VRGetInt( void* base, unsigned int byteOffset )
{
    unsigned char* buf = static_cast<unsigned char*>(base)+byteOffset;
    signed int vv = buf[0];
    vv |= (buf[1] << 8U);
    vv |= (buf[2] << 16U);
    vv |= (buf[3] << 24U);

    return(vv);
}
unsigned int VRGetUInt( void* base, unsigned int byteOffset )
{
    unsigned char* buf = static_cast<unsigned char*>(base)+byteOffset;
    int vv = buf[0];
    vv |= (buf[1] << 8U);
    vv |= (buf[2] << 16U);
    vv |= (buf[3] << 24U);

    return(static_cast<unsigned int>(vv));
}


///////////////////////////////////////////////////////////////////


int VRReadChar(VSILFILE *fp)
{
    unsigned char buf[4]="";
    // size_t ret =
    VSIFReadL(buf, 1, 1, fp);
    unsigned char vv = buf[0];
    // if (ret<1) return(EOF);
    return(vv);
}

int VRReadShort(VSILFILE *fp)
{
    unsigned char buf[4]="";
    // size_t ret =
    VSIFReadL(buf, 1, 2, fp);
    short vv = buf[0];
    vv |= (buf[1] << 8);
    // if (ret<2) return(EOF);
    return(vv);
}

int VRReadInt(VSILFILE *fp)
{
    unsigned char buf[4]="";
    // size_t ret =
    VSIFReadL(buf, 1, 4, fp);
    signed int vv = buf[0];
    vv |= (buf[1] << 8);
    vv |= (buf[2] << 16);
    vv |= (buf[3] << 24);
    // if (ret<4) return(EOF);
    return(vv);
}
int VRReadInt(VSILFILE *fp, unsigned int byteOffset )
{
    if ( VSIFSeekL( fp, byteOffset, SEEK_SET ) ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "VRReadInt cannot seek to VRC byteOffset %d=x%08x",
                  byteOffset, byteOffset);
        return CE_Failure; // dangerous ?
    }
    return VRReadInt(fp);
}

unsigned int VRReadUInt(VSILFILE *fp)
{
    // unsigned int vv;
    unsigned char buf[4]="";
    // size_t ret =
    VSIFReadL(buf, 1, 4, fp);
    // if (ret<4) return(EOF);
    return(VRGetUInt(buf,0));
}
unsigned int VRReadUInt(VSILFILE *fp, unsigned int byteOffset )
{
    if ( VSIFSeekL( fp, byteOffset, SEEK_SET ) ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "VRReadInt cannot seek to VRC byteOffset %d=x%08x",
                  byteOffset, byteOffset);
        return CE_Failure; // dangerous ?
    }
    return VRReadUInt(fp);
}


/*
 *               CRSfromCountry
 *
 * GDAL v1:
 * const char* CRSfromCountry(int nCountry)
 * GDAL v2 and v3:
 * OGRSpatialReference* CRSfromCountry(int nCountry)
 *
 */

// Some CRS use the "old" axis convention
#if GDAL_VERSION_MAJOR >= 3
#define VRC_SWAP_AXES        poSRS->SetAxisMappingStrategy( OAMS_TRADITIONAL_GIS_ORDER )
#else
#define VRC_SWAP_AXES
#endif

#define VRC_EPSG(A) errImport=poSRS->importFromEPSGA(A)

extern OGRSpatialReference* CRSfromCountry(int nCountry)
{
    // OGRSpatialReference* poSRS=nullptr;
    auto* poSRS=new OGRSpatialReference();
    if (poSRS==nullptr) {
        CPLDebug("Viewranger",
                 "CRSfromCountry(%d) failed to create a spatial reference",
                 nCountry);
        return nullptr;
    }

    OGRErr errImport=OGRERR_NONE;

    switch (nCountry) {
        // case 0: break; // Online maps
    case  1: VRC_EPSG(27700);   break; // UK Ordnace Survey
    case  2: VRC_EPSG(29901);   break; // Ireland
    case  5: VRC_EPSG(2393); VRC_SWAP_AXES;   break; // Finland
    case  8: VRC_EPSG(31370);
        // Other possibilities for Belgium include
        //   EPSG:21500, EPSG:31300, EPSG:31370, EPSG:6190 and EPSG:3447.
        // BelgiumOverview.VRC is not EPSG:3812 or EPSG:4171
        // Some Belgium VRH (height) files are case 17:
        break;
    case  9: VRC_EPSG(21781); VRC_SWAP_AXES;  break; // Switzerland
    case 12: VRC_EPSG(28992);   break; // Nederlands
    case 13: VRC_EPSG(3907);    break; // tbc, Slovenia
    case 14: VRC_EPSG(3006); VRC_SWAP_AXES; break; // Sweden SWEREF99
    case 15: VRC_EPSG(25833);   break; // Norway
    case 16: VRC_EPSG(32632);   break; // Italy
    case 17: VRC_EPSG(4267); VRC_SWAP_AXES;
        // USA, Discovery(Spain/Canaries) and Belgium VRH (height) files
        break;
    case 18: VRC_EPSG(2193); VRC_SWAP_AXES; break; // New Zealand
    case 19: VRC_EPSG(2154);    break; // France
    case 20: VRC_EPSG(2100);    break; // Greece
    case 21: VRC_EPSG(3042); VRC_SWAP_AXES;
        // Spain (Including Discovery Walking Guides)
        break;
    case 132: VRC_EPSG(25832);  break; // Austria/Germany/Denmark 
    case 133: VRC_EPSG(25833);
        // Czech Republic / Slovakia, EPSG:25833 tbc may be 32633 or 3045
        break;
    case 155: VRC_EPSG(28355); // not VRC_EPSG(4283);
        // Australia
        // Note that in VRCDataset::GetGeoTransform()
        // we shift 10million metres north
        // (which undoes the false_northing).
        break;
    default:
        CPLDebug("Viewranger",
                 "CRSfromCountry(country %d unknown) assuming WGS 84",
                 nCountry);
        VRC_EPSG(4326);   break;
    }

    if (errImport != OGRERR_NONE) {
        CPLDebug("Viewranger",
                 "failed to import EPSG for CRSfromCountry(%d) error %d",
                 nCountry, errImport);
        delete poSRS;
        poSRS = nullptr;
    }
    return poSRS;
} // CRSfromCountry()

extern const char* CharsetFromCountry(int nCountry)
{
    // CPLDebug("Viewranger", "CharsetFromCountry(%d)", nCountry);
    switch (nCountry) {
        // case 0: return ""; // Online maps
    case  1: return "LATIN9"; // UK Ordnance Survey
    case  2: return "LATIN9"; // Ireland
    case  5: return "LATIN9"; // Finland
    case  8: return "LATIN9"; // Belgium. Some Belgium .VRH files are case 17:
    case  9: return "LATIN9"; // Switzerland
    case 12: return "LATIN9"; // Nederlands
    case 13: return "LATIN9"; // Slovenia
    case 14: return "LATIN9"; // Sweden SWEREF99
    case 15: return "LATIN9"; // Norway
    case 16: return "LATIN9"; // Italy
    case 17: return "LATIN9"; // USA, Discovery(Spain/Canaries)
        // (Belgium .VRH files are also 17, but .VRH files have no strings).
    case 18: return "LATIN9"; // New Zealand
    case 19: return "LATIN9"; // France
    case 20: return "LATIN9"; // Greece
        // case 21: return "UTF-8"; // Spain, but not Discovery Walking Guides ?
    case 132: return "LATIN9"; // Austria/Germany/Denmark 
    case 133: return "LATIN9"; // Czech Republic / Slovakia, EPSG:25833 tbc may be 32633 or 3045
    case 155: return "LATIN9"; // Australia

    default:
        return "UTF-8";
    }
}
// CharsetFromCountry(int nCountry)

// #endif // ifdef FRMT_vrc
