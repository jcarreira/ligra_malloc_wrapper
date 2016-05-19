
CXX=g++
CFLAGS=-O2 -Wall
LDFLAGS=-ldl -fPIC
PROJ=malloc_wrapper

all:
	$(CXX) $(CFLAGS) -o $(PROJ).so -shared $(PROJ).cpp $(LDFLAGS)
