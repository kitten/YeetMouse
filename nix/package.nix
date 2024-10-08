{ shortRev ? "dev" }:
pkgs @ {
  lib,
  bash,
  stdenv,
  coreutils,
  kernel ? pkgs.linuxPackages_latest.kernel,
  ...
}:

stdenv.mkDerivation rec {
  pname = "yeetmouse";
  version = shortRev;
  src = lib.fileset.toSource {
    root = ./..;
    fileset = ./..;
  };

  setSourceRoot = "export sourceRoot=$(pwd)/source";
  nativeBuildInputs = kernel.moduleBuildDependencies ++ [ pkgs.makeWrapper ];
  buildInputs = [ pkgs.glfw3 ];

  makeFlags = kernel.makeFlags ++ [
    "-C"
    "${kernel.dev}/lib/modules/${kernel.modDirVersion}/build"
    "M=$(sourceRoot)/driver"
  ];

  postUnpack = ''
    substituteInPlace source/driver/util.c \
      --replace "#define NUM_USAGES 32" "#define NUM_USAGES 90"
  '';

  preBuild = ''
    cp $sourceRoot/driver/config.sample.h $sourceRoot/driver/config.h
    substituteInPlace $sourceRoot/driver/config.h \
      --replace "#define BUFFER_SIZE 16" "#define BUFFER_SIZE 32"
  '';

  LD_LIBRARY_PATH = "/run/opengl-driver/lib:${lib.makeLibraryPath buildInputs}";

  postBuild = ''
    make "-j$NIX_BUILD_CORES" -C $sourceRoot/gui "M=$sourceRoot/gui" "LIBS=-lglfw -lGL"
  '';

  postInstall = ''
    install -Dm755 $sourceRoot/gui/YeetMouseGui $out/bin/yeetmouse
    install -D $src/install_files/udev/99-leetmouse.rules $out/lib/udev/rules.d/98-leetmouse.rules
    install -Dm755 $src/install_files/udev/leetmouse_bind $out/lib/udev/rules.d/leetmouse_bind
    substituteInPlace $out/lib/udev/rules.d/98-leetmouse.rules \
      --replace "PATH='/sbin:/bin:/usr/sbin:/usr/bin'" ""
    patchShebangs $out/lib/udev/rules.d/leetmouse_bind
    wrapProgram $out/lib/udev/rules.d/leetmouse_bind \
      --prefix PATH : ${lib.makeBinPath [ bash coreutils ]}
    substituteInPlace $out/lib/udev/rules.d/98-leetmouse.rules \
      --replace "leetmouse_bind" "$out/lib/udev/rules.d/leetmouse_bind"
  '';

  buildFlags = [ "modules" ];
  installFlags = [ "INSTALL_MOD_PATH=${placeholder "out"}" ];
  installTargets = [ "modules_install" ];

  meta.mainProgram = "yeetmouse";
}
