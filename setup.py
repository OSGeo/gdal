#!/usr/bin/env python

import string
from distutils.core import setup, Extension
import os, os.path, glob

#
# debug
#
#import sys
#sys.argv.append("build")

f=open(os.path.join("VERSION"))
v=f.read()
f.close()
del f

version=v.strip()
dllversion=version.replace(".","")
soversion=v[:3]

SOURCES=glob.glob(os.path.join("pymod","*.c*"))
print SOURCES
INCLUDE_DIRS=[os.path.join("core"), os.path.join("port"), os.path.join("ogr"), os.path.join("pymod"), ] # only necessary
LIBRARY_DIRS = ["."]

INCLUDE_FILES = [
	glob.glob(os.path.join("core", "*.h")),
	glob.glob(os.path.join("port", "*.h")),
	glob.glob(os.path.join("alg", "*.h")),
	glob.glob(os.path.join("ogr", "*.h")), 
	glob.glob(os.path.join("ogr", "ogrsf_frmts", "*.h")),
	glob.glob(os.path.join("pymod", "*.h"))
]

IF=[]
for i in INCLUDE_FILES:
	IF.extend(i)
INCLUDE_FILES=IF
del IF

HTML_FILES=glob.glob(os.path.join("html", "*"))

#HTML_FILES.remove(os.path.join("html", ".cvsignore"))

#print INCLUDE_FILES

if os.name=="nt":
	DLL="gdal%s.dll" % dllversion
	DATA_FILES=[
		("", [DLL]),
#		("libs", [os.path.join("gdal_i.lib"), os.path.join("pymod","_gdal.lib")]),
		("libs", [os.path.join("gdal_i.lib")]),
	]
	LIBRARIES = ["gdal_i"]
	EXTRA_LINK_ARGS=[]
else:
	DATA_FILES=[
		("lib", ['libgdal.%s.so' % soversion]),
	]
	LIBRARIES = []
	#EXTRA_LINK_ARGS=[os.path.join("gdal.a")]
	LIBRARIES = ["gdal.%s" % soversion]
	EXTRA_LINK_ARGS=[]
	
DATA_FILES.append(("include", INCLUDE_FILES))
DATA_FILES.append((os.path.join("doc","gdal"), HTML_FILES))

setup (name = "Pyhton_GDAL",
       version = version,
       description = "Geospatial Data Abstraction Library: Python Bindings",
       author = "Frank Warmerdam",
       #packager = "Evgeniy Cherkashin",
       author_email = "warmerda@home.com",
       #packager_email = "eugeneai@icc.ru",
       url = "http://www.remotesensing.org/.../gdal/",	# I've forgot the rest
       packages = [''],
       package_dir = {'': 'pymod'},
       extra_path = "gdal",
       ext_modules = [Extension('_gdal',
			sources = SOURCES,
			include_dirs = INCLUDE_DIRS,
			libraries = LIBRARIES,
			library_dirs = LIBRARY_DIRS,
			extra_link_args=EXTRA_LINK_ARGS,
			),
		],
		data_files = DATA_FILES
	)

