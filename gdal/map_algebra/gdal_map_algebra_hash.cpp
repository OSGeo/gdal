#include "private.hpp"

gma_object_t *gma_new_object(GDALRasterBand *b, gma_class_t klass) {
    if (klass == gma_integer)
        return new gma_number_p<int>;
    if (klass == gma_pair)
        return new gma_pair_p<gma_object_t*,gma_object_t* >;
    if (klass == gma_cell_callback)
        return new gma_cell_callback_p;
    if (klass == gma_histogram) {
        fprintf(stderr, "Histogram is not used as an argument.");
        return NULL;
    }   
    switch (b->GetRasterDataType()) {
    case GDT_Byte:
        switch (klass) {
        case gma_number:
            return new gma_number_p<uint8_t>;
        case gma_range:
            return new gma_pair_p<gma_number_p<uint8_t>*,gma_number_p<uint8_t>* >
                (new gma_number_p<uint8_t>, new gma_number_p<uint8_t>);
        case gma_bins:
            return new gma_bins_p<uint8_t>;
        case gma_classifier:
            return new gma_classifier_p<uint8_t>(true);
        case gma_cell:
            return new gma_cell_p<uint8_t>(0, 0, 0);
        case gma_logical_operation:
            return new gma_logical_operation_p<uint8_t>;
        case gma_band:
            return new gma_band_p<uint8_t>(b);
        }
        break;
    case GDT_UInt16:
        switch (klass) {
        case gma_number:
            return new gma_number_p<uint16_t>;
        case gma_range:
            return new gma_pair_p<gma_number_p<uint16_t>*,gma_number_p<uint16_t>* >
                (new gma_number_p<uint16_t>, new gma_number_p<uint16_t>);
        case gma_bins:
            return new gma_bins_p<uint16_t>;
        case gma_classifier:
            return new gma_classifier_p<uint16_t>(true);
        case gma_cell:
            return new gma_cell_p<uint16_t>(0, 0, 0);
        case gma_logical_operation:
            return new gma_logical_operation_p<uint16_t>;
        case gma_band:
            return new gma_band_p<uint16_t>(b);
        }
        break;
    case GDT_Int16:
        switch (klass) {
        case gma_number:
            return new gma_number_p<int16_t>;
        case gma_range:
            return new gma_pair_p<gma_number_p<int16_t>*,gma_number_p<int16_t>* >
                (new gma_number_p<int16_t>, new gma_number_p<int16_t>);
        case gma_bins:
            return new gma_bins_p<int16_t>;
        case gma_classifier:
            return new gma_classifier_p<int16_t>(true);
        case gma_cell:
            return new gma_cell_p<int16_t>(0, 0, 0);
        case gma_logical_operation:
            return new gma_logical_operation_p<int16_t>;
        case gma_band:
            return new gma_band_p<int16_t>(b);
        }
        break;
    case GDT_UInt32:
        switch (klass) {
        case gma_number:
            return new gma_number_p<uint32_t>;
        case gma_range:
            return new gma_pair_p<gma_number_p<uint32_t>*,gma_number_p<uint32_t>* >
                (new gma_number_p<uint32_t>, new gma_number_p<uint32_t>);
        case gma_bins:
            return new gma_bins_p<uint32_t>;
        case gma_classifier:
            return new gma_classifier_p<uint32_t>(true);
        case gma_cell:
            return new gma_cell_p<uint32_t>(0, 0, 0);
        case gma_logical_operation:
            return new gma_logical_operation_p<uint32_t>;
        case gma_band:
            return new gma_band_p<uint32_t>(b);
        }
        break;
    case GDT_Int32:
        switch (klass) {
        case gma_number:
            return new gma_number_p<int32_t>;
        case gma_range:
            return new gma_pair_p<gma_number_p<int32_t>*,gma_number_p<int32_t>* >
                (new gma_number_p<int32_t>, new gma_number_p<int32_t>);
        case gma_bins:
            return new gma_bins_p<int32_t>;
        case gma_classifier:
            return new gma_classifier_p<int32_t>(true);
        case gma_cell:
            return new gma_cell_p<int32_t>(0, 0, 0);
        case gma_logical_operation:
            return new gma_logical_operation_p<int32_t>;
        case gma_band:
            return new gma_band_p<int32_t>(b);
        }
        break;
    case GDT_Float32:
        switch (klass) {
        case gma_number:
            return new gma_number_p<float>;
        case gma_range:
            return new gma_pair_p<gma_number_p<float>*,gma_number_p<float>* >
                (new gma_number_p<float>, new gma_number_p<float>);
        case gma_bins:
            return new gma_bins_p<float>;
        case gma_classifier:
            return new gma_classifier_p<float>(false);
        case gma_cell:
            return new gma_cell_p<float>(0, 0, 0);
        case gma_logical_operation:
            return new gma_logical_operation_p<float>;
        case gma_band:
            return new gma_band_p<float>(b);
        }
        break;
    case GDT_Float64:
        switch (klass) {
        case gma_number:
            return new gma_number_p<double>;
        case gma_range:
            return new gma_pair_p<gma_number_p<double>*,gma_number_p<double>* >
                (new gma_number_p<double>, new gma_number_p<double>);
        case gma_bins:
            return new gma_bins_p<double>;
        case gma_classifier:
            return new gma_classifier_p<double>(false);
        case gma_cell:
            return new gma_cell_p<double>(0, 0, 0);
        case gma_logical_operation:
            return new gma_logical_operation_p<double>;
        case gma_band:
            return new gma_band_p<double>(b);
        }
        break;
    default:
        //goto not_implemented_for_these_datatypes;
        break;
    }
    return NULL;
}

template <> GDALDataType gma_number_p<uint8_t>::get_datatype() { return GDT_Byte; }

template <> bool gma_number_p<uint8_t>::is_integer() { return true; }
template <> bool gma_number_p<uint8_t>::is_float() { return false; }
template <> int gma_number_p<uint8_t>::inf_int(int sign) { return sign < 0 ? 0 : 255; };
template <> double gma_number_p<uint8_t>::inf_double(int sign) { return sign < 0 ? 0 : 255; };

template <> bool gma_number_p<uint16_t>::is_integer() { return true; }
template <> bool gma_number_p<uint16_t>::is_float() { return false; }
template <> int gma_number_p<uint16_t>::inf_int(int sign) { return sign < 0 ? 0 : 65535; };
template <> double gma_number_p<uint16_t>::inf_double(int sign) { return sign < 0 ? 0 : 65535; };

template <> bool gma_number_p<int16_t>::is_integer() { return true; }
template <> bool gma_number_p<int16_t>::is_float() { return false; }
template <> int gma_number_p<int16_t>::inf_int(int sign) { return sign < 0 ? -32768 : 32768; };
template <> double gma_number_p<int16_t>::inf_double(int sign) { return sign < 0 ? -32768 : 32768; };

template <> bool gma_number_p<uint32_t>::is_integer() { return true; }
template <> bool gma_number_p<uint32_t>::is_float() { return false; }
template <> int gma_number_p<uint32_t>::inf_int(int sign) { return sign < 0 ? 0 : 4294967295; };
template <> double gma_number_p<uint32_t>::inf_double(int sign) { return sign < 0 ? 0 : 4294967295; };

template <> bool gma_number_p<int32_t>::is_integer() { return true; }
template <> bool gma_number_p<int32_t>::is_float() { return false; }
template <> int gma_number_p<int32_t>::inf_int(int sign) { return sign < 0 ? -2147483648 : 2147483648; };
template <> double gma_number_p<int32_t>::inf_double(int sign) { return sign < 0 ? -2147483648 : 2147483648; };

template <> bool gma_number_p<float>::is_integer() { return false; }
template <> bool gma_number_p<float>::is_float() { return true; }
template <> int gma_number_p<float>::inf_int(int sign) { return sign < 0 ? -INFINITY : INFINITY; };
template <> double gma_number_p<float>::inf_double(int sign) { return sign < 0 ? -INFINITY : INFINITY; };

template <> bool gma_number_p<double>::is_integer() { return false; }
template <> bool gma_number_p<double>::is_float() { return true; }
template <> int gma_number_p<double>::inf_int(int sign) { return sign < 0 ? -INFINITY : INFINITY; };
template <> double gma_number_p<double>::inf_double(int sign) { return sign < 0 ? -INFINITY : INFINITY; };


template <> const char *gma_band_p<uint8_t>::space() { return "   "; }
template <> const char *gma_band_p<uint16_t>::space() { return "   "; }
template <> const char *gma_band_p<int16_t>::space() { return "   "; }
template <> const char *gma_band_p<uint32_t>::space() { return "   "; }
template <> const char *gma_band_p<int32_t>::space() { return "   "; }
template <> const char *gma_band_p<float>::space() { return "    "; }
template <> const char *gma_band_p<double>::space() { return "     "; }

template <> const char *gma_band_p<uint8_t>::format() { return "%03i "; }
template <> const char *gma_band_p<uint16_t>::format() { return "%04i "; }
template <> const char *gma_band_p<int16_t>::format() { return "%04i "; }
template <> const char *gma_band_p<uint32_t>::format() { return "%04i "; }
template <> const char *gma_band_p<int32_t>::format() { return "%04i "; }
template <> const char *gma_band_p<float>::format() { return "%04.1f "; }
template <> const char *gma_band_p<double>::format() { return "%04.2f "; }

template <> int gma_band_p<uint8_t>::m_log10(gma_block<uint8_t>*, gma_object_t**, gma_object_t*) { return 0; }
template <> int gma_band_p<uint16_t>::m_log10(gma_block<uint16_t>*, gma_object_t**, gma_object_t*) { return 0; }
template <> int gma_band_p<int16_t>::m_log10(gma_block<int16_t>*, gma_object_t**, gma_object_t*) { return 0; }
template <> int gma_band_p<uint32_t>::m_log10(gma_block<uint32_t>*, gma_object_t**, gma_object_t*) { return 0; }
template <> int gma_band_p<int32_t>::m_log10(gma_block<int32_t>*, gma_object_t**, gma_object_t*) { return 0; }


template <> int gma_band_p<float>::_modulus(gma_block<float>*, gma_object_t**, gma_object_t*) { return 0; }
template <> int gma_band_p<double>::_modulus(gma_block<double>*, gma_object_t**, gma_object_t*) { return 0; }

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
        }
    }
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
