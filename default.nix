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
}:
llvmPackages_18.stdenv.mkDerivation {
  pname = "nodo";
  version = lib.strings.trim (builtins.readFile ./VERSION);
  
  src = flake;
  
  postPatch = ''
    perl -i.bak -pe 's/set\(fmt_min_version\s*"[\d\.]+"/set(fmt_min_version "${fmt_11.version}"/' third_party/slang/external/CMakeLists.txt
    perl -i.bak -pe 's/set\(mimalloc_min_version\s*"[\d\.]+"/set(mimalloc_min_version "${mimalloc.version}"/' third_party/slang/external/CMakeLists.txt
  '';
  
  nativeBuildInputs = [
    cmake
    llvmPackages_18.clang-tools
    xxd
    perl
  ];
  
  installPhase = ''
    runHook preInstall
    mkdir -p $out/bin
    cp nodo $out/bin/nodo
    runHook postInstall
  '';
  
  buildInputs = [
    python3
    fmt_11
    mimalloc
  ];
}
