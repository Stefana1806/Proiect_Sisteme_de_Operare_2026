# Protocol de Comunicare si Sincronizare (IPC)


## 1. Structura Memoriei Partajate
Toata comunicarea se intampla intr-o zona de memorie comuna numita `sharedmemoryregion`. Aceasta este impartita in 3 bucati mari:
* **Header (info):** Contine datele generale (ex: cate fisiere am gasit, daca am terminat tot).
* **Job Queue (job_queue):** O lista de foldere care asteapta sa fie scanate.
* **Canale de Rezultate (rezultate[N]):** Fiecare worker are "cutia" lui unde pune datele despre fisierele gasite.

## 2. Cum functioneaza scanarea (Job Queue)
1. **Managerul** pune primul folder (root) in coada de joburi.
2. **Workerii** stau la panda. Cand apare un folder, un worker il ia si il deschide.
3. Daca workerul gaseste un **sub-folder**, il pune inapoi in coada de joburi pentru ca el sau alt coleg sa il scaneze mai tarziu.
4. Daca workerul gaseste un **fisier**, ii ia datele (nume, marime, timp) si le trimite managerului.

## 3. Regula de terminare
Folosim un contor numit `active_jobs`. 
* Cand un folder e pus in coada, contorul creste. 
* Cand un folder e terminat de scanat, contorul scade.
* Cand `active_jobs` ajunge la 0, inseamna ca nu mai exista niciun folder de scanat nicaieri, deci workerii se pot opri.

## 4. Sincronizarea (Semafoare)
Pentru a nu deveni haos, folosim semafoare (ca niste semafoare in trafic):
* **Mutex:** Doar un singur proces are voie sa umble la coada de joburi la un moment dat.
* **Empty/Full:** Workerii asteapta daca nu e nimic de munca si managerul asteapta daca nu sunt rezultate de citit.
