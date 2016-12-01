#include <cstdio>
#include "ogr_xplane_geo_utils.h"

int main( int /* argc */ , char* /* argv */ [])
{
  const double latA = 49;
  const double lonA = 2;
  const double latB = 49.1;
  const double lonB = 2.1;

  double heading = OGRXPlane_Track(latA, lonA, latB, lonB);
  double distance = OGRXPlane_Distance(latA, lonA, latB, lonB);
  double latC = 0.0;
  double lonC = 0.0;
  OGRXPlane_ExtendPosition(latA, lonA, distance, heading, &latC, &lonC);
  printf("heading=%f, distance=%f\n", heading, distance);
  printf("%.15f=%.15f, %.15f=%.15f\n", latB, latC, lonB, lonC);

  heading = OGRXPlane_Track(latB, lonB, latA, lonA);
  distance = OGRXPlane_Distance(latB, lonB, latA, lonA);
  OGRXPlane_ExtendPosition(latB, lonB, distance, heading, &latC, &lonC);
  printf("heading=%f, distance=%f\n", heading, distance);
  printf("%.15f=%.15f, %.15f=%.15f\n", latA, latC, lonA, lonC);

  return 0;
}
