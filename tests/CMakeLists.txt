# Unit tests. The replay and snapshot suites live in scripts/ and are
# registered further down, because they drive the real binary.

add_executable(abuse_unit_tests
    unit/main.cpp
    unit/test_keys.cpp
    unit/test_palette.cpp
    unit/test_paths.cpp
    unit/test_config_file.cpp
    unit/test_render_options.cpp
    unit/test_pacer.cpp
    unit/test_actions.cpp
    unit/test_gamepad.cpp
    unit/test_rumble.cpp
    unit/test_aim.cpp
    unit/test_language.cpp
    unit/test_fonts.cpp
    unit/test_hexfont.cpp
    unit/test_overlay.cpp
    unit/test_menu_list.cpp
    unit/test_uitext.cpp
    unit/test_specs.cpp
    unit/test_audio.cpp
    unit/test_lightmap.cpp
    unit/test_shake.cpp
    ${abuse_SOURCE_DIR}/src/compat.cpp
    ${abuse_SOURCE_DIR}/src/data/paths.cpp
    ${abuse_SOURCE_DIR}/src/data/config_file.cpp
    ${abuse_SOURCE_DIR}/src/render/options.cpp
    ${abuse_SOURCE_DIR}/src/render/lightmap.cpp
    ${abuse_SOURCE_DIR}/src/render/shake.cpp
    ${abuse_SOURCE_DIR}/src/audio/buses.cpp
    ${abuse_SOURCE_DIR}/src/audio/voices.cpp
    ${abuse_SOURCE_DIR}/src/audio/limiter.cpp
    ${abuse_SOURCE_DIR}/src/timing/pacer.cpp
    ${abuse_SOURCE_DIR}/src/input/actions.cpp
    ${abuse_SOURCE_DIR}/src/input/gamepad.cpp
    ${abuse_SOURCE_DIR}/src/input/rumble.cpp
    ${abuse_SOURCE_DIR}/src/input/aim.cpp
    ${abuse_SOURCE_DIR}/src/i18n/language.cpp
    ${abuse_SOURCE_DIR}/src/fnt6x13.cpp
    ${abuse_SOURCE_DIR}/src/ui/hexfont.cpp
    ${abuse_SOURCE_DIR}/src/ui/overlay.cpp
    ${abuse_SOURCE_DIR}/src/ui/menu_list.cpp
    ${abuse_SOURCE_DIR}/src/i18n/uitext.cpp
)

target_include_directories(abuse_unit_tests PRIVATE
    ${abuse_SOURCE_DIR}/src
    ${abuse_SOURCE_DIR}/src/imlib
    ${abuse_BINARY_DIR}
)

target_compile_definitions(abuse_unit_tests PRIVATE
    ABUSE_TEST_DATA_DIR="${abuse_SOURCE_DIR}/data"
    ABUSE_TEST_BUILD_DIR="${CMAKE_CURRENT_BINARY_DIR}")

target_include_directories(abuse_unit_tests PRIVATE ${doctest_SOURCE_DIR})

# detect_language() asks SDL for the user's locale, so the pure-logic suite
# still needs the library it links against.
target_link_libraries(abuse_unit_tests PRIVATE imlib SDL3::SDL3)

if(WIN32 AND CMAKE_VERSION VERSION_GREATER_EQUAL 3.21)
    # Same reason as for the game: this one links SDL3 for the locale check.
    add_custom_command(TARGET abuse_unit_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:abuse_unit_tests> $<TARGET_FILE_DIR:abuse_unit_tests>
        COMMAND_EXPAND_LISTS)
endif()

add_test(NAME unit COMMAND abuse_unit_tests)

# The three suites below are shell scripts. CTest runs a command, not a shell,
# so on a machine without one they simply are not registered: better a suite
# that is visibly absent than one that fails for the wrong reason. Windows
# runners do have bash, which is how they run there.
find_program(ABUSE_BASH bash)

if(ABUSE_BASH)

# Replay determinism. Skipped rather than failed when no replay has been
# recorded yet, so a fresh clone does not report a red suite for a missing
# fixture.
add_test(NAME replays
    COMMAND ${ABUSE_BASH} ${abuse_SOURCE_DIR}/scripts/test-replays.sh $<TARGET_FILE:abuse>
    WORKING_DIRECTORY ${abuse_SOURCE_DIR})
set_tests_properties(replays PROPERTIES SKIP_RETURN_CODE 77)

add_test(NAME snapshots
    COMMAND ${ABUSE_BASH} ${abuse_SOURCE_DIR}/scripts/test-snapshots.sh $<TARGET_FILE:abuse>
    WORKING_DIRECTORY ${abuse_SOURCE_DIR})
set_tests_properties(snapshots PROPERTIES SKIP_RETURN_CODE 77)

# The same frames as presented in the window: scale mode, filter and letterbox.
# The suite above dumps the buffer the game draws into, which is 320x200 no
# matter what any of those are set to, so none of them are visible in it.
add_test(NAME window
    COMMAND ${CMAKE_COMMAND} -E env WINDOW=1
            ${ABUSE_BASH} ${abuse_SOURCE_DIR}/scripts/test-snapshots.sh $<TARGET_FILE:abuse>
    WORKING_DIRECTORY ${abuse_SOURCE_DIR})
set_tests_properties(window PROPERTIES SKIP_RETURN_CODE 77)

# The engine leaks ~210 KB at exit (1882 allocations, spec_directory_cache
# and friends; known and recorded). LeakSanitizer turns
# that into exit code 1 and kills the scripts, whose set -e treats it as a
# test failure. Leak reporting stays available when the binary is run by
# hand; the value of the asan preset is in the memory-error checks, which
# this does not touch.
set_tests_properties(replays snapshots window PROPERTIES
    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")

else()
    message(STATUS "No bash found: the replay and snapshot suites are not registered")
endif()
