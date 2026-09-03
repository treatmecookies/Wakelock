# Wakelock

A tiny, lightweight Windows notification-area application for keeping your system or display awake.

Wakelock is designed to stay out of your way and use as few resources as possible. It uses around **1.6 MB of memory** while running and requires no background service or large framework.

## Usage

Wakelock runs in the notification area.

* **Left click** the tray icon to cycle between execution states.
* **Right click** the tray icon to open the menu.

Available states:

* **Allow sleep** — Windows can sleep normally.
* **Prevent sleep** — Prevents the system from sleeping while allowing the display to turn off.
* **Keep display on** — Prevents both the system and display from sleeping.

Wakelock can also be configured to start automatically with Windows.

Wakelock does **not** modify Windows power plans or power settings. It uses Windows' execution-state API to request the desired power behavior.

The selected execution state remains active while Wakelock is running, including when Windows is locked with **Win + L**.

When switching to another Windows user, the execution state does **not** carry over to the other user session. To keep the selected mode active, **Wakelock must be running in each user session**.

The context menu supports Windows dark mode.

## Building

Wakelock is a native C application for Windows and supports both **LLVM/Clang** and **Microsoft Visual C++ (MSVC)**.

The included `build.bat` script provides a simple way to build the application with LLVM/Clang.

### LLVM/Clang

```cmd
build.bat llvm
```

Additional compiler options can be passed after the `llvm` argument:

```cmd
build.bat llvm -fsanitize=address
```

### MSVC (C)

Requires a Visual Studio Developer Command Prompt.

```cmd
build.bat msvc
```

## License

See `LICENSE`.
