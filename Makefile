# Compiler and flags
CXX = clang++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -g -MMD -MP

# Includes
INCLUDES = \
    -I./game_engine/src \
    -I./game_engine \
    -I./includes \
    -I./includes/glm-0.9.9.8/glm-0.9.9.8/ \
    -I./includes/rapidjson-1.1.0/include/ \
    -I./includes/windows/ \
    -I./includes/windows/SDL2/ \
    -I./includes/windows/SDL2_image/ \
    -I./includes/windows/SDL2_mixer/ \
    -I./includes/windows/SDL2_ttf/ \
    -I./includes/box2d-2.4.1/include \
    -I./includes/box2d-2.4.1/src 

# Engine source files
ENGINE_SRCS = $(wildcard game_engine/src/*.cpp)
ENGINE_OBJS = $(ENGINE_SRCS:.cpp=.o)

# Output binary
TARGET = game_engine_linux

# Libraries
LIBS = -lSDL2 -lSDL2_image -lSDL2_mixer -lSDL2_ttf -llua5.4

# Default rule
all: $(TARGET)

# Linking
$(TARGET): $(ENGINE_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(BOX2D_SRCS) $(LIBS)

# Compilation rule
game_engine/src/%.o: game_engine/src/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Cleanup
clean:
	rm -f $(ENGINE_OBJS) $(TARGET) game_engine/src/*.d

-include $(ENGINE_OBJS:.o=.d)