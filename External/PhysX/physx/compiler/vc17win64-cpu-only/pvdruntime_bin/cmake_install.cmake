# Install script for directory: C:/Users/2250183/Desktop/VVV/External/PhysX/physx/pvdruntime/compiler/cmake

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Users/2250183/Desktop/VVV/External/PhysX/physx/install/vc17win64-cpu-only/PhysX")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/pvdruntime/include" TYPE FILE FILES
    "C:/Users/2250183/Desktop/VVV/External/PhysX/physx/pvdruntime/include/OmniPvdLibraryFunctions.h"
    "C:/Users/2250183/Desktop/VVV/External/PhysX/physx/pvdruntime/include/OmniPvdCommands.h"
    "C:/Users/2250183/Desktop/VVV/External/PhysX/physx/pvdruntime/include/OmniPvdDefines.h"
    "C:/Users/2250183/Desktop/VVV/External/PhysX/physx/pvdruntime/include/OmniPvdReader.h"
    "C:/Users/2250183/Desktop/VVV/External/PhysX/physx/pvdruntime/include/OmniPvdWriter.h"
    "C:/Users/2250183/Desktop/VVV/External/PhysX/physx/pvdruntime/include/OmniPvdReadStream.h"
    "C:/Users/2250183/Desktop/VVV/External/PhysX/physx/pvdruntime/include/OmniPvdWriteStream.h"
    "C:/Users/2250183/Desktop/VVV/External/PhysX/physx/pvdruntime/include/OmniPvdFileReadStream.h"
    "C:/Users/2250183/Desktop/VVV/External/PhysX/physx/pvdruntime/include/OmniPvdFileWriteStream.h"
    "C:/Users/2250183/Desktop/VVV/External/PhysX/physx/pvdruntime/include/OmniPvdMemoryStream.h"
    "C:/Users/2250183/Desktop/VVV/External/PhysX/physx/pvdruntime/include/OmniPvdLibraryHelpers.h"
    "C:/Users/2250183/Desktop/VVV/External/PhysX/physx/pvdruntime/include/OmniPvdLoader.h"
    "C:/Users/2250183/Desktop/VVV/External/PhysX/physx/pvdruntime/include/OmniPvdLoader.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/win.x86_64.vc143.mt/debug" TYPE STATIC_LIBRARY OPTIONAL FILES "C:/Users/2250183/Desktop/VVV/External/PhysX/physx/bin/win.x86_64.vc143.mt/debug/PVDRuntime_64.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Cc][Hh][Ee][Cc][Kk][Ee][Dd])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/win.x86_64.vc143.mt/checked" TYPE STATIC_LIBRARY OPTIONAL FILES "C:/Users/2250183/Desktop/VVV/External/PhysX/physx/bin/win.x86_64.vc143.mt/checked/PVDRuntime_64.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Pp][Rr][Oo][Ff][Ii][Ll][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/win.x86_64.vc143.mt/profile" TYPE STATIC_LIBRARY OPTIONAL FILES "C:/Users/2250183/Desktop/VVV/External/PhysX/physx/bin/win.x86_64.vc143.mt/profile/PVDRuntime_64.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/win.x86_64.vc143.mt/release" TYPE STATIC_LIBRARY OPTIONAL FILES "C:/Users/2250183/Desktop/VVV/External/PhysX/physx/bin/win.x86_64.vc143.mt/release/PVDRuntime_64.lib")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/win.x86_64.vc143.mt/debug" TYPE SHARED_LIBRARY FILES "C:/Users/2250183/Desktop/VVV/External/PhysX/physx/bin/win.x86_64.vc143.mt/debug/PVDRuntime_64.dll")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Cc][Hh][Ee][Cc][Kk][Ee][Dd])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/win.x86_64.vc143.mt/checked" TYPE SHARED_LIBRARY FILES "C:/Users/2250183/Desktop/VVV/External/PhysX/physx/bin/win.x86_64.vc143.mt/checked/PVDRuntime_64.dll")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Pp][Rr][Oo][Ff][Ii][Ll][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/win.x86_64.vc143.mt/profile" TYPE SHARED_LIBRARY FILES "C:/Users/2250183/Desktop/VVV/External/PhysX/physx/bin/win.x86_64.vc143.mt/profile/PVDRuntime_64.dll")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/win.x86_64.vc143.mt/release" TYPE SHARED_LIBRARY FILES "C:/Users/2250183/Desktop/VVV/External/PhysX/physx/bin/win.x86_64.vc143.mt/release/PVDRuntime_64.dll")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    include("C:/Users/2250183/Desktop/VVV/External/PhysX/physx/compiler/vc17win64-cpu-only/pvdruntime_bin/CMakeFiles/PVDRuntime.dir/install-cxx-module-bmi-debug.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Cc][Hh][Ee][Cc][Kk][Ee][Dd])$")
    include("C:/Users/2250183/Desktop/VVV/External/PhysX/physx/compiler/vc17win64-cpu-only/pvdruntime_bin/CMakeFiles/PVDRuntime.dir/install-cxx-module-bmi-checked.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Pp][Rr][Oo][Ff][Ii][Ll][Ee])$")
    include("C:/Users/2250183/Desktop/VVV/External/PhysX/physx/compiler/vc17win64-cpu-only/pvdruntime_bin/CMakeFiles/PVDRuntime.dir/install-cxx-module-bmi-profile.cmake" OPTIONAL)
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    include("C:/Users/2250183/Desktop/VVV/External/PhysX/physx/compiler/vc17win64-cpu-only/pvdruntime_bin/CMakeFiles/PVDRuntime.dir/install-cxx-module-bmi-release.cmake" OPTIONAL)
  endif()
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Users/2250183/Desktop/VVV/External/PhysX/physx/compiler/vc17win64-cpu-only/pvdruntime_bin/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
