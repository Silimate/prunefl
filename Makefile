SOURCES = $(wildcard src/*.cc)
HEADERS = $(wildcard include/*.hh)

all: debug 
debug: CMAKE_FLAGS = -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=-O0
debug: build/prunefl

release: CMAKE_FLAGS = -DCMAKE_BUILD_TYPE=Release
release: build/prunefl

build/prunefl: build/compile_commands.json $(SOURCES) $(HEADERS)
	mkdir -p $(@D)
	$(MAKE) -C $(@D)

build/compile_commands.json: CMakeLists.txt
	mkdir -p $(@D)
	cd $(@D) && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(CMAKE_FLAGS) ..

.PHONY: lint
lint: build/compile_commands.json
	clang-format --dry-run src/* include/*
	clang-tidy -p build/compile_commands.json src/* include/*

.PHONY: format
format:
	clang-format -i src/* include/*

.PHONY: demo
demo: ./build/prunefl
	$< -C test/mvp_model/mvp_model.f --top module_b --debug-out-pfx ./debug/tree
