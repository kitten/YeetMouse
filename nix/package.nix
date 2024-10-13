{ shortRev ? "dev" }:
pkgs @ {
  lib,
  bash,
  stdenv,
  coreutils,
  writeShellScript,
  makeDesktopItem,
  kernel ? pkgs.linuxPackages.kernel,
  ...
}:

let
  mkPackage = overrides @ {
    kernel,
    ...
  }: (stdenv.mkDerivation rec {
    pname = "yeetmouse";
    version = shortRev;
    src = lib.fileset.toSource {
      root = ./..;
      fileset = ./..;
    };

    setSourceRoot = "export sourceRoot=$(pwd)/source";
    nativeBuildInputs = with pkgs; kernel.moduleBuildDependencies ++ [
      makeWrapper
      autoPatchelfHook
      copyDesktopItems
    ];
    buildInputs = [
      stdenv.cc.cc.lib
      pkgs.glfw3
    ];

    makeFlags = kernel.makeFlags ++ [
      "-C"
      "${kernel.dev}/lib/modules/${kernel.modDirVersion}/build"
      "M=$(sourceRoot)/driver"
    ];

    preBuild = ''
      cp $sourceRoot/driver/config.sample.h $sourceRoot/driver/config.h
    '';

    LD_LIBRARY_PATH = "/run/opengl-driver/lib:${lib.makeLibraryPath buildInputs}";

    postBuild = ''
      make "-j$NIX_BUILD_CORES" -C $sourceRoot/gui "M=$sourceRoot/gui" "LIBS=-lglfw -lGL"
    '';

    postInstall = ''
      install -Dm755 $sourceRoot/gui/YeetMouseGui $out/bin/yeetmouse
      # Renaming this to 98- so it takes precedence over `udev.extraRules`
      install -D $src/install_files/udev/99-leetmouse.rules $out/lib/udev/rules.d/98-leetmouse.rules
      install -Dm755 $src/install_files/udev/leetmouse_bind $out/lib/udev/rules.d/leetmouse_bind
      patchShebangs $out/lib/udev/rules.d/leetmouse_bind
      # This is set so the udev script can find the right binaries, however, it overrides the Nix-injected PATH
      substituteInPlace $out/lib/udev/rules.d/98-leetmouse.rules \
        --replace "PATH='/sbin:/bin:/usr/sbin:/usr/bin'" ""
      # Here we instead inject a PATH from Nix for what the script needs
      wrapProgram $out/lib/udev/rules.d/leetmouse_bind \
        --prefix PATH : ${lib.makeBinPath [ bash coreutils ]}
      # Nix complains if we don't use an absolute path here
      substituteInPlace $out/lib/udev/rules.d/98-leetmouse.rules \
        --replace "leetmouse_bind" "$out/lib/udev/rules.d/leetmouse_bind"
    '';

    buildFlags = [ "modules" ];
    installFlags = [ "INSTALL_MOD_PATH=${placeholder "out"}" ];
    installTargets = [ "modules_install" ];

    desktopItems = [
      (makeDesktopItem {
        name = pname;
        exec = writeShellScript "yeetmouse.sh" /*bash*/ ''
          if [ "$XDG_SESSION_TYPE" = "wayland" ]; then
            xhost +SI:localuser:root
            pkexec env DISPLAY="$DISPLAY" XAUTHORITY="$XAUTHORITY" "${pname}"
            xhost -SI:localuser:root
          else
            pkexec env DISPLAY="$DISPLAY" XAUTHORITY="$XAUTHORITY" "${pname}"
          fi
        '';
        type = "Application";
        desktopName = "Yeetmouse GUI";
        comment = "Yeetmouse Configuration Tool";
        categories = [
          "Settings"
          "HardwareSettings"
        ];
      })
    ];

    meta.mainProgram = "yeetmouse";
  }).overrideAttrs (prev: overrides);

  makeOverridable =
    f: origArgs:
    let
      origRes = f origArgs;
    in
    origRes // { override = newArgs: f (origArgs // newArgs); };
in
  makeOverridable mkPackage { inherit kernel; }
