# Keen for iOS: turns Commander Genius into the "Keen" app bundle.
# Included from src/CMakeLists.txt when IOS is set (see .github/workflows/ios.yml).
#
# The app: name "Keen", bundle id io.github.sithewok13.keen, version
# 0.1.<KEEN_IOS_BUILD_NUMBER>. Players copy their own game folders (KEEN1 ...)
# into Files > On My iPhone > Keen > games. The app carries only Commander
# Genius's own files (vfsroot: menus, fonts, scripts), no game data.

set(KEEN_IOS_DIR ${CMAKE_SOURCE_DIR}/ios/keen)
if(NOT KEEN_IOS_BUILD_NUMBER)
  set(KEEN_IOS_BUILD_NUMBER 1)
endif()
if(NOT KEEN_IOS_MIN)
  set(KEEN_IOS_MIN 15.0)
endif()

enable_language(OBJCXX)
add_executable(Keen MACOSX_BUNDLE ${KEEN_IOS_DIR}/main.mm)
set_source_files_properties(${KEEN_IOS_DIR}/main.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
target_include_directories(Keen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${SDL2_INCLUDE_DIR})
target_link_libraries(Keen PRIVATE CGeniusExe
  "-framework Foundation"
  "-framework UIKit"
  "-framework QuartzCore"
  "-framework CoreGraphics"
  "-framework CoreVideo"
  "-framework CoreMotion"
  "-framework CoreAudio"
  "-framework AudioToolbox"
  "-framework AVFoundation"
  "-framework GameController"
  "-framework Metal"
  "-framework OpenGLES"
  "-framework ImageIO"
  "-framework MobileCoreServices"
  "-framework Security"
  "-framework SystemConfiguration"
  "-framework CFNetwork"
  "-weak_framework CoreHaptics"
  "-weak_framework UniformTypeIdentifiers"
  iconv
  z
)

set_target_properties(Keen PROPERTIES
  OUTPUT_NAME Keen
  MACOSX_BUNDLE_INFO_PLIST ${KEEN_IOS_DIR}/Info.plist.in
  MACOSX_BUNDLE_GUI_IDENTIFIER io.github.sithewok13.keen
  MACOSX_BUNDLE_BUNDLE_NAME Keen
  MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.${KEEN_IOS_BUILD_NUMBER}"
  MACOSX_BUNDLE_BUNDLE_VERSION ${KEEN_IOS_BUILD_NUMBER}
)

# Commander Genius's own files next to the program (found through ${BIN},
# see GsKit/base/interface/FindFile.cpp) and the home-screen icon
add_custom_command(TARGET Keen POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/vfsroot $<TARGET_BUNDLE_DIR:Keen>
  COMMAND sips -z 120 120 ${CMAKE_SOURCE_DIR}/vfsroot/cglogo512.png --out $<TARGET_BUNDLE_DIR:Keen>/AppIcon60x60@2x.png
  COMMAND sips -z 180 180 ${CMAKE_SOURCE_DIR}/vfsroot/cglogo512.png --out $<TARGET_BUNDLE_DIR:Keen>/AppIcon60x60@3x.png
  VERBATIM
)
