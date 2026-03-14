cmake_minimum_required(VERSION 3.16)
project(receipt_ocr)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(TESSERACT REQUIRED tesseract)
pkg_check_modules(LEPT REQUIRED lept)
find_package(OpenCV REQUIRED)

add_executable(receipt_ocr src/main.cpp)

target_include_directories(receipt_ocr PRIVATE
    ${TESSERACT_INCLUDE_DIRS}
    ${LEPT_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(receipt_ocr
    ${TESSERACT_LIBRARIES}
    ${LEPT_LIBRARIES}
    ${OpenCV_LIBS}
)