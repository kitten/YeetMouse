{
  description = "A fork of a fork of the Linux mouse driver with acceleration. Now with GUI and some other improvements!";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };

  outputs = inputs @ { self, nixpkgs }: let
    inherit (inputs.nixpkgs) lib;
    packageInputs = { inherit (self) shortRev; };
    eachSystem = lib.genAttrs ["aarch64-linux" "x86_64-linux"];
  in {
    inherit inputs;
    nixosModules.default = import ./module.nix packageInputs;
    overlays.default = import ./overlay.nix packageInputs;
    packages = eachSystem (system: {
      yeetmouse = (import ./package.nix packageInputs nixpkgs.legacyPackages.${system});
    });
  };
}
