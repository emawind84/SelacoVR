LOCAL_PATH := $(call my-dir)/../src


include $(CLEAR_VARS)

LOCAL_MODULE    := qzdoom

# https://developer.android.com/ndk/guides/android_mk#local_cflags
LOCAL_CFLAGS   := -D__MOBILE__ -DOPNMIDI_DISABLE_GX_EMULATOR -DGZDOOM -D__STDINT_LIMITS -DENGINE_NAME=\"gzdoom\"

# https://developer.android.com/ndk/guides/android_mk#local_cppflags
LOCAL_CPPFLAGS := -include g_pch.h -std=c++17 -Wno-switch -Wno-inconsistent-missing-override -Werror=format-security \
    -fexceptions -fpermissive -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp -D__forceinline=inline -DNO_GTK -DNO_SSE -fsigned-char

LOCAL_CFLAGS  += -DNO_SEND_STATS -DMINIZ_NO_STDIO -DUSE_OPENXR -DNO_SWRENDERER

LOCAL_CFLAGS  += -DOPNMIDI_USE_LEGACY_EMULATOR
LOCAL_CFLAGS  += -DADLMIDI_DISABLE_MUS_SUPPORT -DADLMIDI_DISABLE_XMI_SUPPORT -DADLMIDI_DISABLE_MIDI_SEQUENCER

ifeq ($(BUILD_SERIAL),1)
LOCAL_CPPFLAGS += -DANTI_HACK 
endif

	
LOCAL_C_INCLUDES := \
 $(TOP_DIR)/ \
 ${TOP_DIR}/OpenXR-SDK/include \
 ${TOP_DIR}/OpenXR-SDK/src/common \
		$(GZDOOM_TOP_PATH)/src/  \
		$(GZDOOM_TOP_PATH)/src/common/audio/sound \
    	$(GZDOOM_TOP_PATH)/src/common/audio/music \
    	$(GZDOOM_TOP_PATH)/src/common/2d \
    	$(GZDOOM_TOP_PATH)/src/common/thirdparty \
    	$(GZDOOM_TOP_PATH)/src/common/textures \
    	$(GZDOOM_TOP_PATH)/src/common/textures/formats \
    	$(GZDOOM_TOP_PATH)/src/common/textures/hires \
    	$(GZDOOM_TOP_PATH)/src/common/textures \
    	$(GZDOOM_TOP_PATH)/src/common/models \
    	$(GZDOOM_TOP_PATH)/src/common/filesystem/include \
    	$(GZDOOM_TOP_PATH)/src/common/utility \
    	$(GZDOOM_TOP_PATH)/src/common/cutscenes \
    	$(GZDOOM_TOP_PATH)/src/common/startscreen \
    	$(GZDOOM_TOP_PATH)/src/common/widgets \
    	$(GZDOOM_TOP_PATH)/src/common/console \
    	$(GZDOOM_TOP_PATH)/src/common/engine \
    	$(GZDOOM_TOP_PATH)/src/common/menu \
    	$(GZDOOM_TOP_PATH)/src/common/fonts \
    	$(GZDOOM_TOP_PATH)/src/common/objects \
    	$(GZDOOM_TOP_PATH)/src/common/rendering \
    	$(GZDOOM_TOP_PATH)/src/common/thirdparty/libsmackerdec/include \
    	$(GZDOOM_TOP_PATH)/src/common/thirdparty/stb \
    	$(GZDOOM_TOP_PATH)/src/common/thirdparty/utf8proc \
    	$(GZDOOM_TOP_PATH)/src/common/rendering/hwrenderer/data \
    	$(GZDOOM_TOP_PATH)/src/common/rendering/gl_load \
    	$(GZDOOM_TOP_PATH)/src/common/rendering/gl \
    	$(GZDOOM_TOP_PATH)/src/common/rendering/gles \
    	$(GZDOOM_TOP_PATH)/src/common/scripting/vm \
    	$(GZDOOM_TOP_PATH)/src/common/scripting/jit \
    	$(GZDOOM_TOP_PATH)/src/common/scripting/core \
    	$(GZDOOM_TOP_PATH)/src/common/scripting/interface \
    	$(GZDOOM_TOP_PATH)/src/common/scripting/frontend \
    	$(GZDOOM_TOP_PATH)/src/common/scripting/backend \
    	$(GZDOOM_TOP_PATH)/src/common/statusbar \
    	$(GZDOOM_TOP_PATH)/src/g_statusbar \
    	$(GZDOOM_TOP_PATH)/src/console \
    	$(GZDOOM_TOP_PATH)/src/playsim \
    	$(GZDOOM_TOP_PATH)/src/playsim/bots \
    	$(GZDOOM_TOP_PATH)/src/playsim/mapthinkers \
    	$(GZDOOM_TOP_PATH)/src/gamedata \
    	$(GZDOOM_TOP_PATH)/src/gamedata/textures \
    	$(GZDOOM_TOP_PATH)/src/gamedata/fonts \
    	$(GZDOOM_TOP_PATH)/src/rendering \
    	$(GZDOOM_TOP_PATH)/src/rendering/hwrenderer \
    	$(GZDOOM_TOP_PATH)/src/rendering/gl/stereo3d \
    	$(GZDOOM_TOP_PATH)/src/rendering/2d \
    	$(GZDOOM_TOP_PATH)/src/r_data \
    	$(GZDOOM_TOP_PATH)/src/sound \
    	$(GZDOOM_TOP_PATH)/src/sound/backend \
    	$(GZDOOM_TOP_PATH)/src/xlat \
    	$(GZDOOM_TOP_PATH)/src/utility \
    	$(GZDOOM_TOP_PATH)/src/menu \
    	$(GZDOOM_TOP_PATH)/src/utility/nodebuilder \
    	$(GZDOOM_TOP_PATH)/src/scripting \
    	$(GZDOOM_TOP_PATH)/src/scripting/zscript \
    	$(GZDOOM_TOP_PATH)/src/rendering \
    	$(GZDOOM_TOP_PATH)/src/../libraries/glslang/glslang/Public \
    	$(GZDOOM_TOP_PATH)/src/../libraries/glslang/spirv   	 \
    	$(GZDOOM_TOP_PATH)/src/common/platform/posix \
		$(GZDOOM_TOP_PATH)/src/posix/nosdl \
    	$(GZDOOM_TOP_PATH)/libraries/lzma/C \
        $(GZDOOM_TOP_PATH)/libraries/bzip2 \
        $(GZDOOM_TOP_PATH)/libraries/miniz \
        $(GZDOOM_TOP_PATH)/libraries/discordrpc/include \
\
 $(SUPPORT_LIBS)/openal/include/AL \
 $(SUPPORT_LIBS)/jpeg8d \
 $(SUPPORT_LIBS)/libvpx/include \
 $(SUPPORT_LIBS)/ZMusic/include  \
 $(GZDOOM_TOP_PATH)/mobile/src/extrafiles  \
 $(GZDOOM_TOP_PATH)/mobile/src


#############################################################################
# CLIENT/SERVER
#############################################################################


ANDROID_SRC_FILES = \
    ../mobile/src/i_specialpaths_android.cpp

PLAT_POSIX_SOURCES = \
	posix/i_steam.cpp \
	common/platform/posix/i_system_posix.cpp

PLAT_NOSDL_SOURCES = \
	posix/nosdl/crashcatcher.c \
	posix/nosdl/hardware.cpp \
	posix/nosdl/i_gui.cpp \
	posix/nosdl/i_joystick.cpp \
	posix/nosdl/i_system.cpp \
	posix/nosdl/i_input.cpp \
	posix/nosdl/glvideo.cpp \
	posix/nosdl/st_start.cpp

FASTMATH_SOURCES = \
	rendering/swrenderer/r_all.cpp \
	rendering/swrenderer/r_swscene.cpp \
	common/textures/hires/hqnx/init.cpp \
	common/textures/hires/hqnx/hq2x.cpp \
	common/textures/hires/hqnx/hq3x.cpp \
	common/textures/hires/hqnx/hq4x.cpp \
	common/textures/hires/xbr/xbrz.cpp \
	common/textures/hires/xbr/xbrz_old.cpp \
	common/rendering/gl_load/gl_load.c \
	rendering/hwrenderer/hw_dynlightdata.cpp \
	rendering/hwrenderer/scene/hw_bsp.cpp \
	rendering/hwrenderer/scene/hw_fakeflat.cpp \
	rendering/hwrenderer/scene/hw_decal.cpp \
	rendering/hwrenderer/scene/hw_drawinfo.cpp \
	rendering/hwrenderer/scene/hw_drawlist.cpp \
	rendering/hwrenderer/scene/hw_clipper.cpp \
	rendering/hwrenderer/scene/hw_flats.cpp \
	rendering/hwrenderer/scene/hw_portal.cpp \
	rendering/hwrenderer/scene/hw_renderhacks.cpp \
	rendering/hwrenderer/scene/hw_sky.cpp \
	rendering/hwrenderer/scene/hw_skyportal.cpp \
	rendering/hwrenderer/scene/hw_sprites.cpp \
	rendering/hwrenderer/scene/hw_spritelight.cpp \
	rendering/hwrenderer/scene/hw_walls.cpp \
	rendering/hwrenderer/scene/hw_walls_vertex.cpp \
	rendering/hwrenderer/scene/hw_weapon.cpp \
	common/utility/matrix.cpp \



PCH_SOURCES = \
	common/thirdparty/richpresence.cpp \
	am_map.cpp \
	playsim/bots/b_bot.cpp \
	playsim/bots/b_func.cpp \
	playsim/bots/b_game.cpp \
	playsim/bots/b_move.cpp \
	playsim/bots/b_think.cpp \
	bbannouncer.cpp \
	console/c_cmds.cpp \
	console/c_notifybuffer.cpp \
	console/c_functions.cpp \
	ct_chat.cpp \
	d_iwad.cpp \
	d_main.cpp \
	d_defcvars.cpp \
	d_anonstats.cpp \
	d_net.cpp \
	d_netinfo.cpp \
	d_protocol.cpp \
	doomstat.cpp \
	g_cvars.cpp \
	g_dumpinfo.cpp \
	g_game.cpp \
	g_hub.cpp \
	g_level.cpp \
	gameconfigfile.cpp \
	hu_scores.cpp \
	m_cheat.cpp \
	m_misc.cpp \
	playsim/p_acs.cpp \
	playsim/p_actionfunctions.cpp \
	p_conversation.cpp \
	playsim/p_destructible.cpp \
	playsim/p_effect.cpp \
	playsim/p_enemy.cpp \
	playsim/p_interaction.cpp \
	playsim/p_lnspec.cpp \
	playsim/p_map.cpp \
	playsim/p_maputl.cpp \
	playsim/p_mobj.cpp \
	p_openmap.cpp \
	playsim/p_pspr.cpp \
	p_saveg.cpp \
	p_setup.cpp \
	playsim/p_spec.cpp \
	p_states.cpp \
	playsim/p_things.cpp \
	p_tick.cpp \
	playsim/p_user.cpp \
	rendering/r_utility.cpp \
	rendering/r_sky.cpp \
	sound/s_advsound.cpp \
	sound/s_sndseq.cpp \
	sound/s_doomsound.cpp \
	serializer_doom.cpp \
	scriptutil.cpp \
	st_stuff.cpp \
	r_data/v_palette.cpp \
	wi_stuff.cpp \
	gamedata/a_keys.cpp \
	gamedata/a_weapons.cpp \
	gamedata/decallib.cpp \
	gamedata/g_mapinfo.cpp \
	gamedata/g_skill.cpp \
	gamedata/gi.cpp \
	gamedata/umapinfo.cpp \
	gamedata/d_dehacked.cpp \
	gamedata/g_doomedmap.cpp \
	gamedata/info.cpp \
	gamedata/keysections.cpp \
	gamedata/p_terrain.cpp \
	gamedata/statistics.cpp \
	gamedata/teaminfo.cpp \
	playsim/mapthinkers/a_decalfx.cpp \
	playsim/mapthinkers/a_doors.cpp \
	playsim/mapthinkers/a_lightning.cpp \
	playsim/mapthinkers/a_quake.cpp \
	playsim/mapthinkers/a_ceiling.cpp \
	playsim/mapthinkers/a_floor.cpp \
	playsim/mapthinkers/a_lights.cpp \
	playsim/mapthinkers/a_lighttransfer.cpp \
	playsim/mapthinkers/a_pillar.cpp \
	playsim/mapthinkers/a_plats.cpp \
	playsim/mapthinkers/a_pusher.cpp \
	playsim/mapthinkers/a_thruster.cpp \
	playsim/mapthinkers/a_scroll.cpp \
	playsim/mapthinkers/dsectoreffect.cpp \
	playsim/a_pickups.cpp \
	playsim/a_action.cpp \
	playsim/a_corona.cpp \
	playsim/a_decals.cpp \
	playsim/a_dynlight.cpp \
	playsim/a_flashfader.cpp \
	playsim/a_morph.cpp \
	playsim/a_specialspot.cpp \
	playsim/p_secnodes.cpp \
	playsim/p_sectors.cpp \
	playsim/p_sight.cpp \
	playsim/p_switch.cpp \
	playsim/p_tags.cpp \
	playsim/p_teleport.cpp \
	playsim/actorptrselect.cpp \
	playsim/dthinker.cpp \
	playsim/p_3dfloors.cpp \
	playsim/p_3dmidtex.cpp \
	playsim/p_linkedsectors.cpp \
	playsim/p_trace.cpp \
	playsim/po_man.cpp \
	playsim/portal.cpp \
	g_statusbar/hudmessages.cpp \
	g_statusbar/shared_hud.cpp \
	g_statusbar/sbarinfo.cpp \
	g_statusbar/sbar_mugshot.cpp \
	g_statusbar/shared_sbar.cpp \
	rendering/2d/v_blend.cpp \
	rendering/hwrenderer/hw_entrypoint.cpp \
	rendering/hwrenderer/hw_vertexbuilder.cpp \
	rendering/hwrenderer/doom_aabbtree.cpp \
	rendering/hwrenderer/doom_levelmesh.cpp \
	rendering/hwrenderer/hw_models.cpp \
	rendering/hwrenderer/hw_precache.cpp \
	rendering/hwrenderer/scene/hw_lighting.cpp \
	rendering/hwrenderer/scene/hw_drawlistadd.cpp \
	rendering/hwrenderer/scene/hw_setcolor.cpp \
	gl/stereo3d/gl_openxrdevice.cpp \
	maploader/edata.cpp \
	maploader/specials.cpp \
	maploader/maploader.cpp \
	maploader/slopes.cpp \
	maploader/glnodes.cpp \
	maploader/udmf.cpp \
	maploader/usdf.cpp \
	maploader/strifedialogue.cpp \
	maploader/polyobjects.cpp \
	maploader/renderinfo.cpp \
	maploader/compatibility.cpp \
	maploader/postprocessor.cpp \
	menu/doommenu.cpp \
	menu/loadsavemenu.cpp \
	menu/playermenu.cpp \
	menu/profiledef.cpp \
	gamedata/textures/animations.cpp \
	gamedata/textures/anim_switches.cpp \
	gamedata/textures/buildloader.cpp \
	gamedata/p_xlat.cpp \
	gamedata/xlat/parse_xlat.cpp \
	gamedata/xlat/parsecontext.cpp \
	playsim/fragglescript/t_func.cpp \
	playsim/fragglescript/t_load.cpp \
	playsim/fragglescript/t_oper.cpp \
	playsim/fragglescript/t_parse.cpp \
	playsim/fragglescript/t_prepro.cpp \
	playsim/fragglescript/t_script.cpp \
	playsim/fragglescript/t_spec.cpp \
	playsim/fragglescript/t_variable.cpp \
	playsim/fragglescript/t_cmd.cpp \
	intermission/intermission.cpp \
	intermission/intermission_parse.cpp \
	r_data/colormaps.cpp \
	r_data/gldefs.cpp \
	r_data/a_dynlightdata.cpp \
	r_data/r_translate.cpp \
	r_data/sprites.cpp \
	r_data/portalgroups.cpp \
	r_data/voxeldef.cpp \
	r_data/r_canvastexture.cpp \
	r_data/r_interpolate.cpp \
	r_data/r_vanillatrans.cpp \
	r_data/r_sections.cpp \
	r_data/models.cpp \
	scripting/vmiterators.cpp \
	scripting/vmthunks.cpp \
	scripting/vmthunks_actors.cpp \
	scripting/thingdef.cpp \
	scripting/thingdef_data.cpp \
	scripting/thingdef_properties.cpp \
	scripting/backend/codegen_doom.cpp \
	scripting/decorate/olddecorations.cpp \
	scripting/decorate/thingdef_exp.cpp \
	scripting/decorate/thingdef_parse.cpp \
	scripting/decorate/thingdef_states.cpp \
	scripting/zscript/zcc_compile_doom.cpp \
	rendering/swrenderer/textures/r_swtexture.cpp \
	rendering/swrenderer/textures/warptexture.cpp \
	rendering/swrenderer/textures/swcanvastexture.cpp \
	events.cpp \
	common/audio/sound/i_sound.cpp \
	common/audio/sound/oalsound.cpp \
	common/audio/sound/s_environment.cpp \
	common/audio/sound/s_sound.cpp \
	common/audio/sound/s_reverbedit.cpp \
	common/audio/music/music_midi_base.cpp \
	common/audio/music/music.cpp \
	common/audio/music/i_music.cpp \
	common/audio/music/i_soundfont.cpp \
	common/audio/music/music_config.cpp \
	common/2d/v_2ddrawer.cpp \
	common/2d/v_drawtext.cpp \
	common/2d/v_draw.cpp \
	common/2d/wipe.cpp \
	common/thirdparty/animlib.cpp \
	common/thirdparty/gain_analysis.cpp \
	common/thirdparty/sfmt/SFMT.cpp \
	common/startscreen/startscreen.cpp \
	common/startscreen/startscreen_heretic.cpp \
	common/startscreen/startscreen_hexen.cpp \
	common/startscreen/startscreen_strife.cpp \
	common/startscreen/startscreen_generic.cpp \
	common/startscreen/endoom.cpp \
	common/fonts/singlelumpfont.cpp \
	common/fonts/singlepicfont.cpp \
	common/fonts/specialfont.cpp \
	common/fonts/font.cpp \
	common/fonts/hexfont.cpp \
	common/fonts/v_font.cpp \
	common/fonts/v_text.cpp \
	common/textures/hw_ihwtexture.cpp \
	common/textures/hw_material.cpp \
	common/textures/bitmap.cpp \
	common/textures/m_png.cpp \
	common/textures/texture.cpp \
	common/textures/gametexture.cpp \
	common/textures/image.cpp \
	common/textures/imagetexture.cpp \
	common/textures/texturemanager.cpp \
	common/textures/multipatchtexturebuilder.cpp \
	common/textures/skyboxtexture.cpp \
	common/textures/animtexture.cpp \
	common/textures/v_collection.cpp \
	common/textures/formats/automaptexture.cpp \
	common/textures/formats/brightmaptexture.cpp \
	common/textures/formats/buildtexture.cpp \
	common/textures/formats/ddstexture.cpp \
	common/textures/formats/flattexture.cpp \
	common/textures/formats/fontchars.cpp \
	common/textures/formats/imgztexture.cpp \
	common/textures/formats/md5check.cpp \
	common/textures/formats/multipatchtexture.cpp \
	common/textures/formats/patchtexture.cpp \
	common/textures/formats/pcxtexture.cpp \
	common/textures/formats/pngtexture.cpp \
	common/textures/formats/rawpagetexture.cpp \
	common/textures/formats/startuptexture.cpp \
	common/textures/formats/emptytexture.cpp \
	common/textures/formats/shadertexture.cpp \
	common/textures/formats/tgatexture.cpp \
	common/textures/formats/stbtexture.cpp \
	common/textures/formats/anmtexture.cpp \
	common/textures/formats/startscreentexture.cpp \
	common/textures/formats/qoitexture.cpp \
	common/textures/formats/webptexture.cpp \
	common/textures/hires/hqresize.cpp \
	common/models/models_md3.cpp \
	common/models/models_md2.cpp \
	common/models/models_voxel.cpp \
	common/models/models_ue1.cpp \
	common/models/models_obj.cpp \
	common/models/models_iqm.cpp \
	common/models/model.cpp \
	common/models/voxels.cpp \
	common/console/c_commandline.cpp \
	common/console/c_buttons.cpp \
	common/console/c_bind.cpp \
	common/console/c_enginecmds.cpp \
	common/console/c_consolebuffer.cpp \
	common/console/c_cvars.cpp \
	common/console/c_dispatch.cpp \
	common/console/c_commandbuffer.cpp \
	common/console/c_console.cpp \
	common/console/c_notifybufferbase.cpp \
	common/console/c_tabcomplete.cpp \
	common/console/c_expr.cpp \
	common/cutscenes/playmve.cpp \
	common/cutscenes/movieplayer.cpp \
	common/cutscenes/screenjob.cpp \
	common/utility/engineerrors.cpp \
	common/utility/i_module.cpp \
	common/utility/gitinfo.cpp \
	common/utility/m_alloc.cpp \
	common/utility/utf8.cpp \
	common/utility/palette.cpp \
	common/utility/memarena.cpp \
	common/utility/cmdlib.cpp \
	common/utility/configfile.cpp \
	common/utility/i_time.cpp \
	common/utility/m_argv.cpp \
	common/utility/s_playlist.cpp \
	common/utility/name.cpp \
	common/utility/r_memory.cpp \
	common/utility/writezip.cpp \
	common/thirdparty/base64.cpp \
	common/thirdparty/md5.cpp \
 	common/thirdparty/superfasthash.cpp \
	common/thirdparty/libsmackerdec/src/BitReader.cpp \
	common/thirdparty/libsmackerdec/src/FileStream.cpp \
	common/thirdparty/libsmackerdec/src/HuffmanVLC.cpp \
	common/thirdparty/libsmackerdec/src/LogError.cpp \
	common/thirdparty/libsmackerdec/src/SmackerDecoder.cpp \
	common/engine/cycler.cpp \
	common/engine/d_event.cpp \
	common/engine/date.cpp \
	common/engine/stats.cpp \
	common/engine/sc_man.cpp \
	common/engine/palettecontainer.cpp \
	common/engine/stringtable.cpp \
	common/engine/i_net.cpp \
	common/engine/i_interface.cpp \
	common/engine/renderstyle.cpp \
	common/engine/v_colortables.cpp \
	common/engine/serializer.cpp \
	common/engine/m_joy.cpp \
	common/engine/m_random.cpp \
	common/objects/autosegs.cpp \
	common/objects/dobject.cpp \
	common/objects/dobjgc.cpp \
	common/objects/dobjtype.cpp \
	common/menu/joystickmenu.cpp \
	common/menu/menu.cpp \
	common/menu/messagebox.cpp \
	common/menu/optionmenu.cpp \
	common/menu/resolutionmenu.cpp \
	common/menu/menudef.cpp \
	common/menu/savegamemanager.cpp \
	common/statusbar/base_sbar.cpp \
	common/rendering/v_framebuffer.cpp \
	common/rendering/v_video.cpp \
	common/rendering/r_thread.cpp \
	common/rendering/r_videoscale.cpp \
	common/rendering/hwrenderer/hw_draw2d.cpp \
	common/rendering/hwrenderer/data/hw_clock.cpp \
	common/rendering/hwrenderer/data/hw_skydome.cpp \
	common/rendering/hwrenderer/data/flatvertices.cpp \
	common/rendering/hwrenderer/data/hw_viewpointbuffer.cpp \
	common/rendering/hwrenderer/data/hw_modelvertexbuffer.cpp \
	common/rendering/hwrenderer/data/hw_cvars.cpp \
	common/rendering/hwrenderer/data/hw_vrmodes.cpp \
	common/rendering/hwrenderer/data/hw_lightbuffer.cpp \
	common/rendering/hwrenderer/data/hw_bonebuffer.cpp \
	common/rendering/hwrenderer/data/hw_aabbtree.cpp \
	common/rendering/hwrenderer/data/hw_shadowmap.cpp \
	common/rendering/hwrenderer/data/hw_shaderpatcher.cpp \
	common/rendering/hwrenderer/postprocessing/hw_postprocessshader.cpp \
	common/rendering/hwrenderer/postprocessing/hw_postprocess.cpp \
	common/rendering/hwrenderer/postprocessing/hw_postprocess_cvars.cpp \
	common/rendering/hwrenderer/postprocessing/hw_postprocessshader_ccmds.cpp \
	common/rendering/gl_load/gl_interface.cpp \
	common/rendering/gl/gl_renderer.cpp \
	common/rendering/gl/gl_stereo3d.cpp \
	common/rendering/gl/gl_framebuffer.cpp \
	common/rendering/gl/gl_renderstate.cpp \
	common/rendering/gl/gl_renderbuffers.cpp \
	common/rendering/gl/gl_postprocess.cpp \
	common/rendering/gl/gl_postprocessstate.cpp \
	common/rendering/gl/gl_debug.cpp \
	common/rendering/gl/gl_buffers.cpp \
	common/rendering/gl/gl_hwtexture.cpp \
	common/rendering/gl/gl_samplers.cpp \
	common/rendering/gl/gl_shader.cpp \
	common/rendering/gl/gl_shaderprogram.cpp \
	common/scripting/core/maps.cpp \
	common/scripting/core/dictionary.cpp \
	common/scripting/core/dynarrays.cpp \
	common/scripting/core/symbols.cpp \
	common/scripting/core/types.cpp \
	common/scripting/core/scopebarrier.cpp \
	common/scripting/core/vmdisasm.cpp \
	common/scripting/core/imports.cpp \
	common/scripting/vm/vmexec.cpp \
	common/scripting/vm/vmframe.cpp \
	common/scripting/interface/stringformat.cpp \
	common/scripting/interface/vmnatives.cpp \
	common/scripting/frontend/ast.cpp \
	common/scripting/frontend/zcc_compile.cpp \
	common/scripting/frontend/zcc_parser.cpp \
	common/scripting/backend/vmbuilder.cpp \
	common/scripting/backend/codegen.cpp \
	utility/nodebuilder/nodebuild.cpp \
	utility/nodebuilder/nodebuild_classify_nosse2.cpp \
	utility/nodebuilder/nodebuild_events.cpp \
	utility/nodebuilder/nodebuild_extract.cpp \
	utility/nodebuilder/nodebuild_gl.cpp \
	utility/nodebuilder/nodebuild_utility.cpp \
	

QZDOOM_SRC = \
   ../../QzDoom/TBXR_Common.cpp \
   ../../QzDoom/QzDoom_OpenXR.cpp \
   ../../QzDoom/OpenXrInput.cpp \
   ../../QzDoom/VrInputCommon.cpp \
   ../../QzDoom/VrInputDefault.cpp \
   ../../QzDoom/mathlib.c \
   ../../QzDoom/matrixlib.c \
   ../../QzDoom/argtable3.c


LOCAL_SRC_FILES = \
	$(QZDOOM_SRC) \
	$(ANDROID_SRC_FILES) \
	$(PLAT_POSIX_SOURCES) \
	$(PLAT_NOSDL_SOURCES) \
	$(FASTMATH_SOURCES) \
	$(PCH_SOURCES) \
	common/utility/x86.cpp \
	common/thirdparty/strnatcmp.c \
	common/thirdparty/utf8proc/utf8proc.c \
	common/thirdparty/stb/stb_sprintf.c \
	common/utility/zstring.cpp \
	common/utility/findfile.cpp \
	common/thirdparty/math/asin.c \
	common/thirdparty/math/atan.c \
	common/thirdparty/math/const.c \
	common/thirdparty/math/cosh.c \
	common/thirdparty/math/exp.c \
	common/thirdparty/math/isnan.c \
	common/thirdparty/math/log.c \
	common/thirdparty/math/log10.c \
	common/thirdparty/math/mtherr.c \
	common/thirdparty/math/polevl.c \
	common/thirdparty/math/pow.c \
	common/thirdparty/math/powi.c \
	common/thirdparty/math/sin.c \
	common/thirdparty/math/sinh.c \
	common/thirdparty/math/sqrt.c \
	common/thirdparty/math/tan.c \
	common/thirdparty/math/tanh.c \
	common/thirdparty/math/fastsin.cpp \
	common/filesystem/source/filesystem.cpp \
	common/filesystem/source/ancientzip.cpp \
	common/filesystem/source/file_7z.cpp \
	common/filesystem/source/file_grp.cpp \
	common/filesystem/source/file_lump.cpp \
	common/filesystem/source/file_rff.cpp \
	common/filesystem/source/file_wad.cpp \
	common/filesystem/source/file_zip.cpp \
	common/filesystem/source/file_pak.cpp \
	common/filesystem/source/file_whres.cpp \
	common/filesystem/source/file_ssi.cpp \
	common/filesystem/source/file_hog.cpp \
	common/filesystem/source/file_mvl.cpp \
	common/filesystem/source/file_directory.cpp \
	common/filesystem/source/resourcefile.cpp \
	common/filesystem/source/files.cpp \
	common/filesystem/source/files_decompress.cpp \
	common/filesystem/source/fs_findfile.cpp \
	common/filesystem/source/fs_stringpool.cpp \
	common/filesystem/source/unicode.cpp \
	common/filesystem/source/critsec.cpp \
	../libraries/miniz/miniz.c \


# Turn down optimisation of this file so clang doesnt produce ldrd instructions which are missaligned
p_acs.cpp_CFLAGS := -O1

LOCAL_LDLIBS := -ldl -llog -lOpenSLES -landroid
LOCAL_LDLIBS += -lGLESv3

LOCAL_LDLIBS +=  -lEGL

# This is stop a linker warning for mp123 lib failing build
#LOCAL_LDLIBS += -Wl,--no-warn-shared-textrel

LOCAL_STATIC_LIBRARIES :=  lzma_gl3 bzip2_gl3 vpx_player webpmux
LOCAL_SHARED_LIBRARIES :=  openal openxr_loader zmusic

#Strip unused functions/data
LOCAL_CFLAGS += -fvisibility=hidden -fdata-sections -ffunction-sections  -fPIC
LOCAL_LDFLAGS += -Wl,--gc-sections -flto

include $(BUILD_SHARED_LIBRARY)

$(call import-module,AndroidPrebuilt/jni)


