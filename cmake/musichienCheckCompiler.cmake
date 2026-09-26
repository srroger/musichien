# =====================================================================================================================
# Compiler checks
#
# Fails early with an explicit message rather than producing hundreds of cryptic errors later.
# =====================================================================================================================

set(MUSICHIEN_MINIMUM_CLANG_VERSION 17.0)
set(MUSICHIEN_MINIMUM_GCC_VERSION   14.0)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")

    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS ${MUSICHIEN_MINIMUM_CLANG_VERSION})
        message(FATAL_ERROR
            "Musichien requires Clang ${MUSICHIEN_MINIMUM_CLANG_VERSION} or newer "
            "(found ${CMAKE_CXX_COMPILER_VERSION}). See docs/BUILD_AND_SETUP.md.")
    endif()

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")

    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS ${MUSICHIEN_MINIMUM_GCC_VERSION})
        message(FATAL_ERROR
            "Musichien requires GCC ${MUSICHIEN_MINIMUM_GCC_VERSION} or newer "
            "(found ${CMAKE_CXX_COMPILER_VERSION}). See docs/BUILD_AND_SETUP.md.")
    endif()

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")

    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16.0)
        message(FATAL_ERROR
            "Musichien requires AppleClang 16 or newer "
            "(found ${CMAKE_CXX_COMPILER_VERSION}). See docs/BUILD_AND_SETUP.md.")
    endif()

else()
    message(FATAL_ERROR
        "Musichien is only supported with Clang, AppleClang or GCC "
        "(found '${CMAKE_CXX_COMPILER_ID}'). See docs/BUILD_AND_SETUP.md.")
endif()

# The C++26 standard is verified at configure time: if the compiler silently falls back to an older
# standard, ranges / concepts / std::expected would produce confusing errors much later.
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

message(STATUS "Musichien: C++ compiler is ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} (C++26 required)")
