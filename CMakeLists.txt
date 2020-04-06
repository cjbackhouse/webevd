# ======================================================================
#  larsoft main build file
#
#  cd .../path/to/build/directory
#  source .../path/to/larsoft/ups/setup_for_development <-d|-p>
#  cmake [-DCMAKE_INSTALL_PREFIX=/install/path]
#        -DCMAKE_BUILD_TYPE=$CETPKG_TYPE
#        $CETPKG_SOURCE
#  make
#  make test
#  make install
#  make package (builds distribution tarfile)
# ======================================================================
# LarSoft is an empty product
# It sets up all of the larsoft ups products (lardata, larsim, larreco, etc.)
# ======================================================================


# use cmake 2.8 or later
cmake_minimum_required (VERSION 2.8)

project(webevd)

# cetbuildtools contains our cmake modules
SET ( CETBUILDTOOLS_VERSION $ENV{CETBUILDTOOLS_VERSION} )
IF (NOT CETBUILDTOOLS_VERSION)
    MESSAGE (FATAL_ERROR "ERROR: setup cetbuildtools to get the cmake modules")
ENDIF()

set(CMAKE_MODULE_PATH $ENV{CANVAS_ROOT_IO_DIR}/Modules
                      $ENV{CETBUILDTOOLS_DIR}/Modules
                      ${CMAKE_MODULE_PATH})

include(CetCMakeEnv)
cet_cmake_env()

set_install_root()

cet_set_compiler_flags(DIAGS CAUTIOUS
  WERROR
  NO_UNDEFINED
  EXTRA_FLAGS -pedantic -Wno-unused-local-typedefs
)

cet_report_compiler_flags()

# these are minimum required versions, not the actual product versions
find_ups_product( larcoreobj )
find_ups_product( larcore v1_00_00 )
find_ups_product( larevt v1_00_00 )
find_ups_product( lardataobj )
find_ups_product( lardataalg )
find_ups_product( lardata v1_00_00 )
find_ups_product( nutools v1_00_00 )
find_ups_product( nurandom )
find_ups_product( art v1_09_00 )
find_ups_product( art_root_io )
find_ups_product( clhep v2_1_3_0 )
find_ups_product( messagefacility v1_10_00 )
find_ups_product( cetbuildtools v3_10_00 )
find_ups_product( gallery ) # TODO do I need gallery explicitly in ups/product_deps ?
find_ups_product( dunetpc ) # Because of dune/ArtSupport/ -- that needs to be moved

# macros for dictionary and simple_plugin
include(ArtDictionary)
include(ArtMake)
include(BuildPlugins)


add_subdirectory(webevd)
add_subdirectory(test)
add_subdirectory(ups)

# packaging utility
include(UseCPack)
