# Copyright (C) 2020 by Franco Venturi
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

### shared library
add_library(rf103 SHARED
    librf103.c
    ezusb.c
    logging.c
    usb_device.c
    clock_source.c
    adc.c
    tuner.c
)
set_target_properties(rf103 PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(rf103 PROPERTIES SOVERSION 0)

target_include_directories(rf103 PUBLIC
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>  # <prefix>/include
)
target_link_libraries(rf103 PkgConfig::LIBUSB)


# applications
add_executable(rf103_test rf103_test.c)
target_link_libraries(rf103_test rf103)
add_executable(rf103_stream_test rf103_stream_test.c wavewrite.c)
target_link_libraries(rf103_stream_test rf103)
add_executable(rf103_vhf_stream_test rf103_vhf_stream_test.c wavewrite.c)
target_link_libraries(rf103_vhf_stream_test rf103)


# install
install(TARGETS rf103
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(TARGETS rf103_test rf103_stream_test rf103_vhf_stream_test
  DESTINATION ${CMAKE_INSTALL_BINDIR}
)
