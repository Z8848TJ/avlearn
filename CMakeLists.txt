cmake_minimum_required(VERSION 3.26)
project(avlearn C)

set(CMAKE_C_STANDARD 11)


include_directories("D:/software/ffmpeg/ffmpeg/include")

link_directories("D:/software/ffmpeg/ffmpeg/lib")

add_executable(avlearn main.c)

target_link_libraries(
        avlearn
        avcodec
        avdevice
        avfilter
        avformat
        avutil
        postproc
        swresample
        swscale
)



