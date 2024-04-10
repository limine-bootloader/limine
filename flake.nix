# This flake is supposed to enable a convenient development environment.
# It is independent of any packaging in nixpkgs.
#
# See https://github.com/limine-bootloader/limine/issues/330 for more details
# regarding the packaging in nixpkgs.

{
  description = "Limine";

  inputs = {
    flake-parts.url = "github:hercules-ci/flake-parts";
    flake-parts.inputs.nixpkgs-lib.follows = "nixpkgs";
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-23.11";
    nixpkgs-unstable.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };

  outputs = inputs@{ self, nixpkgs, nixpkgs-unstable, flake-parts }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      flake = { };
      # Don't artificially limit users at this point. If a build fails, they
      # will notice it soon enough.
      systems = nixpkgs.lib.systems.flakeExposed;
      perSystem = { config, pkgs, ... }:
        {
          devShells = {
            default = pkgs.mkShell {
              packages = with pkgs; [
                # Dependencies for ./bootstrap
                autoconf
                automake

                # General build dependencies
                cacert
                git
                mtools
                nasm
                pkg-config # Checked for by ./configure but seems unused?

                # gcc toolchain (comes as default, here only for completeness)
                binutils
                gcc
                gnumake

                # llvm toolchain (with TOOLCHAIN_FOR_TARGET=llvm)
                llvmPackages.bintools
                llvmPackages.clang
                llvmPackages.lld

                # Nix
                nixpkgs-fmt

                # Misc
                # typos is not yet frequently updated in the stable channel
                nixpkgs-unstable.legacyPackages.${pkgs.system}.typos
              ];
            };
          };

          # `$ nix fmt`
          formatter = pkgs.nixpkgs-fmt;
        };
    };
}
