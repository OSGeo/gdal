#include "gdal_map_algebra.hpp"
#include "gma_classes.hpp"
#include "gma_band.hpp"
#include "gma_two_bands.hpp"

gma_band_t *gma_new_band(GDALRasterBand *b) {
    switch (b->GetRasterDataType()) {
    case GDT_Byte:
        return new gma_band_p<uint8_t>(b);
    case GDT_UInt16:
        return new gma_band_p<uint16_t>(b);
    case GDT_Int16:
        return new gma_band_p<int16_t>(b);
    case GDT_UInt32:
        return new gma_band_p<uint32_t>(b);
    case GDT_Int32:
        return new gma_band_p<int32_t>(b);
    case GDT_Float32:
        return new gma_band_p<float>(b);
    case GDT_Float64:
        return new gma_band_p<double>(b);
    default:
        //goto not_implemented_for_these_datatypes;
        break;
    }
    return NULL;
}

gma_band_t *gma_new_band(const char *name) {
    return gma_new_band(((GDALDataset*)GDALOpen(name, GA_ReadOnly))->GetRasterBand(1));
}

template <> GDALDataType gma_number_p<uint8_t>::datatype_p() { return GDT_Byte; }
template <> GDALDataType gma_number_p<uint16_t>::datatype_p() { return GDT_UInt16; }
template <> GDALDataType gma_number_p<int16_t>::datatype_p() { return GDT_Int16; }
template <> GDALDataType gma_number_p<uint32_t>::datatype_p() { return GDT_UInt32; }
template <> GDALDataType gma_number_p<int32_t>::datatype_p() { return GDT_Int32; }
template <> GDALDataType gma_number_p<float>::datatype_p() { return GDT_Float32; }
template <> GDALDataType gma_number_p<double>::datatype_p() { return GDT_Float64; }

template <> bool gma_number_p<uint8_t>::is_float() { return false; }
template <> bool gma_number_p<uint16_t>::is_float() { return false; }
template <> bool gma_number_p<int16_t>::is_float() { return false; }
template <> bool gma_number_p<uint32_t>::is_float() { return false; }
template <> bool gma_number_p<int32_t>::is_float() { return false; }
template <> bool gma_number_p<float>::is_float() { return true; }
template <> bool gma_number_p<double>::is_float() { return true; }

template <> const char *gma_number_p<uint8_t>::format() { return "%u"; }
template <> const char *gma_number_p<uint16_t>::format() { return "%u"; }
template <> const char *gma_number_p<int16_t>::format() { return "%i"; }
template <> const char *gma_number_p<uint32_t>::format() { return "%u"; }
template <> const char *gma_number_p<int32_t>::format() { return "%i"; }
template <> const char *gma_number_p<float>::format() { return "%.2f"; }
template <> const char *gma_number_p<double>::format() { return "%.3f"; }

template <> int gma_band_p<uint8_t>::m_log10(gma_block<uint8_t>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_band_p<uint16_t>::m_log10(gma_block<uint16_t>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_band_p<int16_t>::m_log10(gma_block<int16_t>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_band_p<uint32_t>::m_log10(gma_block<uint32_t>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_band_p<int32_t>::m_log10(gma_block<int32_t>*, gma_object_t**, gma_object_t*, int) { return 0; }

template <> int gma_band_p<float>::m_modulus(gma_block<float>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_band_p<double>::m_modulus(gma_block<double>*, gma_object_t**, gma_object_t*, int) { return 0; }

gma_two_bands_t *gma_new_two_bands(GDALDataType type1, GDALDataType type2) {
    switch (type1) {
    case GDT_Byte: {
        switch (type2) {
        case GDT_Byte:
            return new gma_two_bands_p<uint8_t,uint8_t>;
        case GDT_UInt16:
            return new gma_two_bands_p<uint8_t,uint16_t>;
        case GDT_Int16:
            return new gma_two_bands_p<uint8_t,int16_t>;
        case GDT_UInt32:
            return new gma_two_bands_p<uint8_t,uint32_t>;
        case GDT_Int32:
            return new gma_two_bands_p<uint8_t,int32_t>;
        case GDT_Float32:
            return new gma_two_bands_p<uint8_t,float>;
        case GDT_Float64:
            return new gma_two_bands_p<uint8_t,double>;
        default:
            // fixme: call error
            return NULL;
        }
    }
    case GDT_UInt16: {
        switch (type2) {
        case GDT_Byte:
            return new gma_two_bands_p<uint16_t,uint8_t>;
        case GDT_UInt16:
            return new gma_two_bands_p<uint16_t,uint16_t>;
        case GDT_Int16:
            return new gma_two_bands_p<uint16_t,int16_t>;
        case GDT_UInt32:
            return new gma_two_bands_p<uint16_t,uint32_t>;
        case GDT_Int32:
            return new gma_two_bands_p<uint16_t,int32_t>;
        case GDT_Float32:
            return new gma_two_bands_p<uint16_t,float>;
        case GDT_Float64:
            return new gma_two_bands_p<uint16_t,double>;
        default:
            // fixme: call error
            return NULL;
        }
    }
    case GDT_Int16: {
        switch (type2) {
        case GDT_Byte:
            return new gma_two_bands_p<int16_t,uint8_t>;
        case GDT_UInt16:
            return new gma_two_bands_p<int16_t,uint16_t>;
        case GDT_Int16:
            return new gma_two_bands_p<int16_t,int16_t>;
        case GDT_UInt32:
            return new gma_two_bands_p<int16_t,uint32_t>;
        case GDT_Int32:
            return new gma_two_bands_p<int16_t,int32_t>;
        case GDT_Float32:
            return new gma_two_bands_p<int16_t,float>;
        case GDT_Float64:
            return new gma_two_bands_p<int16_t,double>;
        default:
            // fixme: call error
            return NULL;
        }
    }
    case GDT_UInt32: {
        switch (type2) {
        case GDT_Byte:
            return new gma_two_bands_p<uint32_t,uint8_t>;
        case GDT_UInt16:
            return new gma_two_bands_p<uint32_t,uint16_t>;
        case GDT_Int16:
            return new gma_two_bands_p<uint32_t,int16_t>;
        case GDT_UInt32:
            return new gma_two_bands_p<uint32_t,uint32_t>;
        case GDT_Int32:
            return new gma_two_bands_p<uint32_t,int32_t>;
        case GDT_Float32:
            return new gma_two_bands_p<uint32_t,float>;
        case GDT_Float64:
            return new gma_two_bands_p<uint32_t,double>;
        default:
            // fixme: call error
            return NULL;
        }
    }
    case GDT_Int32: {
        switch (type2) {
        case GDT_Byte:
            return new gma_two_bands_p<int32_t,uint8_t>;
        case GDT_UInt16:
            return new gma_two_bands_p<int32_t,uint16_t>;
        case GDT_Int16:
            return new gma_two_bands_p<int32_t,int16_t>;
        case GDT_UInt32:
            return new gma_two_bands_p<int32_t,uint32_t>;
        case GDT_Int32:
            return new gma_two_bands_p<int32_t,int32_t>;
        case GDT_Float32:
            return new gma_two_bands_p<int32_t,float>;
        case GDT_Float64:
            return new gma_two_bands_p<int32_t,double>;
        default:
            // fixme: call error
            return NULL;
        }
    }
    case GDT_Float32: {
        switch (type2) {
        case GDT_Byte:
            return new gma_two_bands_p<float,uint8_t>;
        case GDT_UInt16:
            return new gma_two_bands_p<float,uint16_t>;
        case GDT_Int16:
            return new gma_two_bands_p<float,int16_t>;
        case GDT_UInt32:
            return new gma_two_bands_p<float,uint32_t>;
        case GDT_Int32:
            return new gma_two_bands_p<float,int32_t>;
        case GDT_Float32:
            return new gma_two_bands_p<float,float>;
        case GDT_Float64:
            return new gma_two_bands_p<float,double>;
        default:
            // fixme: call error
            return NULL;
        }
    }
    case GDT_Float64: {
        switch (type2) {
        case GDT_Byte:
            return new gma_two_bands_p<double,uint8_t>;
        case GDT_UInt16:
            return new gma_two_bands_p<double,uint16_t>;
        case GDT_Int16:
            return new gma_two_bands_p<double,int16_t>;
        case GDT_UInt32:
            return new gma_two_bands_p<double,uint32_t>;
        case GDT_Int32:
            return new gma_two_bands_p<double,int32_t>;
        case GDT_Float32:
            return new gma_two_bands_p<double,float>;
        case GDT_Float64:
            return new gma_two_bands_p<double,double>;
        default:
            // fixme: call error
            return NULL;
        }
    }
    default:
        // fixme: call error
        return NULL;
    }
}

template <> int gma_two_bands_p<uint8_t,float>::m_modulus(gma_block<uint8_t>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_two_bands_p<uint8_t,double>::m_modulus(gma_block<uint8_t>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_two_bands_p<uint16_t,float>::m_modulus(gma_block<uint16_t>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_two_bands_p<uint16_t,double>::m_modulus(gma_block<uint16_t>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_two_bands_p<int16_t,float>::m_modulus(gma_block<int16_t>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_two_bands_p<int16_t,double>::m_modulus(gma_block<int16_t>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_two_bands_p<uint32_t,float>::m_modulus(gma_block<uint32_t>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_two_bands_p<uint32_t,double>::m_modulus(gma_block<uint32_t>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_two_bands_p<int32_t,float>::m_modulus(gma_block<int32_t>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_two_bands_p<int32_t,double>::m_modulus(gma_block<int32_t>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_two_bands_p<float,float>::m_modulus(gma_block<float>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_two_bands_p<float,double>::m_modulus(gma_block<float>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_two_bands_p<float,uint8_t>::m_modulus(gma_block<float>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_two_bands_p<float,uint16_t>::m_modulus(gma_block<float>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_two_bands_p<float,int16_t>::m_modulus(gma_block<float>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_two_bands_p<float,uint32_t>::m_modulus(gma_block<float>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_two_bands_p<float,int32_t>::m_modulus(gma_block<float>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_two_bands_p<double,float>::m_modulus(gma_block<double>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_two_bands_p<double,double>::m_modulus(gma_block<double>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_two_bands_p<double,uint8_t>::m_modulus(gma_block<double>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_two_bands_p<double,uint16_t>::m_modulus(gma_block<double>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_two_bands_p<double,int16_t>::m_modulus(gma_block<double>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_two_bands_p<double,uint32_t>::m_modulus(gma_block<double>*, gma_object_t**, gma_object_t*, int) { return 0; }
template <> int gma_two_bands_p<double,int32_t>::m_modulus(gma_block<double>*, gma_object_t**, gma_object_t*, int) { return 0; }
