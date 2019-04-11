cmake_minimum_required (VERSION 2.6)
project(ZipCodes)
add_executable(read_list src/read_list.c)
target_link_libraries(read_list curl sds sqlite3)
target_compile_options(read_list PUBLIC -std=c11 -Wall -Wextra -pedantic)

add_executable(get-zip-codes src/get-zip-codes.c)
target_link_libraries(get-zip-codes curl sqlite3)
target_compile_options(get-zip-codes PUBLIC -std=c11 -Wall -Wextra -pedantic)
