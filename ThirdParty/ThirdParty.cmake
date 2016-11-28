option(WITH_THIRD_PARTY "Enable Third Party Trackers" OFF)
set(THIRD_PARTY_RUNTIME_FLAGS "")

if(WITH_THIRD_PARTY)
	set(LEARNING_TRACKERS "")
	set(LIBS_LEARNING "")
	set(THIRD_PARTY_INCLUDE_DIRS "")
	set(THIRD_PARTY_SPECIFIC_SRC "")
	set(THIRD_PARTY_SPECIFIC_PROPERTIES "")
	set(THIRD_PARTY_EXT_INCLUDE_DIRS "")
	set(MTF_THIRD_PARTY_LIB_SRC "")
	
	set(THIRD_PARTY_SUB_DIRS DSST KCF CMT RCT TLD Struck MIL DFT FRG PFSL3 ViSP GOTURN Xvision)
	foreach(SUB_DIR ${THIRD_PARTY_SUB_DIRS})
	  include(ThirdParty/${SUB_DIR}/${SUB_DIR}.cmake)
	endforeach(SUB_DIR)
	
	addPrefixAndSuffix("${LEARNING_TRACKERS}" "ThirdParty/" ".cc" LEARNING_SRC)
	addPrefix("${THIRD_PARTY_INCLUDE_DIRS}" "ThirdParty/" THIRD_PARTY_INCLUDE_DIRS_ABS)
	addPrefix("${THIRD_PARTY_SPECIFIC_SRC}" "ThirdParty/" THIRD_PARTY_SPECIFIC_SRC_ABS)
	set(MTF_SRC ${MTF_SRC} ${LEARNING_SRC})
	set(MTF_LIBS ${MTF_LIBS} ${LIBS_LEARNING})
	set(MTF_INCLUDE_DIRS ${MTF_INCLUDE_DIRS} ${THIRD_PARTY_INCLUDE_DIRS_ABS})
	set(MTF_EXT_INCLUDE_DIRS ${MTF_EXT_INCLUDE_DIRS} ${THIRD_PARTY_EXT_INCLUDE_DIRS})
	set(MTF_SPECIFIC_SRC ${MTF_SPECIFIC_SRC} ${THIRD_PARTY_SPECIFIC_SRC_ABS})
	set(MTF_SPECIFIC_PROPERTIES ${MTF_SPECIFIC_PROPERTIES} ${THIRD_PARTY_SPECIFIC_PROPERTIES})	
	
	# addPrefix("${MTF_THIRD_PARTY_LIB_SRC}" "ThirdParty/" MTF_THIRD_PARTY_LIB_SRC_ABS)
	# add_library (mtf_thirdparty SHARED ${MTF_THIRD_PARTY_LIB_SRC})
	# target_compile_options(mtf PUBLIC ${WARNING_FLAGS} ${CT_FLAGS})
	# target_link_libraries(mtf ${MTF_LIBS} ${LIBS_LEARNING})
	# target_include_directories(mtf PUBLIC ${MTF_INCLUDE_DIRS} ${MTF_EXT_INCLUDE_DIRS})
	
else(WITH_THIRD_PARTY)
	message(STATUS "Third party trackers disabled")
	set(THIRD_PARTY_RUNTIME_FLAGS ${THIRD_PARTY_RUNTIME_FLAGS} -DDISABLE_THIRD_PARTY_TRACKERS -DDISABLE_XVISION -DDISABLE_VISP -DDISABLE_PFSL3)
endif(WITH_THIRD_PARTY)
set(MTF_RUNTIME_FLAGS ${MTF_RUNTIME_FLAGS} ${THIRD_PARTY_RUNTIME_FLAGS})
