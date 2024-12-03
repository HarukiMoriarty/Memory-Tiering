CXX = g++
CXXFLAGS = -Iinclude -Wall -std=c++17
CFLAGS = -Iinclude -Wall

SRC = src/TieringManager.cpp server.cpp
OBJ = $(SRC:.cpp=.o)
TARGET = server

all: clean $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJ)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

server.o: server.c
	$(CXX) $(CFLAGS) -c server.c -o server.o

clean:
	rm -f $(OBJ) server.o $(TARGET)
