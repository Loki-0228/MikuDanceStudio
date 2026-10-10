cmake_minimum_required(VERSION 3.21)

project(MikuDanceStudio LANGUAGES CXX)

# ---------------------------------------------------------------------------
# MikuDanceStudio: behavioural restoration of MikuMikuDance v932.
# MikuMikuDanceE_v932x64 is the x64 reference.  The x86 binary remains a
# source-recovery aid only.
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MIKUDANCESTUDIO_ENABLE_DX12
    "Build optional DX12 skinning/native preview; OFF keeps the DX9/MME renderer with CPU skinning"
    ON)

if (MSVC)
    # Sources contain UTF-8 comments and literals; do not use the host code page.
    add_compile_options(/W3 /permissive- /utf-8)
    # Keep symbols for crash dumps without disabling Release optimization.
    # Embed compiler symbols in objects to avoid shared-PDB C1902 failures.
    add_compile_options($<$<CONFIG:Release>:/Z7>)
    add_link_options($<$<CONFIG:Release>:/DEBUG:FULL>
                     $<$<CONFIG:Release>:/OPT:REF>
                     $<$<CONFIG:Release>:/OPT:ICF>)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    # The original binary tolerates implicit narrowing everywhere; keep the
    # port close to VC9 behaviour rather than modern strictness.
    add_compile_options(/wd4267 /wd4244 /wd4245)
    # VC9 had no C++17 aligned operator new/delete (P0035).  Without this
    # switch our TUs allocate over-aligned Bullet objects (e.g. the 1456-byte
    # btGeneric6DofSpringConstraint, align 16) through _aligned_malloc while
    # the bullet275 static library's deleting destructors free them with the
    # plain operator delete - free() on an _aligned_malloc block, which
    # corrupts the heap and kills the process at teardown (0xC0000374,
    # see ShutdownCleanup -> DisposePhysicsWorld).  The original shipped
    # exactly the plain-new pairing, so this is also the faithful setting.
    add_compile_options(/Zc:alignedNew-)
    add_compile_options(/Zc:offsetof-)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()


if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(STATUS
        "Configuring the x64 build (reference: MikuMikuDanceE_v932x64, "
        "VC9 SSE2, d3dx9_43).")
endif()

# ---------------------------------------------------------------------------
# Output layout.  The per-configuration runtime directory is the shipped
# directory and must contain the program only, so build intermediates are
# redirected out of it:
#   <build>/Release            MikuMikuDance.exe (+ Data\ runtime assets)
#   <build>/lib/<config>       static libraries and import libraries
#   <build>/symbols/<config>   every PDB, for crash triage / debugging
# ---------------------------------------------------------------------------
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/symbols)
set(CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/symbols)
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/Release" CACHE PATH
        "MMS release package directory" FORCE)
endif()

if (NOT WIN32)
    message(FATAL_ERROR "MikuDanceStudio targets Windows only (Win32/DirectX9).")
endif()

# ---------------------------------------------------------------------------
# Dependencies (provisioned by Conan; see conanfile.py / recipes/)
# ---------------------------------------------------------------------------
find_package(BULLET275 REQUIRED)
find_package(Alembic CONFIG REQUIRED)
find_package(minhook CONFIG REQUIRED)

if (MIKUDANCESTUDIO_ENABLE_DX12)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/render/d3d12_skinning.hlsl" MMD_D3D12_SKINNING_HLSL)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS src/render/d3d12_skinning.hlsl)
    configure_file(src/render/d3d12_skinning_shader.hpp.in generated/d3d12_skinning_shader.hpp @ONLY)
endif()
add_library(mikudancestudio_graphics_runtime STATIC src/render/graphics_runtime.cpp)
target_include_directories(mikudancestudio_graphics_runtime PUBLIC include)

add_library(mikudancestudio_d3d12 STATIC src/render/d3d12_skinning.cpp)
target_include_directories(mikudancestudio_d3d12 PUBLIC include)
target_link_libraries(mikudancestudio_d3d12 PUBLIC mikudancestudio_graphics_runtime)
if (MIKUDANCESTUDIO_ENABLE_DX12)
    target_include_directories(mikudancestudio_d3d12 PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    # Optional runtime APIs must never become startup imports in the main EXE.
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/render/d3d12_native.hlsl" MMD_D3D12_NATIVE_HLSL)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/render/d3d12_sort.hlsl" MMD_D3D12_SORT_HLSL)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS src/render/d3d12_native.hlsl src/render/d3d12_sort.hlsl)
    configure_file(src/render/d3d12_native_shaders.hpp.in generated/d3d12_native_shaders.hpp @ONLY)
    target_compile_definitions(mikudancestudio_d3d12 PRIVATE MMD_ENABLE_DX12=1)
else()
    target_compile_definitions(mikudancestudio_d3d12 PRIVATE MMD_ENABLE_DX12=0)
endif()

# In-memory runtime diagnostics: no log file, no dump - a fatal fault renders
# the recent message ring into a modal report (see src/main/runtime_log.cpp).
# user32 supplies the MessageBoxW that keeps the process alive until the user
# acknowledges the report.
add_library(mikudancestudio_runtime_log STATIC src/main/runtime_log.cpp)
target_include_directories(mikudancestudio_runtime_log PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(mikudancestudio_runtime_log PUBLIC user32)

# ---------------------------------------------------------------------------
# Core static library: application state, math, startup chain, effect API.
# ---------------------------------------------------------------------------
add_library(mikudancestudio_animation_curve STATIC
    src/animation/animation_curve.cpp src/animation/animation_retime.cpp)
target_include_directories(mikudancestudio_animation_curve PUBLIC include)
add_library(mikudancestudio_curve_motion STATIC src/animation/animation_curve_motion.cpp)
target_include_directories(mikudancestudio_curve_motion PUBLIC include)
target_link_libraries(mikudancestudio_curve_motion PUBLIC mikudancestudio_animation_curve)
add_library(mikudancestudio_curve_editor STATIC src/window/animation_curve_editor.cpp)
target_include_directories(mikudancestudio_curve_editor PUBLIC include)
target_link_libraries(mikudancestudio_curve_editor PUBLIC mikudancestudio_animation_curve user32 gdi32 comctl32 gdiplus)
add_executable(animation_curve_tests EXCLUDE_FROM_ALL tests/animation_curve_tests.cpp)
target_link_libraries(animation_curve_tests PRIVATE mikudancestudio_animation_curve)
set_target_properties(animation_curve_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
enable_testing()

add_executable(native_shadow_tests EXCLUDE_FROM_ALL tests/native_shadow_tests.cpp)
target_include_directories(native_shadow_tests PRIVATE include)
target_link_libraries(native_shadow_tests PRIVATE d3d9 user32)
set_target_properties(native_shadow_tests PROPERTIES
    CXX_STANDARD 17 RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")

add_test(NAME native_shadow_tests COMMAND native_shadow_tests "${CMAKE_CURRENT_SOURCE_DIR}/res/assets/skin_effect_sm3.fx")
add_test(NAME animation_curve_tests COMMAND animation_curve_tests)
add_executable(animation_retime_tests EXCLUDE_FROM_ALL tests/animation_retime_tests.cpp)
target_link_libraries(animation_retime_tests PRIVATE mikudancestudio_animation_curve)
set_target_properties(animation_retime_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
add_test(NAME animation_retime_tests COMMAND animation_retime_tests)
add_executable(frame_rate_dialog_tests EXCLUDE_FROM_ALL
    tests/frame_rate_dialog_tests.cpp src/window/frame_rate_dialog.cpp src/app/app_ctor.cpp)
target_include_directories(frame_rate_dialog_tests PRIVATE include)
target_link_libraries(frame_rate_dialog_tests PRIVATE user32 gdi32 comctl32)
set_target_properties(frame_rate_dialog_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
add_test(NAME frame_rate_dialog_tests COMMAND frame_rate_dialog_tests)
add_executable(frame_rate_correction_tests EXCLUDE_FROM_ALL
    tests/frame_rate_correction_tests.cpp src/animation/frame_rate_correction.cpp src/app/app_ctor.cpp)
target_include_directories(frame_rate_correction_tests PRIVATE include)
target_link_libraries(frame_rate_correction_tests PRIVATE mikudancestudio_animation_curve user32 gdi32 comctl32)
set_target_properties(frame_rate_correction_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
add_test(NAME frame_rate_correction_tests COMMAND frame_rate_correction_tests)
add_executable(animation_curve_host_tests EXCLUDE_FROM_ALL tests/animation_curve_host_tests.cpp)
target_link_libraries(animation_curve_host_tests PRIVATE mikudancestudio_core winmm dsound)
set_target_properties(animation_curve_host_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
add_test(NAME animation_curve_host_tests COMMAND animation_curve_host_tests)
add_executable(animation_curve_editor_tests EXCLUDE_FROM_ALL tests/animation_curve_editor_tests.cpp)
target_link_libraries(animation_curve_editor_tests PRIVATE mikudancestudio_curve_editor)
set_target_properties(animation_curve_editor_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
add_test(NAME animation_curve_editor_tests COMMAND animation_curve_editor_tests)
add_executable(animation_curve_motion_tests EXCLUDE_FROM_ALL tests/animation_curve_motion_tests.cpp)
target_link_libraries(animation_curve_motion_tests PRIVATE mikudancestudio_curve_motion)
set_target_properties(animation_curve_motion_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
add_test(NAME animation_curve_motion_tests COMMAND animation_curve_motion_tests)
add_executable(curve_comparison_tests EXCLUDE_FROM_ALL tests/curve_comparison_tests.cpp)
target_link_libraries(curve_comparison_tests PRIVATE mikudancestudio_animation_curve)
set_target_properties(curve_comparison_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
add_test(NAME curve_comparison_tests COMMAND curve_comparison_tests)
add_executable(morph_curve_tests EXCLUDE_FROM_ALL tests/morph_curve_tests.cpp)
target_link_libraries(morph_curve_tests PRIVATE mikudancestudio_core winmm dsound)
set_target_properties(morph_curve_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
add_test(NAME morph_curve_tests COMMAND morph_curve_tests)
add_executable(native_frame_retime_tests EXCLUDE_FROM_ALL tests/native_frame_retime_tests.cpp)
target_link_libraries(native_frame_retime_tests PRIVATE mikudancestudio_core winmm dsound)
set_target_properties(native_frame_retime_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
add_test(NAME native_frame_retime_tests COMMAND native_frame_retime_tests)
add_executable(morph_curve_host_ui_tests EXCLUDE_FROM_ALL tests/morph_curve_host_ui_tests.cpp)
target_link_libraries(morph_curve_host_ui_tests PRIVATE mikudancestudio_core winmm dsound)
set_target_properties(morph_curve_host_ui_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
add_test(NAME morph_curve_host_ui_tests COMMAND morph_curve_host_ui_tests)
add_executable(bone_scale_curve_tests EXCLUDE_FROM_ALL tests/bone_scale_curve_tests.cpp)
target_link_libraries(bone_scale_curve_tests PRIVATE mikudancestudio_core winmm dsound)
set_target_properties(bone_scale_curve_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
add_test(NAME bone_scale_curve_tests COMMAND bone_scale_curve_tests)

add_library(mikudancestudio_geometry_workers STATIC src/model/geometry_workers.cpp)
target_include_directories(mikudancestudio_geometry_workers PUBLIC include)
if (MSVC)
    target_compile_options(mikudancestudio_geometry_workers PRIVATE /openmp)
    # VCOMP reads OMP_WAIT_POLICY during DLL load, before WinMain for a normal
    # import. Delay loading lets the host configure its default first. Publish
    # this to every core consumer, including the independent frame benchmark.
    target_link_options(mikudancestudio_geometry_workers INTERFACE /DELAYLOAD:vcomp140.dll)
    target_link_libraries(mikudancestudio_geometry_workers PUBLIC delayimp)
endif()

add_library(mikudancestudio_core STATIC
    src/animation/animation_curve_host.cpp
    src/animation/native_frame_retime.cpp
    src/animation/frame_rate_correction.cpp
    src/animation/morph_curve.cpp
    src/window/frame_rate_dialog.cpp
    src/window/animation_curve_host_ui.cpp
    src/render/native_mesh.cpp
    src/mmv4mms/host.cpp
    src/window/plugin_menu.cpp
    src/app/mmd_app.cpp
    src/app/globals.cpp
    src/app/app_ctor.cpp
    src/app/app_defaults.cpp
    src/main/charset_conv.cpp
    src/main/text_encoding.cpp
    src/main/model_name_translation.cpp
    src/window/unicode_window.cpp
    src/math/quat_to_mat3x4.cpp
    src/math/color_lerp.cpp
    src/math/kernel_gaps.cpp
    src/window/path_utils.cpp
    src/window/window_init.cpp
    src/window/ui_init.cpp
    src/window/ui_font.cpp
    src/window/wndproc.cpp
    src/window/localize_ui.cpp
    src/window/ui_translation.cpp
    src/window/ui_message.cpp
    src/window/wndproc_stubs.cpp
    src/window/command_dispatch.cpp
    src/window/frame_line_edit.cpp
    src/window/accessory_paste.cpp
    src/media/wave_audio.cpp
    src/media/native_media.cpp
    src/media/wic_mov_recorder.cpp
    src/render/transparent_capture.cpp
    src/media/media_load.cpp
    src/window/command_impl.cpp
    src/app/frame_driver.cpp
    src/app/playback_catchup.cpp
    src/app/playback_state.cpp
    src/app/timeline_advance.cpp
    src/app/frame_modes.cpp
    src/app/frame_modes_bone.cpp
    src/app/key_ladder.cpp
    src/app/keyboard_input.cpp
    src/app/pump_edit_keys.cpp
    src/app/pump_navigation.cpp
    src/physics/physics_create.cpp
    src/physics/physics_scale.cpp
    src/physics/physics_frame.cpp
    src/animation/channel_bake.cpp
    src/model/bone_key_pool.cpp
    src/physics/physics_bake.cpp
    src/physics/physics_bake_settings.cpp
    src/io/alembic_export.cpp
    src/io/mesh_bake_cache.cpp
    src/render/deformed_mesh_capture.cpp
    src/render/vertex_capture.cpp
    src/window/alembic_export_dialog.cpp
    src/physics/scene_create.cpp
    src/render/d3d_init.cpp
    src/render/default_effect.cpp
    src/render/processor_settings.cpp
    src/render/render_states.cpp
    src/render/stereo_nvapi.cpp
    src/render/frame_scene.cpp
    src/render/frame_presentation.cpp
    src/render/sprite_overlay.cpp
    src/render/viewport_cursor_style.cpp
    src/render/overlay_producers.cpp
    src/render/capture_downsample.cpp
    src/render/debug_geometry.cpp
    src/render/accessory.cpp
    src/render/model_renderers.cpp
    src/render/adaptive_shadow.cpp
    src/exports/effect_display.cpp
    src/render/transparent_geometry.cpp
    src/render/transparent_peeling.cpp
    src/render/bg_overlay.cpp
    src/render/device_reset.cpp
    src/app/avi_record_start.cpp
    src/app/record_readback.cpp
    src/app/dshow_record_graph.cpp
    src/app/mf_h264_recorder.cpp
    src/model/model_init.cpp
    src/model/path_resolve.cpp
    src/model/pmd_load.cpp
    src/model/bone_scale.cpp
    src/model/bone_transform.cpp
    src/model/model_keyframe_advance.cpp
    src/model/model_keyframe_edit.cpp
    src/model/bone_edit_undo.cpp
    src/model/model_frame_seek.cpp
    src/model/vmd_load.cpp
    src/model/track_apply.cpp
    src/model/key_registrars.cpp
    src/model/morph_apply.cpp
    src/model/model_skinning.cpp
    src/model/skinning_backend.cpp
    src/window/skinning_menu.cpp
    src/app/viewport_corner_settings.cpp
    src/window/viewport_corner_menu.cpp
    src/window/physics_bake_dialog.cpp
    src/model/pmx_load.cpp
    src/model/add_model.cpp
    src/model/model_query_gaps.cpp
    src/model/model_dispose.cpp
    src/model/bone_sort.cpp
    src/model/post_load_init.cpp
    src/render/fill_panel.cpp
    src/window/wm_paint.cpp
    src/window/ui_timeline_gfx.cpp
    src/window/ui_control_text.cpp
    src/window/ui_ctlcolor.cpp
    src/window/ui_edit_commit.cpp
    src/window/dialog_procs.cpp
    src/window/frame_range_apply.cpp
    src/io/pmm_save.cpp
    src/io/pmm_extension.cpp
    src/io/bone_scale_io.cpp
    src/io/pmm_project_settings.cpp
    src/io/io_gaps.cpp
    src/io/vpd_file.cpp
    src/io/vpd_scale_io.cpp
    src/io/accessory_motion_io.cpp
    src/io/vmd_save.cpp
    src/io/pmm_load_shell.cpp
    src/io/pmm_load_v2.cpp
    src/io/pmm_load_v1.cpp
    src/window/ui_scroll_mouse.cpp
    src/window/ui_panel_paint.cpp
    src/window/ui_refresh.cpp
    src/window/ui_notify.cpp
    src/window/ui_hscroll.cpp
    src/window/ui_dropfiles.cpp
    src/window/ui_panel_sweep.cpp
      src/window/ui_view_refresh.cpp
      src/window/ui_model_reload.cpp
      src/window/ui_selection_reeval.cpp
    src/window/ui_editor_click.cpp
    src/window/ui_frame_step.cpp
    src/window/ui_frame_refresh.cpp
    src/window/ui_mousemove.cpp
    src/window/ui_windowsize.cpp
    src/window/mic_window.cpp
    src/app/subsystem_init.cpp
    src/app/reset_app_state.cpp
    src/app/scene_ownership.cpp
    src/app/app_gap_bodies.cpp
    src/app/shutdown_cleanup.cpp
    src/app/oni_kinect.cpp
    src/app/oni_skeleton_pump.cpp
    src/io/vsq_load.cpp
    src/window/dialog_select_ops.cpp
    src/window/ui_viewport_refresh.cpp
    src/window/ui_viewport_layout.cpp
    src/window/ui_mouse_misc.cpp
    src/window/ui_palette.cpp
    src/window/command_file_menu.cpp
    src/window/command_view_menu.cpp
    src/window/physics_model_dialog.cpp
    src/window/model_edge_dialog.cpp
    src/window/enhance_model_io.cpp
    src/window/misc_dialogs.cpp
    src/window/command_control_400.cpp
    src/window/command_control_450.cpp
    src/window/command_control_500.cpp
    src/features/dialog_gaps.cpp
    src/features/query_audio_gaps.cpp
    src/features/subsystem_gaps.cpp
    src/render/draw_glyph.cpp
    src/render/toon_textures.cpp
    src/render/scene_font.cpp
    src/exports/effect_api.cpp
    src/exports/effect_names.cpp
    src/unported/stubs.cpp
)

option(MIKUDANCESTUDIO_PERF "Enable render-thread phase timing for benchmarks" OFF)
if (MIKUDANCESTUDIO_PERF)
    target_compile_definitions(mikudancestudio_core PUBLIC MIKUDANCESTUDIO_PERF=1)
endif()

target_include_directories(mikudancestudio_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# Pinned optional plugin ABI; the host does not build or link MMV4MMS.
target_include_directories(mikudancestudio_core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/sdk/mmv4mms/include")
# Bullet 2.75 selects its Windows SIMD alignment with WIN32, not _WIN32.
# Keep consumers ABI-compatible even when custom CMAKE_CXX_FLAGS replace
# CMake's default /DWIN32 (for example, AddressSanitizer builds).
target_compile_definitions(mikudancestudio_core PUBLIC WIN32)
target_sources(mikudancestudio_core PRIVATE src/render/d3d12_native.cpp)
if (MIKUDANCESTUDIO_ENABLE_DX12)
    target_include_directories(mikudancestudio_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_definitions(mikudancestudio_core PRIVATE MMD_ENABLE_DX12=1)
else()
    target_compile_definitions(mikudancestudio_core PRIVATE MMD_ENABLE_DX12=0)
endif()
target_link_libraries(mikudancestudio_core PUBLIC mikudancestudio_d3d12 mikudancestudio_curve_editor mikudancestudio_curve_motion)
target_link_libraries(mikudancestudio_core PUBLIC
    mikudancestudio_geometry_workers
    mikudancestudio_runtime_log
    BULLET275::BULLET275
    Alembic::Alembic
    minhook::minhook
    user32      # RegisterClass / CreateWindow / MessageBox
    gdi32       # CreateSolidBrush / GetStockObject
    msimg32     # GradientFill (0x40DF10 panel fill)
    comctl32    # InitCommonControls
    shell32     # DragAcceptFiles
    d3d9        # IDirect3DDevice9 (device slot at sub1d574+120032)
    vfw32       # DrawDibDraw (0x4168D0) + AVIFile*/AVIStream* (0x433250 background AVI)
    avifil32    # AVIFile/AVIStream background playback (0x4168D0)
    ole32       # CoInitialize/CoUninitialize (0x467313 / 0x4096C0)
    oleaut32    # VariantCopy/VariantClear/SysAllocString (encoder settings window)
    strmiids    # DirectShow IIDs/CLSIDs (recording graph 0x408F20..0x409A80)
    uuid        # IID_IPropertyBag / IID_IAMVfwCompressDialogs etc.
    mfplat      # Media Foundation H.264/AAC recording
    mfreadwrite # Media Foundation Sink Writer
    mfuuid      # Media Foundation encoder/media GUIDs
    windowscodecs # WIC PNG encoding for native MOV output
)
if (MSVC)
    # Align the PMX skinning stride workers with the original's VCOMP90
    # parallel regions (x64 _vcomp_fork, default static schedule).
    target_compile_options(mikudancestudio_core PRIVATE /openmp)
    target_compile_options(mikudancestudio_core PRIVATE /Zc:offsetof-)
endif()

add_executable(geometry_workers_tests EXCLUDE_FROM_ALL
    tests/geometry_workers_tests.cpp)
target_link_libraries(geometry_workers_tests PRIVATE mikudancestudio_geometry_workers)
set_target_properties(geometry_workers_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
if (MSVC)
    target_compile_options(geometry_workers_tests PRIVATE /openmp)
endif()
add_test(NAME geometry_workers_default COMMAND geometry_workers_tests default 12)
add_test(NAME geometry_workers_thread_limit COMMAND geometry_workers_tests default 3)
add_test(NAME geometry_workers_active_override COMMAND geometry_workers_tests ACTIVE 12)
add_test(NAME geometry_workers_passive_override COMMAND geometry_workers_tests PASSIVE 12)

# ---------------------------------------------------------------------------
# d3dx9_43 import stubs - MikuMikuEffect (MME) compatibility
# ---------------------------------------------------------------------------
# MME's MMHack.dll hooks the host by rewriting the HOST's import address
# table: it resolves each D3DX9 entry point with GetProcAddress, walks the
# host module's IMAGE_DIRECTORY_ENTRY_IAT and VirtualProtects every slot bound
# to that address.  The port resolves D3DX at runtime (include/.../d3dx_dyn.hpp),
# so its IAT held no such slot, MMHack's patch loop found nothing and MME
# aborted with "Initialize error" - the original MikuMikuDance links
# d3dx9_43.lib from the DirectX SDK and therefore exposes them.
#
# The import library is generated from res/imports/d3dx9_43.def (lib.exe), so
# the build still needs no DirectX SDK; d3dx_dyn.hpp then takes its pointers
# from those slots, which also lets MME's hooks intercept the port's D3DX
# calls.  x64 only: the x86 DLL exports stdcall-decorated names and the x86
# build keeps the runtime-resolution path.
# ---------------------------------------------------------------------------
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(MIKUDANCESTUDIO_D3DX_DEF ${CMAKE_CURRENT_SOURCE_DIR}/res/imports/d3dx9_43.def)
    set(MIKUDANCESTUDIO_D3DX_IMPLIB ${CMAKE_BINARY_DIR}/d3dx9_43.lib)
    add_custom_command(
        OUTPUT ${MIKUDANCESTUDIO_D3DX_IMPLIB}
        COMMAND ${CMAKE_AR} /nologo /machine:x64
                "/def:${MIKUDANCESTUDIO_D3DX_DEF}"
                "/out:${MIKUDANCESTUDIO_D3DX_IMPLIB}"
        DEPENDS ${MIKUDANCESTUDIO_D3DX_DEF}
        COMMENT "Generating the d3dx9_43 import library (MME compatibility)"
        VERBATIM)
    add_custom_target(mikudancestudio_d3dx_imports
                      DEPENDS ${MIKUDANCESTUDIO_D3DX_IMPLIB})
    target_compile_definitions(mikudancestudio_core
                               PUBLIC MIKUDANCESTUDIO_D3DX_STATIC_IMPORTS=1)
    target_link_libraries(mikudancestudio_core PUBLIC ${MIKUDANCESTUDIO_D3DX_IMPLIB})
    add_dependencies(mikudancestudio_core mikudancestudio_d3dx_imports)
endif()

# ---------------------------------------------------------------------------
# Porting-era diagnostics.  The MIKUDANCESTUDIO_* environment probes
# (state dumps, A/B traces, canaries) are comparison tooling against the
# original binaries, not product features.  Default OFF: the shipped build
# contains none of that code.  Turn ON only while running A/B sessions:
#   cmake -B build-x64/diag -DMIKUDANCESTUDIO_DIAG=ON ..
# ---------------------------------------------------------------------------
option(MIKUDANCESTUDIO_DIAG
       "Compile the porting-era diagnostics probes (MIKUDANCESTUDIO_* env dumps/traces)" OFF)
if (MIKUDANCESTUDIO_DIAG)
    target_compile_definitions(mikudancestudio_core PUBLIC MIKUDANCESTUDIO_DIAG=1)
endif()

# ---------------------------------------------------------------------------
# Tests.  Never part of a release artifact: each test executable is
# EXCLUDE_FROM_ALL, so neither the default build nor a release staging
# directory ever receives a test binary.
# ---------------------------------------------------------------------------
option(MIKUDANCESTUDIO_BUILD_INPUT_TESTS "Build keyboard and empty-model regression tests" OFF)
if (MIKUDANCESTUDIO_BUILD_INPUT_TESTS)
    enable_testing()
    # Build explicitly when needed:
    #   cmake --build build --config Release --target keyboard_input_tests
    add_executable(keyboard_input_tests EXCLUDE_FROM_ALL tests/keyboard_input_tests.cpp)
    target_link_libraries(keyboard_input_tests PRIVATE mikudancestudio_core winmm dsound)
    set_target_properties(keyboard_input_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME keyboard_input_regressions COMMAND keyboard_input_tests)
endif()

# Production-entry regression tests for the first repair batch.
add_executable(vmd_load_safety_tests EXCLUDE_FROM_ALL tests/vmd_load_safety_tests.cpp)
target_link_libraries(vmd_load_safety_tests PRIVATE mikudancestudio_core winmm dsound)
set_target_properties(vmd_load_safety_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
add_test(NAME vmd_load_safety COMMAND vmd_load_safety_tests)
add_executable(mmv4mms_physics_input_tests EXCLUDE_FROM_ALL
    tests/mmv4mms_physics_input_tests.cpp src/app/app_ctor.cpp)
target_include_directories(mmv4mms_physics_input_tests PRIVATE include sdk/mmv4mms/include)
target_link_libraries(mmv4mms_physics_input_tests PRIVATE BULLET275::BULLET275)
set_target_properties(mmv4mms_physics_input_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
add_test(NAME mmv4mms_physics_input COMMAND mmv4mms_physics_input_tests)

option(MIKUDANCESTUDIO_BUILD_EFFECT_TESTS "Build effect API regression tests" OFF)
if (MIKUDANCESTUDIO_BUILD_EFFECT_TESTS)
    enable_testing()
    add_executable(d3d12_interop_tests EXCLUDE_FROM_ALL tests/d3d12_interop_tests.cpp)
    add_executable(sequential_file_reader_tests EXCLUDE_FROM_ALL tests/sequential_file_reader_tests.cpp)
    target_include_directories(sequential_file_reader_tests PRIVATE include)
    set_target_properties(sequential_file_reader_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME sequential_file_reader_regressions COMMAND sequential_file_reader_tests)
    target_link_libraries(d3d12_interop_tests PRIVATE d3d9 d3d12 dxgi user32)
    set_target_properties(d3d12_interop_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_executable(d3dx_import_tests EXCLUDE_FROM_ALL tests/d3dx_import_tests.cpp)
    target_link_libraries(d3dx_import_tests PRIVATE mikudancestudio_core)
    set_target_properties(d3dx_import_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME d3dx_import_regressions COMMAND d3dx_import_tests)
    add_executable(effect_api_tests EXCLUDE_FROM_ALL tests/effect_api_tests.cpp)
    target_link_libraries(effect_api_tests PRIVATE mikudancestudio_core winmm dsound)
    # Keep the test away from any user-installed d3d9/MME proxy in Release.
    set_target_properties(effect_api_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME effect_api_regressions COMMAND effect_api_tests)
    add_executable(bone_projection_tests EXCLUDE_FROM_ALL tests/bone_projection_tests.cpp)
    target_link_libraries(bone_projection_tests PRIVATE mikudancestudio_core winmm dsound)
    set_target_properties(bone_projection_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME bone_projection_regressions COMMAND bone_projection_tests)
    add_executable(bone_overlay_tests EXCLUDE_FROM_ALL tests/bone_overlay_tests.cpp)
    target_link_libraries(bone_overlay_tests PRIVATE mikudancestudio_core winmm dsound)
    set_target_properties(bone_overlay_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME bone_overlay_regressions COMMAND bone_overlay_tests)
    add_executable(model_edge_tests EXCLUDE_FROM_ALL tests/model_edge_tests.cpp)
    target_link_libraries(model_edge_tests PRIVATE mikudancestudio_core winmm dsound)
    set_target_properties(model_edge_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME model_edge_regressions COMMAND model_edge_tests)
    add_executable(model_alpha_tests EXCLUDE_FROM_ALL tests/model_alpha_tests.cpp)
    target_include_directories(model_alpha_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(model_alpha_tests PRIVATE d3d9 user32)
    set_target_properties(model_alpha_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME model_alpha_regressions COMMAND model_alpha_tests)
    add_executable(model_alpha_occlusion_tests EXCLUDE_FROM_ALL tests/model_alpha_occlusion_tests.cpp)
    target_link_libraries(model_alpha_occlusion_tests PRIVATE mikudancestudio_core winmm dsound)
    set_target_properties(model_alpha_occlusion_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME model_alpha_occlusion_regressions COMMAND model_alpha_occlusion_tests)
    add_test(NAME model_alpha_intersection_regressions COMMAND model_alpha_occlusion_tests --intersections)
    add_test(NAME model_alpha_low_alpha_regressions COMMAND model_alpha_occlusion_tests --low-alpha)
    add_test(NAME model_alpha_peel_budget_regressions COMMAND model_alpha_occlusion_tests --peel-budget)
    add_test(NAME model_alpha_low_alpha_msaa_regressions COMMAND model_alpha_occlusion_tests --low-alpha --msaa)
    add_test(NAME model_alpha_intersection_msaa_regressions COMMAND model_alpha_occlusion_tests --intersections --msaa)
    add_test(NAME model_alpha_peel_budget_msaa_regressions COMMAND model_alpha_occlusion_tests --peel-budget --msaa)
    add_test(NAME model_alpha_maximum_limit_regressions COMMAND model_alpha_occlusion_tests --maximum-limit)
    add_test(NAME model_alpha_maximum_limit_msaa_regressions COMMAND model_alpha_occlusion_tests --maximum-limit --msaa)
    add_executable(capture_downsample_tests EXCLUDE_FROM_ALL tests/capture_downsample_tests.cpp)
    target_link_libraries(capture_downsample_tests PRIVATE mikudancestudio_core winmm dsound)
    set_target_properties(capture_downsample_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME capture_downsample_regressions COMMAND capture_downsample_tests)
    set_tests_properties(capture_downsample_regressions PROPERTIES TIMEOUT 20)

    foreach (physics_test physics_pose_tests pmx_joint_tests physics_bake_settings_tests physics_scale_tests affine_collision_shape_tests)
        add_executable(${physics_test} EXCLUDE_FROM_ALL tests/${physics_test}.cpp)
        target_link_libraries(${physics_test} PRIVATE mikudancestudio_core winmm dsound)
        set_target_properties(${physics_test} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
        add_test(NAME ${physics_test} COMMAND ${physics_test})
        set_tests_properties(${physics_test} PROPERTIES TIMEOUT 30)
    endforeach()
    add_executable(pmx_text_tests EXCLUDE_FROM_ALL tests/pmx_text_tests.cpp)
    target_link_libraries(pmx_text_tests PRIVATE mikudancestudio_core winmm dsound)
    set_target_properties(pmx_text_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME pmx_text_regressions COMMAND pmx_text_tests)
    set_tests_properties(pmx_text_regressions PROPERTIES TIMEOUT 20)
    add_executable(background_overlay_tests EXCLUDE_FROM_ALL tests/background_overlay_tests.cpp)
    target_link_libraries(background_overlay_tests PRIVATE mikudancestudio_core winmm dsound)
    set_target_properties(background_overlay_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME background_overlay_regressions COMMAND background_overlay_tests)
    add_executable(device_reset_tests EXCLUDE_FROM_ALL tests/device_reset_tests.cpp)
    target_link_libraries(device_reset_tests PRIVATE mikudancestudio_core winmm dsound)
    set_target_properties(device_reset_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME device_reset_regressions COMMAND device_reset_tests)
endif()

if (MIKUDANCESTUDIO_BUILD_EFFECT_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/native_bake_attributes_tests.cpp")
    add_executable(native_bake_attributes_tests EXCLUDE_FROM_ALL tests/native_bake_attributes_tests.cpp)
    target_link_libraries(native_bake_attributes_tests PRIVATE mikudancestudio_core Alembic::Alembic winmm dsound)
    set_target_properties(native_bake_attributes_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME native_bake_attributes COMMAND native_bake_attributes_tests "${CMAKE_BINARY_DIR}/native-bake-attributes-artifacts")
endif()

option(MIKUDANCESTUDIO_BUILD_ENCODING_TESTS "Build filename encoding regression tests" OFF)
if (MIKUDANCESTUDIO_BUILD_ENCODING_TESTS)
    enable_testing()
    add_executable(filename_encoding_tests EXCLUDE_FROM_ALL tests/filename_encoding_tests.cpp)
    target_link_libraries(filename_encoding_tests PRIVATE mikudancestudio_core winmm dsound)
    set_target_properties(filename_encoding_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME filename_encoding_regressions COMMAND filename_encoding_tests)
    add_executable(morph_import_tests EXCLUDE_FROM_ALL tests/morph_import_tests.cpp)
    target_link_libraries(morph_import_tests PRIVATE mikudancestudio_core winmm dsound)
    set_target_properties(morph_import_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME morph_import_regressions COMMAND morph_import_tests)
endif()

# ---------------------------------------------------------------------------
# User-acceptance regression tests: the defects reported by the field test
# round (UI language consistency, panel text, camera FOV, bone editing
# shortcuts, list selection, clipboard dialogs, PMM reload).
# ---------------------------------------------------------------------------
option(MIKUDANCESTUDIO_BUILD_UI_TESTS "Build user-acceptance regression tests" OFF)
if (MIKUDANCESTUDIO_BUILD_UI_TESTS)
    add_executable(native_media_tests EXCLUDE_FROM_ALL tests/native_media_tests.cpp
        src/media/native_media.cpp src/media/wic_mov_recorder.cpp)
    target_include_directories(native_media_tests PRIVATE include)
    target_link_libraries(native_media_tests PRIVATE mfplat mfreadwrite mfuuid ole32 oleaut32 windowscodecs)
    set_target_properties(native_media_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    enable_testing()
    foreach (ui_test ui_language_tests ui_message_tests panel_key_tests bone_registration_tests physics_bake_tests channel_bake_tests window_interaction_tests pmm_reload_tests morph_runtime_tests runtime_layout_tests)
        add_executable(${ui_test} EXCLUDE_FROM_ALL tests/${ui_test}.cpp)
        target_link_libraries(${ui_test} PRIVATE mikudancestudio_core winmm dsound)
        set_target_properties(${ui_test} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
        add_test(NAME ${ui_test} COMMAND ${ui_test})
    endforeach()
    add_executable(bone_key_pool_tests EXCLUDE_FROM_ALL tests/bone_key_pool_tests.cpp src/model/bone_key_pool.cpp)
    target_include_directories(bone_key_pool_tests PRIVATE include)
    set_target_properties(bone_key_pool_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME bone_key_pool COMMAND bone_key_pool_tests)
    set_tests_properties(bone_registration_tests PROPERTIES TIMEOUT 20)
endif()

# ---------------------------------------------------------------------------
# Main executable.  WIN32 subsystem matches the original PE header
# (Subsystem 2, GUI).  Exports (Exp*) come from exports/MikuMikuDance.def
# exactly like the original export table at RVA 0x00141328.
# ---------------------------------------------------------------------------
# Keep compiled dialog resources in sync with their editable templates.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    file(GLOB_RECURSE MIKUDANCESTUDIO_RESOURCE_INPUTS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/res/templates/*"
        "${CMAKE_CURRENT_SOURCE_DIR}/res/assets/*")
    add_custom_command(
        OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/res/gen_resources.res"
               "${CMAKE_CURRENT_SOURCE_DIR}/res/gen_resources_x64.res"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_resources.py" gen
        DEPENDS ${MIKUDANCESTUDIO_RESOURCE_INPUTS}
                scripts/gen_resources.py scripts/dialog_rc.py
                res/mmd_manifest.xml res/mmd_manifest_x64.xml
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Regenerate embedded dialog and menu resources"
        VERBATIM)
endif()
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    # x64 build embeds the original x64 manifest (amd64 dependencies) -
    # the x86 manifest's x86 assemblies
    # make the 64-bit loader fail with STATUS_INVALID_IMAGE_FORMAT.
    set(MIKUDANCESTUDIO_GEN_RES res/gen_resources_x64.res)
else()
    set(MIKUDANCESTUDIO_GEN_RES res/gen_resources.res)
endif()
if(TARGET physics_bake_tests)
    target_sources(physics_bake_tests PRIVATE res/MMD.rc ${MIKUDANCESTUDIO_GEN_RES})
    target_link_options(physics_bake_tests PRIVATE "/MANIFEST:NO"
        "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}")
endif()
if (MIKUDANCESTUDIO_BUILD_UI_TESTS)
    add_executable(menu_language_tests EXCLUDE_FROM_ALL
        tests/menu_language_tests.cpp res/MMD.rc ${MIKUDANCESTUDIO_GEN_RES})
    target_link_libraries(menu_language_tests PRIVATE mikudancestudio_core winmm dsound)
    target_link_options(menu_language_tests PRIVATE "/MANIFEST:NO"
        "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}")
    set_target_properties(menu_language_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME menu_language_regressions COMMAND menu_language_tests)
    add_executable(hidden_morph_tests EXCLUDE_FROM_ALL tests/hidden_morph_tests.cpp
        res/MMD.rc ${MIKUDANCESTUDIO_GEN_RES})
    target_link_libraries(hidden_morph_tests PRIVATE mikudancestudio_core winmm dsound)
    target_link_options(hidden_morph_tests PRIVATE "/MANIFEST:NO"
        "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}")
    set_target_properties(hidden_morph_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    # Requires the separately supplied Rou PMX sample; run explicitly with --model.
endif()
# Exercise the actual embedded model effects in the occlusion regression.
if(TARGET model_alpha_occlusion_tests)
    target_link_options(model_alpha_occlusion_tests PRIVATE "/MANIFEST:NO"
        "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}")
endif()
if (MIKUDANCESTUDIO_BUILD_EFFECT_TESTS)
    if (MIKUDANCESTUDIO_ENABLE_DX12)
        add_executable(graphics_runtime_tests EXCLUDE_FROM_ALL tests/graphics_runtime_tests.cpp)
        target_link_libraries(graphics_runtime_tests PRIVATE mikudancestudio_core winmm dsound)
        set_target_properties(graphics_runtime_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
        foreach(mode system missing-d3d12 missing-dxgi missing-compiler missing-device-export missing-factory2-export)
            add_test(NAME graphics_runtime_${mode} COMMAND graphics_runtime_tests ${mode})
            set_tests_properties(graphics_runtime_${mode} PROPERTIES LABELS "hardware-manual" TIMEOUT 60)
        endforeach()
    endif()
    add_executable(skinning_backend_tests EXCLUDE_FROM_ALL tests/skinning_backend_tests.cpp)
    target_link_libraries(skinning_backend_tests PRIVATE mikudancestudio_core winmm dsound)
    set_target_properties(skinning_backend_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_executable(processor_settings_tests EXCLUDE_FROM_ALL tests/processor_settings_tests.cpp)
    target_link_libraries(processor_settings_tests PRIVATE mikudancestudio_core winmm dsound)
    set_target_properties(processor_settings_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_executable(d3d12_skinning_tests EXCLUDE_FROM_ALL tests/d3d12_skinning_tests.cpp res/MMD.rc ${MIKUDANCESTUDIO_GEN_RES})
    target_link_libraries(d3d12_skinning_tests PRIVATE mikudancestudio_core mikudancestudio_d3d12 winmm dsound)
    target_link_options(d3d12_skinning_tests PRIVATE "/MANIFEST:NO" "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}")
    set_target_properties(d3d12_skinning_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
endif()

add_executable(MikuDanceStudio WIN32
    src/main/winmain.cpp
    res/MMD.rc
    ${MIKUDANCESTUDIO_GEN_RES}
    ${CMAKE_CURRENT_SOURCE_DIR}/exports/MikuMikuDance.def
)
add_custom_command(TARGET MikuDanceStudio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/licenses" "$<TARGET_FILE_DIR:MikuDanceStudio>/licenses")

if (MIKUDANCESTUDIO_BUILD_UI_TESTS)
    add_executable(affine_skinning_tests EXCLUDE_FROM_ALL tests/affine_skinning_tests.cpp)
    target_include_directories(affine_skinning_tests PRIVATE include)
    target_compile_features(affine_skinning_tests PRIVATE cxx_std_17)
    set_target_properties(affine_skinning_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME affine_skinning_tests COMMAND affine_skinning_tests)

    add_executable(bone_scale_io_tests EXCLUDE_FROM_ALL tests/bone_scale_io_tests.cpp)
    target_link_libraries(bone_scale_io_tests PRIVATE mikudancestudio_core winmm dsound)
    set_target_properties(bone_scale_io_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME bone_scale_io_tests COMMAND bone_scale_io_tests)

    add_executable(accessory_motion_io_tests EXCLUDE_FROM_ALL tests/accessory_motion_io_tests.cpp)
    target_link_libraries(accessory_motion_io_tests PRIVATE mikudancestudio_core winmm dsound)
    set_target_properties(accessory_motion_io_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME accessory_motion_io_tests COMMAND accessory_motion_io_tests)

    add_executable(vpd_scale_io_tests EXCLUDE_FROM_ALL tests/vpd_scale_io_tests.cpp src/io/vpd_scale_io.cpp)
    target_include_directories(vpd_scale_io_tests PRIVATE include)
    target_compile_features(vpd_scale_io_tests PRIVATE cxx_std_17)
    set_target_properties(vpd_scale_io_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME vpd_scale_io_tests COMMAND vpd_scale_io_tests)

    add_executable(viewport_corner_layout_tests EXCLUDE_FROM_ALL tests/viewport_corner_layout_tests.cpp)
    target_link_libraries(viewport_corner_layout_tests PRIVATE mikudancestudio_core winmm dsound)
    set_target_properties(viewport_corner_layout_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME viewport_corner_layout_tests COMMAND viewport_corner_layout_tests)

    add_executable(menu_draw_state_tests EXCLUDE_FROM_ALL tests/menu_draw_state_tests.cpp)
    target_link_libraries(menu_draw_state_tests PRIVATE mikudancestudio_core winmm dsound)
    set_target_properties(menu_draw_state_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME menu_draw_state_tests COMMAND menu_draw_state_tests)

    add_executable(bone_scale_tests EXCLUDE_FROM_ALL tests/bone_scale_tests.cpp)
    target_link_libraries(bone_scale_tests PRIVATE mikudancestudio_core winmm dsound)
    set_target_properties(bone_scale_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME bone_scale_tests COMMAND bone_scale_tests)

    # Viewport scale handles: hit testing, the merged mode button's cycle and
    # three-language captions, the panel grid and the handle drag paths.  CPU
    # only - no device, no window.
    add_executable(bone_scale_gizmo_tests EXCLUDE_FROM_ALL tests/bone_scale_gizmo_tests.cpp)
    target_link_libraries(bone_scale_gizmo_tests PRIVATE mikudancestudio_core winmm dsound)
    set_target_properties(bone_scale_gizmo_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME bone_scale_gizmo_tests COMMAND bone_scale_gizmo_tests)
    add_executable(native_editors_integration EXCLUDE_FROM_ALL tests/native_editors_integration.cpp res/MMD.rc ${MIKUDANCESTUDIO_GEN_RES} exports/MikuMikuDance.def)
    target_link_libraries(native_editors_integration PRIVATE mikudancestudio_core winmm dsound)
    target_link_options(native_editors_integration PRIVATE "/MANIFEST:NO" "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}")
    set_target_properties(native_editors_integration PROPERTIES OUTPUT_NAME MikuMikuDance ENABLE_EXPORTS ON RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/native-editors/$<CONFIG>")
    # Exercise the same PMM model-dialog resources that the application embeds.
    target_sources(ui_message_tests PRIVATE res/MMD.rc ${MIKUDANCESTUDIO_GEN_RES})
    add_test(NAME accessory_message_regressions COMMAND ui_message_tests accessory-load)
    set_tests_properties(accessory_message_regressions PROPERTIES TIMEOUT 10)
    target_link_options(ui_message_tests PRIVATE "/MANIFEST:NO"
        "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}")
    # The viewport operation axis and the coordinate-system caption both read
    # embedded resources (XFILE 115 axis.x, the HUD font atlas).
    add_executable(bone_scale_hud_tests EXCLUDE_FROM_ALL
        tests/bone_scale_hud_tests.cpp res/MMD.rc ${MIKUDANCESTUDIO_GEN_RES})
    target_link_libraries(bone_scale_hud_tests PRIVATE mikudancestudio_core winmm dsound)
    target_link_options(bone_scale_hud_tests PRIVATE "/MANIFEST:NO"
        "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}")
    set_target_properties(bone_scale_hud_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME bone_scale_hud_tests COMMAND bone_scale_hud_tests "${CMAKE_BINARY_DIR}/tests/bone-scale-hud")

    add_executable(coordinate_axis_tests EXCLUDE_FROM_ALL
        tests/coordinate_axis_tests.cpp res/MMD.rc ${MIKUDANCESTUDIO_GEN_RES})
    target_link_libraries(coordinate_axis_tests PRIVATE mikudancestudio_core winmm dsound)
    target_link_options(coordinate_axis_tests PRIVATE "/MANIFEST:NO"
        "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}")
    set_target_properties(coordinate_axis_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME coordinate_axis_regressions COMMAND coordinate_axis_tests)
    # Optional real-scene stress runner. No user assets are distributed with it.
    add_executable(physics_scene_tests EXCLUDE_FROM_ALL tests/physics_scene_tests.cpp
        res/MMD.rc ${MIKUDANCESTUDIO_GEN_RES})
    target_link_libraries(physics_scene_tests PRIVATE mikudancestudio_core winmm dsound)
    target_link_options(physics_scene_tests PRIVATE "/MANIFEST:NO"
        "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}")
    set_target_properties(physics_scene_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_executable(vertex_capture_tests EXCLUDE_FROM_ALL tests/vertex_capture_tests.cpp src/render/vertex_capture.cpp)
    target_include_directories(vertex_capture_tests PRIVATE include)
    target_compile_features(vertex_capture_tests PRIVATE cxx_std_17)
    target_link_libraries(vertex_capture_tests PRIVATE d3d9 d3dcompiler)
    set_target_properties(vertex_capture_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME vertex_capture COMMAND vertex_capture_tests)
    add_executable(mesh_bake_cache_tests EXCLUDE_FROM_ALL tests/mesh_bake_cache_tests.cpp
        src/io/mesh_bake_cache.cpp src/io/pmm_extension.cpp)
    target_include_directories(mesh_bake_cache_tests PRIVATE include)
    target_link_libraries(mesh_bake_cache_tests PRIVATE Alembic::Alembic)
    set_target_properties(mesh_bake_cache_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME mesh_bake_cache COMMAND mesh_bake_cache_tests "${CMAKE_BINARY_DIR}/mesh-cache-test-artifacts")
    add_executable(alembic_export_tests EXCLUDE_FROM_ALL tests/alembic_export_tests.cpp
        res/MMD.rc ${MIKUDANCESTUDIO_GEN_RES} exports/MikuMikuDance.def)
    target_link_libraries(alembic_export_tests PRIVATE mikudancestudio_core winmm dsound)
    target_link_options(alembic_export_tests PRIVATE "/MANIFEST:NO" "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}")
    set_target_properties(alembic_export_tests PROPERTIES OUTPUT_NAME "MikuMikuDance" ENABLE_EXPORTS ON
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/alembic/lib/$<CONFIG>"
        PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/alembic/symbols/$<CONFIG>"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/alembic/$<CONFIG>")
    add_executable(media_render_tests EXCLUDE_FROM_ALL tests/media_render_tests.cpp
        res/MMD.rc ${MIKUDANCESTUDIO_GEN_RES} exports/MikuMikuDance.def)
    target_link_libraries(media_render_tests PRIVATE mikudancestudio_core winmm dsound)
    target_link_options(media_render_tests PRIVATE "/MANIFEST:NO" "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}")
    set_target_properties(media_render_tests PROPERTIES OUTPUT_NAME "MikuMikuDance" ENABLE_EXPORTS ON
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/media/lib/$<CONFIG>"
        PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/media/symbols/$<CONFIG>"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/media/$<CONFIG>")
    add_executable(lookup_cache_tests EXCLUDE_FROM_ALL tests/lookup_cache_tests.cpp)
    target_include_directories(lookup_cache_tests PRIVATE include)
    target_compile_features(lookup_cache_tests PRIVATE cxx_std_17)
    set_target_properties(lookup_cache_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME lookup_cache_regressions COMMAND lookup_cache_tests)
    add_executable(frame_benchmark EXCLUDE_FROM_ALL tests/frame_benchmark.cpp
        res/MMD.rc ${MIKUDANCESTUDIO_GEN_RES})
    target_link_libraries(frame_benchmark PRIVATE mikudancestudio_core winmm dsound)
    target_link_options(frame_benchmark PRIVATE "/MANIFEST:NO"
        "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}")
    set_target_properties(frame_benchmark PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_executable(frame_benchmark_mme EXCLUDE_FROM_ALL tests/frame_benchmark.cpp
        res/MMD.rc ${MIKUDANCESTUDIO_GEN_RES} exports/MikuMikuDance.def)
    target_link_libraries(frame_benchmark_mme PRIVATE mikudancestudio_core winmm dsound)
    target_compile_definitions(frame_benchmark_mme PRIVATE REQUIRE_MME=1)
    target_link_options(frame_benchmark_mme PRIVATE "/MANIFEST:NO"
        "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}")
    set_target_properties(frame_benchmark_mme PROPERTIES
        OUTPUT_NAME "MikuMikuDance" ENABLE_EXPORTS ON
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/mme-benchmark/lib/$<CONFIG>"
        PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/mme-benchmark/symbols/$<CONFIG>"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/mme-benchmark/$<CONFIG>")
    add_executable(pmm_extension_tests EXCLUDE_FROM_ALL tests/pmm_extension_tests.cpp src/io/pmm_extension.cpp)
    target_include_directories(pmm_extension_tests PRIVATE include)
    target_compile_features(pmm_extension_tests PRIVATE cxx_std_17)
    set_target_properties(pmm_extension_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME pmm_extension_tests COMMAND pmm_extension_tests)
    add_executable(pmm_project_settings_tests EXCLUDE_FROM_ALL tests/pmm_project_settings_tests.cpp
        res/MMD.rc ${MIKUDANCESTUDIO_GEN_RES})
    target_link_libraries(pmm_project_settings_tests PRIVATE mikudancestudio_core winmm dsound)
    target_link_options(pmm_project_settings_tests PRIVATE "/MANIFEST:NO"
        "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}")
    set_target_properties(pmm_project_settings_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_executable(camera_follow_tests EXCLUDE_FROM_ALL tests/camera_follow_tests.cpp)
    target_link_libraries(camera_follow_tests PRIVATE mikudancestudio_core winmm dsound)
    set_target_properties(camera_follow_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME camera_follow_regressions COMMAND camera_follow_tests)
    add_executable(deferred_effect_tests EXCLUDE_FROM_ALL tests/deferred_effect_tests.cpp
        res/MMD.rc ${MIKUDANCESTUDIO_GEN_RES})
    target_link_libraries(deferred_effect_tests PRIVATE mikudancestudio_core winmm dsound)
    target_link_options(deferred_effect_tests PRIVATE "/MANIFEST:NO"
        "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}")
    set_target_properties(deferred_effect_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_executable(deferred_effect_mme_tests EXCLUDE_FROM_ALL tests/deferred_effect_tests.cpp
        res/MMD.rc ${MIKUDANCESTUDIO_GEN_RES} exports/MikuMikuDance.def)
    target_link_libraries(deferred_effect_mme_tests PRIVATE mikudancestudio_core winmm dsound)
    target_compile_definitions(deferred_effect_mme_tests PRIVATE REQUIRE_MME=1)
    target_link_options(deferred_effect_mme_tests PRIVATE "/MANIFEST:NO"
        "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}")
    set_target_properties(deferred_effect_mme_tests PROPERTIES
        OUTPUT_NAME "MikuMikuDance" ENABLE_EXPORTS ON
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/mme/lib/$<CONFIG>"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/mme/$<CONFIG>")
    add_executable(wave_audio_tests EXCLUDE_FROM_ALL tests/wave_audio_tests.cpp
        res/MMD.rc ${MIKUDANCESTUDIO_GEN_RES})
    target_link_libraries(wave_audio_tests PRIVATE mikudancestudio_core minhook::minhook winmm dsound)
    target_link_options(wave_audio_tests PRIVATE "/MANIFEST:NO"
        "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}")
    set_target_properties(wave_audio_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_executable(scene_lifecycle_tests EXCLUDE_FROM_ALL tests/scene_lifecycle_tests.cpp
        res/MMD.rc ${MIKUDANCESTUDIO_GEN_RES})
    target_link_libraries(scene_lifecycle_tests PRIVATE mikudancestudio_core winmm dsound)
    target_link_options(scene_lifecycle_tests PRIVATE "/MANIFEST:NO"
        "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}")
    set_target_properties(scene_lifecycle_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    # Explicit opt-in host for locally installed MME. MMHack imports the host
    # by filename and ordinal, so a normal test executable cannot exercise it.
    add_executable(scene_lifecycle_mme_tests EXCLUDE_FROM_ALL tests/scene_lifecycle_tests.cpp
        res/MMD.rc ${MIKUDANCESTUDIO_GEN_RES} exports/MikuMikuDance.def)
    target_link_libraries(scene_lifecycle_mme_tests PRIVATE mikudancestudio_core winmm dsound)
    target_compile_definitions(scene_lifecycle_mme_tests PRIVATE REQUIRE_MME=1)
    target_link_options(scene_lifecycle_mme_tests PRIVATE "/MANIFEST:NO"
        "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}")
    set_target_properties(scene_lifecycle_mme_tests PROPERTIES
        OUTPUT_NAME "MikuMikuDance" ENABLE_EXPORTS ON
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/mme/lib/$<CONFIG>"
        PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/mme/symbols/$<CONFIG>"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/mme/$<CONFIG>")
endif()

target_link_libraries(MikuDanceStudio PRIVATE
    mikudancestudio_core
    winmm      # timeGetTime - main loop timing
    dsound     # original timeline/audio subsystem initialization (0x4C2470)
    user32
)

# The core's PUBLIC d3dx9_43 import dependency lands in each consuming
# executable's IAT, including the host table patched by MME.

# gen_resources.res re-embeds the original .rsrc byte-exact; link.exe accepts
# .res directly as an input file (CMake itself would classify it as <None>).
# /MANIFEST:NO keeps the linker from adding a default manifest on top of the
# original one embedded through the .res.
target_link_options(MikuDanceStudio PRIVATE
    "/MAP:${CMAKE_CURRENT_BINARY_DIR}/MikuMikuDance.map"
    "/MANIFEST:NO"
    "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}"
)
set_target_properties(MikuDanceStudio PROPERTIES
    # The original x64 binary is MikuMikuDance.exe, and MikuMikuEffect needs
    # that exact module name: its MMHack.dll statically imports the 15 Exp*
    # entry points from "MikuMikuDance.exe", so a differently named host makes
    # the loader fail to bind them (MME then either reports "Initialize error"
    # or dereferences the unbound slots).  Porting-era comments that mention
    # MikuMikuDanceE_v932x64 refer to the reference *distribution*.
    OUTPUT_NAME "MikuMikuDance"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
    # Export the 37 Exp* symbols from the .def file (ordinals 1..37).
    ENABLE_EXPORTS ON
)

# ---------------------------------------------------------------------------
# MMDxShow - the companion DirectShow push-source filter.  Its source is
# statically linked into the host so AVI codecs remain available without a
# separately-installed DLL.  The optional standalone reference DLL is still
# available for interface tests, but is no longer installed with the app.
# Built for both
# architectures: the x86 build pins the VC8 original's layout
# (../MikuMikuDanceE_v932/Data/MMDxShow.dll), the x64 build pins the VC10
# rebuild (../MikuMikuDanceE_v932x64/Data/MMDxShow.dll) - the per-flavour
# static_asserts live in src/mmdxshow/mmdxshow.hpp.  Both originals export
# the same ordinals 1..5 via the .def (no per-arch def needed).
# ---------------------------------------------------------------------------
add_library(MMDxShow SHARED EXCLUDE_FROM_ALL
    src/mmdxshow/dll_main.cpp
    src/mmdxshow/enumerators.cpp
    src/mmdxshow/source_base.cpp
    src/mmdxshow/push_source.cpp
    src/mmdxshow/push_pin.cpp
    src/mmdxshow/guids.cpp
    src/mmdxshow/crt_ledger.cpp
)

add_library(mmdxshow_embedded STATIC
    src/mmdxshow/dll_main.cpp
    src/mmdxshow/enumerators.cpp
    src/mmdxshow/source_base.cpp
    src/mmdxshow/push_source.cpp
    src/mmdxshow/push_pin.cpp
    src/mmdxshow/crt_ledger.cpp
)
target_include_directories(mmdxshow_embedded PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/mmdxshow)
target_link_libraries(mmdxshow_embedded PUBLIC ole32 strmiids uuid)
target_link_libraries(mikudancestudio_core PUBLIC mmdxshow_embedded)
target_include_directories(MMDxShow PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/mmdxshow)
target_link_libraries(MMDxShow PRIVATE
    kernel32 advapi32 gdi32 user32 ole32 uuid
)
target_link_options(MMDxShow PRIVATE
    "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/mmdxshow/MMDxShow.def")
# The standalone reference filter is built only for interface tests; the host
# embeds the push-source implementation in mmdxshow_embedded.  Do not install
# the standalone DLL into the user-facing build.
set_target_properties(MMDxShow PROPERTIES
    OUTPUT_NAME "MMDxShow"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/reference/$<CONFIG>"
)

if (MIKUDANCESTUDIO_BUILD_UI_TESTS)
    add_executable(mmdxshow_interface_tests EXCLUDE_FROM_ALL tests/mmdxshow_interface_tests.cpp)
    target_link_libraries(mmdxshow_interface_tests PRIVATE ole32 strmiids)
    add_dependencies(mmdxshow_interface_tests MMDxShow)
    set_target_properties(mmdxshow_interface_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME mmdxshow_interface_tests
        COMMAND mmdxshow_interface_tests "$<TARGET_FILE:MMDxShow>")
endif()

# ---------------------------------------------------------------------------
# Release layout. The final runtime package lives in build/Release.
# `cmake --install build --config Release --prefix build/Release` installs
# to that same directory:
#
#   build\Release\MikuMikuDance.exe <- DirectShow push source is embedded
#
# The Windows WIC PNG encoder creates transparent MOV files without a separate codec.
# The regression executables are EXCLUDE_FROM_ALL
# and carry no install rule, static/import libraries and PDBs live under
# build\lib and build\symbols, and no log or dump is produced at runtime.
# The runtime assets (Data\toon*.bmp, mmconfig.ini) come from the reference
# MMD installation and are not part of this source tree.
# ---------------------------------------------------------------------------
install(TARGETS MikuDanceStudio RUNTIME DESTINATION .)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/licenses/" DESTINATION licenses)

if(Python3_Interpreter_FOUND)
    add_test(NAME optional_graphics_imports
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/optional_graphics_imports.py"
                "$<TARGET_FILE:MikuDanceStudio>")
endif()

if(MIKUDANCESTUDIO_BUILD_EFFECT_TESTS)
    add_executable(transparent_material_order_tests EXCLUDE_FROM_ALL
        tests/transparent_material_order_tests.cpp)
    target_include_directories(transparent_material_order_tests PRIVATE include)
    target_compile_features(transparent_material_order_tests PRIVATE cxx_std_17)
    set_target_properties(transparent_material_order_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
    add_test(NAME transparent_material_order_regressions COMMAND transparent_material_order_tests)
endif()

if (MIKUDANCESTUDIO_BUILD_EFFECT_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/transparent_geometry_tests.cpp")
    add_executable(transparent_geometry_tests EXCLUDE_FROM_ALL
        tests/transparent_geometry_tests.cpp src/render/transparent_geometry.cpp)
    target_include_directories(transparent_geometry_tests PRIVATE include)
    set_target_properties(transparent_geometry_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME transparent_geometry_regressions COMMAND transparent_geometry_tests)
endif()

add_executable(mmv4mms_menu_tests EXCLUDE_FROM_ALL tests/mmv4mms_menu_tests.cpp)
target_link_libraries(mmv4mms_menu_tests PRIVATE mikudancestudio_core winmm dsound)
set_target_properties(mmv4mms_menu_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>")
add_test(NAME mmv4mms_menu COMMAND mmv4mms_menu_tests)

if(MIKUDANCESTUDIO_BUILD_UI_TESTS)
    add_executable(display_compatibility_tests EXCLUDE_FROM_ALL tests/display_compatibility_tests.cpp)
    add_executable(mme_display_tests EXCLUDE_FROM_ALL tests/mme_display_tests.cpp
        res/MMD.rc ${MIKUDANCESTUDIO_GEN_RES} exports/MikuMikuDance.def)
    target_link_options(mme_display_tests PRIVATE "/MANIFEST:NO" "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}")
    target_link_libraries(mme_display_tests PRIVATE mikudancestudio_core minhook::minhook winmm dsound)
    set_target_properties(mme_display_tests PROPERTIES OUTPUT_NAME "MikuMikuDance" ENABLE_EXPORTS ON
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/mme-display/lib/$<CONFIG>"
        PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/mme-display/symbols/$<CONFIG>"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/mme-display/$<CONFIG>")
    target_sources(display_compatibility_tests PRIVATE res/MMD.rc ${MIKUDANCESTUDIO_GEN_RES} exports/MikuMikuDance.def)
    target_link_options(display_compatibility_tests PRIVATE "/MANIFEST:NO" "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}")
    target_link_libraries(display_compatibility_tests PRIVATE mikudancestudio_core minhook::minhook winmm dsound)
    set_target_properties(display_compatibility_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/display/$<CONFIG>")
    if(Python3_Interpreter_FOUND)
        add_test(NAME display_compatibility_regressions
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/display_compatibility_runner.py"
                "$<TARGET_FILE:display_compatibility_tests>" "${CMAKE_BINARY_DIR}/tests/display-results")
        set_tests_properties(display_compatibility_regressions PROPERTIES TIMEOUT 90)
    endif()
    add_executable(fullscreen_window_tests EXCLUDE_FROM_ALL tests/fullscreen_window_tests.cpp)
    target_sources(fullscreen_window_tests PRIVATE res/MMD.rc ${MIKUDANCESTUDIO_GEN_RES} exports/MikuMikuDance.def)
    target_link_options(fullscreen_window_tests PRIVATE "/MANIFEST:NO" "${CMAKE_CURRENT_SOURCE_DIR}/${MIKUDANCESTUDIO_GEN_RES}")
    target_link_libraries(fullscreen_window_tests PRIVATE mikudancestudio_core minhook::minhook winmm dsound)
    set_target_properties(fullscreen_window_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/fullscreen/$<CONFIG>")
endif()
