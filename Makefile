# Projet FTL-like avec Raylib
# Makefile polyvalent (build, clean, run)

CXX = g++
CXXFLAGS = -Wall -std=c++17
LDFLAGS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
SRC = $(wildcard src/*.cpp)
OBJ = $(SRC:.cpp=.o)
BIN = bin/ftl_game

.PHONY: all clean run

all: $(BIN)


$(BIN): $(OBJ)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	rm -f $(OBJ)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: all
	./$(BIN)

clean:
	rm -rf bin/*.o bin/ftl_game src/*.o
