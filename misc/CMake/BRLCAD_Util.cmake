# Pretty-printing macro that generates a box around a string and prints the
# resulting message.
MACRO(BOX_PRINT input_string border_string)
	STRING(LENGTH ${input_string} MESSAGE_LENGTH)
	STRING(LENGTH ${border_string} SEPARATOR_STRING_LENGTH)
	WHILE(${MESSAGE_LENGTH} GREATER ${SEPARATOR_STRING_LENGTH})
		SET(SEPARATOR_STRING "${SEPARATOR_STRING}${border_string}")
		STRING(LENGTH ${SEPARATOR_STRING} SEPARATOR_STRING_LENGTH)
	ENDWHILE(${MESSAGE_LENGTH} GREATER ${SEPARATOR_STRING_LENGTH})
	MESSAGE("${SEPARATOR_STRING}")
	MESSAGE("${input_string}")
	MESSAGE("${SEPARATOR_STRING}")
ENDMACRO()

# Windows builds need a DLL variable defined per-library, and BRL-CAD
# uses a fairly standard convention - try and automate the addition of
# the definition.
MACRO(DLL_DEFINE libname)
		  IF(MSVC)
					 STRING(REGEX REPLACE "lib" "" LOWERCORE "${libname}")
					 STRING(TOUPPER ${LOWERCORE} UPPER_CORE)
					 add_definitions(
								-D${UPPER_CORE}_EXPORT_DLL
								)
		  ENDIF(MSVC)
ENDMACRO()

# Core routines for adding executables and libraries to the build and
# install lists of CMake
MACRO(BRLCAD_ADDEXEC execname srcs libs)
  STRING(REGEX REPLACE " " ";" srcslist "${srcs}")
  STRING(REGEX REPLACE " " ";" libslist1 "${libs}")
  STRING(REGEX REPLACE "-framework;" "-framework " libslist "${libslist1}")
  add_executable(${execname} ${srcslist})
  target_link_libraries(${execname} ${libslist})
  INSTALL(TARGETS ${execname} RUNTIME DESTINATION ${BRLCAD_INSTALL_BIN_DIR})
ENDMACRO(BRLCAD_ADDEXEC execname srcs libs)

MACRO(BRLCAD_ADDLIB libname srcs libs)
  STRING(REGEX REPLACE " " ";" srcslist "${srcs}")
  STRING(REGEX REPLACE " " ";" libslist1 "${libs}")
  STRING(REGEX REPLACE "-framework;" "-framework " libslist "${libslist1}")
  DLL_DEFINE(${libname})
  IF(BUILD_SHARED_LIBS)
	  add_library(${libname} SHARED ${srcslist})
	  if(NOT ${libs} MATCHES "NONE")
		  target_link_libraries(${libname} ${libslist})
	  endif(NOT ${libs} MATCHES "NONE")
	  INSTALL(TARGETS ${libname} DESTINATION ${BRLCAD_INSTALL_LIB_DIR})
  ENDIF(BUILD_SHARED_LIBS)
  IF(BUILD_STATIC_LIBS)
	  add_library(${libname}-static STATIC ${srcslist})
	  if(NOT ${libs} MATCHES "NONE")
		  target_link_libraries(${libname}-static ${libslist})
	  endif(NOT ${libs} MATCHES "NONE")
	  SET_TARGET_PROPERTIES(${libname}-static PROPERTIES OUTPUT_NAME "${libname}")
	  IF(WIN32)
		  # We need the lib prefix on win32, so add it even if our add_library
		  # wrapper function has removed it due to the target name - see
		  # http://www.cmake.org/Wiki/CMake_FAQ#How_do_I_make_my_shared_and_static_libraries_have_the_same_root_name.2C_but_different_suffixes.3F
		  SET_TARGET_PROPERTIES(${libname}-static PROPERTIES PREFIX "lib")
	  ENDIF(WIN32)
	  INSTALL(TARGETS ${libname}-static DESTINATION ${BRLCAD_INSTALL_LIB_DIR})
  ENDIF(BUILD_STATIC_LIBS)

  # Enable extra compiler flags if local libraries and/or global options dictate
  SET(LOCAL_COMPILE_FLAGS "")
  FOREACH(extraarg ${ARGN})
	  IF(${extraarg} MATCHES "STRICT" AND BRLCAD-ENABLE_STRICT)
		  SET(LOCAL_COMPILE_FLAGS "${LOCAL_COMPILE_FLAGS} ${STRICT_FLAGS}")
	  ENDIF(${extraarg} MATCHES "STRICT" AND BRLCAD-ENABLE_STRICT)
  ENDFOREACH(extraarg ${ARGN})
  IF(LOCAL_COMPILE_FLAGS)
	  IF(BUILD_SHARED_LIBS)
		  SET_TARGET_PROPERTIES(${libname} PROPERTIES COMPILE_FLAGS ${LOCAL_COMPILE_FLAGS})
	  ENDIF(BUILD_SHARED_LIBS)
	  IF(BUILD_STATIC_LIBS)
		  SET_TARGET_PROPERTIES(${libname}-static PROPERTIES COMPILE_FLAGS ${LOCAL_COMPILE_FLAGS})
	  ENDIF(BUILD_STATIC_LIBS)
  ENDIF(LOCAL_COMPILE_FLAGS)

ENDMACRO(BRLCAD_ADDLIB libname srcs libs)
