cmake_minimum_required(VERSION 3.15)

project(DigitalColleague LANGUAGES CXX)

find_package(Boost COMPONENTS system thread json REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)

add_executable(digitalcolleague src/main.cpp
  src/irc_client.hpp src/irc_client.cpp
  src/console.hpp)

target_compile_features(digitalcolleague PRIVATE cxx_std_17)

target_link_libraries(digitalcolleague
  PRIVATE OpenSSL::SSL
          Boost::boost Boost::system Boost::thread Boost::json
          SQLite::SQLite3
)

