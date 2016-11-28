VISP_ROOT_DIR = ThirdParty/ViSP
VISP_SRC_DIR = ${VISP_ROOT_DIR}/src
VISP_INCLUDE_DIR = ${VISP_ROOT_DIR}/include
VISP_HEADER_DIR = ${VISP_INCLUDE_DIR}/mtf/${VISP_ROOT_DIR}

ViSP_HEADERS = $(addprefix  ${VISP_HEADER_DIR}/, ViSP.h)

vp ?= 0

ifeq (${vp}, 1)
THIRD_PARTY_TRACKERS += ViSP
THIRD_PARTY_RUNTIME_FLAGS += -I/usr/local/include
THIRD_PARTY_LINK_LIBS += -L/usr/local/lib/x86_64-linux-gnu -lvisp_core -lvisp_tt -lvisp_tt_mi
THIRD_PARTY_HEADERS += ${ViSP_HEADERS}
THIRD_PARTY_INCLUDE_DIRS += ${VISP_INCLUDE_DIR}
else
THIRD_PARTY_RUNTIME_FLAGS += -D DISABLE_VISP
endif

${BUILD_DIR}/ViSP.o: ${VISP_SRC_DIR}/ViSP.cc ${ViSP_HEADERS} ${ROOT_HEADER_DIR}/TrackerBase.h ${UTILITIES_HEADER_DIR}/excpUtils.h ${UTILITIES_HEADER_DIR}/miscUtils.h 
	${CXX} -c -fPIC ${WARNING_FLAGS} ${OPT_FLAGS} ${MTF_COMPILETIME_FLAGS} $< ${FLAGS64} ${FLAGSCV} -I${VISP_INCLUDE_DIR} -I${ROOT_INCLUDE_DIR} -I${UTILITIES_INCLUDE_DIR} -o $@
	