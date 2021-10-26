.. _vector.mdb:

Access MDB databases
====================

.. shortname:: MDB

.. build_dependencies:: JDK/JRE and Jackcess

.. deprecated_driver:: version_targeted_for_removal: 3.5
   env_variable: GDAL_ENABLE_DEPRECATED_DRIVER_MDB
   message: You should consider using the generic ODBC driver with an updated MDBTools Access driver instead.

OGR optionally supports reading access .mdb files by using the Java
`Jackcess <http://jackcess.sourceforge.net/>`__ library.

This driver is primarily meant as being used on Unix platforms to
overcome the issues often met with the MDBTools library that acts as the
ODBC driver for MDB databases.

The driver can detect ESRI Personal Geodatabases and Geomedia MDB
databases, and will deal them exactly as the :ref:`PGeo <vector.pgeo>`
and :ref:`Geomedia <vector.geomedia>` drivers do. For other MDB
databases, all the tables will be presented as OGR layers.

How to build the MDB driver (on Linux)
--------------------------------------

You need a JDK (a JRE is not enough) to build the driver. On Ubuntu
10.04 with the openjdk-6-jdk package installed,

::

   ./configure --with-java=yes --with-mdb=yes

On others Linux flavors, you may need to specify :

::

   ./configure --with-java=/path/to/jdk/root/path --with-jvm-lib=/path/to/libjvm/directory --with-mdb=yes

where /path/to/libjvm/directory is for example
/usr/lib/jvm/java-6-openjdk/jre/lib/amd64/server

It is possible to add the *--with-jvm-lib-add-rpath* option (no value or
"yes") to embed the path to the libjvm.so in the GDAL library.

How to run the MDB driver (on Linux)
------------------------------------

You need a JRE and 3 external JARs to run the driver.

#. If you didn't specify --with-jvm-lib-add-rpath at configure time, set
   the path of the directory that contains libjvm.so in LD_LIBRARY_PATH
   or in /etc/ld.so.conf.
#. Download jackcess-1.2.XX.jar (but 2.X does not with the current
   driver), commons-lang-2.4.jar and commons-logging-1.1.1.jar (other
   versions might work)
#. Put the 3 JARs either in the lib/ext directory of the JRE (e.g.
   /usr/lib/jvm/java-6-openjdk/jre/lib/ext) or in another directory and
   explicitly point to each of them with the CLASSPATH environment
   variable.

Resources
---------

-  `Jackcess <http://jackcess.sourceforge.net/>`__ library home page
-  Utility that contains the needed `JARs
   dependencies <https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/mdb-sqlite/mdb-sqlite-1.0.2.tar.bz2>`__

See also
--------

-  :ref:`PGeo <vector.pgeo>` driver page
-  :ref:`Geomedia <vector.geomedia>` driver page
