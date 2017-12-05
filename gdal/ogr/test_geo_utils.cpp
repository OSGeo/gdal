#include <cstdio>
#include "ogr_geo_utils.h"

int main( int /* argc */ , char* /* argv */ [])
{
  const double latA = 49;
  const double lonA = 2;
  const double latB = 49.00001;
  const double lonB = 2.00001;

  double heading = OGR_GreatCircle_InitialHeading(latA, lonA, latB, lonB);
  double distance = OGR_GreatCircle_Distance(latA, lonA, latB, lonB);
  double latC = 0.0;
  double lonC = 0.0;
  OGR_GreatCircle_ExtendPosition(latA, lonA, distance, heading, &latC, &lonC);
  printf("heading=%f, distance=%f\n", heading, distance);
  printf("%.15f=%.15f, %.15f=%.15f\n", latB, latC, lonB, lonC);

  heading = OGR_GreatCircle_InitialHeading(latB, lonB, latA, lonA);
  distance = OGR_GreatCircle_Distance(latB, lonB, latA, lonA);
  OGR_GreatCircle_ExtendPosition(latB, lonB, distance, heading, &latC, &lonC);
  printf("heading=%f, distance=%f\n", heading, distance);
  printf("%.15f=%.15f, %.15f=%.15f\n", latA, latC, lonA, lonC);

  OGR_GreatCircle_ExtendPosition(0, 100, 100000, 0, &latC, &lonC);
  printf("lat=%.15f, lon=%.15f\n", latC, lonC);

  OGR_GreatCircle_ExtendPosition(0, 100, 100000, 90, &latC, &lonC);
  printf("lat=%.15f, lon=%.15f\n", latC, lonC);

  OGR_GreatCircle_ExtendPosition(0, 100, 100000, 180, &latC, &lonC);
  printf("lat=%.15f, lon=%.15f\n", latC, lonC);

  OGR_GreatCircle_ExtendPosition(0, 100, 100000, 270, &latC, &lonC);
  printf("lat=%.15f, lon=%.15f\n", latC, lonC);

  OGR_GreatCircle_ExtendPosition(0, 0, 100000, 0, &latC, &lonC);
  printf("lat=%.15f, lon=%.15f\n", latC, lonC);

  OGR_GreatCircle_ExtendPosition(0, 0, 100000, 90, &latC, &lonC);
  printf("lat=%.15f, lon=%.15f\n", latC, lonC);

  OGR_GreatCircle_ExtendPosition(0, 0, 100000, 180, &latC, &lonC);
  printf("lat=%.15f, lon=%.15f\n", latC, lonC);

  OGR_GreatCircle_ExtendPosition(0, 0, 100000, 270, &latC, &lonC);
  printf("lat=%.15f, lon=%.15f\n", latC, lonC);

  return 0;
}
