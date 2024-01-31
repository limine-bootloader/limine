# Building Limine with all features in Nix.
#
# Independent of the packing in nixpkgs, This is convenient for prototyping and
# local development.
#
# These derivations always builds Limine from the current src tree and not some
# stable release. Further, unlike the nixpkgs derivation, this derivation runs
# the ./bootstrap step which needs network access. Due to the nature of the
# self-hacked Git submodules download approach, packaging this project in Nix
# is especailly complicated. The complicated multi-derivation approach below
# is the best I can get after multiple hours of trying. :).

{
  # Helpers
  fd
, lib
, nix-gitignore
, stdenv

  # Actual derivation dependencies.
, autoconf
, automake
, cacert
, git
, llvmPackages
, mtools
, nasm
}:

let
  currentRepoSrc = nix-gitignore.gitignoreSource [
    # Additional git ignores:
    "flake.nix" # otherwise
    "flake.lock"
    "nix/"
  ] ../.;

  # Contains the sources downloaded by the Git submodule-like initialation done
  # in ./bootstrap.
  #
  # ALWAYS update the hash when one of the network dependencies in ./bootstrap
  # changes. Also, before updating, it is recommended to run "make clean"
  # beforehand.
  # bootstrappedSrcHash = lib.fakeHash;
  #
  # TODO: Unfortunately, currently this hash changes for almost every repository
  # change. We need to strip down this derivation further to only contain the
  # changed sources.
  bootstrappedSrcHash = "sha256-UU5pkdbaKXPs/i/hnuk4vZcxiag1cTsTCcn2LGzPuMs=";
  bootstrappedSrc = stdenvNoCC.mkDerivation {
    pname = "limine-bootstrapped";
    version = "0.0.0";
    src = currentRepoSrc;
    nativeBuildInputs = [
      cacert
      git
      fd
    ];
    buildPhase = ''
      runHook preBuild

      # `true` refers to the binary/bash-builtin to prevent any configuration
      # steps apart from downloading sources.
      AUTOMAKE=true AUTORECONF=true ./bootstrap

      # When cloning, Git automatically creates hooks. Unfortunately, in a Nix
      # environment / on a NixOS system, this includes Nix store paths.
      # However, me must prevent to have any Nix store path inside the final
      # directory, as otherwise we get the error
      # "illegal path references in fixed-output derivation"! Further, we must
      # remove all git artifacts (.git dirs) as they affect the hash of the
      # derivation in a non-deterministic way.
      fd -u --type=d "^.git$" --min-depth=2 . --exec rm -rf {}

      # This should report nothing. Othewise, the Nix build will fail.
      # grep -r /nix/store .

      runHook postBuild
    '';
    dontPatchShebangs = true;
    installPhase = ''
      runHook preInstall
      mkdir $out

      cp -r . $out

      runHook postInstall
    '';
    doCheck = false;
    dontFixup = true;
    # See "fixed output derivation".
    outputHashAlgo = "sha256";
    outputHashMode = "recursive";
    outputHash = bootstrappedSrcHash;
  };

  # Common build dependencies apart from the compiler toolchain.
  commonBuildDeps = [
    autoconf
    automake

    mtools
    nasm
  ];
in
stdenv.mkDerivation {
  pname = "limine-dev";
  version = "0.0.0";
  src = bootstrappedSrc;
  nativeBuildInputs = commonBuildDeps ++ [
    # gcc is used to build the host tools and clang to (cross)compile all
    # the bootloader files
    llvmPackages.bintools
    llvmPackages.clang
    llvmPackages.lld
  ];
  enableParallelBuilding = true;
  preConfigure = ''
    # The default input source of this derivation is what we aggregated
    # from `./bootstrap`. As this derivation holds all files but we are only
    # interested in the ones that are not in `currentRepoSrc`, we just
    # override all.
    #
    # This way we can use the actual current repo sources but still use the
    # populated sources from the ./bootstrap script.
    #
    # It's very complicated, I know. But that way we can make it work, at
    # least.
    cp -RTf ${currentRepoSrc} .

    # Extracted from ./bootstrap. To see why, check the `bootstrapedSrc`
    # derivation.
    #
    # TODO, we could also do this in ./bootstrap but add a special flag.
    autoreconf -fvi -Wall
  '';
  configureFlags = [ "--enable-all" ];
  outputs = [ "out" "doc" "dev" "man" ];
}
