# Loop Fusion
Il file `As04Pass.cpp` implementa il passo di ottimizzazione per la *Loop Fusion*.
L'obiettivo del passo è identificare cicli consecutivi che operano su intervalli compatibili e che non presentano dipendenze che ne impediscano la combinazione.
Questi cicli possono quindi essere fusi in un unico loop, preservando il comportamento del programma ma riducendo l'overhead del controllo di flusso (come le condizioni e gli incrementi del loop) e migliorando la località dei dati, con potenziali benefici in termini di efficienza e cache performance.

Il codice implementa un *function pass* che analizza coppie di loop all'interno di ciascuna funzione per determinare se possono essere fusi in un singolo ciclo, mantenendo la correttezza semantica del programma.
Affinché due loop siano considerati idonei alla fusione, devono soddisfare le seguenti condizioni:

1. devono essere adiacenti 

2. devono iterare lo stesso numero di volte

3. devono essere aderenti nel controllo di flusso (control-flow equivalent)

4. non devono avere distanza negativa.

I controlli sopra elencati vengono effettuati nella funzione `mainFuseLoops`.

## Loop adiacenti
La funzione `areAdjacentLoops` verifica l'adiacenza nel controllo di flusso di due loop candidati alla loop fusion.

La funzione prende in input due oggetti Loop (l1 e l2) e restituisce true se i due cicli sono adiacenti, false altrimenti.

L’analisi fa distinzione tra due casi principali:

1. entrambi i loop sono guarded
2. entrambi i loop non sono guarded

### Caso loop guarded
Nel caso i due loop siano guarded:
1. si verifica che le *BranchInst* di guardia abbiano condizioni identiche (tramite la funzione `checkGuardCondition`);

2. se è così, si cerca tra i successori del primo *guardBranch* se uno di questi è proprio il BasicBlock che contiene il secondo *guardBranch*;

3. in caso positivo, si verifica che la condizione (*cond*) del secondo *guardBranch* sia la prima istruzione del blocco;

4. se tutti i controlli sono positivi, la funzione considera i loop adiacenti ma, per ora, restituisce comunque false in quanto la fusione di loop guarded non è supportata.

### Caso loop non guarded
Nel caso i due loop non siano guarded, si verifica che:

1. il primo loop abbia un unico exit block (tramite la funzione `getExitBlock`);

2. il secondo loop abbia un preheader e questo deve coincidere con l’exit block del primo;

3. la prima istruzione dell’exit block sia un’istruzione di salto (*BranchInst*), indicando che non ci sono altre istruzioni tra i due loop.

### Caso misto: solo uno è guarded
Se solo uno dei due loop è guarded, la funzione restituisce direttamente *false*, in quanto la fusione è ammessa solo se i loop sono o entrambi guarded o entrambi non guarded.

## Numero di iterazioni uguali
La funzione `iterateEqualTimes` confronta il numero di iterazioni dei due loop candidati.

La funzione utilizza l’analisi *ScalarEvolution (SE)*, che fornisce una rappresentazione simbolica del numero di iterazioni di un ciclo sotto forma di oggetti *SCEV*.

Fasi dell’analisi:
1. estrazione del numero di iterazioni: tramite *SE.getBackedgeTakenCount(&lX)*, si ricava il numero di volte che viene eseguito il back-edge del ciclo (ossia, il numero di iterazioni meno uno);
2. verifica se il conteggio è computabile: se per uno dei due loop il numero di iterazioni non può essere calcolato (ossia il risultato è di tipo *SCEVCouldNotCompute*), la funzione restituisce *false*;
3. confronto tra le due espressioni *SCEV*: 
    1. se le due espressioni *SCEV* ottenute sono identiche (puntano allo stesso oggetto), si considera che i due loop iterano lo stesso numero di volte e si restituisce *true*;
    2. in caso contrario, anche se *SE.getMinusSCEV(itTimes1, itTimes2)->isZero()* (ossia se la differenza tra i due conteggi è simbolicamente zero), la funzione non lo utilizza come criterio per determinare l’uguaglianza: fa affidamento esclusivamente sul confronto dei puntatori.

## Control flow equivalence
La funzione `areControlFlowEq` verifica l’equivalenza nel controllo di flusso dei due loop candidati.

L’analisi fa distinzione tra due casi principali:

1. entrambi i loop sono guarded
2. entrambi i loop non sono guarded

I guardBlock sono ottenuti tramite una funzione chiamata `getGuardBlock`.

### Caso loop guarded
Si verifica che:
1. il *guardBlock* del loop1 domina il *guardBlock* del loop2
2. il *guardBlock* del loop2 postdomina il *guardBlock* del loop1

Se è così, la funzione ritorna *true*.

### Caso loop non guarded
Si verifica che:
1. il *Preheader* del loop1 domina il *Preheader* del loop2
2. il *Preheader* del loop2 postdomina il *Preheader* del loop1

Se è così, la funzione ritorna *true*.

### Caso misto: solo uno è guarded
I loop non sono control flow equivalente, quindi viene ritornato *false*.

## Negative distance
La funzione `haveNoNegativeDistance` verifica l’assenza di dipendenze con distanza negativa tra istruzioni di memoria (ottenute tramite la funzione `getMemInst` che, tramite un booleano, restituisce le istruzioni di load o store di un loop passato) presenti nei loop candidati.

Si considera un caso di dipendenza negativa tra una store e una load (o viceversa), entrambe indirizzate su elementi della stessa base (array, struct, puntatore) e la load accede a un indirizzo che sarà scritto solo in un’iterazione futura, causando una violazione della semantica se i loop venissero fusi.

La distanza tra i due accessi è calcolata tramite *Scalar Evolution (SCEV)*, presente nella funzione `haveNegativeDistanceDiff`.

Fase di analisi: Per ogni coppia store e load:

1. si estraggono i *GEP* (*GetElementPtr*) usati per calcolare gli indirizzi;

2. si confrontano le base pointer: se sono diversi, gli accessi non sono correlati e si passa oltre.

3. si calcolano i *SCEV* degli indirizzi *storePtr* e *loadPtr* nel contesto dei rispettivi loop;

4. si calcola la differenza tra le due evoluzioni tramite *SE.getMinusSCEV(...)*;

    1. si verifica se la differenza è nota negativamente: la dipendenza negativa è certa;
    
    2. altrimenti, si prova a valutare simbolicamente la differenza con la funzione *evaluateMinusSCEV*, e si controlla se il valore è negativo.

Se viene trovata una distanza negativa, la funzione restituisce *true*; altrimenti, restituisce *false* (nessuna dipendenza negativa trovata).

## Fuse Loops
La funzione `fuseLoops` esegue la fusione di due loop seguendo questi passi:

1. verifica che gli header di entrambi i loop contengano un *PHInode*, utilizzando la funzione `getPHIFromHeader`;

2. recupera il blocco di uscita del primo loop e quello di uscita del secondo, controllando che il primo abbia un'istruzione di salto (*BranchInst*) valida;

3. modifica il salto del primo loop per collegarlo direttamente all'uscita del secondo loop, concatenando i due cicli nel flusso di controllo;

4. verifica che i latch di entrambi i loop siano privi di dipendenze, usando la funzione`isLatchDepFree`; se uno dei due non lo è, la fusione viene interrotta;

5. sostituisce l'induction variable del secondo loop con quella del primo, eliminando il *PHINode* del secondo loop;

6. determina se il secondo loop è ruotato o meno:

    1. se non ruotato, estrae il corpo effettivo come successore dell’header (che è anche l’exiting block), poi elimina l’header;
    
    2. se ruotato, mantiene l’header come ingresso;

7. elimina il preheader del secondo loop, ormai inutile;

8. collega i predecessori del latch del primo loop al corpo del secondo loop, creando continuità tra i due corpi. In questa fase viene utilizzata la funzione `getLatchPreds`, che restituisce tutti i blocchi che hanno un salto verso il latch del primo loop;

9. collega i predecessori del latch del secondo loop al latch del primo, completando la chiusura del loop fuso. Anche qui viene usata `getLatchPreds` per ottenere i predecessori del latch del secondo loop;

10. sposta il latch del primo loop prima del secondo nel codice IR, per mantenere l’ordine logico, e rimuove definitivamente il latch del secondo loop;

11. restituisce *true* per indicare che la fusione è andata a buon fine.