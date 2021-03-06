##############################################################################
# In this file we handle all env and customer's settings
##############################################################################

##############################################################################
# Setup build and dependencies information 
##############################################################################
SET(DEPENDENCY_INSTALL_PREFIX "/opt/dependency")
IF($ENV{DEPENDENCY_INSTALL_PREFIX})
	SET(DEPENDENCY_INSTALL_PREFIX $ENV{DEPENDENCY_INSTALL_PREFIX})
ENDIF()

SET(DEPENDENCY_DIST_PACKAGE_NAME "dependency-dist-package.tar.gz")
IF($ENV{DEPENDENCY_DIST_PACKAGE_NAME})
	SET(DEPENDENCY_DIST_PACKAGE_NAME $ENV{DEPENDENCY_DIST_PACKAGE_NAME})
ENDIF()

SET(CMAKE_PREFIX_PATH "${DEPENDENCY_INSTALL_PREFIX}" ${CMAKE_PREFIX_PATH})
SET(CMAKE_PREFIX_PATH "${DEPENDENCY_INSTALL_PREFIX}/package" ${CMAKE_PREFIX_PATH})
SET(CMAKE_PREFIX_PATH "${DEPENDENCY_INSTALL_PREFIX}/tools" ${CMAKE_PREFIX_PATH})

SET(DEPENDENCY_LIBRARY_PATH "${DEPENDENCY_INSTALL_PREFIX}/package/lib:${DEPENDENCY_LIBRARY_PATH}")
SET(DEPENDENCY_LIBRARY_PATH "${DEPENDENCY_INSTALL_PREFIX}/package/lib64:${DEPENDENCY_LIBRARY_PATH}")

##############################################################################
# Setup build flags
##############################################################################
OPTION(ENABLE_COVERAGE "enable code coverage." OFF)

IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

IF(ENABLE_COVERAGE STREQUAL ON)
    INCLUDE(CodeCoverage)
ENDIF(ENABLE_COVERAGE STREQUAL ON)

IF(CMAKE_BUILD_TYPE MATCHES Debug)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")    
ENDIF()

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fno-omit-frame-pointer -fno-strict-aliasing")

IF(ENABLE_AVX STREQUAL ON)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx -mno-avx2")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAVX_OPT")
ELSE()
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mno-avx -mno-avx2")
ENDIF()

#c++11 is needed to provide thread saft singleton implementation. 
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wno-deprecated-register")
#-Rpass-missed=loop-vectorize  -Wall -Wconversion

IF(CMAKE_COMPILER_IS_CLANG)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-limit-debug-info -stdlib=libc++ -DUSE_CLANG")
    IF(OS_LINUX)
        SET(CLANG_LDFLAGS "-lc++abi -lc++" ${CLANG_LDFLAGS})
    ENDIF(OS_LINUX)
ENDIF(CMAKE_COMPILER_IS_CLANG)

TRY_COMPILE(INT64T_EQUAL_LONGLONG
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/CMake/CMakeTestCompileInt64tType.cc
    OUTPUT_VARIABLE OUTPUT)

IF(INT64T_EQUAL_LONGLONG)
    MESSAGE(STATUS "Checking whether int64_t is typedef to long long -- yes")
ELSE(INT64T_EQUAL_LONGLONG)
    MESSAGE(STATUS "Checking whether int64_t is typedef to long long -- no")
ENDIF(INT64T_EQUAL_LONGLONG)
