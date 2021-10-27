Security policy
===============

GDAL is a large software dealing literally with hundreds of file formats and protocols.
Some effort is made to ensure the code is safe, but analysis tools like ossfuzz continually
find issues in the code. Data from untrusted sources should be treated with caution.
When security is a major concern, consider running GDAL or code using GDAL in a
restricted sandbox environment.

Our current policy for anyone wanting to report a security related issue is just
to use the [public issue tracker](https://github.com/OSGeo/gdal/issues/new) for it.
However please refrain from publicly posting exploits with harmful consequences (data destruction,
etc.). Only people with the github handles from the [Project Steering Committee](https://gdal.org/community/index.html#project-steering-committee)
(or people that they would explicitly allow) are allowed to ask you privately for
such dangerous reproducers if that was needed.

Note also that we have [listed a number of potential security issues](https://trac.osgeo.org/gdal/wiki/SecurityIssues)
depending on how you use GDAL (caution: the linked page is somewhat outdated)
