#!/bin/bash
# objs=()
# arch=$1

# for i in "1 256"; do
#     set -- $i
#     s=S_$1_$2.s
#     o=S_$1_$2.o
#     python3 ./codegen.py -o $s -m $1 -n $2 --arch $arch
#     objs+=($o)
# done

python3 ./codegen.py --is-scale -o A_S_S_256_4_gfx942.s -t S -d S -w 256 -c 4 --arch gfx942 --toolchain /opt/rocm/llvm/bin/clang++

rm -r build
mkdir build
cd build
cmake ..
make -j

# /opt/rocm/llvm/bin/clang++ -target amdgcn-amdhsa -o softmax_$arch.co ${objs[@]}
# /opt/rocm/llvm/bin/clang++ -target amdgcn-amdhsa -o A_S_S_256_4_gfx90a.co A_S_S_256_4_gfx90a.o