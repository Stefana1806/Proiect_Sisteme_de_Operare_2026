#!/bin/bash 

PROJECT="proiect_31091001011ROSL251071"

init_p() {
    local directoare=("bin" "src" "include" "data" "logs" "reports" "tmp" "tests" "doc" "tools" "tmp/obj")
    for i in "${directoare[@]}"; do
        [ ! -d "$i" ] && mkdir -p "$i"
    done
    if ! gcc --version &> /dev/null; then 
        echo "Eroare: GCC nu este instalat"
        return 1
    fi
}

build_p() {
    local DIR
    if [ "$1" = "--src" ]; then 
        DIR="${2:-src}"
    elif [ -n "$1" ]; then
        DIR="$1"
    else 
        DIR="src"
    fi 
    
    [ -z "$RADACINA" ] && RADACINA="$DIR"

    mkdir -p "tmp/obj" "bin"

    for i in "$DIR"/*; do
        [ -e "$i" ] || continue
        if [ -d "$i" ]; then
            build_p "$i" 
        elif [[ "$i" == *.c ]]; then
            local filename=$(basename "$i")
            local obiect_n="${filename%.c}.o"
            local obiect_path="tmp/obj/$obiect_n"

            if [ ! -f "$obiect_path" ] || [ "$i" -nt "$obiect_path" ]; then
                gcc $CFLAGS -Iinclude -c "$i" -o "$obiect_path"
            fi
        fi
    done

  if [ "$DIR" == "$RADACINA" ]; then
        local main_files=$(find "$RADACINA" -name "main_*.c" 2>/dev/null)
        for m in $main_files; do
            local i_file=$(basename "$m")
            local nume_final=${i_file#main_}
            nume_final=${nume_final%.c}
            local exec_path="bin/$nume_final"

            local obiect_main="tmp/obj/${i_file%.c}.o"
            local obiecte_utilitare=$(find tmp/obj -name "*.o" ! -name "main_*.o")
            
            echo "Se creează binarul: $exec_path"
            gcc $CFLAGS -Iinclude "$obiect_main" $obiecte_utilitare -o "$exec_path"
        done
        unset RADACINA
    fi
}

run_p() {
    if [ -z "$1" ]; then return 1; fi
    local ex_cale="./bin/$1"
    if [ -f "$ex_cale" ] && [ -x "$ex_cale" ]; then
        "$ex_cale" "${@:2}"
    else return 1; 
    fi 
}

clean_p() {
    rm -f tmp/obj/* bin/*
    echo "Curățare finalizată."
}

test_p() {
    local report_file="reports/T2_tests.txt"
    local exit_global=0
    echo "--- Rezultate Teste T2 - $(date +'%Y-%m-%d %H:%M:%S') ---" > "$report_file"
    
    local all_tests=$(find tests/ -name "*.sh" 2>/dev/null)

    if [ -z "$all_tests" ]; then
        echo "EROARE: Nu s-au găsit fișiere .sh în tests/" | tee -a "$report_file"
        return 1
    fi

    for test_script in $all_tests; do
        [ -e "$test_script" ] || continue
        local test_name=$(basename "$test_script")
        local test_path=$(dirname "$test_script")
        
        echo "Execut: $test_name (din $test_path)"
        
        bash "$test_script" >> "$LOG_FILE" 2>&1
        local status=$?
        
        if [ $status -eq 0 ]; then
            echo "$test_name: PASS" >> "$report_file"
            echo "[OK] $test_name"
        else
            echo "$test_name: FAIL" >> "$report_file"
            echo "[FAIL] $test_name (Exit code: $status)"
            exit_global=1
        fi
    done
    return $exit_global
}

SUBCOMANDA="$1"
START_TS=$(date +%s)
START_DT=$(date +"%Y-%m-%d %H:%M:%S")
LOG_FILE="logs/fileops_$(date +"%Y%m%d_%H%M%S").log"

{
    case "$SUBCOMANDA" in
        init)  
        init_p 
        ;;
        build) 
        shift; 
        build_p "$@" 
        ;;
        run)   
        shift; 
        [ "$1" == "--" ] && shift; 
        run_p "$@" 
        ;;
        clean) 
        clean_p 
        ;;
        test)  
        test_p 
        ;;
        *)     
    echo "Utilizare: $0 {init|build|run|clean|test}"; exit 1 ;;
    esac
}
EXIT_CODE=$?

END_TS=$(date +%s)
DURATA=$((END_TS - START_TS))

{
    echo "SUBCOMANDA RULATA: $SUBCOMANDA"
    echo "TIMESTAMP START:   $START_DT"
    echo "TIMESTAMP END:     $(date +"%Y-%m-%d %H:%M:%S")"
    echo "DURATA EXECUTIE:   $DURATA secunde"
    echo "EXIT CODE:         $EXIT_CODE"
} >> "$LOG_FILE"

exit $EXIT_CODE
