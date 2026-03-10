{ pkgs ? import <nixpkgs> { } }:

with pkgs;

mkShell rec {
  nativeBuildInputs = [
    pkg-config
  ];
  buildInputs = [
    renderdoc
    extra-cmake-modules
    vulkan-headers vulkan-loader vulkan-tools vulkan-validation-layers shader-slang
    libxkbcommon wayland wayland-scanner wayland-protocols xorg.libXinerama xorg.libX11 xorg.libXcursor xorg.libXi xorg.libXrandr xorg.libXxf86vm libGL libffi
    cloc
  ];
  LD_LIBRARY_PATH = lib.makeLibraryPath buildInputs;
}
