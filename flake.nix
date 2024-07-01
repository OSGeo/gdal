{
  description = "GDAL";

  nixConfig = {
    bash-prompt = "\\[\\033[1m\\][gdal-dev]\\[\\033\[m\\]\\040\\w >\\040";
  };

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };

  outputs = inputs@{ flake-parts, ... }:
    flake-parts.lib.mkFlake { inherit inputs; } {

      systems = [ "x86_64-linux" ];

      perSystem = { config, self', inputs', pkgs, system, ... }: {

        packages = rec {
          gdal = pkgs.callPackage ./package.nix { };
          python-gdal = pkgs.python3.pkgs.toPythonModule (gdal);
        };

        devShells = rec {
          dev = pkgs.mkShell {
            inputsFrom = [ self'.packages.gdal ];

            # additional packages
            buildInputs = with pkgs.python3Packages; [
              pytest
            ];

            shellHook = ''
              function dev-help {
                echo -e "\nWelcome to a GDAL development environment !"
                echo "Build GDAL using following commands:"
                echo
                echo " 1.  cmake \\"
                echo "       -DCMAKE_INSTALL_PREFIX=$(pwd)/app \\"
                # echo "       -DCMAKE_C_FLAGS="-Werror" \\"
                # echo "       -DCMAKE_CXX_FLAGS=\"-Werror -DGDAL_BANDMAP_TYPE_CONST_SAFE\" \\"
                echo "       -DUSE_CCACHE=ON"
                echo " 2.  make -j$(nproc)"
                echo " 3.  make install"
                echo
                echo "Run tests:"
                echo
                echo " 1.  make quicktest"
                echo " 2.  make test"
                echo " 3.  TODO: pytest"
                echo
                echo "Note: run 'nix flake update' from time to time to update dependencies."
                echo
                echo "Run 'dev-help' to see this message again."
              }

              dev-help
            '';
          };

          user =
            let
              pythonEnv = pkgs.python3.withPackages (p: [
                self'.packages.python-gdal
              ]);
            in
            pkgs.mkShell {
              buildInputs = [
                pythonEnv
              ];

              shellHook = ''
                function dev-help {
                  echo -e "\nWelcome to a GDAL user environment !"
                  echo "Run GDAL CLI:"
                  echo
                  echo " 1.  gdalinfo --help"
                  echo " 2.  ogrinfo --help"
                  echo
                  echo "Run Python interpreter:"
                  echo
                  echo " 1.  python -c 'import osgeo; print(osgeo)'"
                  echo
                  echo "Note: run 'nix flake update' from time to time to update dependencies."
                  echo
                  echo "Run 'user-help' to see this message again."
                }

                dev-help
              '';
            };

          default = dev;
        };
      };

      flake = { };
    };
}
