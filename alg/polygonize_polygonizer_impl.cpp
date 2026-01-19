#include "polygonize_polygonizer.cpp"

namespace gdal
{
namespace polygonizer
{

template class Polygonizer<GInt32, std::int64_t>;

template class Polygonizer<GInt32, float>;

template class Polygonizer<GInt32, double>;

template class OGRPolygonWriter<std::int64_t>;

template class OGRPolygonWriter<float>;

template class OGRPolygonWriter<double>;

}  // namespace polygonizer
}  // namespace gdal
