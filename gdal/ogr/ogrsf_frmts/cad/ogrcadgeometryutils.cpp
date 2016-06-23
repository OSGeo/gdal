/************************************************************************/
/*                            AngleCorrect()                            */
/*                                                                      */
/*      Convert from a "true" angle on the ellipse as returned by       */
/*      the DWG API to an angle of rotation on the ellipse as if the    */
/*      ellipse were actually circular.                                 */
/************************************************************************/
#include "cpl_conv.h"
#include "ogrcadgeometryutils.h"

double CADUtils::AngleCorrect( double dfTrueAngle, double dfRatio )
{
    double dfRotAngle;
    double dfDeltaX, dfDeltaY;

    dfTrueAngle *= (M_PI / 180); // convert to radians.

    dfDeltaX = cos(dfTrueAngle);
    dfDeltaY = sin(dfTrueAngle);

    dfRotAngle = atan2( dfDeltaY, dfDeltaX * dfRatio);

    dfRotAngle *= (180 / M_PI); // convert to degrees.

    if( dfTrueAngle < 0 && dfRotAngle > 0 )
        dfRotAngle -= 360.0;

    if( dfTrueAngle > 360 && dfRotAngle < 360 )
        dfRotAngle += 360.0;

    return dfRotAngle;
}