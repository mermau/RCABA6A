# Verifica della v0.8.1 (ChatGPT) e correzioni della v0.9.0

Riferimenti: manuale RCA IB-18-2035-2 (Technical Data p. 1, Fig. 1 pannello, Fig. 3 schema a blocchi,
Fig. 4 caratteristica, Fig. 5 risposta in frequenza, Fig. 6/7 forme d'onda, Fig. 8 schema,
"Front-Panel Controls", "Chassis Controls", "Preliminary Adjustments", "Meter Indications").

## A. Fedeltà all'hardware (errori bloccanti)

| # | v0.8.1 | Hardware reale | v0.9.0 |
|---|---|---|---|
| 1 | INPUT −24…+24, 25 scatti | OFF + 20 scatti da 2 dB, dial in dBm al limite: **+14 … −24** (Technical Data: min −24, max +14 dBm) | 21 posizioni a 15°, legende dalla Fig. 1 (−4 a ore 12) |
| 2 | OUTPUT 0…30 a passi di 2, 16 scatti | OFF + 20 scatti da **1 dB**, dial in dBm al limite: **11 … 30** | 21 posizioni a 15°, "20" a ore 12 come in Fig. 1 |
| 3 | METER: IN, V1–V6, FIL, COMPR, BAL | OFF, GR, V1–V6, FIL, COARSE BAL, FINE BAL | posizioni reali, ognuna con la propria lettura |
| 4 | L'ago mostrava sempre la riduzione di guadagno | Ogni posizione misura un punto diverso (tabella "Meter Indications") | GR = corrente catodica delle 6SK7; V1–V6 nella banda CHECK; FIL sulla tacca HEATER; BAL = segnale d'uscita raddrizzato |
| 5 | BAL-LIMIT nell'ordine B, A, OFF… | A, B, OFF (ore 12), SINGLE, DUAL | ordine e angoli reali |
| 6 | Strumento VU −20…+3 con zona rossa | "DB GAIN REDUCTION" 20-15-10-7-5-3-0, banda CHECK, HEATER | quadrante ridisegnato; posizioni delle tacche verificate sulla foto (errore < 0,012) |
| 7 | "COMPRESSOR / LIMITER" | "Limiting Amplifier" | "LIMITING AMPLIFIER" |
| 8 | Quattro manopole diverse, indicatori bianchi stampati nella foto (doppio indice), corpo a barra fermo mentre ruota solo la linea | Quattro manopole identiche a barra con linea bianca | Una manopola renderizzata a 24 angoli: la geometria ruota, la luce resta ferma |
| 9 | Scala INPUT disordinata (−8, −18, −16), legende OUTPUT sovrapposte e sbavate, due font diversi | — | Legende vettoriali generate dalle stesse tabelle del DSP |

## B. DSP

| # | v0.8.1 | Correzione |
|---|---|---|
| 10 | Soglia fissa a −26 dBFS e attenuatore "+24 = 0 dB": le etichette non indicavano il livello di intervento | Calibrazione dichiarata (0 dBm = −18 dBFS in ingresso, +36 dBm = 0 dBFS in uscita): le legende INPUT/OUTPUT corrispondono ai dBm al limite (test: errore ≤ 0,65 dB, come lo scarto "29,5 ±0,5" della specifica) |
| 11 | Riduzione = 9,526 × eccesso in dB, aggiunta a mano | Anello di retroazione reale: 6H6 con polarizzazione fissa, controllo sulle griglie, legge µ variabile. Caratteristica di Fig. 4 riprodotta entro 0,19 dB, compressione 20 → 2,06 dB |
| 12 | DUAL = max(inseguitore veloce, inseguitore lento) più uno smoothing aggiuntivo di 1,5/7 ms | Rete reale C1 0,1 µF ‖ R11 3,3 MΩ, più R37 3,3 MΩ + C18 0,27 µF, risolta per campione (Eulero implicito). Rilascio SINGLE misurato 0,34 s; DUAL lento solo dopo picchi prolungati (Fig. 6/7) |
| 13 | Guadagno applicato come moltiplicazione prima di tre tanh in cascata | Push-pull di 6SK7 a taglio remoto: corrente ∝ (1−ηu)^(−1/η), conduzione di griglia, squilibrio della coppia, colpi ("thump") sui cambi di polarizzazione |
| 14 | HP 17 Hz e LP 19 kHz del primo ordine (−1,9 dB a 15 kHz) | Risposta di Fig. 5: senza limitazione −0,4 dB a 20 Hz e −1,6 dB a 20 kHz; con 10 dB di riduzione +0,6 dB a 20 Hz e −0,8 dB a 15 kHz (catena del rivelatore) |
| 15 | Nessun sovracampionamento | Oversampling 2× (FIR) a 44,1/48 kHz, latenza dichiarata all'host, bypass allineato |
| 16 | BALANCE A/B "colorano" la distorsione; ZERO senza effetto; HEATER = più drive | BALANCE A = offset catodico, BALANCE B = griglia schermo; in A/B c'è il 60 Hz in modo comune, e la procedura del manuale converge (test). ZERO tara il fondo scala; HEATER varia l'emissione e la lettura FIL |
| 17 | Distorsione non verificata | THD verificata sui limiti RCA: < 0,6 % sotto il limite, < 1,2 % a 30 Hz, < 0,75 % a 10 dB e < 1,0 % a 15 dB di riduzione |
| 18 | Nessun rumore né colpi di commutazione | Fruscio a −54 dBm (S/N 83 dB, disattivabile); rapporto segnale/colpo 29 dB (specifica: 21 min, 26 medio) |
| 19 | POWER muto istantaneo | Spegnimento con scarica dell'anodica; all'accensione circa 8 s di riscaldamento dei filamenti (strumento compreso) |

## C. Software

| # | Problema | Correzione |
|---|---|---|
| 20 | OUTPUT su OFF di default: silenzio all'inserimento | default 18 dBm (guadagno unitario al limite) |
| 21 | Commutatori come parametri float a passi | `AudioParameterChoice` con le legende reali (automazione e KeyLab mostrano "−12", "SINGLE"…) |
| 22 | Test con `assert()`, che con `-DCMAKE_BUILD_TYPE=Release` (CI) non vengono compilati: i test di calibrazione non verificavano nulla | Test con controlli espliciti; 88 verifiche DSP e 32 d'integrazione (v0.9.1) |
| 23 | Il test "Approved_GUI_Regression" garantiva che l'immagine restasse uguale a quella sbagliata | Rimosso; la CI salva gli screenshot reali dell'editor |
| 24 | Il pannello SERVICE copriva le manopole: impossibile eseguire il bilanciamento (servono METER e BAL-LIMIT) | Cassetto sotto il pannello, l'editor si allunga; istruzioni p. 7 del manuale stampate sul telaio |
| 25 | `getRawParameterValue("…")` cercato per nome a ogni blocco | puntatori in cache |
| 26 | Ridisegno dell'intera finestra a 30 Hz | ridisegna solo lo strumento a 60 Hz, con balistica di 2° ordine |
| 27 | Avvisi `-Woverloaded-virtual`, JUCE 8.0.15 | nessun avviso nel codice del progetto; JUCE 8.0.16-security (correzione sull'analisi di stringhe malformate, utile al caricamento degli stati) |
| 28 | Nessuna validazione | `auval` e `pluginval` (strictness 10) in CI; pluginval 10 superato anche in locale sul VST3 Linux |

## D. v0.9.1: ulteriori migliorie di fedeltà

| # | Aggiunta | Fondamento |
|---|---|---|
| 29 | **Nucleo di T1 davanti all'attenuatore INPUT**: la distorsione alle basse frequenze dipende dal livello assoluto d'ingresso (0,75 % a 30 Hz con INPUT su +14 e +13 dBm; trascurabile a livelli normali) | R1 è sulle griglie, dopo T1 (Fig. 8). Le regolazioni INPUT alte suonano quindi diverse, come sull'hardware |
| 30 | **Abbassamento dell'anodica**: alimentazione 5R4GY con choke d'ingresso da 4,5 H; con uscita forte e prolungata il punto di saturazione delle 6V6 scende fino al 5 % (−0,24 dB dopo 2 s a +12 dB oltre il limite, BAL-LIMIT OFF) | L1, C17A/B, 6V6 in classe AB |
| 31 | **Ronzio residuo** a 60/120/180 Hz (accensioni in alternata di 6J7/6V6) insieme al fruscio: rumore totale −53,7 dBm (RCA: −54 dBm massimo) | solo le 6SK7 hanno i filamenti in continua (SR1) |
| 32 | **Rete a 60 o 50 c/s**: frequenza del ronzio e del segnale di bilanciamento | "Power Required: 50-60 cycles" |
| 33 | Levette del telaio ridisegnate come un unico pezzo (dado, boccola, collare, leva cromata, estremità arrotondata) | prima la leva sembrava staccata dal perno |
| 34 | Marchio Collateral Effects: nome produttore, codice `Clfx`, bundle id, icona e sigla sul pannello | richiesta |

## Cosa resta un'approssimazione (dichiarato)

Il modello è circuitale a blocchi, non una simulazione nodale (SPICE) completa:

- I trasformatori T1/T2/T3 sono filtri lineari più una perdita del nucleo dipendente dal flusso (solo T3). Mancano isteresi e risonanze misurate.
- Gli stadi 6J7/6V6 in controreazione sono una non linearità istantanea tarata sulla specifica (0,18 % al limite, +38 dBm massimi).
- Il disaccoppiamento residuo della coppia di 6SK7 è dimensionato sulla specifica dei colpi, non misurato su valvole reali.
- Le letture V3–V6 sono nominali.
- Non esistono misure di un BA-6A reale. Tutto è calibrato sui dati pubblicati da RCA: le tolleranze dei test sono quelle della specifica, non quelle di un esemplare.
