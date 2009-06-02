include Make.path

CXX = $(MPICXX)
INCDIR = -I$(MADNESSDIR)/include -I$(BOOSTDIR) -I./src -I./Tests
LIBS = -lMADworld
CXXFLAGS = -g -Wall -fmessage-length=0 $(INCDIR)

OBJS = src/tilemap.o src/env.o TiledArrayTest.o Tests/coordinatestest.o Tests/permutationtest.o \
  Tests/range1test.o Tests/rangetest.o Tests/shapetest.o Tests/tilemaptest.o Tests/tiletest.o \
  Tests/arraytest.o

TARGET =	TiledArrayTest

$(TARGET):	$(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(LIBDIR) $(LIBS) $(DEBUGLEVEL)

src/tilemap.o: src/tilemap.h
src/env.o: src/env.h
Tests/coordinatestest.o: Tests/coordinatestest.h src/coordinates.h
Tests/permutationtest.o: Tests/permutationtest.h src/permutation.h src/coordinates.h
Tests/range1test.o: Tests/range1test.h src/range1.h
Tests/rangetest.o: Tests/rangetest.h src/range.h
Tests/shapetest.o: Tests/shapetest.h src/shape.h src/predicate.h
Tests/tilemaptest.o: Tests/tilemaptest.h src/tilemap.h src/tilemap.o
Tests/arraytest.o: Tests/arraytest.h src/array.h
Tests/tiletest.o: Tests/tiletest.h src/tile.h

src/debug.h:
src/coordinate_system.h:
src/permutation.h: 
src/iterator.h: src/coordinates.h
src/coordinates.h: src/coordinate_system.h
src/block.h: src/debug.h src/coordinate_system.h src/iterator.h
src/array_storage.h: src/coordinate_system.h src/debug.h
src/range1.h:
src/range.h: src/range1.h src/iterator.h src/coordinates.h
src/predicate.h: src/coordinates.h src/permutation.h
src/shape.h: src/range.h src/iterator.h src/predicate.h
src/tile.h: src/coordinates.h src/permutation.h src/iterator.h
src/array.h: src/tile.h src/range.h src/shape.h src/iterator.h src/permutation.h
src/local_array.h: src/array.h
src/replcated_array.h: src/array.h
src/distributed_array.h: src/array.h

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
