# prunefl

`prunefl` is a utility that given a list of SystemVerilog files and a top module,
returns the minimal list of files required for the top module to compile.

# Building

## Requirements

* git
* CMake 3.30+
* Python
* clang-tools (optional, for linting)

## Steps

```bash
# configure will automatically be called
make -j$(nproc)
```

## Nix

```
nix build -L
```

The binary will be in `./result/bin/prunefl`.

# Usage

Make sure to use exactly -C or implicit macro resolution may go haywire.

```console
$ ./build/prunefl -C test/mvp_model/mvp_model.f --top module_b --debug-out-pfx ./debug/tree
```

The files will be printed in topological order.

# License

The MIT License. See [License](./License).
