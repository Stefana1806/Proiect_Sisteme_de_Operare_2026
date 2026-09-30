#!/bin/bash

mkdir -p tmp/scenariu_test_src/app
mkdir -p tmp/scenariu_test_src/lib/include

echo '#ifndef UTIL_H
      #define UTIL_H
      
      int util_add(int a,int b);
      #endif' > tmp/scenariu_test_src/lib/include/util.h
echo '#include <stdio.h>
      #include "util.h"
      
      int util_add(int a,int b)
      {
      return a+b;
      }
     ' > tmp/scenariu_test_src/lib/util.c
echo '#include <stdio.h>
      #include "util.h" 
      
      int main(){
      int rez=util_add(2,3);
      printf("%d\n",rez);
      return 0;
      }' > tmp/scenariu_test_src/app/main_demo.c
      
export CFLAGS="-Itmp/scenariu_test_src/lib/include -std=c11 -Wall -Wextra" 
      
./tools/fileops.sh build --src tmp/scenariu_test_src

if [ -x bin/demo ]; then 
   echo "Executabilul exista si poate fi executat"
else echo "Executabil nu exista"
     exit 1
fi

./bin/demo > tmp/demo_out.txt

cod_exit=$?
rezultat=$(cat tmp/demo_out.txt)
if [ "$rezultat" == "5" ] && [ $cod_exit -eq 0 ]; then
    echo " Rezultat test 2: PASS"
    exit 0
else echo " Rezultat test 2: FAIL"
     exit 1
fi



