{ ansi
, argc
, fd
, lib
, writeShellScriptBin
}:

writeShellScriptBin "keep-directory-diff" ''
  # The following @-annotations belong to https://github.com/sigoden/argc
  #
  # @describe
  # This script takes two directories. The first directory is the base source.
  # The second is the base source plus potentially additional or modified
  # sources. It removes all files from the second directory that are unchanged
  # in the first directory. The use case is to get the actual difference that
  # preparation scripts of a source tree cause, such as through downloading
  # certain resources into the tree.
  #
  # Only file content is checked, not file attributes. Symlinks are ignored.
  #
  # @arg base! Base directory
  # Path to source directory
  # @arg target! Target directory
  # Path to target directory. This directory is modified.

  # Bash strict mode.
  set -eou pipefail

  export PATH="${lib.makeBinPath([
      ansi
      argc
      fd
    ])
  }:$PATH"

  # Do the "argc" magic. Reference: https://github.com/sigoden/argc
  eval "$(argc --argc-eval "$0" "$@")"

  # Find directories and regular files, don't follow symlinks.
  readarray -d "" BASE_FILES < <(cd "$argc_base" && fd --unrestricted --print0 --type file)

  for FILE in "''${BASE_FILES[@]}"; do
      # Check if the content of the file was changed.
      TARGET_FILE=$(realpath "$argc_target/$FILE" --relative-to=$PWD)
      FILE=$(realpath "$argc_base/$FILE" --relative-to=$PWD)
      echo -e "base file  : $(ansi bold)$FILE$(ansi reset)"
      echo -e "target file: $(ansi bold)$TARGET_FILE$(ansi reset)"
      if cmp -s "$FILE" "$TARGET_FILE"; then
          echo -e "Removing $(ansi bold)$TARGET_FILE$(ansi reset) as it hasn't changed."
          rm -f "$TARGET_FILE"
      else
          echo -e "Keeping $(ansi bold)$TARGET_FILE$(ansi reset) as it was modified"
      fi
  done
''
