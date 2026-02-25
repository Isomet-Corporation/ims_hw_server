# --- Generate C# API source files -------------------------------------------
if(GENERATE_PROTO_CSHARP)

    set(CSHARP_OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated/csharp)
    file(MAKE_DIRECTORY ${CSHARP_OUT_DIR})

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/csharp_stamp
        COMMAND protobuf::protoc
            --proto_path=${CMAKE_SOURCE_DIR}/protos
            --csharp_out=${CSHARP_OUT_DIR}
            --grpc_out=${CSHARP_OUT_DIR}
            --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_csharp_plugin>
            ${ims_hw_server_proto_files}
        COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/csharp_stamp
        DEPENDS ${ims_hw_server_proto_files}
    )

    add_custom_target(proto_csharp ALL
        DEPENDS ${CMAKE_BINARY_DIR}/csharp_stamp
    )
endif()


# --- Generate Python API source files -------------------------------------------

if(GENERATE_PROTO_PYTHON)

    set(PYTHON_OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated/python)
    file(MAKE_DIRECTORY ${PYTHON_OUT_DIR})

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/python_stamp
        COMMAND protobuf::protoc
            --proto_path=${CMAKE_SOURCE_DIR}/protos
            --python_out=${PYTHON_OUT_DIR}
            --grpc_out=${PYTHON_OUT_DIR}
            --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_python_plugin>
            ${ims_hw_server_proto_files}
        COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/python_stamp
        DEPENDS ${ims_hw_server_proto_files}
    )

    add_custom_target(proto_python ALL
        DEPENDS ${CMAKE_BINARY_DIR}/python_stamp
    )
endif()


# --- Generate Objective-C API source files -------------------------------------------

if(GENERATE_PROTO_OBJC)

    set(OBJC_OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated/objective_c)
    file(MAKE_DIRECTORY ${OBJC_OUT_DIR})

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/objective_c_stamp
        COMMAND protobuf::protoc
            --proto_path=${CMAKE_SOURCE_DIR}/protos
            --objc_out=${OBJC_OUT_DIR}
            --grpc_out=${OBJC_OUT_DIR}
            --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_objective_c_plugin>
            ${ims_hw_server_proto_files}
        COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/objective_c_stamp
        DEPENDS ${ims_hw_server_proto_files}
    )

    add_custom_target(proto_objective_c ALL
        DEPENDS ${CMAKE_BINARY_DIR}/objective_c_stamp
    )
endif()


# --- Generate PHP API source files -------------------------------------------

if(GENERATE_PROTO_PHP)

    set(PHP_OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated/php)
    file(MAKE_DIRECTORY ${PHP_OUT_DIR})

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/php_stamp
        COMMAND protobuf::protoc
            --proto_path=${CMAKE_SOURCE_DIR}/protos
            --php_out=${PHP_OUT_DIR}
            --grpc_out=${PHP_OUT_DIR}
            --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_php_plugin>
            ${ims_hw_server_proto_files}
        COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/php_stamp
        DEPENDS ${ims_hw_server_proto_files}
    )

    add_custom_target(proto_php ALL
        DEPENDS ${CMAKE_BINARY_DIR}/php_stamp
    )
endif()


# --- Generate Ruby API source files -------------------------------------------

if(GENERATE_PROTO_RUBY)

    set(RUBY_OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated/ruby)
    file(MAKE_DIRECTORY ${RUBY_OUT_DIR})

    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/ruby_stamp
        COMMAND protobuf::protoc
            --proto_path=${CMAKE_SOURCE_DIR}/protos
            --ruby_out=${RUBY_OUT_DIR}
            --grpc_out=${RUBY_OUT_DIR}
            --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_ruby_plugin>
            ${ims_hw_server_proto_files}
        COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/ruby_stamp
        DEPENDS ${ims_hw_server_proto_files}
    )

    add_custom_target(proto_ruby ALL
        DEPENDS ${CMAKE_BINARY_DIR}/ruby_stamp
    )
endif()
