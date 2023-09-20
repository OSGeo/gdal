#include "polygonize_polygonizer.cpp"

namespace gdal
{
namespace polygonizer
{

template class Polygonizer<int32_t, std::int64_t>;

template class Polygonizer<int32_t, float>;

template class OGRPolygonWriter<std::int64_t>;

template class OGRPolygonWriter<float>;

}  // namespace polygonizer
}  // namespace gdal
