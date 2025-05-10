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

Tali controlli vengono eseguiti nella funzione `findCodeMotionCandidates`, che filtra il vettore di istruzioni loop-invariant, mantenendo solo quelle che soddisfano i controlli forniti dalle funzioni `domsAllLivePaths`, `hasMultipleDef`, e `dominatesAllUses`. Questo garantisce che siano sicure da spostare nel preheader del ciclo.

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
La funzione `domsAllLivePaths` verifica se un'istruzione candidata alla code motion si trova in un blocco che domina tutte le uscite del loop in cui la variabile da essa definita è ancora viva.

Per ciascun exit block del loop, se il blocco contenente l’istruzione non domina l’uscita in cui la variabile è viva (condizione verificata con `isDeadOutsideLoop`), la funzione restituisce *false*. Questo indica che la variabile potrebbe essere utilizzata fuori dal loop lungo un percorso non dominato dall’istruzione, rendendo la code motion non sicura.

La funzione `isDeadOutsideLoop` determina se una variabile definita da un’istruzione è morta al di fuori del loop, in relazione a un particolare exit block.

In particolare, per ciascun uso della variabile:

1. Se l'uso è un'istruzione al di fuori del loop, allora la variabile è viva e restituisce *false*.

2. Se l'uso è un *PHINode*, la funzione richiama se stessa ricorsivamente sul PHInode. Se il *PHINode* ha usi esterni al loop, la variabile è considerata viva fuori dal ciclo.

3. Se nessun uso esterno viene trovato, la funzione restituisce *true*, indicando che la variabile è morta al di fuori del loop.

## Ha definizioni multiple all'interno del loop
La funzione `hasMultipleDef` controlla se una stessa variabile (ovvero il valore definito da un'istruzione `I`) viene ridefinita più volte all'interno del loop.

In particolare la funzione, per ogni uso dell’istruzione `I` che è un `PHINode`:

1. verifica se il blocco che contiene il `PHINode` si trova all'interno del loop;

2. per ogni basic block che arriva al blocco del `PHINode`:
    1. controlla che il blocco sia diverso da quello in cui è definita `I` (ossia il valore arrivi da un'altra definizione)
    2. e che tale blocco sia anch'esso contenuto nel loop.

Se entrambe le condizioni sono soddisfatte per almeno un blocco, la funzione restituisce *true*, in quanto significa che esistono più definizioni della variabile all’interno del loop.

## Una istruzione domina tutti i suoi usi
Il controllo che verifica se una istruzione domina tutti i suoi usi non è necessario nella forma SSA, in quando la proprietà è sempre verificata.

## Spostamento delle istruzioni nel pre-header
Lo spostamento delle istruzioni avviene solo dopo il superamento dei controlli descritti in precedenza; al termine della verifica, le istruzioni che risultano idonee vengono trasferite nel pre-header del ciclo.

La funzione `codeMotion` verifica l'esistenza del pre-header (normalmente presente, poiché si considerano solo cicli in forma normale) e, finché il vettore delle istruzioni che hanno superato i controlli non è vuoto, invoca ripetutamente la funzione `Move`.

La funzione `Move` sposta ricorsivamente un'istruzione loop invariant nel preheader del ciclo, garantendo che i suoi operandi siano già stati spostati. 

In sintesi, esegue i seguenti passi:

1. controllo che l'istruzione sia presente nel vettore loopInvInstr, ovvero che sia già stata identificata come loop-invariant

2. controllo ricorsivo gli operandi:

    1. per ogni operando, se non è "movibile" tenta di spostare l'istruzione corrispondente all'operando

    2. se lo spostamento di un operando fallisce, l'istruzione corrente viene rimossa da `loopInvInstr` e non viene spostata

3. spostamento effettivo: l'istruzione viene spostata nel pre-header e rimossa da loopInvInstr dopo lo spostamento.

La funzione `isMovable` controlla se un operando può essere spostato e, in tal caso ritorna *true*.

Un operando è considerato movibile se:

1. è una costante

2. è un argomento di una funzione

3. non è dentro al loop (è già stato spostato)