.. _csharp_usage:

================================================================================
C# Bindings Usage Advice
================================================================================

Adding reference to the GDAL/OGR assemblies
-------------------------------------------

TODO

Using the interface classes
---------------------------

TODO


Modifying Local Search Path
---------------------------


If you want to add a folder to PATH during run-time, so you don't have to pollute system PATH permanently, you can do it this way, in C#

.. code-block:: C#

    using System.Runtime.InteropServices;

    ...

    [DllImport("kernel32.dll", CharSet=CharSet.Auto, SetLastError=true)]
    public static extern bool
    SetEnvironmentVariable(string lpName, string lpValue);

    ...

    string GDAL_HOME = @";C:\Program Files\FWTools\bin";  // for example

    string path = Environment.GetEnvironmentVariable("PATH");
    path += ";" + GDAL_HOME;
    SetEnvironmentVariable("PATH", path);


MSDN documentation:

* `http://msdn2.microsoft.com/en-us/library/ms686206.aspx <http://msdn2.microsoft.com/en-us/library/ms686206.aspx>`__
* `http://msdn2.microsoft.com/en-us/library/system.environment.setenvironmentvariable.aspx <http://msdn2.microsoft.com/en-us/library/system.environment.setenvironmentvariable.aspx>`__

Instead of the P/Invoke call to :program:`SetEnvironmentVariable()`, you can use C# native method :program:`Environment.SetEnvironmentVariable()`. Read the doc carefully, because there are two versions of this method. Unlike the Win32 API call accessed through P/Invoke, the method :program:`Environment.SetEnvironmentVariable()` has an overload that *may* change environment permanently, across processes.
