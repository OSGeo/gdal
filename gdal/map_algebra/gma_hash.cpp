#include "gdal_map_algebra_private.h"

gma_object_t *gma_new_object(GDALRasterBand *b, gma_class_t klass) {
    if (klass == gma_integer)
        return new gma_number_p<int>;
    if (klass == gma_pair)
        return new gma_pair_p<gma_object_t*,gma_object_t* >;
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
        case gma_histogram:
            break;
        case gma_reclassifier:
            break;
        case gma_cell:
            break;
        case gma_logical_operation:
            break;
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
        case gma_histogram:
            break;
        case gma_reclassifier:
            break;
        case gma_cell:
            break;
        case gma_logical_operation:
            break;
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
        case gma_histogram:
        case gma_reclassifier:
        case gma_cell:
        case gma_logical_operation:
            break;
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
        case gma_histogram:
        case gma_reclassifier:
        case gma_cell:
        case gma_logical_operation:
            break;
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
        case gma_histogram:
        case gma_reclassifier:
        case gma_cell:
        case gma_logical_operation:
            break;
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
        case gma_histogram:
        case gma_reclassifier:
        case gma_cell:
        case gma_logical_operation:
            break;
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
        case gma_histogram:
        case gma_reclassifier:
        case gma_cell:
        case gma_logical_operation:
            break;
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

template <> bool gma_number_p<uint16_t>::is_integer() { return true; }
template <> bool gma_number_p<uint16_t>::is_float() { return false; }
template <> int gma_number_p<uint16_t>::inf_int(int sign) { return sign < 0 ? 0 : 255; };

template <> bool gma_number_p<int16_t>::is_integer() { return true; }
template <> bool gma_number_p<int16_t>::is_float() { return false; }
template <> int gma_number_p<int16_t>::inf_int(int sign) { return sign < 0 ? 0 : 255; };

template <> bool gma_number_p<uint32_t>::is_integer() { return true; }
template <> bool gma_number_p<uint32_t>::is_float() { return false; }
template <> int gma_number_p<uint32_t>::inf_int(int sign) { return sign < 0 ? 0 : 255; };

template <> bool gma_number_p<int32_t>::is_integer() { return true; }
template <> bool gma_number_p<int32_t>::is_float() { return false; }
template <> int gma_number_p<int32_t>::inf_int(int sign) { return sign < 0 ? 0 : 255; };

template <> bool gma_number_p<float>::is_integer() { return false; }
template <> bool gma_number_p<float>::is_float() { return true; }
template <> int gma_number_p<float>::inf_int(int sign) { return sign < 0 ? 0 : 255; };

template <> bool gma_number_p<double>::is_integer() { return false; }
template <> bool gma_number_p<double>::is_float() { return true; }
template <> int gma_number_p<double>::inf_int(int sign) { return sign < 0 ? 0 : 255; };

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

int compare_int32s(const void *a, const void *b)
{
  const int32_t *da = (const int32_t *) a;
  const int32_t *db = (const int32_t *) b;
  return (*da > *db) - (*da < *db);
}
