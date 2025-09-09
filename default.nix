{
  src ? ./.,
  lib,
  llvmPackages_18, # new c++
  cmake, # build system
  ninja, # build utility
  python3, # glue
  perl, # sed and awk are tools used by masochists
  # runtime deps
  fmt_11,
  mimalloc,
  boost,
}:
llvmPackages_18.stdenv.mkDerivation {
  pname = "prunefl";
  version = lib.strings.trim (builtins.readFile ./VERSION);
  
  inherit src;
  
  # Substitute minimum versions for those available in nixpkgs - likely overconstrained by slang itself
  postPatch = ''
    perl -i.bak -pe 's/set\(fmt_min_version\s*"[\d\.]+"/set(fmt_min_version "${fmt_11.version}"/' third_party/slang/external/CMakeLists.txt
    perl -i.bak -pe 's/set\(mimalloc_min_version\s*"[\d\.]+"/set(mimalloc_min_version "${mimalloc.version}"/' third_party/slang/external/CMakeLists.txt
  '';
  
  nativeBuildInputs = [
    cmake
    llvmPackages_18.clang-tools
    python3
    perl
    ninja
  ];
  
  buildInputs = [
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
