yeetmouseOverlay:
{ pkgs, config, lib, ... }:

with lib;
let
  cfg = config.hardware.yeetmouse;

  degToRad = x: x * 0.017453292;
  floatRange = lower: upper: types.addCheck types.float (x: x >= lower && x <= upper);
      apply = x: if x != null then x else 0.0;

  parameterBasePath = "/sys/module/yeetmouse/parameters";

  rotationType = types.submodule {
    description = "Adjusts for mouse rotational input and optionally applies an angle to snap to";
    options = {
      angle = mkOption {
        type = floatRange (-180.0) 180.0;
        default = 0.0;
        apply = degToRad;
        description = "Rotation adjustment to apply to mouse inputs (in degrees)";
      };

      snappingAngle = mkOption {
        type = floatRange 0.0 179.9;
        default = 0.0;
        apply = degToRad;
        description = "Rotation angle to snap to";
      };

      snappingThreshold = mkOption {
        type = floatRange 0.0 179.9;
        default = 0.0;
        apply = degToRad;
        description = "Threshold until applying snapping angle";
      };
    };
  };

  modesType = types.attrTag {
    linear = types.submodule {
      description = ''
        Simplest acceleration mode. Accelerates at a constant rate by multiplying acceleration.
        See [RawAccel: Linear](https://github.com/RawAccelOfficial/rawaccel/blob/5b39bb6/doc/Guide.md#linear)
      '';
      options = {
        acceleration = mkOption {
          type = floatRange 0.0005 1.0;
          default = 0.15;
          description = "Linear acceleration multiplier";
        };
      };
    };
    power = types.submodule {
      description = ''
        Acceleration mode based on an exponent and multiplier as found in Source Engine games.
        See [RawAccel: Power](https://github.com/RawAccelOfficial/rawaccel/blob/5b39bb6/doc/Guide.md#power)
      '';
      options = {
        acceleration = mkOption {
          type = floatRange 0.0005 5.0;
          default = 0.15;
          description = "Power acceleration pre-multiplier";
        };
        exponent = mkOption {
          type = floatRange 0.0005 1.0;
          default = 0.2;
          description = "Power acceleration exponent";
        };
      };
    };
    classic = types.submodule {
      description = ''
        Acceleration mode based on an exponent and multiplier as found in Quake 3.
        See [RawAccel: Classic](https://github.com/RawAccelOfficial/rawaccel/blob/5b39bb6/doc/Guide.md#power)
      '';
      options = {
        acceleration = mkOption {
          type = floatRange 0.0005 5.0;
          default = 0.15;
          apply = toString;
          description = "Classic acceleration pre-multiplier";
        };
        exponent = mkOption {
          type = floatRange 2.0 5.0;
          default = 2.0;
          apply = toString;
          description = "Classic acceleration exponent";
        };
      };
    };
    motivity = types.submodule {
      description = ''
        Acceleration mode based on a sigmoid function with a set mid-point.
        See [RawAccel: Motivity](https://github.com/RawAccelOfficial/rawaccel/blob/5b39bb6/doc/Guide.md#motivity)
      '';
      options = {
        acceleration = mkOption {
          type = floatRange 0.01 10.0;
          default = 0.15;
          apply = toString;
          description = "Motivity acceleration dividend";
        };
        start = mkOption {
          type = floatRange 0.1 50.0;
          default = 10.0;
          apply = toString;
          description = "Motivity acceleration mid-point";
        };
      };
    };
    jump = types.submodule {
      description = ''
        Acceleration mode applying gain above a mid-point.
        Optionally, the transition mid-point can be smoothened and a smoothness may be applied to the whole sigmoid function.
        See [RawAccel: Jump](https://github.com/RawAccelOfficial/rawaccel/blob/5b39bb6/doc/Guide.md#jump)
      '';
      options = {
        acceleration = mkOption {
          type = floatRange 0.01 10.0;
          default = 0.15;
          description = "Jump acceleration dividend";
        };
        midpoint = mkOption {
          type = floatRange 0.1 50.0;
          default = 0.15;
          description = "Jump acceleration mid-point";
        };
        smoothness = mkOption {
          type = floatRange 0.01 1.0;
          default = 0.2;
          description = "Jump curve smoothness (smoothness of the applied output curve)";
        };
        useSmoothing = mkOption {
          type = types.bool;
          default = true;
          description = "Enable Jump smoothing (whether the transition mid-point is smoothed out into the gain curve";
          apply = x: if x then "1" else "0";
        };
      };
    };
    lut = let
      tuple = ts: mkOptionType {
        name = "tuple";
        merge = mergeOneOption;
        check = xs: all id (zipListsWith (t: x: t.check x) ts xs);
        description = "tuple of" + concatMapStrings (t: " (${t.description})") ts;
      };
      lutVec = tuple [
        ((floatRange 0.0 100.0) // { description = "Input speed (x)"; })
        ((floatRange 0.0 100.0) // { description = "Output speed ratio (y)"; })
      ];
    in types.submodule {
      description = ''
        Acceleration mode following a custom curve.
        The curve is specified using individual `[x, y]` points.
        See [RawAccel: Lookup Table](https://github.com/RawAccelOfficial/rawaccel/blob/5b39bb6/doc/Guide.md#look-up-table)
      '';
      options = {
        data = mkOption {
          type = types.listOf lutVec;
          default = [];
          apply = ls: map (t: "${toString t[0]},${toString t[1]}") ls;
          description = "Lookup Table data (a list of `[x, y]` points)";
        };
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

    sensitivity = let
      sensitivityValue = floatRange 0.01 10.0;
      anisotropyValue = types.submodule {
        description = "Anisotropic sensitivity, separating X and Y movement";
        options = {
          x = mkOption {
            type = sensitivityValue;
            description = "Horizontal sensitivity";
          };
          y = mkOption {
            type = sensitivityValue;
            description = "Vertical sensitivity";
          };
        };
      };
    in mkOption {
      type = types.either sensitivityValue anisotropyValue;
      default = 1.0;
      description = "Mouse base sensitivity";
      apply = sens: [
        {
          value = if isAttrs sens then toString sens.x else toString sens;
          param = "Sensitivity";
        }
        {
          value = if isAttrs sens then toString sens.y else toString sens;
          param = "SensitivityY";
        }
      ];
    };

    inputCap = mkOption {
      type = types.nullOr (floatRange 0.0 200.0);
      default = null;
      description = "Limit the maximum pointer speed before applying acceleration";
      apply = x: {
        value = if x != null then toString x else "0";
        param = "InputCap";
      };
    };

    outputCap = mkOption {
      type = types.nullOr (floatRange 0.0 100.0);
      default = null;
      description = "Cap maximum sensitivity.";
      apply = x: {
        value = if x != null then toString x else "0";
        param = "OutputCap";
      };
    };

    offset = mkOption {
      type = types.nullOr (floatRange (-50.0) 50.0);
      default = 0.0;
      description = "Acceleration curve offset";
      apply = x: {
        value = toString x;
        param = "Offset";
      };
    };

    preScale = mkOption {
      type = floatRange 0.01 10.0;
      default = 1.0;
      description = "Parameter to adjust for DPI";
      apply = x: {
        value = toString x;
        param = "PreScale";
      };
    };

    rotation = mkOption {
      type = rotationType;
      default = { };
      description = "Adjust mouse rotation input and optionally apply a snapping angle";
      apply = x: [
        {
          value = toString x.angle;
          param = "RotationAngle";
        }
        {
          value = toString x.snappingAngle;
          param = "AngleSnap_Angle";
        }
        {
          value = toString x.snappingThreshold;
          param = "AngleSnap_Threshold";
        }
      ];
    };

    mode = mkOption {
      type = modesType;
      default = {
        linear = { };
      };
      description = "Acceleration mode to apply and their parameters";
      apply = params: []
        ++ (optionals (params ? linear) [
          {
            value = "1";
            param = "AccelerationMode";
          }
          {
            value = toString params.linear.acceleration;
            param = "Acceleration";
          }
        ])
        ++ (optionals (params ? power) [
          {
            value = "2";
            param = "AccelerationMode";
          }
          {
            value = toString params.power.acceleration;
            param = "Acceleration";
          }
          {
            value = toString params.power.exponent;
            param = "Exponent";
          }
        ])
        ++ (optionals (params ? classic) [
          {
            value = "3";
            param = "AccelerationMode";
          }
          {
            value = toString params.classic.acceleration;
            param = "Acceleration";
          }
          {
            value = toString params.classic.exponent;
            param = "Exponent";
          }
        ])
        ++ (optionals (params ? motivity) [
          {
            value = "4";
            param = "AccelerationMode";
          }
          {
            value = toString params.motivity.acceleration;
            param = "Acceleration";
          }
          {
            value = toString params.motivity.start;
            param = "Midpoint";
          }
        ])
        ++ (optionals (params ? jump) [
          {
            value = "5";
            param = "AccelerationMode";
          }
          {
            value = toString params.jump.acceleration;
            param = "Acceleration";
          }
          {
            value = toString params.jump.midpoint;
            param = "Midpoint";
          }
          {
            value = toString params.jump.smoothness;
            param = "Exponent";
          }
          {
            value = params.jump.useSmoothing;
            param = "UseSmoothing";
          }
        ])
        ++ (optionals (params ? lut) [
          {
            value = "6";
            param = "AccelerationMode";
          }
          {
            value = concatStringsSep ";" params.lut.data;
            param = "LutDataBuf";
          }
          {
            value = length params.lut.data;
            param = "LutSize";
          }
        ]);
    };
  };

  config = mkIf cfg.enable {
    nixpkgs.overlays = [ yeetmouseOverlay ];

    boot.extraModulePackages = [ yeetmouse ];
    environment.systemPackages = [ yeetmouse ];
    services.udev = {
      extraRules = let
        echo = "${pkgs.coreutils}/bin/echo";
        yeetmouseConfig = let
          globalParams = [ cfg.inputCap cfg.outputCap cfg.offset cfg.preScale ];
          params = globalParams ++ cfg.sensitivity ++ cfg.rotation ++ cfg.mode;
          paramToString = entry: ''
            ${echo} "${entry.value}" > "${parameterBasePath}/${entry.param}"
          '';
        in pkgs.writeShellScriptBin "yeetmouseConfig" ''
          ${concatMapStrings (s: (paramToString s) + "\n") params}
          ${echo} "1" > /sys/module/yeetmouse/parameters/update
        '';
      in ''
        SUBSYSTEMS=="usb|input|hid", ATTRS{bInterfaceClass}=="03", ATTRS{bInterfaceSubClass}=="01", ATTRS{bInterfaceProtocol}=="02", ATTRS{bInterfaceNumber}=="00", RUN+="${yeetmouseConfig}/bin/yeetmouseConfig"
      '';
    };
  };
}
