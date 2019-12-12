project(precision-gamepad-source)

if(MSVC)
	set(precision-gamepad-source_PLATFORM_DEPS
		w32-pthreads)
endif()

set(precision-gamepad-source_SOURCES
	precision-gamepad-source.c)

add_library(precision-gamepad-source MODULE
	${precision-gamepad-source_SOURCES})
target_link_libraries(precision-gamepad-source
	libobs
	XInput
	${precision-gamepad-source_PLATFORM_DEPS})
