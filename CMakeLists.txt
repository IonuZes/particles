cmake_minimum_required(VERSION 3.24)
project(particles)

set(BINARY_DIR ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${BINARY_DIR})
make_directory(${BINARY_DIR})

add_subdirectory(SDL)
add_executable(particles WIN32 main.c shader.c)
set_target_properties(particles PROPERTIES C_STANDARD 11)
target_link_libraries(particles PRIVATE SDL3::SDL3)

if(MSVC)
    set(SHADERCROSS SDL_shadercross/msvc/shadercross.exe)
else()
    message("Unsupported platform. Skipping shader building")
endif()
function(add_shader FILE)
    set(DEPENDS ${ARGN})
    set(GLSL ${CMAKE_SOURCE_DIR}/${FILE})
    set(SPV ${CMAKE_SOURCE_DIR}/bin/${FILE}.spv)
    set(DXIL ${CMAKE_SOURCE_DIR}/bin/${FILE}.dxil)
    set(MSL ${CMAKE_SOURCE_DIR}/bin/${FILE}.msl)
    set(JSON ${CMAKE_SOURCE_DIR}/bin/${FILE}.json)
    function(_add_shader PROGRAM SOURCE OUTPUT)
        if(MSVC)
            add_custom_command(
                OUTPUT ${OUTPUT}
                COMMAND ${PROGRAM} ${SOURCE} -o ${OUTPUT}
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                DEPENDS ${SOURCE} ${DEPENDS}
                COMMENT ${OUTPUT}
            )
        endif()
        get_filename_component(NAME ${OUTPUT} NAME)
        set(BINARY ${BINARY_DIR}/${NAME})
        add_custom_command(
            OUTPUT ${BINARY}
            COMMAND ${CMAKE_COMMAND} -E copy ${OUTPUT} ${BINARY}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            DEPENDS ${OUTPUT}
            COMMENT ${BINARY}
        )
        string(REPLACE . _ NAME ${NAME})
        add_custom_target(${NAME} DEPENDS ${BINARY})
        add_dependencies(particles ${NAME})
    endfunction()
    _add_shader(glslc ${GLSL} ${SPV})
    _add_shader(${SHADERCROSS} ${SPV} ${DXIL})
    _add_shader(${SHADERCROSS} ${SPV} ${MSL})
    _add_shader(${SHADERCROSS} ${SPV} ${JSON})
endfunction()
add_shader(render.frag)
add_shader(render.vert)
add_shader(update.comp)

file(COPY LICENSE.txt DESTINATION ${BINARY_DIR})
file(COPY README.md DESTINATION ${BINARY_DIR})