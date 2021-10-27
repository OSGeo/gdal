.. _rfc-2:

===============================================
RFC 2: Migration to OSGeo Subversion Repository
===============================================

Author: Frank Warmerdam

Contact: warmerdam@pobox.com

Status: Adopted

Summary
-------

It is proposed that the GDAL source tree be moved a subversion
repository in such a manner as to preserve the history existing in the
CVS repository. A 1.3.x branch will be created after automatic updating
of the header format.

Details
-------

1. The conversion will be done by Howard Butler using the cvs2svn tool.
2. At least 24 hours notice will be provided before the transition
   starts to allow committers to commit any outstanding work that is
   ready to into the repository.
3. When the conversion starts, the GDAL (and gdalautotest) trees will be
   removed from cvs.maptools.org, and archived to avoid any confusion.
4. Frank Warmerdam will modify the "daily cvs snapshot" capability to
   work from SVN.
5. Frank will be responsible for updating the source control information
   in the documentation.
6. All source files in SVN will have the svn:keywords property set to
   "Id" by Frank after they are created.
7. Committers will need to get a login on osgeo.org and notify Frank to
   regain commit access. Committer access on the new repository will be
   enabled after the above changes are all complete.
8. The GDAL committers document should be updated, removing non-GDAL
   committers (ie. libtiff, geotiff, etc).

Header Format
-------------

SVN does not support history insertion in source files, and to keep the
old history listings around without keeping them up to date would be
very confusing. So it is proposed that Frank Warmerdam write a script to
strip the history logs out. Changing this:

::

   /******************************************************************************
    * $Id: RFC2_SVN.dox 10627 2007-01-17 05:20:16Z warmerdam $
    *
    * Project:  GDAL Core
    * Purpose:  Color table implementation.
    * Author:   Frank Warmerdam, warmerdam@pobox.com
    *
    ******************************************************************************
    * Copyright (c) 2000, Frank Warmerdam
   ...
    ******************************************************************************
    * $Lcg: RFC2_SVN.dox,v $
    * Revision 1.6  2006/03/28 14:49:56  fwarmerdam
    * updated contact info
    *
    * Revision 1.5  2005/09/05 19:29:29  fwarmerdam
    * minor formatting fix
    */

   #include "gdal_priv.h"

   CPL_CVSID("$Id: RFC2_SVN.dox 10627 2007-01-17 05:20:16Z warmerdam $");

to this:

::

   /******************************************************************************
    * $Id: RFC2_SVN.dox 10627 2007-01-17 05:20:16Z warmerdam $
    *
    * Project:  GDAL Core
    * Purpose:  Color table implementation.
    * Author:   Frank Warmerdam, warmerdam@pobox.com
    *
    ******************************************************************************
    * Copyright (c) 2000, Frank Warmerdam
   ...
    *****************************************************************************/

   #include "gdal_priv.h"

   CPL_CVSID("$Id: RFC2_SVN.dox 10627 2007-01-17 05:20:16Z warmerdam $");

.. _branch-for-13:

Branch for 1.3
--------------

Once the headers have been updated appropriately, a 1.3 branch will be
established in subversion. The intent is that further 1.3.x releases
would be made against this "stable branch" while trunk work is towards a
1.4.0 release targeted for around the time of the OSGeo conference.
