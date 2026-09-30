## 1. Canalul de Control (Pipe/FIFO)


* Creare și Alocare: Înainte de a efectua operația fork(), Managerul instanțiază o pereche de descriptori folosind apelul de sistem pipe(). Descriptorul de scriere este moștenit și reținut de Worker (transmis explicit prin flag-ul --control-fd), în timp ce descriptorul de citire este monitorizat de către Manager.
* Non-interferență: Canalul este utilizat exclusiv pentru mesaje administrative și de progres (Control Plane), fără a altera performanța fluxului de date din mmap.
* Atomicitate: Toate raportările transmise pe pipe sunt formatate într-un singur șir de caractere și trimise printr-un singur apel de sistem write(). Dimensiunea fiecărui mesaj este garantată sub limita PIPE_BUF (4096 octeți pe sisteme Linux), eliminând riscul de intercalare a mesajelor de la diferiți workeri în bufferul Managerului.


## 2. Formatul Mesajelor (T5MSG)


Fiecare linie respectă structura:
T5MSG type=<TIP_MESAJ> worker_id=<ID> [atribute_specifice_cheie=valoare]\n

Tipuri de mesaje implementate:

1. type=JOB_DONE
   * Moment: Transmis de Worker imediat după ce a terminat de scanat complet un director din coadă.
   * Format: T5MSG type=JOB_DONE worker_id=%d jobs=%d files=0 bytes=0\n
   * Rol: Permite Managerului să contorizeze progresul real pe baza statisticii asincrone.

2. type=ERROR
   * Moment: Transmis atunci când un Worker întâmpină o eroare critică de sistem (ex: EACCES din cauza unui director restrictiv fără permisiuni de citire/execuție).
   * Format: T5MSG type=ERROR worker_id=%d errno=%d where=<opendir|lstat>\n
   * Rol: Semnalează defecțiunile de mediu din sistemul de fișiere fără a prăbuși workerul.

3. type=WORKER_EXITING
   * Moment: Transmis în faza de curățare a Workerului, înainte ca acesta să își încheie execuția.
   * Format: T5MSG type=WORKER_EXITING worker_id=%d reason=<shutdown|normal_exit>\n
   * Rol: Notifică Managerul că workerul respectiv și-a eliberat resursele și nu mai consumă din coadă.


## 3. Interogarea Asincronă de Stare (STATUS)


* Trimitere Semnal: Utilizatorul sau scriptul de test trimite semnalul standard SIGUSR1 către procesul Manager (kill -SIGUSR1 <PID_MANAGER>).
* Handler Sigur: Managerul instalează un handler de semnal non-blocant prin intermediul structurii sigaction cu flag-ul SA_RESTART. Handlerul modifică atomic o variabilă globală de tipul volatile __sig_atomic_t g_status.
* Afișare Stabilă: În bucla principală de multiplexare (poll), Managerul verifică periodic valoarea g_status. La detectarea valorii 1, consumă semnalul și scrie la stdout linia obligatorie de stare:
  
  STATUS queued_jobs=<X> active_jobs=<Y> files=<Z>
  
  * queued_jobs: Numărul de directoare rămase în bufferul circular job_queue din Shm.
  * active_jobs: Contorul global al joburilor aflate în procesare sau programate.
  * files: Numărul actual de fișiere salvate în mod real în baza de date.


## 4. Shutdown Grațios (SIGINT / SIGTERM)


1. Propagarea Semnalului: La recepționarea SIGINT/SIGTERM în Manager, variabila g_shutdown este setată pe 1. Managerul activează flag-ul shutdown_flag în regiunea mmap pentru a stopa preluarea de noi joburi și trimite în mod explicit un semnal SIGTERM către toate procesele fiice (Workerii) înregistrate în tabela de PIDs.
2. Reap și Clean-up (Garanție Timeout): Managerul intră într-o buclă de așteptare limitată în timp de argumentul --graceful-timeout (exprimat în secunde). În acest interval monitorizează descriptorii de pipe pentru a consuma ultimele mesaje text trimise de workeri (WORKER_EXITING) și apelează waitpid(..., WNOHANG) pentru a îngropa procesele zombi într-un mod non-blocant.
3. Mecanism de Fallback (SIGKILL): Dacă workerii blocați în operații I/O nu își încheie execuția în limita timeout-ului configurat, Managerul trimite semnalul distructiv SIGKILL pentru a garanta eliberarea completă a resurselor de sistem și efectuează waitpid în mod sincron.


## 5. Serializarea Stării Bazei de Date Parțiale (complete=0)


* Header Valid: La pornire, Managerul rezervă spațiul pentru structura header la începutul fișierului, mută cursorul și adaugă înregistrările de fișiere secvențial, pe măsură ce sosesc de la workeri.
* Sincronizare Flag: Înainte de a elibera resursele și a închide descriptorul bazei de date (atât în cazul încheierii normale cât și în cazul unui Shutdown grațios), Managerul reasamblează structura de antet.
* Gestiunea complete_flag:
  * Dacă inventarierea s-a finalizat natural (toate joburile au fost procesate, active_jobs == 0), câmpul complete_flag din header este setat pe 1.
  * Dacă execuția a fost întreruptă de semnale (SIGINT/SIGTERM), câmpul complete_flag este salvat explicit cu valoarea 0.
* Compatibilitate --verify: Modul de verificare (--verify) citește headerul bazei de date parțiale, recunoaște structura ca fiind validă (întorcând codul de retur 0), dar raportează starea conform dump-ului: Complete: 0 (inventariere parțială salvată cu succes).
