@call C:\gdal\miniforge3\condabin\conda activate c:\gdal\condaenv\gdal
@set "OUTFILE=C:\gdal\.bashrc"
@echo export PATH=/c/gdal/condaenv/gdal/Library/bin:/c/gdal/miniforge3/Scripts:/c/gdal/msys64/usr/bin:^$PATH> "%OUTFILE%"
@echo source /c/gdal/condaenv/gdal/Library/share/bash-completion/completions/gdal>> "%OUTFILE%"
@set "PS1=(gdal) \w\$ "
@call C:\gdal\msys64\usr\bin\bash.exe --init-file C:\gdal\.bashrc
