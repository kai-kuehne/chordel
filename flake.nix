# perf cannot be installed here, because it is tied to the kernel.
# Install it on the system.

{
  description = "Chordel development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        isLinux = pkgs.stdenv.isLinux;

        # Runtime dependencies needed to build and run the app
        runtimeLibs = with pkgs; [
          SDL2
          SDL2_ttf
          cairo
          libusb1
          physfs
          portmidi
        ];

        # Development tools (conditionally adding Linux-only ones)
        devTools = with pkgs; [
          clang
          clang-tools
          cppcheck
          flawfinder
          python3
          zip
        ] ++ (if isLinux then [ pkgs.valgrind ] else []);

        libPath = pkgs.lib.makeLibraryPath runtimeLibs;
      in {
        devShells.default = pkgs.mkShell {
          buildInputs = runtimeLibs ++ devTools;
          nativeBuildInputs = [ pkgs.pkg-config ];

          shellHook = ''
            export LD_LIBRARY_PATH=${libPath}:$LD_LIBRARY_PATH

            echo
            echo "Chordel dev environment loaded (Nix)"
            echo "------------------------------------------------------------"
            echo " SDL2         : ${pkgs.SDL2.version}"
            echo " SDL2_ttf     : ${pkgs.SDL2_ttf.version}"
            echo " cairo        : ${pkgs.cairo.version}"
            echo " libusb1      : ${pkgs.libusb1.version}"
            echo " portmidi     : ${pkgs.portmidi.version}"
            echo " physfs       : ${pkgs.physfs.version}"
            echo " clang        : ${pkgs.clang.version}"
            echo " clangd       : ${pkgs.clang-tools.version}"
            echo " cppcheck     : ${pkgs.cppcheck.version}"
            echo " flawfinder   : ${pkgs.flawfinder.version}"
          '' + (if isLinux then ''
            echo " valgrind     : ${pkgs.valgrind.version}"
          '' else "") + ''
            echo "------------------------------------------------------------"
          '';
        };
      }
    );
}
