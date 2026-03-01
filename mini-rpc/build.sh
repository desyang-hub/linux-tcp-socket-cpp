g++ -std=c++11 -o rpc_test \
    src/main.cpp \
    src/rpc_provider.cpp \
    src/user_service_impl.cpp \
    -I./include \
    -pthread