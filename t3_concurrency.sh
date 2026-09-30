#!/bin/bash

mkdir -p data reports 
rm -rf data/*
rm -f reports/T3_filediff.txt reports/T3_procdiff.txt reports/T2_tests.txt

./tools/fileops.sh --run --fileops_indexer --root ./src --db data/index.db &  ./tools/fileops.sh --run --fileops_indexer --root ./reports --db data/index.db &  ./tools/fileops.sh --run --fileops_indexer --root ./logs --db data/index.db &  ./tools/fileops.sh --run --fileops_indexer --root ./tests --db data/index.db & 

wait 

if [ -f "data/index.db" ]; then 
    echo -e "Fisierul index.db a fost generat corect.\n"
fi

if strings data/index.db | grep -q "SEALED"; then 
    echo "Fisierul index.db este SEALED!"
fi

count_src=$(find ./src -mindepth 1 | wc -l)
count_reports=$(find ./reports -mindepth 1 | wc -l)
count_logs=$(find ./logs -mindepth 1 | wc -l)
count_tests=$(find ./tests -mindepth 1 | wc -l)

suma=$((count_src + count_reports + count_logs + count_tests))
record_count=$(od -j 20 -N 4 -A n -d data/index.db | tr -d ' ')

if [ "$suma" -eq "$record_count" ]; then 
    echo "Instantele indexer contribuie la acelasi snapshot"
fi

./tools/fileops.sh --run -- proc_snapshot --db data/proc_concur.db & ./tools/fileops.sh --run -- proc_snapshot --db data/proc_concur.db & ./tools/fileops.sh --run -- proc_snapshot --db data/proc_concur.db &

wait

if strings data/proc_concur.db | grep -q "SEALED"; then 
    echo "Fisierul proc_concur.db este SEALED!"
fi

./tools/fileops.sh run -- fileops_indexer --root ./src --db data/index_1.db
echo "test_diff" > ./src/new_test.txt
sleep 1
./tools/fileops.sh run -- fileops_indexer --root ./src --db data/index_2.db
./tools/fileops.sh run -- db_diff --old data/index_1.db --new data/index_2.db > reports/T3_filediff.txt
rm ./src/new_test.txt

./tools/fileops.sh run -- proc_snapshot --db data/proc_1.db
sleep 1 & 
new_pid=$!
./tools/fileops.sh run -- proc_snapshot --db data/proc_2.db
./tools/fileops.sh run -- db_diff --old data/proc_1.db --new data/proc_2.db > reports/T3_procdiff.txt
kill $new_pid 2>/dev/null

if [ -s "reports/T3_filediff.txt" ]; then
    echo "Exista si contine informatii T3_filediff.txt"
fi

if [ -s "reports/T3_procdiff.txt" ]; then 
    echo "Exista si contine informatii T3_procdiff.txt"
fi