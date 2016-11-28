option(WITH_VISP "Enable ViSP Template Trackers" ON)
if(WITH_VISP)
	find_package(VISP)
	if(VISP_FOUND)
		set(LIBS_LEARNING ${LIBS_LEARNING} -L/usr/local/lib/x86_64-linux-gnu ${VISP_LIBRARIES})
		set(THIRD_PARTY_INCLUDE_DIRS ${THIRD_PARTY_INCLUDE_DIRS} ViSP/include)
		set(THIRD_PARTY_EXT_INCLUDE_DIRS ${THIRD_PARTY_EXT_INCLUDE_DIRS} ${VISP_INCLUDE_DIRS})
		set(LEARNING_TRACKERS ${LEARNING_TRACKERS} ViSP/src/ViSP)
	else(VISP_FOUND)
		set(THIRD_PARTY_RUNTIME_FLAGS ${THIRD_PARTY_RUNTIME_FLAGS} -DDISABLE_VISP)
		message(STATUS "ViSP not found so its template tracker module cannot be enabled")
	endif(VISP_FOUND)	
else(WITH_VISP)		
	set(THIRD_PARTY_RUNTIME_FLAGS ${THIRD_PARTY_RUNTIME_FLAGS} -DDISABLE_VISP)
	message(STATUS "ViSP disabled")
	# message(STATUS "ViSP: THIRD_PARTY_RUNTIME_FLAGS: ${THIRD_PARTY_RUNTIME_FLAGS}")
endif(WITH_VISP)
