# 必须添加-O2 -DNDEBUG 否则会发生异常
g++ --std=c++14 \
    -I/home/des/miniconda3/envs/tf114/include \
    login.cc login.pb.cc \
    -L/home/des/miniconda3/envs/tf114/lib \
    -lprotobuf \
    -pthread \
    -O2 -DNDEBUG \
    -o userlogin

./userlogin