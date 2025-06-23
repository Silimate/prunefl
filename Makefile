SOURCES = $(wildcard src/*.cc)
HEADERS = $(wildcard include/*.hh)

all: build/prunefl

build/prunefl: build/compile_commands.json $(SOURCES) $(HEADERS)
	mkdir -p $(@D)
	$(MAKE) -C $(@D)

build/compile_commands.json: CMakeLists.txt
	mkdir -p $(@D)
	cd $(@D) && cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug ..

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
