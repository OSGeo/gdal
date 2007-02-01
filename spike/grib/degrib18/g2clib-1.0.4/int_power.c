#include "grib2.h"
/*
 * w. ebisuzaki
 *
 *  return x**y
 *
 *
 *  input: double x
 *         int y
 */
double int_power(double x, g2int y) {

        double value;

        if (y < 0) {
                y = -y;
                x = 1.0 / x;
        }
        value = 1.0;

        while (y) {
                if (y & 1) {
                        value *= x;
                }
                x = x * x;
                y >>= 1;
        }
        return value;
}

