yeetmouseOverlay:
{ pkgs, config, lib, ... }:

with lib;
let
  cfg = config.hardware.yeetmouse;

  accelerationModeValues = [ "linear" "power" "classic" "motivity" "jump" "lut" ];

  modeValueToInt = modeValue:
    (lists.findFirstIndex
      (value: modeValue == value) 0 accelerationModeValues) + 1;

  parametersType = let
    degToRad = 0.017453292;
    floatRange = lower: upper: addCheck types.float (x: x >= lower && x <= upper);
  in types.submodule {
    options = {
      AccelerationMode = mkOption {
        type = types.enum accelerationModeValues;
        default = "linear";
        apply = x: toString (modeValueToInt x);
        description = "Sets the algorithm to be used for acceleration";
      };
      InputCap = mkOption {
        type = nullOr (floatRange 0.0 200.0);
        default = null;
        apply = x: if x != null then toString x else "0";
        description = "Limit the maximum pointer speed before applying acceleration";
      };
      Sensitivity = mkOption {
        type = floatRange 0.01 10.0;
        default = 1.0;
        apply = toString;
        description = "Mouse base sensitivity";
      };
      Acceleration = mkOption {
        type = types.float;
        default = floatRange 0.01 10.0; # NOTE: Differs between modes
        apply = toString;
        description = "Acceleration";
      };
      OutputCap = mkOption {
        type = nullOr (floatRange 0.0 100.0);
        default = null;
        apply = x: if x != null then toString x else "0";
        description = "Cap maximum sensitivity.";
      };
      Offset = mkOption {
        type = nullOr (floatRange -50.0 50.0);
        default = 0.0;
        apply = toString;
        description = "Acceleration curve offset";
      };
      Exponent = mkOption {
        type = floatRange 0.01 1.0;
        default = 0.2;
        apply = toString;
        description = "Exponent for algorithms that use it";
      };
      Midpoint = mkOption {
        type = floatRange 0.05 50.0;
        default = 6.0;
        apply = toString;
        description = "Midpoint for sigmoid function";
      };
      PreScale = mkOption {
        type = floatRange 0.01 10.0;
        default = 1.0;
        apply = toString;
        description = "Parameter to adjust for DPI";
      };
      RotationAngle = mkOption {
        type = floatRange -180.0 180.0;
        default = 0.0;
        apply = x: toString (x * deg2Rad);
        description = "Amount of clockwise rotation (in degrees)";
      };
      UseSmoothing = mkOption {
        type = types.bool;
        default = true;
        apply = x: if x then "1" else "0";
        description = "Whether to smooth out functions (only applies to Jump acceleration)";
      };
    };
  };

  yeetmouse = pkgs.yeetmouse.override {
    kernel = config.boot.kernelPackages.kernel;
  };
in {
  options.hardware.yeetmouse = {
    enable = mkOption {
      type = types.bool;
      default = false;
      description = "Enable yeetmouse kernel module to add configurable mouse acceleration";
    };

    parameters = mkOption {
      type = parametersType;
      default = { };
    };
  };

  config = mkIf cfg.enable {
    nixpkgs.overlays = [ yeetmouseOverlay ];

    boot.extraModulePackages = [ yeetmouse ];
    environment.systemPackages = [ yeetmouse ];
    services.udev = {
      extraRules = let
        echo = "${pkgs.coreutils}/bin/echo";
        yeetmouseConfig = with cfg.parameters; pkgs.writeShellScriptBin "yeetmouseConfig" ''
          ${echo} "${Acceleration}" > /sys/module/yeetmouse/parameters/Acceleration
          ${echo} "${Exponent}" > /sys/module/yeetmouse/parameters/Exponent
          ${echo} "${InputCap}" > /sys/module/yeetmouse/parameters/InputCap
          ${echo} "${Midpoint}" > /sys/module/yeetmouse/parameters/Midpoint
          ${echo} "${Offset}" > /sys/module/yeetmouse/parameters/Offset
          ${echo} "${OutputCap}" > /sys/module/yeetmouse/parameters/OutputCap
          ${echo} "${PreScale}" > /sys/module/yeetmouse/parameters/PreScale
          ${echo} "${RotationAngle}" > /sys/module/yeetmouse/parameters/RotationAngle
          ${echo} "${Sensitivity}" > /sys/module/yeetmouse/parameters/Sensitivity
          ${echo} "${AccelerationMode}" > /sys/module/yeetmouse/parameters/AccelerationMode
          ${echo} "${UseSmoothing}" > /sys/module/yeetmouse/parameters/UseSmoothing
          ${echo} "1" > /sys/module/yeetmouse/parameters/update
        '';
      in ''
        SUBSYSTEMS=="usb|input|hid", ATTRS{bInterfaceClass}=="03", ATTRS{bInterfaceSubClass}=="01", ATTRS{bInterfaceProtocol}=="02", ATTRS{bInterfaceNumber}=="00", RUN+="${yeetmouseConfig}/bin/yeetmouseConfig"
      '';
    };
  };
}
