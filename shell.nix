{ pkgs ? import <nixpkgs> {} }: 
pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    cmake
    llvmPackages.clang-tools
    python3
    perl
    ninja
  ];
  
  buildInputs = with pkgs; [
    fmt_11
    mimalloc
    boost
  ];
}
