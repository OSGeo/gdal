.. _rfc-112:

=====================================================================
RFC 112: Drop Mono Support from CMAKE build system
=====================================================================

============== =============================================
Author:        Paul Harwood
Contact:       runette @ gmail.com
Started:       2026-04-14
Status:        Adopted, implemented
Target:        GDAL 3.14
============== =============================================

Summary
-------
When the CMAKE build system was created, Mono build using `mcs` was grandfathered in.

Mono is in maintenance mode by Microsoft and the recommendation for some time has been `to move to dotnet on all platforms <https://www.mono-project.com/>`__.

Dotnet is now widely available on `all reasonable platforms <https://dotnet.microsoft.com/en-us/download/dotnet/10.0/>`__.

Motivation
----------

1. Removing Mono support from the CMAKE scripts will make them considerably simpler and easier to maintain.
2. Only supporting dotnet will reduce the number of tests in the CI configuration, and thus reduce the time it takes to run the CI tests.
3. Supporting Mono limits the C# language version that can be used in the sample C# applications to version 6+. This is not a production issue but, increasingly, anyone updating those sample apps has to "reset" their mindset back a decade or more - increasing errors and CI failures. We should update the language version and explicitly declare it.

Implementation
--------------
The Mono support in the CMAKE build system will be removed.

The documentation will be updated.

An agreed C# language version will be set for the sample C# projects, currently it is proposed that should be C# 10 to be inline with .NET SDK 6.0 LTS which is very widely used and available.

The CI tests will be updated to remove all Mono builds and confirm dotnet based tests are working and correct.

Voting history
--------------

+1 from PSC members EvenR, JukkaR, MikeS, JavierJS

.. below is an allow-list for spelling checker.

.. spelling:word-list::
    Harwood
    runette
    Dotnet
    dotnet
