# YeetMouse NixOS Flake

YeetMouse may be built using this folder's packaged Nix flake.
As the `flake.nix` file is in the `nix/` folder the flake is accessible under the `github:AndyFilter/YeetMouse?dir=nix` URL.

## NixOS Module Installation

You may install this flake by adding it to your NixOS configuration.
The following example shows the `inputs.yeetmouse` input and the
`yeetmouse.nixosModules.default` NixOS module being added to a system
in your `flake.nix` file.
    
```nix
{
  # Add this to your flake inputs
  # Note that `inputs.nixpkgs` assumes that you have an input called
  # `nixpkgs` and you might need to change it based on your `nixpkgs`
  # input's name.
  inputs.yeetmouse = {
    url = "github:AndyFilter/YeetMouse?dir=nix";
    inputs.nixpkgs.follows = "nixpkgs";
  };
  # <rest of your config> ...

  outputs = { nixpkgs, yeetmouse, ... }: {
    # This is an example of a NixOS system configuration
    nixosConfigurations.HOSTNAME = nixpkgs.lib.nixosSystem {
      system = "x86_64-linux";
      modules = [
        # Add the `yeetmouse` input's NixOS Module to your system's modules:
        yeetmouse.nixosModules.default
      ];
    };
 };
}
```

This will expose a new `hardware.yeetmouse` configuration option in your NixOS system's `config`,
which you can use to activate `yeetmouse`'s device driver, udev rules, and executable.

```nix
{
  hardware.yeetmouse = {
    enable = true;
    sensitivity = 1.0;
  };
}
```

Then, rebuild and switch into your new system (using `nixos-rebuild`). After a reboot, the
`yeetmouse` driver should be loaded for your connected mouse with the parameters you specified.

After restarting your system, to verify that the driver works start up the yeetmouse GUI:

```sh
sudo -E yeetmouse
```

## Manual Overlay Installation

Instead of adding the entire NixOS module, you may also manually add `pkgs.yeetmouse` as an
overlay to your system.

The following example also adds the `inputs.yeetmouse` input, but instad of addding the NixOS module
it adds the `yeetmouse.overlays.default` overlay.

```nix
{
  # Add this to your flake inputs
  # Note that `inputs.nixpkgs` assumes that you have an input called
  # `nixpkgs` and you might need to change it based on your `nixpkgs`
  # input's name.
  inputs.yeetmouse = {
    url = "github:AndyFilter/YeetMouse?dir=nix";
    inputs.nixpkgs.follows = "nixpkgs";
  };
  # <rest of your config> ...

  outputs = { nixpkgs, yeetmouse, ... }: {
    # This is an example of a NixOS system configuration
    nixosConfigurations.HOSTNAME = nixpkgs.lib.nixosSystem {
      system = "x86_64-linux";
      overlays = [
        # Add the `yeetmouse` input's overlay to your system's overlays:
        yeetmouse.overlays.default
      ];
    };
 };
}
```

This will only add a `pkgs.yeetmouse` package to your NixOS system configuration, and all further setup
needs to be done manually. Check the `module.nix` if you're unsure of how to use the `yeetmouse` package
manually.

```nix
{ pkgs, config, ... }:

let
  yeetmouse = pkgs.yeetmouse.override { inherit (config.boot.kernelPackages) kernel; };
in {
  # This installs the yeetmouse kernel module:
  boot.extraModulePackages = [ yeetmouse ];
  # This installs the yeetmouse GUI CLI package:
  environment.systemPackages = [ yeetmouse ];
  services.udev = {
    # This installs the yeetmouse udev rules:
    packages = [ yeetmouse ];

    # You may define a custom udev rule to apply default settings to the yeetmouse driver:
    extraRules = let
      echo = "${pkgs.coreutils}/bin/echo";
      yeetmouseConfig = pkgs.writeShellScriptBin "yeetmouseConfig" ''
        ${echo} "1.0" > /sys/module/yeetmouse/parameters/Acceleration
        ${echo} "1" > /sys/module/yeetmouse/parameters/update
      '';
    in ''
      SUBSYSTEMS=="usb|input|hid", ATTRS{bInterfaceClass}=="03", ATTRS{bInterfaceSubClass}=="01", ATTRS{bInterfaceProtocol}=="02", ATTRS{bInterfaceNumber}=="00", RUN+="${yeetmouseConfig}/bin/yeetmouseConfig"
    '';
  };
}
```

Since the `pkgs.yeetmouse` package contains a kernel module, it'll be built against a specific version
of the Linux kernel. By default this will be `pkgs.linxPackages.kernel` - the default version in nixpkgs.
To override this, `.override { inherit (config.boot.kernelPackages) kernel; }` is called in the example
above to use the selected kernel for your NixOS system instead.

## Flake Builds

This flake exposes a `packages.${system}.yeetmouse` output, which allows you to build
and test the `yeetmouse` package locally:

```sh
nix build .#yeetmouse --json --keep-failed
```

## Configuration Options

The NixOS module exposes all configuration options that the Yeetmouse GUI exposes and aims to represent
them with the exact terminology that the GUI represents them in, and in the same structure they'd appear
in the GUI.

### Sensitivity and Anisotropy

Sensitivity may be specified as a single float value.

```nix
{
  hardware.yeetmouse = {
    enable = true;
    sensitivity = 1.0;
  };
}
```

But it may also be separated into separate sensitivity multiplies for the `x` and `y` axis to separate
the sensitivity multipliers into horizontal and vertical values ("Anisotropy").

```nix
{
  hardware.yeetmouse = {
    enable = true;
    sensitivity = {
      x = 1.0;
      y = 0.8;
    };
  };
}
```

### Global Options

The remaining global options on `hardware.yeetmouse` control the
- `inputCap` (the maximum input pointer speed that's passed to the acceleration function)
- `outputCap` (the maximum output pointer speed the acceleration function may produce)
- `offset` (a curve offset applied to the acceleration function's input)
- `preScale` (an input multiplier to adjust for Mouse DPI changes)
All of these options are set to no-op values by default. If they're not altered, they have no effect.

```nix
{
  hardware.yeetmouse = {
    enable = true;
    inputCap = 100.0;
    outputCap = 300.0;
    offset = -5.0;
    preScale = 0.5;
  };
}
```

### Mouse Rotation and Snapping Angles

A rotation angle may be applied to the pointer movement input.
This adjusts the input to account for movement while the mouse is held at an angle.

```nix
{
  hardware.yeetmouse = {
    enable = true;
    rotation.angle = 5.0; # in degrees
  };
}
```

Optionally, a snapping angle may be defined, which is an axis at an angle that the
mouse movement will "snap" to when under a threshold.

```nix
{
  hardware.yeetmouse = {
    enable = true;
    rotation = {
      snappingAngle = 45.0; # in degrees
      snappingThreshold = 2.0; # in degrees
    };
  };
}
```

### Acceleration Modes

The acceleration modes are defined as exclusive sub-options on `hardware.yeetmouse.mode`.

The `mode` option accepts any of the following mode options `linear`, `power`, `classic`, `motivity`, `jump`, and `lut`.
These options are mutually exclusive and only one must be specified at a time.

#### Linear

Simplest acceleration mode. Accelerates at a constant rate by multiplying acceleration.

```nix
{
  hardware.yeetmouse = {
    enable = true;
    mode.linear = {
      acceleration = 1.2;
    };
  };
}
```

See [RawAccel: Linear](https://github.com/RawAccelOfficial/rawaccel/blob/5b39bb6/doc/Guide.md#linear)

#### Power

Acceleration mode based on an exponent and multiplier as found in Source Engine games.

```nix
{
  hardware.yeetmouse = {
    enable = true;
    mode.power = {
      acceleration = 1.2;
      exponent = 0.2;
    };
  };
}
```

See [RawAccel: Power](https://github.com/RawAccelOfficial/rawaccel/blob/5b39bb6/doc/Guide.md#power)

#### Classic

Acceleration mode based on an exponent and multiplier as found in Quake 3.

```nix
{
  hardware.yeetmouse = {
    enable = true;
    mode.classic = {
      acceleration = 1.2;
      exponent = 0.2;
    };
  };
}
```

See [RawAccel: Classic](https://github.com/RawAccelOfficial/rawaccel/blob/5b39bb6/doc/Guide.md#power)

#### Motivity

Acceleration mode based on a sigmoid function with a set mid-point.

```nix
{
  hardware.yeetmouse = {
    enable = true;
    mode.motivity = {
      acceleration = 1.2;
      start = 10.0;
    };
  };
}
```

See [RawAccel: Motivity](https://github.com/RawAccelOfficial/rawaccel/blob/5b39bb6/doc/Guide.md#motivity)

#### Jump

Acceleration mode applying gain above a mid-point.

```nix
{
  hardware.yeetmouse = {
    enable = true;
    mode.jump = {
      acceleration = 2.0;
      midpoint = 5.0;
      smoothness = 0.2;
      useSmoothing = true;
    };
  };
}
```

The transition at the midpoint is smoothened by default, which may be disabled by setting `useSmoothing = false;`.
A smoothness is also applied to the whole sigmoid function which is controlled by `smoothness`.

See [RawAccel: Jump](https://github.com/RawAccelOfficial/rawaccel/blob/5b39bb6/doc/Guide.md#jump)

#### Look-up Table

Acceleration mode following a custom curve. The curve is specified using individual `[x, y]` points.

```nix
{
  hardware.yeetmouse = {
    enable = true;
    mode.lut = {
      # A list of points specified as [x y] tuples
      # NOTE: This is just an example and not a valid LUT
      data = [
        [1.1 1.2]
        [5.2 4.8]
        [10.0 10.0]
        [60.0 40.0]
      ];
    };
  };
}
```

See [RawAccel: Lookup Table](https://github.com/RawAccelOfficial/rawaccel/blob/5b39bb6/doc/Guide.md#look-up-table)
