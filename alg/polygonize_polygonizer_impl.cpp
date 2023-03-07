#include "polygonize_polygonizer.cpp"

namespace gdal
{
namespace polygonizer
{

template class Polygonizer<GInt32, std::int64_t>;

template class Polygonizer<GInt32, float>;

template class OGRPolygonWriter<std::int64_t>;

template class OGRPolygonWriter<float>;

}  // namespace polygonizer
}  // namespace gdal
