# Building Limine with all features in Nix.
#
# Independent of the packing in nixpkgs, but convenient for prototyping and
# development.
#
# These derivations always builds limine from the current src tree and not some
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
, stdenvNoCC

  # Actual derivation dependencies.
, autoconf
, automake
, cacert
, git
, gnumake
, llvmPackages
, mtools
, nasm
, pkg-config
}:

let
  currentRepoSrc = nix-gitignore.gitignoreSource [
    # Prevent that the hash ov the sources change, when we change it in Nix.
    "flake.nix"
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
  bootstrappedSrcHash = "sha256-kuQGKvBWthSeOIQWqWfKoJKiqtgVUyUjr6oiqBLxils=";
  bootstrappedSrc = stdenvNoCC.mkDerivation {
    pname = "limine-bootstrapped";
    version = "0.0.0";
    src = currentRepoSrc;
    nativeBuildInputs = [
      cacert
      fd
      git
    ];
    buildPhase = ''
      runHook preBuild
      # This derivation only contains downloaded sources.
      bash ./bootstrap --skip-autoconf

      # When cloning, Git automatically creates hooks. Unfortunately, in a Nix
      # environment / on a NixOS system, this includes Nix store paths.
      # However, me must prevent to have any Nix store path inside the final
      # directory, as otherwise we get the error
      # "illegal path references in fixed-output derivation"! Further, we must
      # remove all git artifacts (.git dirs) as they affect the hash of the
      # derivation in a non-deterministic way.
      fd -u --type=d "^.git$" --min-depth=2 . --exec rm -rf {}

      # Remove all files that were not changed.
      #
      # Iterate through each file in current working dir
      # and delete everything from this derivation that already comes from
      # currentRepoSrc. This prevents hash pollution if the Git modules didn't
      # change.
      # TODO this needs to be done recursively, then it should be "perfect".
      shopt -s dotglob # also iterate hidden files
      for currentFile in ./*; do
          # Extract the file name from the full path
          filename=$(basename "$currentFile")
          # Check if the file exists in reference folder
          if [[ -e "${currentRepoSrc}/$filename" ]]; then
              # Check if the content of the file in Folder A is the same as in Folder B
              if cmp -s "$currentFile" "${currentRepoSrc}/$filename"; then
                  rm "$currentFile"
                  echo "Removed $filename"
              else
                  echo "Keeping $filename as it was modified by ./bootstrap"
              fi
          else
              echo "$filename was created by ./bootstrap"
          fi
      done

      # This should report nothing. Othewise, the Nix build will fail.
      # grep -r /nix/store .

      runHook postBuild
    '';
    dontPatchShebangs = true; # probalby unneeded
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

    gnumake
    mtools
    nasm
    pkg-config # Checked for by ./configure but seems unused?!
  ];
in
{
  inherit bootstrappedSrc;
  # Full build with all platforms.
  limine = lib.makeOverridable stdenvNoCC.mkDerivation {
    pname = "limine-dev";
    version = "0.0.0";
    src = bootstrappedSrc;
    nativeBuildInputs = commonBuildDeps ++ [
      llvmPackages.bintools
      llvmPackages.clang
      llvmPackages.lld
    ];
    configurePhase = ''
      runHook preConfigure

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

      echo Using clang:
      clang --version
      export TOOLCHAIN_FOR_TARGET=llvm
      ./configure --enable-all

      runHook postConfigure
    '';
    buildPhase = ''
      runHook preBuild

      make all -j $(nproc)

      runHook postBuild
    '';
    installPhase = ''
      runHook preInstall

      mkdir -p $out/bin
      cp -R bin/. $out

      ln -s $out/limine $out/bin/limine

      runHook postInstall
    '';
  };
}
