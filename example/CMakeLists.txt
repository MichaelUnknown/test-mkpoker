##########################################
## exmaples ##
##########################################

# /W3 warning in msvc fixed with 3.15
cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

add_executable(demo_equity demo_equity.cpp)
target_link_libraries(demo_equity PRIVATE mkpoker::mkpoker)

add_executable(demo_handstrength demo_handstrength.cpp)
target_link_libraries(demo_handstrength PRIVATE mkpoker::mkpoker)

