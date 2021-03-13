cmake_minimum_required(VERSION 3.15)

project(DigitalColleague LANGUAGES CXX)

find_package(Boost COMPONENTS system thread json REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)

add_executable(digitalcolleague src/main.cpp
  src/twitch.hpp src/twitch.cpp
  src/discord/session.hpp src/discord/session.cpp
  src/discord/gateway.hpp src/discord/gateway.cpp
  src/discord/request.hpp src/discord/request.cpp
  src/discord/snowflake.hpp src/discord/snowflake.cpp
  src/discord/user.hpp src/discord/user.cpp
  src/discord/bot.hpp src/discord/bot.cpp src/discord/event.hpp
  src/console.hpp src/console.cpp)

target_compile_features(digitalcolleague PRIVATE cxx_std_17)

target_link_libraries(digitalcolleague
  PRIVATE OpenSSL::SSL
          Boost::boost Boost::system Boost::thread Boost::json
          SQLite::SQLite3
)

