# Explicit compile list: no globbing, and the SDK translation unit is taken
# from the resolved MetaHook tree instead of being copied into the plugin.
set(VELOHUD_SOURCES
    "${METAHOOK_SOURCE_PATH}/include/HLSDK/common/interface.cpp"
    "src/plugins.cpp"
    "src/exportfuncs.cpp"
)
