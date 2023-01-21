# Automatic irrigation system BLE


<table class="tab">
    <tr>
        <th>
        <p align= "center">
        <img src="https://i.ibb.co/TcgpnJq/3772970-40-removebg-preview.png" alt="Unisannio" width= 70%>
        </p>
        </th>
        <th>
        <p align= "center">
        <img src="https://i.ibb.co/Hn9MkrL/51-Jq-LMeu-Tc-L-removebg-preview.png" alt="Unisannio" width= 50%>
        </p>
        </th>
    </tr>
</table>

<style>
td, th {
   border: none!important;
}
</style>

<p align="center">
    <img src="https://img.shields.io/badge/C++-blue">
    <img src="https://img.shields.io/badge/MQTT-blue">
    <img src="https://img.shields.io/badge/MBED-blue" alt="Unisannio">
    <img src="https://img.shields.io/badge/NUCELO-STM32F401RE-blue" alt="Unisannio">
    <img src="https://img.shields.io/badge/XNUCLEO-IDB05A1-blue" alt="Unisannio">
    <img src="https://img.shields.io/badge/ESP-8266-blue" alt="Unisannio">
    <img src="https://img.shields.io/badge/Xiaomi-Mi Flora-blue" alt="Unisannio">
</p>

#
#### Componenti del gruppo:
 - Francesco Mazzitelli (Github: FrancescoMazzitelli)
 - Pio Antonio Perugini (Github: Pi0Ant0ni0)
 - Ermanno Nicoletti (Github: ermanno00)
 - Achille Melillo (Github: OptimusGold)

## Descrizione
Sviluppo di un'applicazione IOT per NUCLEO-STM32F401RE in grado di leggere le informazioni acquisite da un sensore BLE "Mi Flora" e di trasmetterle su un broker MQTT consultabile da un Hub Home Assistant.
Le informazioni che vengono trasmesse fanno riferimento a:
- Temperatura (°C)
- Umidità (%)
- Luce (lux)
- Conduttività (µS/cm)

## Configurazione
Per lo sviluppo del progetto è stato utilizzato il compilatore online KEIL Studio.
Le librerie utilizzate sono:
- mbed-os 5.13.1 (che comprende le librerie per interagire con la ESP8266 e X-NUCLEO IDB05A);
- mbed-mqtt-paho: per interagire con l’istanza del broker in rete.

In particolare, è necessario definire dei parametri di configurazione aggiuntivi all’interno del
file mbed-app.json.

## Funzionamento
Per leggere i valori del sensore si è reso necessario cambiarne la modalità di funzionamento scrivendo 2 byte (0xa01f) nell'handle specifico di cambio modalità (0x33).
Successivamente è possibile leggere i dati del sensore in tempo reale sfruttando l'handle dati (0x35).
Al fine di testare il corretto funzionamento di MiFloraCare è possibile scrivere nell'handle specifico
di cambio modalità (0x33) un pacchetto costituito da 2 byte (0xfdff) che fa lampeggiare il sensore.
In aggiunta è possibile monitorare in tempo reale lo stato della batteria e la versione del firmware
del dispositivo andando ad effettuare una lettura sull’handle 0x38.


## Tabella caratteristiche
#
<table>
    <tr>
        <th>
            <p> Service UIID
        </th>
        <th>
            <p> Characteristics UIID
        </th>
        <th>
            <p> Handle
        </th>
        <th>
            <p> R/W
        </th>
        <th>
            <p> Description
        </th>
    </tr>
        <th>
            <p> 00001204-
0000-1000-
8000-
00805f9b34fb
        </th>
        <th>
            <p> 00001a00-0000-
1000-8000-
00805f9b34fb
        </th>
        <th>
            <p> 0x0033
        </th>
        <th>
            <p> W
        </th>
        <th>
            <p> Used for
device
discovery
        </th>
    <tr>
        <th>
            <p> -
        </th>
        <th>
            <p> 00001a01-0000-
1000-8000-
00805f9b34fb
        </th>
        <th>
            <p> 0x0035
        </th>
        <th>
            <p> R
        </th>
        <th>
            <p> Get realtime sensor
values
        </th>
    </tr>
    <tr>
        <th>
            <p> -
        </th>
        <th>
            <p> 00001a02-0000-
1000-8000-
00805f9b34fb
        </th>
        <th>
            <p> 0x0038
        </th>
        <th>
            <p> R
        </th>
        <th>
            <p> Get
firmware
version and
battery
level
        </th>
    </tr>
    <tr>
        <th>
            <p> -
        </th>
        <th>
            <p> 00001a11-0000-
1000-8000-
00805f9b34fb
        </th>
        <th>
            <p> 0x003c
        </th>
        <th>
            <p> R
        </th>
        <th>
            <p> Get
historical
sensor
values
        </th>
    </tr>
</table>