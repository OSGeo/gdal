-------------------------------------------------------------------------------
PROJ.4 port for Windows CE
Author: Mateusz Loskot (mateusz@loskot.net)
-------------------------------------------------------------------------------

This readme explains how to build PROJ.4 for Windows CE based
operating systems like Windows Mobile and custom versions.

-----------------------------------
Building PROJ.4 for Windows CE
-----------------------------------

Currently, project files for Microsoft Visual C++ 2005 are provided.
Although, PROJ.4 will build using Microsoft eMbedded Visual C++ 3.0 or 4.0 as well.

1. Requirements

In order to build PROJ.4 for Windows CE you need to download WCELIBCEX library

http://sourceforge.net/projects/wcelibcex

The WCELIBCEX is a library providing helpful features while
porting Windows/Unix libraries to Windows CE and can be downloaded
directly from Subversion repository:

svn co https://wcelibcex.svn.sourceforge.net/svnroot/wcelibcex/trunk/ wcelibcex

or as a source distribution package - wcelibcex-1.0.zip - from Download link.

Next, you will find project file of static library for Visual C+ 2005
located in wcelibcex/msvc80.

2. Configure WCELIBCEX

2.1 Open one of solutions for PROJ.4: projce_dll.sln or projce_lib.sln
    in the Visual C++ 2005 IDE.

2.2 Add WCELIBCEX project to the PORJ.4 solution by selecting wcelibcex_lib.vcproj file.

2.3 Configure WCELIBCEX_DIR path in projce_common.vsprops Property Sheet:

    1. Open View -> Property Manager.
    2. Expand one of node under projce_dll or projce_lib project
    3. Double-click on projce_common property sheet
    4. Go to User Macros
    5. Select WCELIBCEX_DIR macro and set path pointing directly to directory
       where you downloaded the WCELIBCEX library sources tree.
    6. Click OK and close the dialog box

3. Build

Select Build -> Build Solution

4. Output binaries:

4.1 projce_lib.sln - static library

projced.lib - Debug configuration
projce.lib  - Release configuration

4.2 projce_dll.sln - Dynamic-link library

proj_i.lib - import library
proj.dll - dynamic-link library

NOTE: There is no 'ced' or 'ce' token in DLL binaries names.
      This is intentional and don't change it.
      GDAL requires PROJ.4 DLL named as proj.dll.

-----------------------------------
Notes
-----------------------------------

Preferable place to ask for help, is the official mailing
list - proj@lists.maptools.org

Author: Mateusz Loskot <mateusz@loskot.net>
