
CXX=g++
CFLAGS=-O2 -Wall -ggdb
LDFLAGS=-ldl -fPIC
PROJ=malloc_wrapper

all:
	$(CXX) $(CFLAGS) -o $(PROJ).so -shared $(PROJ).cpp $(LDFLAGS)
