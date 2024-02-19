SET(OPENTELEMETRY_CPP_TAG "opentelemetry-cpp-1.10.0" CACHE INTERNAL "")

add_library(otel_api INTERFACE)

# Note: For now make it phony target on platforms other than Linux

if(NOT (WIN32 OR APPLE OR CMAKE_SYSTEM_NAME MATCHES "SunOS"))

  message(STATUS "Adding OTel support")

  set(TELEMETRY ON CACHE INTERNAL "Whether connector is built with OTel support")

  target_include_directories(otel_api INTERFACE
    "${PROJECT_SOURCE_DIR}/extra/otel/${OPENTELEMETRY_CPP_TAG}/api/include"
  )

  target_compile_definitions(otel_api INTERFACE TELEMETRY)

  if(WIN32)
    # Note: warning C4996 is triggered by OTel headers (as of version 1.10.0)
    target_compile_definitions(otel_api INTERFACE _SILENCE_CXX17_RESULT_OF_DEPRECATION_WARNING)
  endif()

endif()

