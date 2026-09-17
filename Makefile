# Projet FTL-like avec Raylib
#   make          build optimisé et durci      -> bin/ftl_game
#   make debug    UBSan (trap) + conteneurs vérifiés -> bin/ftl_game_debug
#   make asan     ASan + UBSan (nécessite libasan/libubsan) -> bin/ftl_game_asan
#   make run / make clean

CXX       = g++
COMMON    = -Wall -Wextra -std=c++20 -MMD -MP -D_GLIBCXX_ASSERTIONS
RELEASE   = -O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fstack-clash-protection -fPIE
DEBUG     = -O1 -g -fsanitize=undefined -fsanitize-trap=all -D_GLIBCXX_DEBUG -fstack-protector-all -fno-omit-frame-pointer
ASAN      = -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
LIBS      = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
LD_REL    = -pie -Wl,-z,relro,-z,now

SRC       = $(wildcard src/*.cpp src/*/*.cpp)
OBJ_REL   = $(patsubst src/%.cpp,build/release/%.o,$(SRC))
OBJ_DBG   = $(patsubst src/%.cpp,build/debug/%.o,$(SRC))
OBJ_ASAN  = $(patsubst src/%.cpp,build/asan/%.o,$(SRC))
BIN       = bin/ftl_game
BIN_DBG   = bin/ftl_game_debug
BIN_ASAN  = bin/ftl_game_asan

.PHONY: all debug asan clean run

all: $(BIN)

debug: $(BIN_DBG)

asan: $(BIN_ASAN)

$(BIN): $(OBJ_REL)
	@mkdir -p $(dir $@)
	$(CXX) $(COMMON) $(RELEASE) -o $@ $^ $(LD_REL) $(LIBS)

$(BIN_DBG): $(OBJ_DBG)
	@mkdir -p $(dir $@)
	$(CXX) $(COMMON) $(DEBUG) -o $@ $^ $(LIBS)

$(BIN_ASAN): $(OBJ_ASAN)
	@mkdir -p $(dir $@)
	$(CXX) $(COMMON) $(ASAN) -o $@ $^ $(LIBS)

build/release/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(COMMON) $(RELEASE) -c $< -o $@

build/debug/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(COMMON) $(DEBUG) -c $< -o $@

build/asan/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(COMMON) $(ASAN) -c $< -o $@

run: all
	./$(BIN)

clean:
	rm -rf build $(BIN) $(BIN_DBG) $(BIN_ASAN)

-include $(OBJ_REL:.o=.d) $(OBJ_DBG:.o=.d) $(OBJ_ASAN:.o=.d)
