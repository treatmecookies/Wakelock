# Wakelock

A tiny, lightweight Windows notification-area application for keeping your system or display awake.

Wakelock is designed to stay out of your way and use as few resources as possible. It uses around **1.4 MB of memory (Windows 10)** while running and requires no background service or large framework.

## Usage

Wakelock runs in the notification area.

* **Left click** the tray icon to cycle between execution states.
* **Right click** to open the menu.

Available states:

* **Allow sleep**
* **Prevent sleep**
* **Keep display on**

You can also configure Wakelock to start with Windows or show a console for diagnostic logs.

Wakelock does **not modify Windows power plans or power settings**. It only uses Windows' execution-state API to request the desired power behavior.

## Building

Native C application for Windows. Supports LLVM/Clang and Microsoft Visual C++.

## License

See `LICENSE`.
