# GZDoom-VR 
Based on gz3doom (http://rotatingpenguin.com/gz3doom/) and OpenVr Doom https://github.com/Fishbiter/gz3doom.
The aim of this fork is to update base GZDoom version.

Built/tested on Oculus Quest, but other VR setups should work.

This build exposes OpenVR controller input for definition (you will need to define the controls).

One hand (right by default) is tracked for the weapon. *You will need 3D weapons* 
I have included two modified weapon packs authored by Fishbiter.

# Not done

No comfort options/teleporting etc. I am not intend to implement these.

# New console variables

openvr_rightHanded - set to 0 for left hand

openvr_drawControllers - mostly for debug, note textures aren't correct.

openvr_weaponRotate - A pitch to change how weapons sit in the hand.

openvr_scale - Number of doom units in a metre. gz3doom has this set to 27, but I found 30 more comfortable.

### Licensed under the GPL v3
##### https://www.gnu.org/licenses/quick-guide-gplv3.en.html
---



