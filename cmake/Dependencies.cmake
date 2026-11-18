set(METAHOOK_SOURCE_PATH "$ENV{METAHOOK_SOURCE_PATH}" CACHE PATH "MetaHook source tree; empty fetches SDK main")
set(VGUI2EXTENSION_SOURCE_PATH "$ENV{VGUI2EXTENSION_SOURCE_PATH}" CACHE PATH "VGUI2Extension source tree providing ISurface2/IVGUI2Extension; empty fetches the pinned commit")

# The plugin compiles part of the HLSDK and needs both MetaHook's public
# headers and VGUI2Extension's interface headers.
function(velohud_validate_metahook_source source)
    foreach(required include/metahook.h include/HLSDK/common/interface.cpp
        include/HLSDK/common/cvardef.h include/Interface/IPlugins.h)
        if(NOT EXISTS "${source}/${required}" OR IS_DIRECTORY "${source}/${required}")
            message(FATAL_ERROR "METAHOOK_SOURCE_PATH is missing ${required}: ${source}")
        endif()
    endforeach()
endfunction()

function(velohud_validate_vgui2extension_source source)
    foreach(required include/Interface/IVGUI2Extension.h
        include/Interface/VGUI/ISurface2.h)
        if(NOT EXISTS "${source}/${required}" OR IS_DIRECTORY "${source}/${required}")
            message(FATAL_ERROR "VGUI2EXTENSION_SOURCE_PATH is missing ${required}: ${source}")
        endif()
    endforeach()
endfunction()

function(velohud_prepare_dependencies)
    # External trees are read-only inputs; validate explicit paths before downloading anything.
    if(METAHOOK_SOURCE_PATH)
        get_filename_component(metahook_source "${METAHOOK_SOURCE_PATH}" ABSOLUTE BASE_DIR "${PROJECT_SOURCE_DIR}")
        velohud_validate_metahook_source("${metahook_source}")
    else()
        include(FetchContent)
        FetchContent_Declare(velohud_metahook
            GIT_REPOSITORY https://github.com/MetaHookSv/MetaHook
            # MetaHook is tracked as a branch: always fetch the latest main.
            GIT_TAG origin/main
            GIT_SUBMODULES ""
            # This SDK directory has no CMakeLists.txt: populate without building the launcher.
            SOURCE_SUBDIR include
        )
        FetchContent_MakeAvailable(velohud_metahook)
        set(metahook_source "${velohud_metahook_SOURCE_DIR}")
    endif()
    velohud_validate_metahook_source("${metahook_source}")
    set(METAHOOK_SOURCE_PATH "${metahook_source}" PARENT_SCOPE)
    message(STATUS "METAHOOK_SOURCE_PATH: ${metahook_source}")

    if(VGUI2EXTENSION_SOURCE_PATH)
        get_filename_component(vgui2extension_source "${VGUI2EXTENSION_SOURCE_PATH}" ABSOLUTE BASE_DIR "${PROJECT_SOURCE_DIR}")
    else()
        include(FetchContent)
        FetchContent_Declare(velohud_vgui2extension
            GIT_REPOSITORY https://github.com/MetaHookSv/VGUI2Extension
            GIT_TAG b60ec0f03c60cb7c8c6d3e59c3c38f4753cf1d0d
            GIT_SUBMODULES ""
            GIT_SUBMODULES_RECURSE FALSE
            SOURCE_SUBDIR include
        )
        FetchContent_MakeAvailable(velohud_vgui2extension)
        set(vgui2extension_source "${velohud_vgui2extension_SOURCE_DIR}")
    endif()
    velohud_validate_vgui2extension_source("${vgui2extension_source}")
    set(VGUI2EXTENSION_SOURCE_PATH "${vgui2extension_source}" PARENT_SCOPE)
    message(STATUS "VGUI2EXTENSION_SOURCE_PATH: ${vgui2extension_source}")
endfunction()
