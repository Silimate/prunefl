{
  inputs = {
    nix-eda.url = github:fossi-foundation/nix-eda;
  };

  outputs = {
    self,
    nix-eda,
    ...
  }: let
    nixpkgs = nix-eda.inputs.nixpkgs;
    lib = nixpkgs.lib;
  in {
    overlays = {
      default = lib.composeManyExtensions [
        (pkgs': pkgs: let
          callPackage = lib.callPackageWith pkgs';
        in {
          # nodo = callPackage ./default.nix {
          #   flake = self;
          # };
        })
      ];
    };

    legacyPackages = nix-eda.forAllSystems (
      system:
        import nix-eda.inputs.nixpkgs {
          inherit system;
          overlays = [nix-eda.overlays.default self.overlays.default];
        }
    );

    packages = nix-eda.forAllSystems (
      system: let
        pkgs = self.legacyPackages."${system}";
        callPackage = lib.callPackageWith pkgs;
      in {
        # inherit (pkgs) nodo;
        # default = pkgs.nodo;
      }
    );

    devShells = nix-eda.forAllSystems (
      system: let
        pkgs = self.legacyPackages."${system}";
        callPackage = lib.callPackageWith pkgs;
      in {
        default = pkgs.mkShell.override {stdenv = pkgs.llvmPackages_18.stdenv;} {
          nativeBuildInputs = [
            pkgs.cmake
            pkgs.ninja
            pkgs.llvmPackages_18.clang-tools
          ];
        };
      }
    );
  };
}
