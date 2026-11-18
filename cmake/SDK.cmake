function(velohud_use_sdk target)
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_compile_definitions(${target} PRIVATE
        WIN32
        _WINDOWS
        _USRDLL
        _MBCS
        _CRT_SECURE_NO_WARNINGS
        NO_MALLOC_OVERRIDE
    )

    # VGUI2Extension headers win over MetaHook's historical copies: the plugin
    # talks to ISurface2 and IVGUI2Extension, which are owned by that extension.
    set(_vgui2_include "")
    if(VGUI2EXTENSION_SOURCE_PATH)
        list(APPEND _vgui2_include
            "${VGUI2EXTENSION_SOURCE_PATH}/include"
            "${VGUI2EXTENSION_SOURCE_PATH}/include/Interface"
            "${VGUI2EXTENSION_SOURCE_PATH}/include/Interface/VGUI")
    endif()

    target_include_directories(${target} PRIVATE
        ${_vgui2_include}
        "${PROJECT_SOURCE_DIR}/src"
        "${METAHOOK_SOURCE_PATH}/include"
        "${METAHOOK_SOURCE_PATH}/include/Interface"
        "${METAHOOK_SOURCE_PATH}/include/Interface/VGUI"
        "${METAHOOK_SOURCE_PATH}/include/HLSDK/common"
        "${METAHOOK_SOURCE_PATH}/include/HLSDK/engine"
        "${METAHOOK_SOURCE_PATH}/include/HLSDK/cl_dll"
        "${METAHOOK_SOURCE_PATH}/include/HLSDK/pm_shared"
        "${METAHOOK_SOURCE_PATH}/include/HLSDK/public"
        "${METAHOOK_SOURCE_PATH}/include/SourceSDK"
        "${METAHOOK_SOURCE_PATH}/include/SourceSDK/tier0"
        "${METAHOOK_SOURCE_PATH}/include/SourceSDK/tier1"
        "${METAHOOK_SOURCE_PATH}/include/SourceSDK/tier3"
        "${METAHOOK_SOURCE_PATH}/include/SourceSDK/vstdlib"
        "${METAHOOK_SOURCE_PATH}/include/VGUI"
        "${METAHOOK_SOURCE_PATH}/include/vgui_controls"
    )
endfunction()
