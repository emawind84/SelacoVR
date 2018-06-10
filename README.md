# OpenVR Doom with motion controls
Based on gz3doom (http://rotatingpenguin.com/gz3doom/)

Built/tested on WMR, but other VR setups should work.

This build exposes OpenVR controller input for definition (you will need to define the controls).

One hand (right by default) is tracked for the weapon. *You will need 3D weapons* I have included a 3d weapon pack in the release, which I have rescaled/moved to work correctly. I've only tested the original doom weapons.

You will also need a doom wad file. You need to start with the commandline:

gzdoom.exe +vr_mode 10 -file WeaponsForVR.pk3

I made the 3D weapons from a file called "st-models-complete.pk3" which I found somewhere on the internet. It wasn't completely compatible with this version of gzdoom, so I cut it down to just the weapons (which are). Hopefully the original authors won't mind me redistributing this!

# New console variables

openvr_rightHanded - set to 0 for left hand

openvr_drawControllers - mostly for debug, note textures aren't correct.

openvr_weaponRotate - A pitch to change how weapons sit in the hand.

openvr_scale - Number of doom units in a metre. gz3doom has this set to 27, but I found 30 more comfortable.




