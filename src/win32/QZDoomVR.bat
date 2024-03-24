REM This .bat file is for launching QuestZDoom in OpenVR virtual reality mode
REM Drag and Drop of .pk3 and PWAD files is supported
REM Change working folder to .bat location, so we can find the executable, even if a file from another folder is dragged onto this .bat
cd /d "%~dp0"
REM Place extra arguments first, since that seems to work better for brutal doom pk3 file.
qzdoom.exe %* ^
 +vid_preferbackend 0 ^
 +vid_vsync false ^
 +vr_mode 10 ^
 +turbo 80 ^
 +gl_max_lights=80000 ^
 +use_joystick true ^
 +joy_xinput true ^
 +m_use_mouse false ^
 +smooth_mouse false ^
 +vr_hud_fixed_pitch=true ^
 +vr_hud_fixed_roll=false ^
 +vr_automap_use_hud=true ^
 +vid_defheight=1400 ^
 +vid_defwidth=1400 ^
 +vid_scalemode=5 ^
 +dimamount=0.4 ^
 +gl_menu_blur=0.5