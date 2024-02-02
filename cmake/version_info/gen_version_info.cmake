message("Re-generating version info resource defintions: ${RC}")
message("For output file: ${OUTPUT}")
message("Of type: ${TYPE}")
message("Build configuration: ${CONFIG}")

include("${VERSION}")


set(PATH "${OUTPUT}")
get_filename_component(BASE "${OUTPUT}" NAME)
get_filename_component(BASE_WE "${OUTPUT}" NAME_WE)

set(ODBC_VERSION "${CONNECTOR_NUMERIC_VERSION}")
string(REPLACE "." "," ODBC_VERSION_RAW "${ODBC_VERSION}")

if("STATIC_LIBRARY" STREQUAL TYPE)
  set(FILETYPE "VFT_STATIC_LIB")
elseif(TYPE MATCHES "_LIBRARY")
  set(FILETYPE "VFT_DLL")
elseif("EXECUTABLE" STREQUAL TYPE)
  set(FILETYPE "VFT_APP")
else()
  set(FILETYPE 0)
endif()

set(FILEFLAGS 0)
if(CONFIG MATCHES "[Dd][Ee][Bb][Uu][Gg]")
  set(FILEFLAGS "VS_FF_DEBUG")
endif()


configure_file("${CMAKE_CURRENT_LIST_DIR}/version_info.rc.in" "${RC}" @ONLY)
