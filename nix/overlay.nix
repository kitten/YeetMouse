{ shortRev ? "dev" }:
final: prev:

{
  yeetmouse = import ./package.nix { inherit shortRev; } final;
}
