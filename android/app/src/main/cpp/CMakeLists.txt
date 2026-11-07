cmake_minimum_required(VERSION 3.22.1)
project(rechan LANGUAGES C CXX)
find_package(OpenXR REQUIRED CONFIG)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# cpp -> main -> src -> app -> android -> repo root
set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")
get_filename_component(REPO_ROOT "${REPO_ROOT}" ABSOLUTE)

# --- native_app_glue -------------------------------------------------------

add_library(native_app_glue STATIC
    "${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c")
target_include_directories(native_app_glue INTERFACE
    "${ANDROID_NDK}/sources/android/native_app_glue")
target_compile_options(native_app_glue PRIVATE -w)
target_compile_definitions(native_app_glue PRIVATE NDEBUG)
# Nothing in the game references glue symbols, so without this flag the
# linker drops the object and ANativeActivity_onCreate goes missing at load
# time (UnsatisfiedLinkError on NativeActivity startup). NOTE: unquoted —
# "-u SYM" must be TWO list items, not one.
target_link_options(native_app_glue INTERFACE -u ANativeActivity_onCreate)

# --- libp3d (p3d core + GLES pddi backend + ImGui core) ---------------------

file(GLOB P3D_CORE_SOURCES "${REPO_ROOT}/vendor/libp3d/p3d/*.cpp")

set(P3D_GLES_SOURCES
    "${REPO_ROOT}/vendor/libp3d/pddi/gles/glesrender.cpp"
    "${REPO_ROOT}/vendor/libp3d/pddi/gles/AndroidPlatform.cpp"
    "${REPO_ROOT}/vendor/libp3d/pddi/quest/QuestXR.cpp"
    "${REPO_ROOT}/vendor/libp3d/pddi/quest/QuestPassthrough.cpp"
    "${REPO_ROOT}/vendor/libp3d/pddi/quest/MRInteraction.cpp"
    "${REPO_ROOT}/vendor/libp3d/pddi/quest/MRTestObject.cpp"
    "${REPO_ROOT}/vendor/libp3d/pddi/quest/MRCharacter.cpp"
    "${REPO_ROOT}/vendor/libp3d/pddi/quest/MRRoomGeometry.cpp"
    "${REPO_ROOT}/vendor/libp3d/pddi/quest/QuestRoomScene.cpp"
    "${REPO_ROOT}/vendor/libp3d/pddi/quest/QuestScenePermission.cpp"
    "${REPO_ROOT}/vendor/libp3d/pddi/quest/MRPortalSurface.cpp"
    "${REPO_ROOT}/vendor/libp3d/pddi/quest/MRPortalSurfaceRenderer.cpp"
    "${REPO_ROOT}/vendor/libp3d/pddi/quest/MRPortalRenderer.cpp"
    "${REPO_ROOT}/vendor/libp3d/pddi/quest/QuestHands.cpp"
    "${REPO_ROOT}/vendor/libp3d/pddi/quest/MRPointerRenderer.cpp"
    "${REPO_ROOT}/vendor/libp3d/pddi/quest/QuestDiagnostic.cpp"
    "${REPO_ROOT}/vendor/libp3d/pddi/quest/QuestPortal.cpp")

set(IMGUI_SOURCES
    "${REPO_ROOT}/vendor/libp3d/vendor/imgui/imgui.cpp"
    "${REPO_ROOT}/vendor/libp3d/vendor/imgui/imgui_draw.cpp"
    "${REPO_ROOT}/vendor/libp3d/vendor/imgui/imgui_tables.cpp"
    "${REPO_ROOT}/vendor/libp3d/vendor/imgui/imgui_widgets.cpp"
    "${REPO_ROOT}/vendor/libp3d/vendor/imgui/imgui_demo.cpp")

add_library(p3d STATIC ${P3D_CORE_SOURCES} ${P3D_GLES_SOURCES} ${IMGUI_SOURCES})
target_include_directories(p3d PUBLIC
    "${REPO_ROOT}"
    "${REPO_ROOT}/vendor/libp3d"
    "${REPO_ROOT}/vendor/libp3d/vendor/imgui")
target_compile_definitions(p3d PUBLIC RC_PLATFORM_ANDROID RC_PLATFORM_QUEST XR_USE_PLATFORM_ANDROID XR_USE_GRAPHICS_API_OPENGL_ES IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS)
target_compile_options(p3d PUBLIC -fno-fast-math)
target_link_libraries(p3d PUBLIC OpenXR::openxr_loader)

# --- rechan game sources ----------------------------------------------------

set(RECHAN_SOURCE_DIRS ai extra fe gen pc radlib radmovie snd xclib)
set(RECHAN_SOURCES "")
foreach(dir IN LISTS RECHAN_SOURCE_DIRS)
    file(GLOB DIR_SOURCES "${REPO_ROOT}/src/${dir}/*.cpp")
    list(APPEND RECHAN_SOURCES ${DIR_SOURCES})
endforeach()

add_library(rechan SHARED
    ${RECHAN_SOURCES}
    AndroidShell.cpp
    AndroidInput.cpp)

target_include_directories(rechan PRIVATE
    "${REPO_ROOT}/src"
    "${REPO_ROOT}/vendor/libp3d"
    "${REPO_ROOT}/vendor/libp3d/vendor/imgui"
    "${REPO_ROOT}/vendor/miniaudio"
    "${REPO_ROOT}/vendor/cgltf")

target_compile_definitions(rechan PRIVATE
    RC_PLATFORM_ANDROID
    RECHAN_QUEST_VERSION="${RECHAN_QUEST_VERSION}"
    _CRT_SECURE_NO_WARNINGS)

target_link_libraries(rechan PRIVATE p3d native_app_glue EGL GLESv3 android log m)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(rechan PRIVATE -Og -g)
else()
    target_compile_options(rechan PRIVATE -O2)
endif()
