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

Make sure to use exactly --single-unit -f for implicit macro resolution while
still properly propagating things like commandline defines.

```console
$ ./build/prunefl --single-unit -f test/mvp_model/mvp_model.f --top module_b --output-flags-to flags.f --output pruned.f
```

`pruned.f` will contain the pruned file list, but to propagate defines,
libraries, include paths and such forward you may want to pass flags.f to the
tool instead (which points to the pruned file list using a `-C` in a single
compilation unit.)

# License

The MIT License. See [License](./License).
