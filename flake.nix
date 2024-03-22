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
  };

  outputs = inputs@{ self, nixpkgs, flake-parts }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      flake = { };
      # Don't artificially limit users at this point. If a build fails, they
      # will notice it soon enough.
      systems = nixpkgs.lib.systems.flakeExposed;
      perSystem = { config, pkgs, ... }:
        let
          keep-directory-diff = pkgs.callPackage ./nix/keep-directory-diff.nix { };
          limine = pkgs.callPackage ./nix/build.nix { inherit keep-directory-diff; };
        in
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

                # gcc toolchain (comes as default, here only for completness)
                binutils
                gcc
                gnumake

                # llvm toolchain (with TOOLCHAIN_FOR_TARGET=llvm)
                llvmPackages.bintools
                llvmPackages.clang
                llvmPackages.lld

                # Nix
                nixpkgs-fmt
              ];
            };
          };

          # `$ nix fmt`
          formatter = pkgs.nixpkgs-fmt;

          # `$ nix build .#<attr>`
          packages = {
            inherit limine;
            default = limine;
          };
        };
    };
}
