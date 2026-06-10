# CMake generated Testfile for 
# Source directory: C:/Users/nikit/OneDrive/documents/Nikreon/external/NikreonUI
# Build directory: C:/Users/nikit/OneDrive/documents/Nikreon/build-rel/external/NikreonUI
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test([=[NikreonUIWidgetTests]=] "C:/Users/nikit/OneDrive/documents/Nikreon/build-rel/external/NikreonUI/Debug/NikreonUIWidgetTests.exe")
  set_tests_properties([=[NikreonUIWidgetTests]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/nikit/OneDrive/documents/Nikreon/external/NikreonUI/CMakeLists.txt;74;add_test;C:/Users/nikit/OneDrive/documents/Nikreon/external/NikreonUI/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test([=[NikreonUIWidgetTests]=] "C:/Users/nikit/OneDrive/documents/Nikreon/build-rel/external/NikreonUI/Release/NikreonUIWidgetTests.exe")
  set_tests_properties([=[NikreonUIWidgetTests]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/nikit/OneDrive/documents/Nikreon/external/NikreonUI/CMakeLists.txt;74;add_test;C:/Users/nikit/OneDrive/documents/Nikreon/external/NikreonUI/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test([=[NikreonUIWidgetTests]=] "C:/Users/nikit/OneDrive/documents/Nikreon/build-rel/external/NikreonUI/MinSizeRel/NikreonUIWidgetTests.exe")
  set_tests_properties([=[NikreonUIWidgetTests]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/nikit/OneDrive/documents/Nikreon/external/NikreonUI/CMakeLists.txt;74;add_test;C:/Users/nikit/OneDrive/documents/Nikreon/external/NikreonUI/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test([=[NikreonUIWidgetTests]=] "C:/Users/nikit/OneDrive/documents/Nikreon/build-rel/external/NikreonUI/RelWithDebInfo/NikreonUIWidgetTests.exe")
  set_tests_properties([=[NikreonUIWidgetTests]=] PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/nikit/OneDrive/documents/Nikreon/external/NikreonUI/CMakeLists.txt;74;add_test;C:/Users/nikit/OneDrive/documents/Nikreon/external/NikreonUI/CMakeLists.txt;0;")
else()
  add_test([=[NikreonUIWidgetTests]=] NOT_AVAILABLE)
endif()
