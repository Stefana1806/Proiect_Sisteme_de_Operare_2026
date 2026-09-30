#!/bin/bash

manager="bin/fileops_manager"
test_dir="test_propriu"
db_file="data/inventory.db"
ipc_file="data/ipc.mmap"

echo "Incepe testarea T4: "

#creare scenariu
rm -rf "$test_dir"
mkdir -p "$test_dir/subdir"
echo "test1" > "$test_dir/file1.txt"
echo "test2" > "$test_dir/file2.txt"
echo "test3" > "$test_dir/subdir/file3.txt"

#rulare test
./$manager --root "$test_dir" --workers 2 --db "$db_file" --ipc "$ipc_file"

#verificare baza de date
if [ ! -f "$db_file" ]; then
    echo "[FAIL] Fisierul nu a fost creat!"
    exit 1
fi

echo "[PASS] Fisier creat cu succes!"

#verify
echo "Verificare db (--verify)"
./$manager --db "$db_file" --verify
if [ $? -eq 0 ]; then 
    echo "[PASS] db-ul este valid conform --verify!"
else 
    echo "[FAIL] --verify a raportat erori!"
    exit 1
fi

#dump
echo "Verificare db (--dump)"
dump_out=$(./$manager --db "$db_file" --dump)

if echo "$dump_out" | grep -q "INV4"; then
    echo "[PASS] Magic e corect!"
else 
    echo "[FAIL] Magic nu e bun!"
    exit 1
fi

#verificare complete
if echo "$dump_out" | grep -q "Complete: 1"; then 
    echo "[PASS] Complete flag-ul e 1!"
else 
    echo "[FAIL] Complete flag-ul nu e 1!"
    exit 1
fi

#verificare file record
if echo "$dump_out" | grep -q "Files: 3"; then
    echo "[PASS] file_record_count e corect!"
else 
    echo "[FAIL] file_record_count nu e bun (asteptat: 3)!"
    exit 1
fi

#verificare worker
if echo "$dump_out" | grep -q "Workers: 2"; then
    echo "[PASS] worker_count e corect!"
else 
    echo "[FAIL] worker_count nu e corect!"
    exit 1
fi

#mesaj finalizare test
echo "Test t4 finalizat cu succes"
exit 0
