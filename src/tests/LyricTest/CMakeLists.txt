project(LyricTest VERSION 0.0.0.1)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

ck_add_files(_src
        CURRENT
        DIRS gui
        PATTERNS *.h *.cpp res.qrc
        )

ck_add_test(${PROJECT_NAME})

ck_target_components(${PROJECT_NAME}
    INCLUDE_CURRENT_PRIVATE
    SOURCES ${_src}
    QT_LINKS Core Widgets Network
    LINKS QsFramework
    LINKS_PRIVATE CkIKg2p
)