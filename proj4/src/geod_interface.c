#include "projects.h"
#include "geod_interface.h"

void geod_ini(void) {
  geod_init(&GlobalGeodesic, geod_a, geod_f);
}

void geod_pre(void) {
  double
    lat1 = phi1 / DEG_TO_RAD, lon1 = lam1 / DEG_TO_RAD,
    azi1 = al12 / DEG_TO_RAD;
  geod_lineinit(&GlobalGeodesicLine, &GlobalGeodesic, lat1, lon1, azi1, 0U);
}

void geod_for(void) {
  double
    s12 = geod_S, lat2, lon2, azi2;
  geod_position(&GlobalGeodesicLine, s12, &lat2, &lon2, &azi2);
  azi2 += azi2 >= 0 ? -180 : 180; /* Compute back azimuth */
  phi2 = lat2 * DEG_TO_RAD;
  lam2 = lon2 * DEG_TO_RAD;
  al21 = azi2 * DEG_TO_RAD;
}

void geod_inv(void) {
  double
    lat1 = phi1 / DEG_TO_RAD, lon1 = lam1 / DEG_TO_RAD,
    lat2 = phi2 / DEG_TO_RAD, lon2 = lam2 / DEG_TO_RAD,
    azi1, azi2, s12;
  geod_inverse(&GlobalGeodesic, lat1, lon1, lat2, lon2, &s12, &azi1, &azi2);
  azi2 += azi2 >= 0 ? -180 : 180; /* Compute back azimuth */
  al12 = azi1 * DEG_TO_RAD; al21 = azi2 * DEG_TO_RAD; geod_S = s12;
}
