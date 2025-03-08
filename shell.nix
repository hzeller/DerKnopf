# This is a nix-shell for use with the nix package manager.
# If you have nix installed, you may simply run `nix-shell`
# in this repo, and have all dependencies ready in the new shell.

{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell {
  buildInputs = with pkgs;
    [
      # PCB tools
      kicad
      python3Packages.kicad  # Generate gerbers
      python3
      gerbv
      git     # to tag the pcb versions.
      zip     # pack gerbers

      # Firmware
      pkgsCross.avr.buildPackages.gcc9
      avrdude
    ];
}
