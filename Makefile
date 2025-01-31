CXX = g++
CXXFLAGS = -Wall -std=c++17 -pthread -DBOOST_LOG_DYN_LINK
INCLUDES = -Iinclude \
          -Iinclude/client \
          -Iinclude/server \
          -Iinclude/common \
          -Ilibs/cxxopts/include
LDFLAGS = -lboost_log_setup -lboost_log -lboost_filesystem -lboost_thread -lboost_system -lboost_chrono -lpthread

# Directory structure
SRC_DIR = src
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
LIBS_DIR = libs

# Source files
CLIENT_SRC = $(wildcard $(SRC_DIR)/client/*.cpp)
SERVER_SRC = $(wildcard $(SRC_DIR)/server/*.cpp)
COMMON_SRC = $(wildcard $(SRC_DIR)/common/*.cpp)
MAIN_SRC = main.cpp

# Object files
CLIENT_OBJ = $(patsubst $(SRC_DIR)/client/%.cpp,$(OBJ_DIR)/client/%.o,$(CLIENT_SRC))
SERVER_OBJ = $(patsubst $(SRC_DIR)/server/%.cpp,$(OBJ_DIR)/server/%.o,$(SERVER_SRC))
COMMON_OBJ = $(patsubst $(SRC_DIR)/common/%.cpp,$(OBJ_DIR)/common/%.o,$(COMMON_SRC))
MAIN_OBJ = $(OBJ_DIR)/main.o

# Final target
TARGET = $(BUILD_DIR)/main

# Check for required libraries
REQUIRED_LIBS = cxxopts

.PHONY: all clean directories check-libs

all: check-libs directories $(TARGET)

check-libs:
	@for lib in $(REQUIRED_LIBS); do \
		if [ ! -d "$(LIBS_DIR)/$$lib" ]; then \
			echo "Error: Required library '$$lib' not found in $(LIBS_DIR)"; \
			echo "Please ensure all required libraries are installed in the libs directory"; \
			exit 1; \
		fi \
	done

$(TARGET): $(CLIENT_OBJ) $(SERVER_OBJ) $(COMMON_OBJ) $(MAIN_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compilation rules
$(OBJ_DIR)/client/%.o: $(SRC_DIR)/client/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/server/%.o: $(SRC_DIR)/server/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/common/%.o: $(SRC_DIR)/common/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR)/main.o: $(MAIN_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Create required directories
directories:
	mkdir -p $(BUILD_DIR)
	mkdir -p $(OBJ_DIR)/client
	mkdir -p $(OBJ_DIR)/server
	mkdir -p $(OBJ_DIR)/common

clean:
	rm -rf $(BUILD_DIR)