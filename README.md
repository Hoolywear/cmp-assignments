# Assignment Compilatori 2024/25
Repository contenente gli assignments per il corso di Compilatori (seconda parte) tenuto al terzo anno della Laurea Triennale in Informatica dal professor Andrea Marongiu.

Il gruppo è composto da Daniele Fassetta, Iacopo Ruzzier e Anna Semeraro

# Istruzioni per l'installazione

Come prima cosa clonare la repository in locale tramite `git clone https://github.com/Hoolywear/cmp-assignments.git`. Al suo interno si possono trovare directory separate per ciascun assignment, di nome `assignment_0N`.

## Struttura delle sottocartelle `assignment`

In ogni subdirectory si troveranno il/i sorgente/i del passo (es. `As01Pass.cpp`), una cartella `test/` con all'interno i sorgenti in C su cui eseguire il passo, e due script per compilarlo ed eseguirlo (`setup.sh`, `compile.sh`)

### `setup.sh`

Lo script prevede il passaggio del flag `-d` per specificare la directory contenente i binari di LLVM, usando come default il path usato da Linux. Il resto dello script si occupa di creare la cartella `build/` se non ancora esistente, e di eseguire i comandi per compilare il passo.

Esempio di utilizzo:

```bash
# All'interno di assignment_0N
./setup.sh -d a/different/path/to/llvm
# Per usare il default /usr/bin :
./setup.sh
```

### `compile.sh`

Questo script permette di eseguire le ottimizzazioni su file arbitrari, tipicamente quelli nella directory `test/`. Usiamo il flag `-m` o `-l` per specificare il sistema operativo usato (lo script considera estensione `.dylib` se su MacOS, `.so` se su Linux), `-f` per specificare il path al file di test (es. `test/Foo.c`), `-o` per il nome del passo di ottimizzazione da applicare (es. `alg-id`).

Esempio d'uso:
```bash
# All'interno di assignment_0N

# Eseguo su sistema Linux, ottimizzando test/Foo.c:
# 1. Genero un primo test/Foo.ll con clang -S -emit-llvm -Xclang -disable-O0-optnone -O0 ...
# 2. Genero una sua versione ottimizzata in test/Foo.bc con il passo mem2reg di opt
# 3. Ottimizzo con il nostro passo alg-id e disassemblo in test/Foo-opt.ll
./compile.sh -l -f test/Foo.c -o alg-id
```

# Assignment 01

I passi di ottimizzazione implementati per questo assignment sono
- Algebraic identity (`alg-id`)
- Advanced strength reduction (`adv-str-red`)
- Multi instruction optimization (`multi-inst-opt`)

e si invocano utilizzando gli stessi nomi.