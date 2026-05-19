add_library(xenia-gpu-metal STATIC
  metal_command_processor.cc
  metal_command_processor.h
  metal_graphics_system.cc
  metal_graphics_system.h
  metal_heap_pool.cc
  metal_heap_pool.h
  metal_primitive_processor.cc
  metal_primitive_processor.h
  metal_render_target_cache.cc
  metal_render_target_cache.h
  metal_shader_cache.cc
  metal_shader_cache.h
  metal_shared_memory.cc
  metal_shared_memory.h
  metal_texture_cache.cc
  metal_texture_cache.h
  msl_bindings.h
  msl_shader.cc
  msl_shader.h
  msl_tess_factor_kernels.h
)
target_include_directories(xenia-gpu-metal PRIVATE
  ${PROJECT_SOURCE_DIR}/third_party/glslang
)
target_link_libraries(xenia-gpu-metal PUBLIC
  xenia-base
  xenia-gpu
  xenia-ui-metal
  spirv-cross
  "-framework Foundation"
  "-framework Metal"
  "-framework MetalKit"
)
target_link_libraries(xenia-gpu-metal PRIVATE glslang-spirv)
xe_shader_rules_metal(xenia-gpu-metal ${CMAKE_CURRENT_SOURCE_DIR}/../shaders)
if(APPLE)
  set(_metal_shader_args --msl)
  set(_direct_host_resolve_generated_root "${PROJECT_BINARY_DIR}/generated")
  set(_direct_host_resolve_bytecode_dir
      "${_direct_host_resolve_generated_root}/xenia/gpu/shaders/bytecode/metal")
  set(_direct_host_resolve_outputs)
  file(MAKE_DIRECTORY "${_direct_host_resolve_bytecode_dir}")

  macro(_xe_metal_direct_host_resolve_variant id input)
    set(_variant_define_args)
    foreach(_define ${ARGN})
      list(APPEND _variant_define_args --define "${_define}")
    endforeach()
    set(_variant_out "${_direct_host_resolve_bytecode_dir}/${id}.h")
    set(_variant_dep "${_variant_out}.d")
    list(APPEND _direct_host_resolve_outputs "${_variant_out}")
    add_custom_command(
      OUTPUT "${_variant_out}"
      COMMAND $<TARGET_FILE:xenia-shader-cc>
              ${_metal_shader_args}
              --identifier "${id}"
              ${_variant_define_args}
              --depfile "${_variant_dep}"
              "${input}"
              "${_variant_out}"
      DEPENDS "${input}" xenia-shader-cc
      DEPFILE "${_variant_dep}"
      COMMENT "Metal: ${id}"
      VERBATIM
    )
  endmacro()

  set(_resolve_host_color_entry
      "${CMAKE_CURRENT_SOURCE_DIR}/../shaders/resolve_host_color_entry.xesli")
  set(_resolve_host_color_full_entry
      "${CMAKE_CURRENT_SOURCE_DIR}/../shaders/resolve_host_color_full_entry.xesli")
  set(_resolve_host_depth_entry
      "${CMAKE_CURRENT_SOURCE_DIR}/../shaders/resolve_host_depth_entry.xesli")

  foreach(_source_uint 0 1)
    foreach(_bpp 32 64)
      foreach(_msaa 1 2 4)
        foreach(_scaled 0 1)
          set(_id "resolve_host_color")
          if(_source_uint)
            string(APPEND _id "_uint")
          endif()
          string(APPEND _id "_${_bpp}bpp_${_msaa}xmsaa")
          set(_defines
              "XE_RESOLVE_HOST_COLOR_BPP=${_bpp}"
              "XE_RESOLVE_HOST_COLOR_MSAA_SAMPLES=${_msaa}"
              "XE_RESOLVE_HOST_COLOR_SOURCE_UINT=${_source_uint}")
          if(_scaled)
            string(APPEND _id "_scaled")
            list(APPEND _defines "XE_RESOLVE_RESOLUTION_SCALED=1")
          endif()
          string(APPEND _id "_cs")
          _xe_metal_direct_host_resolve_variant(
              "${_id}" "${_resolve_host_color_entry}" ${_defines})
        endforeach()
      endforeach()
    endforeach()

    foreach(_bpp 8 16 32 64 128)
      foreach(_msaa 1 2 4)
        foreach(_scaled 0 1)
          set(_id "resolve_host_color_full")
          if(_source_uint)
            string(APPEND _id "_uint")
          endif()
          string(APPEND _id "_${_bpp}bpp_${_msaa}xmsaa")
          set(_defines
              "XE_RESOLVE_HOST_COLOR_FULL_DEST_BPP=${_bpp}"
              "XE_RESOLVE_HOST_COLOR_MSAA_SAMPLES=${_msaa}"
              "XE_RESOLVE_HOST_COLOR_SOURCE_UINT=${_source_uint}")
          if(_scaled)
            string(APPEND _id "_scaled")
            list(APPEND _defines "XE_RESOLVE_RESOLUTION_SCALED=1")
          endif()
          string(APPEND _id "_cs")
          _xe_metal_direct_host_resolve_variant(
              "${_id}" "${_resolve_host_color_full_entry}" ${_defines})
        endforeach()
      endforeach()
    endforeach()
  endforeach()

  foreach(_msaa 1 2 4)
    foreach(_scaled 0 1)
      set(_id "resolve_host_depth_32bpp_${_msaa}xmsaa")
      set(_defines "XE_RESOLVE_HOST_DEPTH_MSAA_SAMPLES=${_msaa}")
      if(_scaled)
        string(APPEND _id "_scaled")
        list(APPEND _defines "XE_RESOLVE_RESOLUTION_SCALED=1")
      endif()
      string(APPEND _id "_cs")
      _xe_metal_direct_host_resolve_variant(
          "${_id}" "${_resolve_host_depth_entry}" ${_defines})
    endforeach()
  endforeach()

  add_custom_target(xenia-gpu-metal-direct-host-resolve-shaders
                    DEPENDS ${_direct_host_resolve_outputs})
  add_dependencies(xenia-gpu-metal
                   xenia-gpu-metal-direct-host-resolve-shaders)
  target_include_directories(xenia-gpu-metal BEFORE PRIVATE
                             "${_direct_host_resolve_generated_root}")
endif()
xe_target_defaults(xenia-gpu-metal)
