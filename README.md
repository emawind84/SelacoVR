# Selaco VR port powered by QuestZDoom!

[![Build Status](https://github.com/emawind84/selacovr/actions/workflows/continuous_integration.yml/badge.svg?branch=main)](https://github.com/emawind84/gzdoom/actions/workflows/continuous_integration.yml)


## How to play

> **README**
>
> In the release page you will only find the engine, in order to play you need to own the game. 
> The game will default to VR mode but it can also be played in 2D mode by changing the cvar `vr_mode` to `0` or by going in `Stereo 3D VR` and changing the `3D mode` to `normal`.

1. Download the engine from https://github.com/emawind84/SelacoVR/releases
2. Extract the content where you want, possibly not in Program File where permissions are limited
3. Take the ipk3 file from the game folder and place it inside the engine folder
4. Run SelacoVR.bat or Selaco.exe


## Mods

You can place mods inside the mods folder and they will be loaded automatically.
If the auto load is not what you want, instead you prefer the gzdoom way of loading mods, by using a launcher or appending them to the executable, you can do so by appending the argument `-noautoload` to the executable or by changing the cvar `disableautoload` available under `Miscellaneous Options`.

> **README**
>
> If you decide to disable the autoload is important that you still load the mod `selaco-vr.pk3` since it fix errors and improve the experience in VR


## Controller Info

### Index Controllers
To get the most out of your Index Controllers, choose the Community Binding "Index Controller Bindings" by gameflorist in SteamVR. It makes the maximum buttons available for binding in GZDoom.

> **README**
>
> To access the game menu on PC the default button combination is `Main Grip` + `B` and it can be changed in game, if it doesn't work the game is using the wrong layout, go in `Customize Map Controls` and change the layout to one starting with `QZD` and don't forget to reset or update missing defaults.

#
Copyright (c) 1998-2023 ZDoom + GZDoom teams, and contributors

Doom Source (c) 1997 id Software, Raven Software, and contributors

Please see license files for individual contributor licenses

### Licensed under the GPL v3
##### https://www.gnu.org/licenses/quick-guide-gplv3.en.html
---



# Resources
- https://store.steampowered.com/app/1592280/Selaco/ - Selaco on Steam
- https://selacogame.com/ - About Selaco
- https://selacogame.com/#team - Altered Orbit Studios
Credits
-------

* [The ZDoom Teams](https://zdoom.org/index) - The team behind the engine this based upon.
* [Emile Belanger](http://www.beloko.com/) - The developer behind the android porting.
* [DrBeef & Teams](https://www.questzdoom.com) - For the awesome work behind the VR port for the Oculus Quest device
* [Altered Orbit Studios](https://selacogame.com/#team) - The team behind Selaco game