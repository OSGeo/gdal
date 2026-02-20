.. _rfc-111:

=====================================================================
RFC 111: Add a AI/LLM tool policy
=====================================================================

============== =============================================
Author:        Even Rouault (for bringing it to GDAL)
               Original authors of the LLVM policy that we
               deeply borrow: Reid Kleckner, Hubert Tong and
               "maflcko"
Contact:       even.rouault @ spatialys.com
Started:       2026-02-09
Status:        Adopted
============== =============================================

Summary
-------

LLM/AI are nowadays a reality, that starts reaching GDAL, and whether we like
it or not, introduces new challenges in our collaborative environment that need
to be addressed.

Details
-------

A new document, :ref:`ai_tool_policy`, is added to our documentation, linked from
the https://gdal.org/community.html,
https://gdal.org/development/dev_practices.html, pull request and issue templates.

As detailed at the bottom of that new page, this policy is directly derived from
the one adopted by the LLVM compiler project.

That document may evolve over time, as the best practices about that topic mature.
Substantial evolutions will be subject to approval of the revisions.

Similar moves in other projects
-------------------------------

- GRASS GIS policy: https://github.com/OSGeo/grass/blob/main/CONTRIBUTING.md
- QGIS policy: https://github.com/qgis/QGIS-Enhancement-Proposals/pull/363

Voting history
--------------

+1 from PSC members EvenR, JukkaR, MikeS, DanB, DanielM, JavierJS, KurtS, FrankW, HowardB and NormanB

.. below is an allow-list for spelling checker.

.. spelling:word-list::
    Reid
    Kleckner
    Hubert
    Tong
    maflcko
