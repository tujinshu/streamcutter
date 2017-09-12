cmake_minimum_required(VERSION 2.8)

project(realtimecutter)

set(CMAKE_CXX_FLAGS "-g -Wall -pthread -D__STDC_CONSTANT_MACROS")

find_package(PkgConfig)

pkg_check_modules(FFmpeg REQUIRED
    libavdevice
    libavformat
    libavfilter
    libavcodec
    libswresample
    libswscale
    libavutil)

message(${FFmpeg_LIBRARIES})
message(${FFmpeg_INCLUDE_DIRS})
message(${FFmpeg_LIBRARY_DIRS})

pkg_check_modules(GRPC REQUIRED)
message(${GRPC_LIBRARIES})
message(${GRPC_INCLUDE_DIRS})
message(${GRPC_LIBRARY_DIRS})

#pkg_check_modules(PROTO REQUIRED protobuf)
#message(${PROTO_LIBRARIES})
#message(${PROTO_INCLUDE_DIRS})
#message(${PROTO_LIBRARY_DIRS})


INCLUDE_DIRECTORIES(ms)
INCLUDE_DIRECTORIES(${FFmpeg_INCLUDE_DIRS})
LINK_DIRECTORIES(${FFmpeg_LIBRARY_DIRS})

#generate grpc and proto files
#execute_process(COMMAND sh -c "echo xx && cd ${CMAKE_SOURCE_DIR}/protos && ./test.sh")

set(GRPC_SOURCES
    protos/segmenter.pb.cc
    protos/segmenter.pb.h
    protos/segmenter.grpc.pb.cc
    protos/segmenter.grpc.pb.h
    task_manager_impl.cpp
    )


set(SOURCES
    ms/ms_util.h
    ms/ms_util.cpp
    ms/input_file.h
    ms/input_file.cpp
    ms/output_file.h
    ms/output_file.cpp
    ms/demuxer.h
    ms/demuxer.cpp
    ms/decoder.h
    ms/decoder.cpp
    ms/video_encoder.h
    ms/video_encoder.cpp
    ms/audio_encoder.h
    ms/audio_encoder.cpp
    ms/muxer.h
    ms/muxer.cpp
    stream_encoder.h
    stream_encoder.cpp
    audio_filter.h
    audio_filter.cpp
    log.h
    stream_info.h
    stream_info.cpp
    segmenter.h
    segmenter.cpp
    )

set (TEST_SOURCES
    ${SOURCES}
    main.cpp
    )

set (GRPC_SERVICE_SOURCES
    ${SOURCES}
    ${GRPC_SOURCES}
    )

add_executable(${PROJECT_NAME}_test ${TEST_SOURCES})
target_link_libraries(${PROJECT_NAME}_test
    ${FFmpeg_LIBRARIES})

#add_executable(${PROJECT_NAME} ${GRPC_SERVICE_SOURCES})
#target_link_libraries(${PROJECT_NAME}
#    ${FFmpeg_LIBRARIES})
