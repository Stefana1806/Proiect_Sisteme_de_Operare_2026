#!/bin/bash

BIN_DIR="bin"
DATA_DIR="data"
LOG_DIR="logs"

PID_FILE="$DATA_DIR/manager.pid"
DB_FILE="$DATA_DIR/inventory.db"
IPC_FILE="$DATA_DIR/ipc.mmap"
LOG_FILE="$LOG_DIR/t5_output.log"

TEST_TREE="/tmp/t5_determinist_tree"

echo "Pornire testare separată pentru Control Plane (T5)..."

mkdir -p "$DATA_DIR" "$LOG_DIR"

rm -f "$PID_FILE" "$DB_FILE" "$IPC_FILE" "$LOG_FILE"
rm -rf "$TEST_TREE"

echo "[T5-TEST] Generare arbore de fișiere controlat..."
mkdir -p "$TEST_TREE/dir_stabil/sub_folder"
mkdir -p "$TEST_TREE/dir_eroare"

for i in {1..15}; do
    echo "Date test determinist $i" > "$TEST_TREE/dir_stabil/sub_folder/file_$i.txt"
done

mkdir -p "$TEST_TREE/dir_eroare/restrictiv"
touch "$TEST_TREE/dir_eroare/restrictiv/f_ascuns.txt"
chmod 000 "$TEST_TREE/dir_eroare/restrictiv"

if [ ! -f "$BIN_DIR/fileops_manager" ]; then
    echo "EROARE: Binarul $BIN_DIR/fileops_manager nu a fost găsit!" >&2
    chmod 755 "$TEST_TREE/dir_eroare/restrictiv" && rm -rf "$TEST_TREE"
    exit 1
fi

echo "[T5-TEST] Se lansează binarul în fundal cu delay controlat (simulate-work)..."
"$BIN_DIR/fileops_manager" \
    --root "$TEST_TREE" \
    --workers 3 \
    --db "$DB_FILE" \
    --ipc "$IPC_FILE" \
    --simulate-work-ms 150 \
    --graceful-timeout 4 \
    --pid-file "$PID_FILE" > "$LOG_FILE" 2>&1 &

echo "[T5-TEST] Se așteaptă generarea fișierului PID de către manager..."
RETRY=10
while [ ! -f "$PID_FILE" ] && [ "$RETRY" -gt 0 ]; do
    sleep 0.3
    RETRY=$((RETRY - 1))
done

if [ ! -f "$PID_FILE" ]; then
    echo "EROARE CRITICĂ: Managerul nu a pornit sau nu a salvat fișierul PID în timp util." >&2
    chmod 755 "$TEST_TREE/dir_eroare/restrictiv" && rm -rf "$TEST_TREE"
    exit 1
fi

MGR_PID=$(cat "$PID_FILE")
echo "[T5-TEST] Manager detectat activ cu PID-ul: $MGR_PID"
sleep 0.5 

echo "[T5-TEST] Trimitere semnal interogare stare: SIGUSR1..."
kill -USR1 "$MGR_PID"
sleep 0.4

echo "[T5-TEST] Trimitere semnal oprire timpurie (Shutdown grațios): SIGTERM..."
kill -TERM "$MGR_PID"

wait "$MGR_PID" || true
echo "[T5-TEST] Procesul Manager s-a oprit conform semnalelor primite."

chmod 755 "$TEST_TREE/dir_eroare/restrictiv"



if grep -q "T5MSG type=JOB_DONE" "$LOG_FILE"; then
    echo "[PASSED] Mesajele de progres JOB_DONE transmise corect."
else
    echo "[FAILED] Lipsesc mesajele JOB_DONE din Control Plane."
fi

if grep -q "STATUS queued_jobs=" "$LOG_FILE"; then
    echo "[PASSED] Semnalul SIGUSR1 interceptează și generează linia STATUS stabilă."
else
    echo "[FAILED] Linia STATUS lipsește în urma interogării asincrone."
fi

if grep -q "T5MSG type=WORKER_EXITING" "$LOG_FILE"; then
    echo "[PASSED] Workerii raportează WORKER_EXITING la primirea semnalului SIGTERM."
else
    echo "[FAILED] Workerii nu au fost notificați sau nu au apucat să raporteze închiderea."
fi

if grep -q "T5MSG type=ERROR" "$LOG_FILE"; then
    echo "[PASSED] Eroarea controlată (EACCES) propagată stabil prin pipe."
else
    echo "[FAILED] Eroarea de sistem din directorul restrictiv nu a fost prinsă."
fi

echo "[T5-TEST] Executare --verify pe baza de date generată parțial..."
if "$BIN_DIR/fileops_manager" --verify "$DB_FILE" > /dev/null 2>&1; then
    echo "[PASSED] Modul --verify acceptă baza ca fiind structural validă (cod retur 0)."
else
    echo "[FAILED] Modul --verify consideră baza defectă/coruptă (cod de ieșire eronat)."
fi

echo "[T5-TEST] Verificare flag de finalizare prin modul --dump..."
"$BIN_DIR/fileops_manager" --dump "$DB_FILE" > "$DATA_DIR/dump_output.txt" 2>&1 || true

if grep -q "Complete: 0" "$DATA_DIR/dump_output.txt"; then
    echo "[PASSED] Flagul 'Complete: 0' (inventariere parțială) salvat cu succes în header."
else
    echo "[FAILED] Flagul 'Complete' nu reflectă oprirea grațioasă din semnale."
fi

rm -rf "$TEST_TREE" "$DATA_DIR/dump_output.txt"
echo "[T5-TEST] Testare T5 a fost finalizată cu succes!"