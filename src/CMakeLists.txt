#

# SUBDIRS = lisp net imlib sdlport
add_subdirectory(lisp)
add_subdirectory(net)
add_subdirectory(imlib)
add_subdirectory(sdlport)

if (WIN32)
    # FIXME: Where should this logic reside? Whatever, first generate the
    # resource script...
    get_filename_component(abuse_ICON_PATH ${CPACK_WIX_PRODUCT_ICON} ABSOLUTE)
    configure_file(win32/resources.rc.in ${CMAKE_CURRENT_BINARY_DIR}/resources.rc ESCAPE_QUOTES NEWLINE_STYLE WIN32)
    # And then include it
    # FIXME: I think this means the build location can't have spaces
    set(abuse_RESOURCE_FILES "${CMAKE_CURRENT_BINARY_DIR}/resources.rc")
else()
    set(abuse_RESOURCE_FILES "")
endif()

# The SDL_GPU shaders, built into the binary rather than installed beside
# it. Configure time, because the blobs are committed and only change when
# someone reruns tools/shaders/build-shaders.sh.
include(${abuse_SOURCE_DIR}/cmake/EmbedShaders.cmake)
abuse_embed_shaders("${CMAKE_CURRENT_BINARY_DIR}/gpu_shaders.h"
                    palette.vert palette.frag overlay.frag present.frag
                    bright.frag blur.frag)

add_executable(abuse
    common.h
    compat.cpp compat.h
    data/config_file.cpp data/config_file.h data/paths.cpp data/paths.h
    render/options.cpp render/options.h
    render/lightmap.cpp render/lightmap.h
    render/shake.cpp render/shake.h
    render/particles.cpp render/particles.h
    imlib/hd.cpp imlib/hd.h
    sdlport/gpu_present.cpp sdlport/gpu_present.h
    audio/buses.cpp audio/buses.h
    audio/voices.cpp audio/voices.h
    audio/limiter.cpp audio/limiter.h
    audio/formats.cpp audio/formats.h
    timing/pacer.cpp timing/pacer.h
    input/actions.cpp input/actions.h
    input/gamepad.cpp input/gamepad.h
    input/rumble.cpp input/rumble.h
    input/aim.cpp input/aim.h
    i18n/language.cpp i18n/language.h
    i18n/uitext.cpp i18n/uitext.h
    ui/hexfont.cpp ui/hexfont.h
    ui/overlay.cpp ui/overlay.h
    ui/menu_list.cpp ui/menu_list.h
    ui/options_screen.cpp ui/options_screen.h
    ui/classic_data_screen.cpp ui/classic_data_screen.h
    ui/language_screen.cpp ui/language_screen.h
    ui/start_menu.cpp ui/start_menu.h
    ui/hud.cpp ui/hud.h
    lol/matrix.cpp lol/matrix.h
    lol/timer.cpp lol/timer.h
    specache.cpp specache.h
    netcfg.cpp netcfg.h
    innet.cpp
    chat.cpp chat.h
    endgame.cpp
    loadgame.cpp loadgame.h
    profile.cpp profile.h
    cop.cpp cop.h
    statbar.cpp
    compiled.cpp compiled.h
    ant.cpp ant.h
    sensor.cpp
    demo.cpp demo.h
    lcache.cpp lcache.h
    nfclient.cpp nfclient.h
    clisp.cpp clisp.h
    gui.cpp gui.h
    transp.cpp transp.h
    collide.cpp
    property.cpp property.h
    cache.cpp cache.h
    particle.cpp particle.h
    objects.cpp objects.h
    extend.cpp extend.h
    console.cpp console.h
    ability.cpp ability.h
    items.cpp items.h
    dev.cpp dev.h
    chars.cpp chars.h
    level.cpp level.h
    smallfnt.cpp
    automap.cpp automap.h
    help.cpp help.h
    intsect.cpp intsect.h
    loader2.cpp loader2.h
    seq.cpp seq.h
    points.cpp points.h
    fnt6x13.cpp
    morpher.cpp morpher.h
    menu.cpp menu.h
    director.cpp director.h
    view.cpp view.h
    configuration.cpp configuration.h
    game.cpp game.h
    harness.cpp harness.h
    light.cpp light.h
    devsel.cpp devsel.h
    crc.cpp crc.h
    gamma.cpp gamma.h
    id.h netface.h isllist.h sbar.h
    nfserver.h
    ui/volumewindow.cpp ui/volumewindow.h
    sdlport/util.h
    ${abuse_RESOURCE_FILES}
)

set_target_properties(abuse PROPERTIES MACOSX_BUNDLE true)
# Note this doesn't install the icon, that's done later
set_target_properties(abuse PROPERTIES MACOSX_BUNDLE_ICON_FILE "abuse")
set_target_properties(abuse PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "Abuse")
set_target_properties(abuse PROPERTIES MACOSX_BUNDLE_SHORT_VERSION_STRING "${abuse_VERSION}")
set_target_properties(abuse PROPERTIES MACOSX_BUNDLE_BUNDLE_VERSION "${abuse_VERSION}")

target_link_libraries(abuse PRIVATE lisp)
target_link_libraries(abuse PRIVATE sdlport)
target_link_libraries(abuse PRIVATE imlib)
target_link_libraries(abuse PRIVATE net)
target_link_libraries(abuse PUBLIC SDL3::SDL3)
target_link_libraries(abuse PUBLIC SDL3_mixer::SDL3_mixer)

if(APPLE)
    # Link CoreFoundation
    target_link_libraries(abuse PRIVATE "-framework CoreFoundation")
endif()

if(WIN32 AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)
    # Windows looks for a DLL beside the executable, and CPM leaves SDL3.dll
    # and SDL3_mixer.dll in their own build directories. The install rules
    # below already collect them; this is about running from the build tree,
    # which is what the test suites and anyone hacking on the game do.
    #
    # Without it the binary starts and dies with "SDL3.dll: cannot open
    # shared object file", which is how the replay suite first failed on the
    # Windows runner.
    add_custom_command(TARGET abuse POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:abuse> $<TARGET_FILE_DIR:abuse>
        COMMAND_EXPAND_LISTS)
endif()

add_executable(abuse-tool tool/abuse-tool.cpp crc.cpp crc.h)

target_link_libraries(abuse-tool imlib)

include_directories(
    ${abuse_SOURCE_DIR}/src
    ${abuse_SOURCE_DIR}/src/lisp
    ${abuse_SOURCE_DIR}/src/imlib
    ${abuse_SOURCE_DIR}/src/net
    ${CMAKE_CURRENT_BINARY_DIR}   # gpu_shaders.h, written at configure time
)
if(WIN32)
    # Under Windows, throw them into the root
    install(TARGETS abuse abuse-tool
        RUNTIME DESTINATION "."
        LIBRARY DESTINATION ".")
    install(FILES $<TARGET_FILE:SDL3::SDL3> DESTINATION ".")
    install(FILES $<TARGET_FILE:SDL3_mixer::SDL3_mixer> DESTINATION ".")
    # The Original mode needs data this project cannot ship. The fetcher and
    # the checksum list travel with the game, so a player who unpacked a zip
    # has everything they need to get the sound without a clone.
    install(FILES
        ${abuse_SOURCE_DIR}/scripts/fetch-classic-data.ps1
        ${abuse_SOURCE_DIR}/data/classic.sha256
        DESTINATION ".")
else()
    # Under Linux, we want the tools in bin
    install(TARGETS abuse abuse-tool RUNTIME DESTINATION bin
    BUNDLE DESTINATION "${CMAKE_INSTALL_PREFIX}")
    if(APPLE)
        # Fix up the bundle (basically, install SDL with it)
        install(CODE "
            include(BundleUtilities)
            set(BU_CHMOD_BUNDLE_ITEMS TRUE)
            fixup_bundle(
                \"${CMAKE_INSTALL_PREFIX}/abuse.app\"
                \"\"
                \"${SDL3_BINARY_DIR};${SDL3_mixer_BINARY_DIR}\"
            )
        ")
        # Add icon
        install(FILES "../macOS/abuse.icns" DESTINATION "${CMAKE_INSTALL_PREFIX}/abuse.app/Contents/Resources")
    endif()
endif()
