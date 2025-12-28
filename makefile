CXX = g++
CXXFLAGS = -Wall -std=c++17 -Iinclude
TARGET = server
OBJS = src/server.o src/xslog.o

all: $(TARGET)

$(TARGET) : $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET)

src/server.o: src/server.cpp include/xslog.hpp
	$(CXX) $(CXXFLAGS) -c src/server.cpp -o src/server.o

src/xslog.o: src/xslog.cpp include/xslog.hpp
	$(CXX) $(CXXFLAGS) -c src/xslog.cpp -o src/xslog.o

.PHONY: clean
clean:
	rm -f $(TARGET) $(OBJS)