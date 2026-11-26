# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Declares the project name. The project name can be accessed via ${ PROJECT_NAME},
# Since this is the top level CMakeLists.txt, the project name is also accessible
# with ${CMAKE_PROJECT_NAME} (both CMake variables are in-sync within the top level
# build script scope).
function(fas_dependency name fallback)
    if(NOT DEFINED ${name})
        if(DEFINED ENV{${name}})
            set(fallback "$ENV{${name}}")
        endif()
        file(TO_CMAKE_PATH "${fallback}" default_path)
        set(${name} "${default_path}" CACHE PATH "FreedomApopicStyle dependency path")
    endif()
endfunction()

fas_dependency(FasAndroidDxLib "C:/DxLib_Android/Lib_AndroidStudio")
fas_dependency(FasSocketHeaders "C:/Socket_h-main/include")
fas_dependency(FasJsonHeaders "C:/json/include")
fas_dependency(FasZipHeaders "C:/cppunzip-main")
fas_dependency(FasLibarrierHeaders "C:/libarrier-main/include")
fas_dependency(FasMagicEnumHeaders "C:/magic_enum-v0.9.8/include")
fas_dependency(FasFastFloatHeaders "C:/fast_float")

project("freedomapopicstyle")

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.
#
# In this top level CMakeLists.txt, ${CMAKE_PROJECT_NAME} is used to define
# the target library name; in the sub-module's CMakeLists.txt, ${PROJECT_NAME}
# is preferred for the same purpose.
#
# In order to load a library into your app from Java/Kotlin, you must call
# System.loadLibrary() and pass the name of the library defined here;
# for GameActivity/NativeActivity derived applications, the same library name must be
# used in the AndroidManifest.xml file.
# PC 版と同じソースをそのまま使う (このプロジェクトは FreedomApopicStyle/Android にあり、ソースは 5 つ上のフォルダ)
set(FAS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")
include_directories("${FAS_ROOT}")
file(GLOB FAS_LIBRARY_HEADERS CONFIGURE_DEPENDS "${FAS_ROOT}/Library/*.hpp" "${FAS_ROOT}/Library/*.h")
file(GLOB FAS_GAME_SOURCES CONFIGURE_DEPENDS "${FAS_ROOT}/Scenes/*.cpp" "${FAS_ROOT}/System/*.cpp")
add_library(native-lib
        SHARED
        "${FAS_ROOT}/Main.cpp"
        "${FAS_ROOT}/System.h"
        "${FAS_ROOT}/Game.hpp"
        "${FAS_ROOT}/GameDependencies.hpp"
        "${FAS_ROOT}/SystemCommon.hpp"
        "${FAS_ROOT}/SystemCommon.cpp"
        ${FAS_GAME_SOURCES}
        ${FAS_LIBRARY_HEADERS})

target_include_directories(native-lib PRIVATE
        "${FasAndroidDxLib}/${ANDROID_ABI}" "${FasSocketHeaders}" "${FasJsonHeaders}"
        "${FasZipHeaders}" "${FasLibarrierHeaders}" "${FasMagicEnumHeaders}" "${FasFastFloatHeaders}")
target_link_directories(native-lib PRIVATE "${FasAndroidDxLib}/${ANDROID_ABI}")

# Specifies libraries CMake should link to your target library. You
# can link libraries from various origins, such as libraries defined in this
# build script, prebuilt third-party libraries, or Android system libraries.
target_link_libraries(native-lib
        # List libraries link to the target library
        log
        android
        GLESv1_CM
        EGL
        GLESv2
        OpenSLES
        m
        DxLib
        DxUseCLib
        jpeg
        png
        zlib
        tiff
        theora_static
        vorbis_static
        vorbisfile_static
        ogg_static
        bullet
        opus)
target_compile_options(native-lib PRIVATE -ferror-limit=0)
