{
  flake,
  lib,
  llvmPackages_18,
  cmake,
  xxd,
  perl,
  python3,
  fmt_11,
  mimalloc,
  boost,
}:
llvmPackages_18.stdenv.mkDerivation {
  pname = "prunefl";
  version = lib.strings.trim (builtins.readFile ./VERSION);
  
  src = flake;
  
  # Substitute minimum versions for those available in nixpkgs - likely overconstrained by slang itself
  postPatch = ''
    perl -i.bak -pe 's/set\(fmt_min_version\s*"[\d\.]+"/set(fmt_min_version "${fmt_11.version}"/' third_party/slang/external/CMakeLists.txt
    perl -i.bak -pe 's/set\(mimalloc_min_version\s*"[\d\.]+"/set(mimalloc_min_version "${mimalloc.version}"/' third_party/slang/external/CMakeLists.txt
  '';
  
  nativeBuildInputs = [
    cmake
    llvmPackages_18.clang-tools
    perl
  ];
  
  buildInputs = [
    python3
    fmt_11
    mimalloc
    boost
  ];
  
  installPhase = ''
    runHook preInstall
    mkdir -p $out/bin
    cp prunefl $out/bin/prunefl
    runHook postInstall
  '';
  
  meta = {
    description = "SystemVerilog File Pruning Utility";
    homepage = "https://github.com/silimate/prunefl";
    license = lib.licenses.mit;
    platforms = lib.platforms.all;
  };
}
