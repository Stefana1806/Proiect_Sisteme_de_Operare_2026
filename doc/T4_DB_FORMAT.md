# Structura Bazei de Date Binare (Inventory DB)

## 1. Header-ul Fisierului (Inceputul)
Primele date din fisier ne spun ce fel de fisier este acesta:
* **Magic Number (INV4):** Patru litere la inceput care ne confirma ca este fisierul nostru.
* **Version:** Ne spune versiunea formatului.
* **File Count:** Cate fisiere am gasit in total.
* **Worker Count:** Cati workeri au muncit la acest inventar.
* **Complete Flag:** Daca este 1, inseamna ca totul s-a terminat cu bine. Daca e 0, fisierul e incomplet (corupt).

## 2. Inregistrarile de Fisiere (File Records)
Dupa header, urmeaza o lista lunga cu datele fiecarui fisier gasit. Pentru fiecare fisier salvam:
* **Path:** Calea completa catre fisier (unde se afla el pe disc).
* **Size:** Cat de mare este fisierul in bytes.
* **mtime:** Data si ora cand a fost modificat ultima oara (ca un numar lung).
* **ID-uri (uid/gid):** Cine este proprietarul fisierului.

## 3. Statisticile Workerilor (Finalul)
La finalul fisierului, dupa ce am terminat cu toate fisierele, punem un tabel cu detalii despre performanta:
* **PID:** Codul de proces al fiecarui worker.
* **Files Emitted:** Cate fisiere a gasit fiecare in parte.
* **Bytes Emitted:** Cat de multa informatie a procesat fiecare.
* **Exit Status:** Daca workerul a inchis corect sau a dat eroare.
