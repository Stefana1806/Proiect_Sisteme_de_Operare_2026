#!/bin/bash

./tools/fileops.sh init

directoare=("bin" "src" "include" "data" "logs" "reports" "tmp" "tests" "doc" "tools")
cate_lipsesc=0

for i in "${directoare[@]}"; do
    if [ ! -d "$i" ]; then
        cate_lipsesc=$((cate_lipsesc + 1))
     fi
done
if [ $cate_lipsesc -eq 0 ]; then
    exit 0
 else exit 1
fi


