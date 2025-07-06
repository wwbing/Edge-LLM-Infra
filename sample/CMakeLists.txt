cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Og -g3 -fno-inline)  
set(CMAKE_BUILD_TYPE Debug) 



include_directories("../hybrid-comm/include/")
include_directories("../hybrid-comm/include/libzmq")

add_executable(zmq_pub pub.cc ../hybrid-comm/src/pzmq_data.cpp )
target_link_libraries(zmq_pub 
    zmq 
    pthread
)

add_executable(zmq_sub sub.cc ../hybrid-comm/src/pzmq_data.cpp )

target_link_libraries(zmq_sub
    zmq 
    pthread
)

add_executable(rpc_client rpc_call.cc ../hybrid-comm/src/pzmq_data.cpp )

target_link_libraries(rpc_client
    zmq 
    pthread
)

add_executable(rpc_server rpc_server.cc ../hybrid-comm/src/pzmq_data.cpp )

target_link_libraries(rpc_server
    zmq 
    pthread
)

