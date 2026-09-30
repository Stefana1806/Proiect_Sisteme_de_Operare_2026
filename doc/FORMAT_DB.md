1. Structura Header:
- *char magic[4]* : un camp de 4 caractere folosit ca identificator pentru validarea rapida a formatului;  *IDX1* sau *PRC1*
- *int format_version* : un numar intreg care indica versiunea structurii interne, permitand interpretarea corecta a datelor 
- *int snapshot_id* : un identificator unic generat la initializarea unei sesiuni noi - folosind functia *time()*
- *int active_writers* : numarul de instante care lucreaza simultan la acelasi fisier 
- *int record_count* : un camp care stocheaza totalul de fisiere/procese scris in baza de date 
- *char snapshot_state[10]* : un camp ce indica daca baza de date accepta contributii (**OPEN**) sau este finalizata (**SEALED**)

2. Structura Record fileops_indexer :
- *char path[512]* : calea absolută a fisierului sau directorului 
- *char nume_binar[512]* : un identificator unic format din numarul dispozitivului (**st_dev**) si numarul inodului (**st_ino**)
- *off_t size* : dimensiunea în bytes, calculata doar pentru fisierele regulate (0 pentru restul) 
- *time_t time* : data si ora ultimei modificari
- *char tip_fisier[13]* : identificarea entitătii ca fisier regulat, director, symlink sau fifo
- *int hash* : o suma de control calculata prin adunarea valorilor ASCII ale caracterelor continutului fisierului

3. Structura Record proc_snapshot :
- *int pid* : un numar care reprezinta identificatorul procesului
- *int ppid* : un numar ce reprezinta parintele procesului 
- *char state* : un caracter ce reprezinta starea curenta a procesului
- *char comm[512]* : acesta reprezinta numele scurt al procesului, asa cum este el cunoscut de nucleul
- *char cmdline[1024]* : acesta este campul care ofera contextul complet despre cum a fost pornit procesul respectiv
- *int rss* : dimensiunea memoriei rezidente citita din *cale* 
- *double cpu* : timpul total de executie calculat prin insumarea **utime** si **stime** raportat la frecventa ceasului sistemului
- *char rss_cale[256]* : calea de unde a fost citita dimensiunea memoriei rezidente 

4. Limite alese :
- *max_path (512 caractere)* : s-a ales limita de 512 pentru a echilibra consumul de disc cu necesitatile practice; aceasta acopera majoritatea structurilor de directoare uzuale fara a irosi spatiu excesiv in baza de date
- *max_cmdline (1024 caractere)* : limita de 1024 a fost aleasa pentru a captura argumentele esentiale ale unui proces, fiind o valoare standard care permite identificarea contextului de rulare fara a degrada performanta operatiilor de citire/scriere
- *comm (16 caractere)* : limita numelui unui proces este de 16 caractere
- *max_pids (32768)* : aceasta este o limita interna bazata pe valoarea default pid_max din sistemele linux pe 32 de biti, asigurand ca tablourile de procese folosite in memorie pentru comparatie nu depasesc resursele rezonabile ale sistemului

5. Strategia de actualizare :
- *mecanism de blocare* : se utilizeaza exclusiv functia **fcntl()** pentru a pune lacate de tip **F_WRLCK** direct pe regiunile relevante ale fisierului bazei de date 
- *gestiunea sesiunii* : prima instanta initializeaza header-ul. instantele ulterioare incrementeaza contorul **active_writers** sub lock; ultima instanta care isi termina executia marcheaza snapshot-ul ca **sealed**
- *prevenirea duplicatelor* : nu se permite append-ul necontrolat; inregistrarile sunt gestionate astfel incat calea absoluta (pentru fisiere) sau pid-ul (pentru procese) sa fie unice in cadrul aceluiasi snapshot
- *rezervarea spatiului* : extinderea fisierului pentru noi inregistrari se face prin blocarea regiunii de final pentru a evita alocarea aceleiasi zone de catre doua procese diferite

6. Conditiile minime pentru ca o bază de date să fie considerată validă :
- *integritatea header-ului* : fisierul trebuie sa aiba o dimensiune minima egala cu **sizeof(struct header)**; daca dimensiunea este mai mica, fisierul este considerat corupt sau neinitializat
- *corespondenta magic number* : campul magic trebuie sa contina exact sirul **IDX1** (pentru fisiere) sau **PRC1** (pentru procese); daca acest camp nu corespunde cu tipul operatiei dorite, utilitarul va refuza procesarea pentru a preveni interpretarea gresita a datelor
- *versiunea formatului* : valoarea **format_version** trebuie sa fie compatibila cu cea a utilitarului (in acest caz, versiunea 1); o versiune diferita indica un format binar structurat altfel, care ar duce la citirea unor date eronate
- *starea sesiunii (sealed)*: pentru operatiile de comparare (**db_diff**), baza de date trebuie sa fie in starea **sealed**; o baza de date in starea **open** este considerata incompleta, deoarece una sau mai multe instante pot fi inca in proces de scriere a record-urilor
- *alinierea datelor* : dimensiunea totala a fisierului dupa header trebuie sa fie un multiplu exact al dimensiunii structurii de record (**record** sau **process**); orice rest indica o scriere intrerupta sau o corupere a fisierului binar

7. Regulile de comparare folosite de **db_diff** :
- *criterii pentru fisiere* : compararea se face dupa calea absoluta; o intrare este marcata ca **modificata** daca difera tipul, dimensiunea, mtime-ul sau hash-ul continutului
- *criterii pentru procese* : compararea se face dupa pid; un proces este considerat **modificat semnificativ**  daca exista schimbari in starea acestuia sau in utilizarea resurselor (rss, cpu)
- *validare* : programul verifica obligatoriu campurile **magic** si **format_version** din headerele celor doua fisiere inainte de a incepe analiza
