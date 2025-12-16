cmake_minimum_required(VERSION 3.15)
project(Inferno VERSION 4.0.0 LANGUAGES CXX)

# تعيين معايير C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# إعدادات النظام
if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DUNICODE -D_UNICODE)
    set(CMAKE_RC_COMPILER_INIT windres)
    enable_language(RC)
endif()

# ملفات المصادر
set(SOURCES
    inferno.cpp
)

# ملفات الرأس
set(HEADERS
    # يمكن إضافة ملفات الرأس هنا لاحقاً
)

# ملفات الموارد
set(RESOURCES
    resources.rc
)

# إنشاء الهدف التنفيذي
add_executable(inferno ${SOURCES} ${HEADERS} ${RESOURCES})

# إعدادات خاصة بـ Windows
if(WIN32)
    # روابط مكتبات Windows
    target_link_libraries(inferno
        comctl32
        shell32
        setupapi
        winmm
        shlwapi
        ole32
        uuid
        gdi32
        user32
        advapi32
    )
    
    # إعدادات المانيفست
    if(MSVC)
        set_target_properties(inferno PROPERTIES
            WIN32_EXECUTABLE TRUE
            LINK_FLAGS "/SUBSYSTEM:WINDOWS /MANIFEST:NO"
        )
    elseif(MINGW)
        target_link_options(inferno PRIVATE -mwindows)
    endif()
    
    # إعدادات المانيفست
    set_target_properties(inferno PROPERTIES
        RC_FLAGS "-DVER_MAJOR=4 -DVER_MINOR=0 -DVER_PATCH=0"
    )
endif()

# إعدادات الإصدار
target_compile_definitions(inferno PRIVATE
    INFERNO_VERSION="4.0.0"
    INFERNO_BUILD="2024.01"
)

# نسخ الملفات بعد البناء
if(WIN32)
    add_custom_command(TARGET inferno POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/inferno.png"
        "${CMAKE_BINARY_DIR}/inferno.png"
        COMMENT "نسخ ملف الأيقونة"
    )
endif()

# تثبيت
if(NOT CMAKE_SKIP_INSTALL_RULES)
    install(TARGETS inferno
        RUNTIME DESTINATION bin
        BUNDLE DESTINATION .
    )
    
    if(EXISTS "${CMAKE_SOURCE_DIR}/inferno.png")
        install(FILES "${CMAKE_SOURCE_DIR}/inferno.png"
            DESTINATION bin
        )
    endif()
endif()
