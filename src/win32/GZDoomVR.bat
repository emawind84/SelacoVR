REM This .bat file is for launching GZDoom in OpenVR virtual reality mode
REM Drag and Drop of .pk3 and PWAD files is supported
REM Change working folder to .bat location, so we can find gz3doom, even if a file from another folder is dragged onto this .bat
cd /d "%~dp0"
REM Place extra arguments first, since that seems to work better for brutal doom pk3 file.
gzdoom.exe %* ^
 +vid_vsync false ^
 +vr_mode 10 ^
 +turbo 80 ^
 +screenblocks 11 ^
 +crosshair 1 ^
 +crosshairscale 1 ^
 +use_joystick true ^
 +joy_xinput true ^
 +m_use_mouse false ^
 +smooth_mouse false
