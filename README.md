# ELABORATO HDR-RENDERING

Per utilizzare il progetto, basta aprire l'eseguibile hdr.exe nella cartella bin oppure creare una build con CMake

Prima di creare il progetto con CMake è consigliabile andare a modificare i file di settings in cui sono gestite le configurazioni (settings/config.json):

- **Window** : si può modificare larghezza e altezza della finestra 
- **Camera** : si può modificare la posizione della camera
- **Illumination** :
    1. *type* : cambiare il tipo di hdr tone-mapping (0=nessuno , 1=Reinhard, 2=Exponential, 3=Drago)
    2. *exposure*
    3. *dynamic_exp* : esposizione dinamica attiva o disattiva
    4. *adaptation_speed* : velocità di adattamento al cambio di luminosità dell'immagine
    5. *max_change* : massimo cambiamento di esposizione per frame
    6. *min_exposure* : minima esposizione raggiungibile con esposizione dinamica quando stiamo sopra una soglia di luminanza
    7. *avg_exposure* : esposizione media raggiungibile con esposizione dinamica che si ha quando stiamo in un range di luminanza
    8. *max_exposure* : massima esposizione raggiungibile con esposizione dinamica quando stiamo sotto una soglia di luminanza
    9. *inf_cap_luminance* : soglia inferiore di luminanza
    10. *sup_cap_luminance* : soglia superiore di luminanza
    11. *bloom.state* : bloom attivo o disattivo
    12. *bloom.standard_deviation* : deviazione standard della gaussiana per il blur
    13. *bloom.kernel_size* : dimensione del kernel per il blur
    14. *bloom.two_dim_blur_pass* : numero di volte che viene applicato il blur

Comandi utilizzabili:

- **W** : Movimento in avanti
- **A** : Movimento a sinistra
- **S** : Movimento indietro
- **D** : Movimento a destra
- **ESCAPE** : Chiudi finestra
- **0** : Attiva modalità "NO_HDR"
- **1** : Attiva modalità "REINHARD_HDR"
- **2** : Attiva modalità "EXPOSURE_HDR"
- **3** : Attiva modalità "DRAGO_HDR"
- **SPACE** : Attiva/Disattiva esposizione dinamica
- **B** : Attiva/Disattiva bloom
- **Q** : Aumenta esposizione
- **E** : Diminuisci esposizione

