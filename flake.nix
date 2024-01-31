# This flake is supposed to enable a convenient development environment.
# It is not yet devided if and how this flake exports the actually built
# package, once this is in nixpkgs.
# See https://github.com/limine-bootloader/limine/issues/330

{
  description = "limeboot";

  inputs = {
    flake-parts.url = "github:hercules-ci/flake-parts";
    flake-parts.inputs.nixpkgs-lib.follows = "nixpkgs";
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-23.11";
  };

  outputs = inputs@{ self, nixpkgs, flake-parts }:

    flake-parts.lib.mkFlake { inherit inputs; } {
      flake = { };
      # Don't artificially limit users at this point. If a build fails, they
      # will notice it soon enough.
      systems = nixpkgs.lib.systems.flakeExposed;
      perSystem = { config, pkgs, ... }: {
        devShells = {
          default = pkgs.mkShell {
            packages = with pkgs; [
              autoconf269
              automake
              cacert
              gcc # TODO switch to clang for cross-build?
              git
              mtools
              nasm
              # Not in cache.nixos.org; building takes ages.
              # But without it, `--enable-uefi-riscv64 --enable-uefi-aarch64`
              # won't work.
              # pkgsCross.aarch64-multiplatform.gcc
              # This is in cache but produces an build error.
              # pkgsCross.aarch64-multiplatform.stdenv.cc
            ];
          };
          formatter = pkgs.nixpkgs-fmt;
        };
      };
    };


}
