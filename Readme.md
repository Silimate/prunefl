# nodo

```
mkdir -p build
cd build
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug ..
ninja
cd ..
clang-tidy include/* src/* -p build
./build/nodo -C test/mvp_model/mvp_model.f --top module_a
```
