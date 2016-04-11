#include "gdal_map_algebra_private.h"

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
            break;
        case gma_logical_operation:
            return new gma_logical_operation_p<uint8_t>;
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
            break;
        case gma_logical_operation:
            return new gma_logical_operation_p<uint16_t>;
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
            break;
        case gma_logical_operation:
            return new gma_logical_operation_p<int16_t>;
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
            break;
        case gma_logical_operation:
            return new gma_logical_operation_p<uint32_t>;
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
            break;
        case gma_logical_operation:
            return new gma_logical_operation_p<int32_t>;
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
            break;
        case gma_logical_operation:
            return new gma_logical_operation_p<float>;
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
            break;
        case gma_logical_operation:
            return new gma_logical_operation_p<double>;
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

template <typename type>
int my_xy_snprintf(char *s, type x, type y) {
    return snprintf(s, 20, "%i,%i", x, y);
}

template <>
int my_xy_snprintf<float>(char *s, float x, float y) {
    return snprintf(s, 40, "%f,%f", x, y);
}

template <>
int my_xy_snprintf<double>(char *s, double x, double y) {
    return snprintf(s, 40, "%f,%f", x, y);
}
