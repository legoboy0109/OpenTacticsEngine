# The native editor (Qt 6 Widgets). Only configured with -DOTE_BUILD_NATIVE_EDITOR=ON; see docs/NATIVE_EDITOR.md.
#
#   ote_native_model  document model, commands, project, problems runner (QtCore + QtGui for QUndoStack, no widgets)
#   ote_native_ui     item models, editors, dialogs, main window (QtWidgets)
#   ote-editor-native the program (working name; the user-visible names are all in names.h)
#
# Qt needs exceptions and RTTI (moc output, Qt's headers), but ote_core hands -fno-exceptions -fno-rtti to every program
# that links it. The top-level CMakeLists leaves those two flags off any target that has the property
# OTE_RTTI_EXCEPTIONS_OK set, so every target of this folder sets it (ote_native_target below) and compiles with the
# compiler's normal C++ defaults. The engine libraries themselves stay built without RTTI; only plain, non-polymorphic
# types (ote::Json, the editor core's functions) cross that line, which is safe.
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Test)
set(CMAKE_AUTOMOC ON)
enable_testing()

function(ote_native_target tgt)
  set_target_properties(${tgt} PROPERTIES OTE_RTTI_EXCEPTIONS_OK ON)
  target_compile_options(${tgt} PRIVATE ${OTE_WARN})
endfunction()

file(GLOB NATIVE_MODEL_SRC CONFIGURE_DEPENDS model/*.cpp)
add_library(ote_native_model STATIC ${NATIVE_MODEL_SRC})
ote_native_target(ote_native_model)
target_include_directories(ote_native_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} model)
target_link_libraries(ote_native_model PUBLIC Qt6::Core Qt6::Gui ote_editor_core)  # QUndoStack lives in QtGui

file(GLOB NATIVE_UI_SRC CONFIGURE_DEPENDS ui/*.cpp)
add_library(ote_native_ui STATIC ${NATIVE_UI_SRC})
ote_native_target(ote_native_ui)
target_include_directories(ote_native_ui PUBLIC ui)
target_link_libraries(ote_native_ui PUBLIC Qt6::Widgets ote_native_model)

# The schema files (editor/schema/*.json) are also compiled into the program, so it works without them next to it.
# At run time a folder next to the program ("schema") or one named with --schema-dir / OTE_SCHEMA_DIR wins.
file(GLOB OTE_SCHEMA_FILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/editor/schema/*.json)
qt_add_resources(ote_native_ui ote_schemas PREFIX "/schema" BASE "${CMAKE_SOURCE_DIR}/editor/schema" FILES ${OTE_SCHEMA_FILES})

add_executable(ote-editor-native main.cpp)
ote_native_target(ote-editor-native)
target_link_libraries(ote-editor-native PRIVATE ote_native_ui)

# Tests: QtTest programs, run headless (QT_QPA_PLATFORM=offscreen, no window is ever shown on the desktop) with their
# settings and scratch files in a temp folder. Registered only here, i.e. only when the option is ON.
if(OTE_BUILD_TESTS)
  file(GLOB NATIVE_TEST_SRC CONFIGURE_DEPENDS tests/test_*.cpp)
  foreach(src IN LISTS NATIVE_TEST_SRC)
    get_filename_component(name ${src} NAME_WE)
    add_executable(native_${name} ${src})
    ote_native_target(native_${name})
    target_include_directories(native_${name} PRIVATE tests)
    target_link_libraries(native_${name} PRIVATE ote_native_ui Qt6::Test)
    target_compile_definitions(native_${name} PRIVATE OTE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME native_${name} COMMAND native_${name})
    # QTEST_FUNCTION_TIMEOUT: QtTest aborts a test function after 5 minutes by default. The conformance test writes a
    # game folder and runs the whole project check once per schema key (thousands), which takes minutes on a busy
    # machine with the scratch folder on disk; give every native test half an hour instead of a false failure.
    set_tests_properties(native_${name} PROPERTIES TIMEOUT 1800
      ENVIRONMENT "QT_QPA_PLATFORM=offscreen;OTE_TEST_TMP=${CMAKE_BINARY_DIR}/test-tmp;QTEST_FUNCTION_TIMEOUT=1800000")
  endforeach()
  # The conformance test's tables (which fixture tries which key) live in small files of their own.
  file(GLOB CONFORMANCE_TABLES CONFIGURE_DEPENDS tests/conformance_*.cpp)
  target_sources(native_test_conformance PRIVATE ${CONFORMANCE_TABLES})
endif()
