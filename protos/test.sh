proto_file=./segmenter.proto
GRPC_CPP_PLUGIN=grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH=`which ${GRPC_CPP_PLUGIN}`
protoc -I . --grpc_out=. --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_PATH}  ${proto_file}
protoc -I . --cpp_out=. ${proto_file}

