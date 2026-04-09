function(ipass_copy_webgpu_runtime TARGET_NAME)
  if(EMSCRIPTEN)
    return()
  endif()

  if(COMMAND target_copy_webgpu_binaries)
    target_copy_webgpu_binaries(${TARGET_NAME})
  else()
    message(WARNING
      "ipass_copy_webgpu_runtime: target_copy_webgpu_binaries not found. "
      "WebGPU runtime may not be copied for target '${TARGET_NAME}'. "
      "Make sure FetchContent_MakeAvailable(webgpu) has been called first."
    )
  endif()
endfunction()
