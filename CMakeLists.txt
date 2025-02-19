cmake_minimum_required(VERSION 3.20)

project(CppDiplomProject)



# TODO: Define by your own needs:
set (BOOST_ROOT "C:/vcpkg/installed/x64-windows")
set (BOOST_INCLUDEDIR "C:/vcpkg/installed/x64-windows/include")
set (BOOST_LIBRARYDIR "C:/vcpkg/installed/x64-windows/lib")
set (OPENSSL_ROOT_DIR "C:/vcpkg/installed/x64-windows")


set(libpqxx_DIR "C:\\Work\\libpqxx-7.8.1")


# TODO: If you are running on Windows below 10, change this:

add_compile_definitions(_WIN32_WINNT=0x0A01)



# Boost:
find_package(Boost 1.80.0 REQUIRED COMPONENTS system thread locale)

if(NOT Boost_FOUND)
    message(FATAL_ERROR "Could not find Boost")
endif()


# PQXX
set(PostgreSQL_ADDITIONAL_VERSIONS "15")

add_subdirectory(${libpqxx_DIR} libpqxx-build) 


# OpenSSL
find_package(OpenSSL REQUIRED)



add_subdirectory(spider)

add_subdirectory(http_server)
