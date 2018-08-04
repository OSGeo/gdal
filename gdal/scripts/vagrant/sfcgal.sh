#!/bin/bash
# Build CGAL
wget https://github.com/CGAL/cgal/releases/download/releases%2FCGAL-4.11.3/CGAL-4.11.3.tar.xz
tar xf CGAL-4.11.3.tar.xz
(cd CGAL-4.11.3 && cmake . && make && sudo make install)
#rm -rf CGAL-4.11.3 CGAL-4.11.3.tar.gz

# Build SFCGAL
wget https://codeload.github.com/Oslandia/SFCGAL/tar.gz/v1.3.5 -O SFCGAL1.3.5.tar.gz
tar xf SFCGAL1.3.5.tar.gz
(cd SFCGAL-1.3.5 && cmake . && make && sudo make install)
sudo ldconfig
