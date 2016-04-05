#include "gdal_map_algebra_private.h"

template <> GDALDataType gma_number_p<uint8_t>::get_datatype() { return GDT_Byte; }
template <> bool gma_number_p<uint8_t>::is_integer() { return true; }
template <> bool gma_number_p<uint8_t>::is_float() { return false; }

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
