
set(EXAMPLES_DIR ${CMAKE_CURRENT_SOURCE_DIR})
function(example name)
  add_executable(zio_example_${name} ${name}.cpp)
  target_link_libraries(zio_example_${name} PUBLIC zio)
  file(RELATIVE_PATH dir ${EXAMPLES_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
  set_property(GLOBAL APPEND PROPERTY ZIO::EXAMPLES ${dir}/${name})
  set_target_properties(zio_example_${name} PROPERTIES
    CXX_STANDARD 20
  )
  # add_definitions(-DDEBUG=1)
endfunction(example)

example(tcp_chatroom)
example(tcp_connect_server)
example(tcp_echo_server)
example(input)
example(http_server)
example(socks_proxy)

target_link_libraries(zio_example_input
  PRIVATE ftxui::screen
  PRIVATE ftxui::dom
  PRIVATE ftxui::component
)