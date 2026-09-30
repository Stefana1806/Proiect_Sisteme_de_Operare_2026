#!/bin/bash
START=$(date +%s)
TIMESTAMP_START=$(date "+%Y-%m-%d %H:%M:%S")
ls -l -a > ./reports/fs/A1_ls_long.txt
find . -type f -name "*.sh" >./reports/fs/A2_find-sh.txt
du -h --max-depth=1 > ./reports/fs/A3_du_level1.txt

ps -eo pid,comm,%mem --sort=-%mem | head -n 10 > ./reports/process/B1_top_mem.txt
pstree > ./reports/process/B2_pstree.txt
sleep 60& pgrep sleep | xargs  ps -l -p > ./reports/process/B3_pgrep_sleep.txt

cat /proc/cpuinfo  | grep "model name" |uniq > ./reports/proc/C1_cpu_model.txt
grep MemAvailable /proc/meminfo > ./reports/proc/C2_mem_totala.txt 
grep MemTotal /proc/meminfo > ./reports/proc/C2_mem_totala.txt
cat /proc/uptime > ./reports/proc/C3_uptime.txt

find . -type 'f' -exec du -b {} + |  sort -t' ' -k 1rn | head -n 5 > ./reports/pipeline/D1_top5_large_files.txt
ps -eo pid,comm --sort=-%mem | tail -n +6 | head -5 | uniq > ./reports/pipeline/D2_top5_proc_mem_pid_name.txt
grep "comanda:" ./doc/T1_comenzi.md | sort -k2 | wc -l > ./reports/pipeline/D3_count_commands.txt

touch ./reports/T1_summary.txt 
END=$(date +%s)
TIMESTAMP_END=$(date "+%Y-%m-%d %H:%M:%S")

{
  echo "=== SUMAR ==="
  echo "Start:  $TIMESTAMP_START"
  echo "End:    $TIMESTAMP_END"
  echo "Durata: $((END - START)) secunde"
  echo ""
  echo "=== FISIERE CREATE ==="
find -type d,f >> /home/stefana/Desktop/proiect_31091001011ROSL251071/reports/T1_summary.txt 
} > /home/stefana/Desktop/proiect_31091001011ROSL251071/reports/T1_summary.txt
