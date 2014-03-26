TBB_INC=${HOME}/tbb/src/tbb42_20131118oss/include 
TBB_LIB=${HOME}/tbb/src/tbb42_20131118oss/build/linux_intel64_gcc_cc4.4.7_libc2.12_kernel2.6.32_release
GSL_INC=${HOME}/gsl/installpath/include
GSL_LIB=${HOME}/gsl/installpath/lib
JEMALLOC_INC=${HOME}/jemalloc/installpath/include
JEMALLOC_LIB=${HOME}/jemalloc/installpath/lib
TBBFLAGS=-I$(TBB_INC) -Wl,-rpath,$(TBB_LIB) -L$(TBB_LIB) -ltbb
GSLFLAGS=-I$(GSL_INC) -Wl,-rpath,$(GSL_LIB) -L$(GSL_LIB) -lgsl -lgslcblas -lm
JEMALLOCFLAGS=-I$(JEMALLOC_INC) -Wl,-rpath,$(JEMALLOC_LIB) -L$(JEMALLOC_LIB) -ljemalloc
CC=g++
CFLAGS= -O3  -lrt -g -lpthread
SRC= ./src/LockBasedIBst.c ./src/TestLockBasedIBst.c
OBJ= ./bin/LockBasedIBst.o
ibst: ./src/LockBasedIBst.c ./src/TestLockBasedIBst.c
	$(CC) $(CFLAGS) $(TBBFLAGS) $(GSLFLAGS) $(JEMALLOCFLAGS) -o $(OBJ) $(SRC)
clean:
	rm -rf ./bin/*.o

