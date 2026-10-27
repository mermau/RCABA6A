# RCA BA-6A — analisi del circuito e derivazione del modello (v0.9.1)

Fonti: RCA IB-18-2035-2 (manuale), schema Fig. 8, schema corretto 2004 (BA6A DIY), scheda commerciale RCA.
Tutte le costanti sono in `Source/DSP/BA6AEngine.h/.cpp` e `Source/DSP/ControlCalibration.h`.

## 1. Catena del segnale

```
ingresso 600/150 Ω → T1 → R1 (2 × 25 kΩ, 20 scatti da 2 dB, sulle griglie)
  → V1/V2 6SK7 push-pull, pendenza variabile (taglio remoto)
  → T2 → V3/V4 6J7 → V5/V6 6V6GT (guadagno fisso, controreazione C7/R27, C8/R26)
  → T3 → R36 a T ponte (20 scatti da 1 dB) → R35 FINE (100 Ω in serie) → 600 Ω
rivelatore: placche 6V6 → rete di isolamento (C14/C15 0,25 µF, 56 kΩ) → V7 6H6 a doppia semionda,
  catodi polarizzati a +66 V → C1 0,1 µF ‖ R11 3,3 MΩ  [+ R37 3,3 MΩ – C18 0,27 µF in DUAL] → griglie 6SK7
```

## 2. Stadio a µ variabile (6SK7)

Per ogni valvola la corrente, normalizzata a quella di riposo e con la tensione di griglia `u` in unità di V0 (≈ 7,5 V), è

    i(u) = (1 − η·u)^(−1/η),     η = 0,099

È un esponenziale con pendenza locale V0·(1 + η·|u|), cioè la curva a taglio remoto. Ne seguono:

- **riduzione di guadagno** (transconduttanza): GR = 8,686·(1 + 1/η)·ln(1 + η·c) dB, con `c` la tensione di controllo;
- **corrente catodica** letta dallo strumento in GR: 10^(−0,91·GR/20).

La scala stampata da RCA (misurata sulla foto della Fig. 1) coincide con questa legge:

| GR (dB) | 20 | 15 | 10 | 7 | 5 | 3 | 1 |
|---|---|---|---|---|---|---|---|
| foto (frazione di fondo scala) | 0,123 | 0,201 | 0,362 | 0,483 | 0,587 | 0,727 | 0,898 |
| modello | 0,123 | 0,208 | 0,351 | 0,480 | 0,592 | 0,730 | 0,900 |

Altri aspetti dello stadio:

- **Pilotaggio di griglia** al limite di intervento: 0,165·V0 di picco differenziale. Il valore è tarato sulla distorsione dichiarata a 10 e 15 dB di riduzione.
- **Conduzione di griglia**: la griglia non può superare il catodo (5,15 V di polarizzazione ≈ 0,69 V0). Sui transitori, prima che il controllo intervenga, la conduzione tosa il segnale.
- **Coppia non selezionata**: rimane una differenza di curvatura del 22 % che il bilanciamento non può eliminare. I trimmer agiscono così:
  - BALANCE A sposta la polarizzazione catodica (±0,06 V0);
  - BALANCE B agisce sulla griglia schermo, con uno spostamento proporzionale alla polarizzazione (±0,10).

  Con i trimmer a 0 la coppia è bilanciata nei punti A (5 dB) e B (10 dB), come al termine della procedura del manuale.
- **Colpi (thump)**: la differenza delle correnti continue cambia con la polarizzazione e passa attraverso T2. Il rapporto segnale/colpo misurato è 35 dB per un gradino di 5 dB e 29 dB per uno di 10 dB; la specifica dà 21 dB minimo e 26 dB medio.

## 3. Rivelatore e costanti di tempo

Il diodo 6H6 è modellato come conduttanza 1/6 kΩ con un ginocchio morbido (softplus, larghezza 0,06·V0), e la rete è integrata per campione:

    C1·dV1/dt  = i_diodo − V1/R11 − [DUAL]·(V1 − V18)/R37
    C18·dV18/dt = [DUAL]·(V1 − V18)/R37

Da qui le costanti di tempo: Rs·C1 = 0,6 ms; R11·C1 = 0,33 s; R37·C18 = 0,89 s; (R37 + R11)·C18 = 1,78 s.
Gli stati del commutatore si comportano così:

- in OFF la tensione di controllo è cortocircuitata;
- in SINGLE C18 resta isolato e conserva la carica, che viene restituita al ritorno in DUAL;
- in A/B le griglie ricevono una polarizzazione fissa, pari a 5 e 10 dB sullo strumento (tabella "Meter Indications"), più il 60 Hz in modo comune. L'uscita è staccata dalla linea.

**Calibrazione statica (Fig. 4).** Il guadagno del rivelatore, 8,6 (≈ 66 V di polarizzazione del 6H6 diviso V0), e la soglia, −0,32 dB rispetto alla retta, sono stati ottenuti con un fit ai minimi quadrati della curva digitalizzata dal manuale (errore quadratico medio 0,03 dB). In simulazione dinamica lo scarto massimo è 0,19 dB.

## 4. Risposta in frequenza (Fig. 5)

| Elemento | Modello | Effetto |
|---|---|---|
| T2, T3 | passa-alto del 1° ordine a 4,4 Hz ciascuno | −0,4 dB a 20 Hz |
| Uscita | Butterworth del 4° ordine a 21,5 kHz | −0,1 dB a 15 kHz, −1,6 dB a 20 kHz |
| Rivelatore | passa-alto a 10,5 Hz (C14/C15 con 56 kΩ: 11,4 Hz) e lieve esaltazione degli acuti (+1,2 dB a 15 kHz) | sotto limitazione i bassi vengono ridotti meno e gli acuti di più |

Risultato con 10 dB di riduzione: +0,6 dB a 20 Hz e −0,8 dB a 15 kHz, come le curve tratteggiate di Fig. 5.

## 5. Stadi fissi, trasformatore d'uscita, rumore

- **6J7 + 6V6 in controreazione**: s − 0,008·s³ + 0,0015·s², con saturazione di ordine 6 a 3,0 volte il livello al limite. Al limite la THD è 0,18 %; il massimo con limitazione OFF è +38,1 dBm (RCA: 38,5).
- **T3**: perdita del nucleo proporzionale al quadrato del flusso. La THD a 30 Hz al limite è 0,4 % (RCA: < 1,2 %).
- **T1** (davanti all'attenuatore): stessa legge di perdita del nucleo, normalizzata a +14 dBm a 30 Hz (0,5 %). Dipende dal livello assoluto d'ingresso, non dalla posizione di INPUT.
- **Alimentazione**: la media di |uscita| con costante di 40 ms abbassa il punto di saturazione delle 6V6 fino al 5 %, con una costante di 120 ms (choke d'ingresso L1 4,5 H, C17 40 µF).
- **Rumore residuo**: fruscio −56 dBm e ronzio −59 dBm (fondamentale di rete + 2ª e 3ª armonica, filamenti in alternata di 6J7/6V6); totale −53,7 dBm (RCA: −54 dBm). Si disattiva con NOISE + HUM. Le 6SK7 sono alimentate in continua da SR1 e non contribuiscono. HUM ADJ. agisce solo sull'alimentazione di un preamplificatore esterno.
- **Rete**: 60 c/s (originale USA) o 50 c/s; vale per il ronzio e per il segnale di bilanciamento nelle posizioni A/B.

## 6. Strumento

| Posizione | Lettura |
|---|---|
| GR | corrente catodica media delle due 6SK7 × (1 + 0,12·ZERO ADJ) |
| V1, V2 | correnti delle due 6SK7 (si spostano con BALANCE A e calano sotto limitazione) |
| V3–V6 | valori nominali nella banda CHECK |
| FIL | tacca HEATER × Vfil/6,5 V |
| COARSE / FINE BAL | uscita raddrizzata; fondo scala −24 / −44 dB rispetto al livello al limite |

Balistica: sistema del 2° ordine, fn = 1,7 Hz, ζ = 0,72, con arresti meccanici.

## 7. Stereo

Il BA-6A è mono. In stereo il plugin modella due unità con le reti di controllo collegate (STEREO LINK) oppure due unità indipendenti.
