# Loop Invariant Code Motion (terzo assignment)
Il file `As03Pass.cpp` implementa il passo di ottimizzazione per la *Loop Invariant Code Motion* (LICM).
L'obiettivo del passo è identificare le istruzioni all'interno di un loop che producono sempre lo stesso risultato ad ogni iterazione, in quanto non dipendono da valori modificati all'interno del ciclo stesso.
Tali istruzioni possono quindi essere spostate all'esterno del ciclo senza alterare il comportamento del programma, migliorandone l'efficienza.

Il codice implementa un *function pass* che analizza ogni loop all'interno della funzione, identifica le istruzioni loop-invariant e verifica se ciascuna di esse può essere spostata fuori dal ciclo senza alterare il comportamento del programma.
Affinché un’istruzione sia considerata idonea per la LICM, deve soddisfare le seguenti condizioni:
1. deve essere loop-invariant

2. deve trovarsi in un blocco che domina tutte le uscite del loop, oppure la variabile da essa definita deve essere morta in tutte le uscite che non sono dominate

3. deve assegnare un valore a variabili non assegnate altrove nel loop

4. deve trovarsi in un blocco che domina tutti i blocchi del ciclo in cui viene utilizzata la variabile a cui assegna il valore

Viene in seguito spiegata la logica per i controlli all'interno del passo di ottimizzazione.

## Loop Invariant Instructions
La ricerca delle istruzioni loop invariant avviene in modo ricorsivo tramite le seguenti funzioni:
1. `getLoopInvInstructions`: è la funzione principale che analizza ciascun loop. Scorre tutte le istruzioni nei suoi Basic Blocks e, per ciascuna istruzione binaria (`BinaryOp`), verifica se è loop invariant tramite `isLoopInvInstr`. Se sì, la aggiunge al vettore *loopInvInstr*.

2. `isLoopInvInstr`: prende una singola istruzione binaria e ne analizza i due operandi. Se entrambi sono loop invariant (verificato tramite `isLoopInvOp`), allora anche l’istruzione lo è.

3. `isLoopInvOp`: determina se un singolo operando è loop invariant. Un operando viene considerato tale se:
    1. è una costante (*ConstantInt*)
    2. è un argomento della funzione (*Argument*)
    3. è un’istruzione già riconosciuta come loop invariant
    4. è definito fuori dal ciclo
    5. oppure, ricorsivamente, è un’istruzione aritmetica che risulta loop invariant

La funziona popola progressivamente il vettore e termina quando sono state analizzate tutte le istruzioni del ciclo.

## Domina tutte le uscite del loop oppure la variabile è dead all'uscita non dominata
La funzione `domsAllLivePaths` verifica se l'istruzione candidata alla code motion si trova in un blocco che domina tutte le uscite del loop in cui la variabile da essa definita è ancora viva.
In particolare, per ogni exit block del loop:

1. se il blocco che contiene l’istruzione non domina l’uscita
2. e se la variabile è viva dopo quell’uscita (condizione verificata chiamando `isAliveOutsideLoop`)

    >La funzione `isAliveOutsideLoop` verifica se la variabile definita da un'istruzione è morta fuori dal loop, in relazione a uno specifico exit block.
    >
    >In particolare, per ciascun uso della variabile:
    >1. recupera il basic block dell'uso
    >2. se il blocco non è contenuto nel loop (uso esterno)
    >3. e se è dominato dal blocco di uscita

allora la funzione restituisce *false*, poiché l'uso si verifica fuori dal loop per un percorso non dominato da quella specifica uscita.


## Ha definizioni multiple all'interno del loop
La funzione `hasMultipleDef` controlla se una stessa variabile (ovvero il valore definito da un'istruzione `I`) viene ridefinita più volte all'interno del loop.
In particolare la funzione, per ogni uso dell’istruzione `I` che è un `PHINode`:

1. verifica se il blocco che contiene il `PHINode` si trova all'interno del loop;

1. per ogni basic block che arriva al blocco del `PHINode`:
    1. controlla che il blocco sia diverso da quello in cui è definita `I` (ossia il valore arrivi da un'altra definizione)
    1. e che tale blocco sia anch'esso contenuto nel loop.

Se entrambe le condizioni sono soddisfatte per almeno un blocco, la funzione restituisce `true`, in quanto significa che esistono più definizioni della variabile all’interno del loop.

## Una istruzione domina tutti i suoi usi
Il controllo che verifica se una istruzione domina tutti i suoi usi non è necessario nella forma SSA, in quando la proprietà è sempre verificata.

DA CONTROLLARE: non è necessario che una istruzione domini il PHI node in quanto non è formalmente un uso.  