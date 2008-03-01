iom - The INTERLIS Object Model

Features
- generic read and write of INTERLIS 2 transfer files
- generic read of INTERLIS 1 and 2 model files
- pure c API (no C++ in the public interface)

How to use it?
- include iom.h in your source
- link with iom.lib
- see samples and test programs for API usage

License
IOM is licensed under the MIT/X.
ili2c.jar is licensed under the LGPL.
IOM and ili2c include software developed by the Apache Software 
Foundation (http://www.apache.org/).

Latest Version
The current version of iom can be found at
http://iom.sourceforge.net/

System Configuration
Although IOM is written in C/C++, to use IOM in a application, a Java Runtime Environment is required.
In order to compile IOM, a C/C++ compiler (on Windows MSVC 6; on Linux GCC 3.2.3) is required.
To test IOM, python and a diff utility is required.
To recreate the distribution zip-file, the build tool ant is required. Download it from http://ant.apache.org and install it as documented there.

Installation
In order to install IOM, extract the ZIP file into a new directory.

How to compile it on Windows using Microsoft Visual C++?
To build IOM from the source distribution (using MSVC), you will need to open the workspace containing the project. 
The workspace containing the IOM project file and all other samples is in:
 iom\projects\win32\vc6\iom.dsw
Once you are inside MSVC, you need to build the project marked iom.
If you are building your application, you may want to add the IOM project inside your applications's workspace.
You need to pick up:
 iom\projects\win32\vc6\iom\iom.dsp
You must make sure that you are linking your application with the iom.lib library and also make sure that the associated DLL (xerces_c), the INTERLIS-Compiler (ili2c) and the JAVA virtual machine (java) is somewhere in your path.

How to compile it on Linux using GCC?
FIXME

To build a distribution, use
 ant dist

Dependencies
- xerces-c (www.apache.com) 
- ili2c (www.interlis.ch)
- JRE (www.java.com)

Comments/Suggestions
Please send comments to ce@eisenhutinformatik.ch

