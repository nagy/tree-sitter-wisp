{
  description = "Wisp (SRFI-119) grammar for tree-sitter";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems f;
    in
    {
      devShells = forAllSystems (system: {
        default =
          let
            pkgs = import nixpkgs { inherit system; };
          in
          pkgs.mkShell {
            packages = with pkgs; [
              tree-sitter
              nodejs
              gcc
            ];
          };
      });
    };
}
