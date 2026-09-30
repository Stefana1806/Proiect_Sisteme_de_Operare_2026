##Comenzi Filesystem 

1. **A1.** Listează conținutul directorului curent în format “long” și include fișierele ascunse.
-**comanda:** ls -l -a >/home/stefana/Desktop/proiect_31091001011ROSL251071/reports/fs/A1_ls_long.txt
-**fisier de output:** reports/fs/A1_ls_long.txt

2. **A2.** Găsește în proiect toate fișierele cu extensia .sh și afișează calea lor relativă.
-**comanda:** find . -type f -name *.sh
-**fisier de output:** reports/fs/A2_find_sh.txt

3. **A3.** Afișează dimensiunea (în mod human-readable) pentru directoarele de nivel 1 din
proiect.
-**comanda:** du -h --max-depth=1 */ >> /home/stefana/Desktop/proiect_31091001011ROSL251071/reports/fs/A3_du_level1.txt
-**fisier de output:** reports/fs/A3_du_level1.txt

##Procese -3 comenzi 

1. **B1** Afișează primele 10 procese ordonate descrescător după consumul de memorie (RSS sau %MEM).
-**comanda:** top -b -o %MEM -n 1 | head -n 17 > /home/stefana/Desktop/proiect_31091001011ROSL251071/reports/process/B1_top_mem.txt 
-**fisier de output:** reports/process/B1_top_mem.txt

2. **B2** Afișează arborele de procese (process tree) pentru sistem, cu PID-uri vizibile.
-**comanda:** pstree > /home/stefana/Desktop/proiect_31091001011ROSL251071/reports/process/B2_pstree.txt 
-**fisier de output:** reports/process/B2_pstree.txt

3. **B3** Pornește un proces de test (ex: sleep 60 în background) și demonstrează că îl poți identifica după nume și PID.
-**comanda:** sleep 60 &
-**comanda:** pgrep -l sleep
-**fisier de output:** reports/process/B3_pgrep_sleep.txt​
- pentru a opri comanda *sleep 60* din terminal poate fi scris *pkill sleep* sau de la tastatura CTRL+C;


##/proc

1. **C1** Extrage modelul CPU (sau “model name”) din /proc/cpuinfo.
-**comanda:** cat /proc/cpuinfo  | grep "model name" |uniq >> /home/stefana/Desktop/proiect_31091001011ROSL251071/reports/proc/C1_cpu_model.txt
-**fisier de output:** reports/proc/C1_cpu_model.txt

2. **C2** Extrage “MemTotal” și “MemAvailable” din /proc/meminfo.
-**comanda:** grep MemAvailable /proc/meminfo >> /home/stefana/Desktop/proiect_31091001011ROSL251071/reports/proc/C2_mem_totala.txt 
-**comanda:** grep MemTotal /proc/meminfo >> /home/stefana/Desktop/proiect_31091001011ROSL251071/reports/proc/C2_mem_totala.txt 
-**fisier de output:** reports/proc/C2_mem_total_avail.txt

3. **C3** Afișează “uptime” din /proc/uptime (valoarea brută, în secunde).
-**comanda:** cat /proc/uptime
-**fisier de output:** reports/proc/C3_uptime.txt

##Pipeline 

1. **D1** Construiește un top 5 al celor mai “mari” fișiere (după dimensiune) din proiect,
afișând doar dimensiunea și calea.​
-**comanda:** find . -type f | xargs du -sh | sort -rh | head -5
-**fisier de output:** reports/pipeline/D1_top5_large_files.txt

2. **D2** Construiește un “top 5” procese după memorie, extrăgând doar câmpurile relevante
(PID și nume), eliminând duplicatele (dacă apar) și limitând la 5 linii de output.​
-**comanda:** ps -eo pid,comm --sort=-%mem | tail -n +6 | head -5 | uniq
-**fisiser de output:** reports/pipeline/D2_top5_proc_mem_pid_name.txt

3. **D3** Din fișierul doc/T1_comenzi.md, extrage liniile care conțin comenzi (de ex. cele din
blocuri marcate sau cele care încep cu un prompt), sortează-le și numără câte sunt.​
-**comanda:** grep "comanda:" /home/stefana/Desktop/proiect_31091001011ROSL251071/doc/T1_comenzi.md | sort -k2 | wc -l
-**fisier de output:** reports/pipeline/D3_count_commands.txt
