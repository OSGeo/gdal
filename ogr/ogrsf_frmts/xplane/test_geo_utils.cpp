#include <stdio.h>
#include "ogr_xplane_geo_utils.h"

int main(int argc, char* argv[])
{
  double latA = 49, lonA = 2;
  double latB = 49.1, lonB = 2.1;
  double latC, lonC;
  double heading;
  double distance;
  
  heading = OGRXPlane_Track(latA, lonA, latB, lonB);
  distance = OGRXPlane_Distance(latA, lonA, latB, lonB);
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
