/*
 * $Id$
 *
 * helper function to convert OGRError numbers into
 * character strings.
 *
 * $Log$
 * Revision 1.2  2005/09/26 08:16:48  cfis
 * Ruby does not seem to support the %fragment directive.
 *
 * Revision 1.1  2005/09/13 02:58:19  kruland
 * Put the OGRErr to char * mapping in a file so multiple bindings can use it.
 *
 *
 */

#ifdef SWIGRUBY
%header 
#else
%fragment("OGRErrMessages","header") 
#endif
%{

static char const *
OGRErrMessages( int rc ) {
  switch( rc ) {
  case 0:
    return "OGR Error: None";
  case 1:
    return "OGR Error: Not enough data";
  case 2:
    return "OGR Error: Not enough memory";
  case 3:
    return "OGR Error: Unsupported geometry type";
  case 4:
    return "OGR Error: Unsupported operation";
  case 5:
    return "OGR Error: Corrupt data";
  case 6:
    return "OGR Error: General Error";
  case 7:
    return "OGR Error: Unsupported SRS";
  default:
    return "OGR Error: Unknown";
  }
}
%}
