cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(pico_rtos C CXX ASM)

pico_sdk_init()

add_library(pico_rtos STATIC
    src/mutex.c
    src/queue.c
    src/semaphore.c
    src/task.c
    src/timer.c
)

target_include_directories(pico_rtos PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(pico_rtos
    pico_stdlib
)

add_subdirectory(examples/led_blinking)
add_subdirectory(examples/task_synchronization)

add_executable(mutex_test tests/mutex_test.c)
target_link_libraries(mutex_test pico_rtos)

add_executable(queue_test tests/queue_test.c)
target_link_libraries(queue_test pico_rtos)

add_executable(semaphore_test tests/semaphore_test.c)
target_link_libraries(semaphore_test pico_rtos)

add_executable(task_test tests/task_test.c)
target_link_libraries(task_test pico_rtos)

add_executable(timer_test tests/timer_test.c)
target_link_libraries(timer_test pico_rtos)