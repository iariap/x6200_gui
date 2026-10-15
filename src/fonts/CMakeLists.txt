# The font families are generated (tools/fonts/fonts.py) and the list is data:
# src/fonts/font_families.h carries the families, their FONT_* values and their
# symbols, and this directory holds one .c per family and size. Globbing them
# keeps a new candidate from being compiled-but-unnamed (or named but not
# compiled), which a hand-written list of ~270 files would not.
file(GLOB FONT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.c)
target_sources(${PROJECT_NAME} PUBLIC ${FONT_SOURCES})
