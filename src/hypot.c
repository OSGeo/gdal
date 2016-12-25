/* hypot - sqrt(x * x + y * y)
**
**	Because this was omitted from the ANSI standards, this version
** 	is included for those systems that do not include hypot as an
**	extension to libm.a.  Note: GNU version was not used because it
**	was not properly coded to minimize potential overflow.
**
**	The proper technique for determining hypot is to factor out the
**	larger of the two terms,  thus leaving a possible case of float
**	overflow when max(x,y)*sqrt(2) > max machine value.  This allows
**	a wider range of numbers than the alternative of the sum of the
**	squares < max machine value.  For an Intel x87 IEEE double of
**	approximately 1.8e308, only argument values > 1.27e308 are at
**	risk of causing overflow.  Whereas, not using this method limits
**	the range to values less that 9.5e153 --- a considerable reduction
**	in range!
*/
extern double sqrt(double);
	double
hypot(double x, double y) {
	if ( x < 0.)
		x = -x;
	else if (x == 0.)
		return (y < 0. ? -y : y);
	if (y < 0.)
		y = -y;
	else if (y == 0.)
		return (x);
	if ( x < y ) {
		x /= y;
		return ( y * sqrt( 1. + x * x ) );
	} else {
		y /= x;
		return ( x * sqrt( 1. + y * y ) );
	}
}
