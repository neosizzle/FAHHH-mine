set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)

add_subdirectory(packer)
add_executable(Famine)
target_sources(Famine
	PRIVATE
        main.asm
    PUBLIC
		FILE_SET HEADERS
		BASE_DIRS
		.
		FILES
		main.inc
)
target_link_options(Famine PRIVATE
    -nostdlib
    -static
)
option(ROOT_MODE "Enable root mode" OFF)
option(DISPLAY_INFECT "Enable infection logging" ON)

foreach(FLAG ROOT_MODE DISPLAY_INFECT)
    if(${FLAG})
        target_compile_options(Famine PRIVATE
            $<$<COMPILE_LANGUAGE:ASM_NASM>:-D${FLAG}>
        )
    endif()
endforeach()


# after the executable Famine is compiled, run the application 'packer' on Famine
add_custom_command(
    TARGET Famine
    POST_BUILD
    COMMAND $<TARGET_FILE:packer> $<TARGET_FILE:Famine> $<TARGET_FILE_DIR:Famine>/Famine.packed || true
    COMMENT "Packing Famine"
    VERBATIM
)

