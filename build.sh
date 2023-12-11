# 开启shell扩展跟踪 打印完整命令
set -x

rm -rf `pwd`/build/*

cd `pwd`/build && cmake .. && make

