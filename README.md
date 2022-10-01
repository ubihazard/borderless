<p align="center"><img alt="BORDERless" src="icon/icon256.png"/></p>
<h1 align="center">BORDERless</h1>

<!--
![BORDERless](icon/icon256.png)

# BORDERless
-->

Hide and restore window borders and/or menu bar.

Download the [latest release](https://github.com/ubihazard/borderless/releases).

## Description

Some (legacy) applications show horrible ugly borders around window edges in full screen mode on Windows 10 (8? 8.1? 11?). This tiny utility consumes literally no system resources and helps to turn these borders off individually for each affected window and restore them back, if needed.

You can use this tool on regular (non-fullscreen) windows too, but depending on what kind of window it is, results sometimes can be unpredictable.

As a bonus feature BORDERless can also toggle window menu bars. This can be very handy to hide white menu bars in dark mode UI apps or anywhere else where menu bar feels annoying and/or undesirable.

Menu bar hidden in a dark mode app:

![Hidden menu](img/example.webp)

*Note that BORDERless can only hide standard Windows menu bars. If an application has a custom menu implemented through some graphical interface toolkit, BORDERless wouldn’t be able to affect it.*

## How to Use

BORDERless now works on active windows and uses Windows global hotkeys API to trigger its actions. The default shortcuts are <kbd>Alt+B</kbd> to toggle window borders and <kbd>Alt+M</kbd> to toggle menu.

Make sure the window you are trying to fix is focused and press the appropriate key combination for the desired effect. If a certain hotkey isn’t working, then it’s probably already in use by some other app running on your system.

It is possible to configure your own hotkeys:

![Configuring BORDERless](img/configure.png)

This window can be accessed from the system tray by clicking on BORDERless icon.

Run `install.bat` to create the Start Menu shortcut.

No bloat: BORDERless is written in pure C / WinAPI, has no bloated GUI dependencies, and consumes bare minimum of system resources (around a megabyte of RAM). So you can safely let it running in background.

*Note: some windows require you to <kbd>Alt-Tab</kbd> away and back to them after applying the fix in order to actually see the effect. That’s because Windows doesn’t bother to repaint them immediately. Doh.*

### Changing the Way Borders Are Hidden

Borders are hidden by applying window style masks. These masks can be modified by editing the configuration file `config` located in the program directory on lines 3-4. In order for this file to appear BORDERless needs to be run at least once.

By default, `0xcf0000` and `0x20301` values are used, which work best for hiding borders in fullscreen multimedia windows, but might cause graphical UI weirdness when applied to regular windows. For regular windows the values `0xcb0000` and `0x20300` are recommended instead.

## ⭐ Support

Making quality software is hard and time-consuming. If you find [BORDERless](https://github.com/ubihazard/borderless) useful, you can [buy me a ☕](https://www.buymeacoffee.com/ubihazard "Donate")!
