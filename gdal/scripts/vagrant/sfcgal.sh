#!/bin/bash
# Build CGAL
wget https://gforge.inria.fr/frs/download.php/file/34400/CGAL-4.5.1.tar.gz
tar xf CGAL-4.5.1.tar.gz

(cd CGAL-4.5.1 && cmake . && make && sudo make install)
rm -rf CGAL-4.5.1 CGAL-4.5.1.tar.gz
# Build SFCGAL
wget https://codeload.github.com/Oslandia/SFCGAL/tar.gz/v1.3.0 -O SFCGAL1.3.0.tar.gz
tar xf SFCGAL1.3.0.tar.gz
(cd SFCGAL-1.3.0 && cmake . && make && sudo make install)
sudo ldconfig
