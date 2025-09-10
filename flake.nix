{
  inputs = {
    self.submodules = true;
    nix-eda.url = "github:fossi-foundation/nix-eda";
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
          prunefl = callPackage ./default.nix {
            src = self;
          };
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
        inherit (pkgs) prunefl;
        default = pkgs.prunefl;
      }
    );
  };
}
