
CXXFLAGS = -g -Wall -I../src -I/usr/local/include -DDEBUG

OBJ =   test_main.o testpcidskexception.o testpcidskfile.o testsegments.o \
	testpcidskcreate.o testpcidskupdate.o testoverviews.o \
	testvectorread.o testprojections.o

LIBS =	-L../src -lpcidsk -L/usr/local/lib -ljpeg -lcppunit -ldl # ../src/pcidsk.a 

default:	test_main pcidsk_read pcidsk2pcidsk

pcidsk_read:	pcidsk_read.o  ../src/pcidsk.so
	g++  pcidsk_read.o $(LIBS) -o pcidsk_read

pcidsk2pcidsk:	pcidsk2pcidsk.o  ../src/pcidsk.so
	g++ pcidsk2pcidsk.o $(LIBS) -o pcidsk2pcidsk

test_main:	$(OBJ) ../src/pcidsk.so
	g++ $(OBJ) $(LIBS) -o test_main

../src/pcidsk.so:
	(cd ../src ; $(MAKE))

check:  test_main
	LD_LIBRARY_PATH=../src DYLD_LIBRARY_PATH=../src ./test_main

clean:
	-rm -f *.o
	-rm -f test_main
	-rm -rf pcidsk2pcidsk
	-rm -rf pcidsk_read

%.o:	%.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<
