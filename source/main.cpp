/* mbed Microcontroller Library
 * Copyright (c) 2006-2015 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstdlib>
#include <events/mbed_events.h>
#include <mbed.h>
#include "ble/BLE.h"
#include "ble/DiscoveredCharacteristic.h"
#include "ble/DiscoveredService.h"
#include "ble/gap/Gap.h"
#include "ble/gap/AdvertisingDataParser.h"
#include "pretty_printer.h"
#include <string>
#include <MQTTClientMbedOs.h>


WiFiInterface *wifi;

const char *sec2str(nsapi_security_t sec)
{
    switch (sec) {
        case NSAPI_SECURITY_NONE:
            return "None";
        case NSAPI_SECURITY_WEP:
            return "WEP";
        case NSAPI_SECURITY_WPA:
            return "WPA";
        case NSAPI_SECURITY_WPA2:
            return "WPA2";
        case NSAPI_SECURITY_WPA_WPA2:
            return "WPA/WPA2";
        case NSAPI_SECURITY_UNKNOWN:
        default:
            return "Unknown";
    }
}

int scan_demo(WiFiInterface *wifi)
{
    WiFiAccessPoint *ap;

    printf("Scan:\n");

    int count = wifi->scan(NULL,0);

    if (count <= 0) {
        printf("scan() failed with return value: %d\n", count);
        return 0;
    }

    /* Limit number of network arbitrary to 15 */
    count = count < 15 ? count : 15;

    ap = new WiFiAccessPoint[count];
    count = wifi->scan(ap, count);

    if (count <= 0) {
        printf("scan() failed with return value: %d\n", count);
        return 0;
    }

    for (int i = 0; i < count; i++) {
        printf("Network: %s secured: %s BSSID: %hhX:%hhX:%hhX:%hhx:%hhx:%hhx RSSI: %hhd Ch: %hhd\n", ap[i].get_ssid(),
               sec2str(ap[i].get_security()), ap[i].get_bssid()[0], ap[i].get_bssid()[1], ap[i].get_bssid()[2],
               ap[i].get_bssid()[3], ap[i].get_bssid()[4], ap[i].get_bssid()[5], ap[i].get_rssi(), ap[i].get_channel());
    }
    printf("%d networks available.\n", count);

    delete[] ap;
    return count;
}








const static char *peerAddress = "5c857eb0075e";
static UUID serviceUUID("00001204-0000-1000-8000-00805f9b34fb");
// the characteristic of the remote service we are interested in
static UUID uuid_version_battery("00001a02-0000-1000-8000-00805f9b34fb");
static UUID uuid_sensor_data("00001a01-0000-1000-8000-00805f9b34fb");
static UUID uuid_write_mode("00001a00-0000-1000-8000-00805f9b34fb");
static TCPSocket socket;
static MQTTClient client(&socket);

ble::connection_handle_t connhandle;
//magick packet per avviare la lettura
uint8_t bufmagic[2] = {0xa0, 0x1f};
//fa blikare il sensore
uint8_t  bufblink[2] = {0xfd, 0xff};

static EventQueue event_queue(/* event count */ 10 * EVENTS_EVENT_SIZE);

static DiscoveredCharacteristic battery_characteristic;
static bool trigger_battery_characteristic = false;

static DiscoveredCharacteristic blink_characteristic;
static bool trigger_blink_characteristic = false;

static DiscoveredCharacteristic data_characteristic;
static bool trigger_data_characteristic = false;
static bool connectedBT=false;

void sendDataMQTT(float temperature, int brightness, int moisture, int conductivity){
    

    char* json;
    asprintf(&json,"{\"temperature\": %.2f,\"brightness\": %d,\"moisture\": %d,\"conductivity\": %d}",temperature,brightness,moisture,conductivity);
    MQTT::Message message;
    message.qos = MQTT::QOS1;
    message.retained = true;
    message.dup = false;
    message.payload = (void*)json;
    char buff = json[0];
    int i=0;
    while(buff!='}'){
        i++;
        buff=json[i];
    }
    i++;
    message.payloadlen =i;
    if(!client.isConnected()) {
        printf("Riconnessione a MQTT\r\n");
        socket.close();
        socket.open(wifi);
        socket.connect("dev.nicolettinetwork.com", 5001);
        MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
        data.MQTTVersion = 3;
        data.clientID.cstring = "mbed-publisher";
        data.username.cstring = "dev";
        data.password.cstring = "dev";
        nsapi_error_t error = client.connect(data);

        if(error==NSAPI_ERROR_OK){
            printf("Connesso a MQTT\r\n");
        }else printf("Errore durante la riconnessione a MQTT. Codice errore: %d\r\n",error);

        
    }
    nsapi_error_t error = client.publish("mbed-irrigation-bridge-topic", message);
    if(error==NSAPI_ERROR_OK){
            printf("DATA PUBLISHED ON MQTT\r\n");
        }else {
            printf("Errore nell'invio a MQTT\r\n");
            printf("Riconnessione a MQTT\r\n");
            socket.close();
            socket.open(wifi);
            socket.connect("dev.nicolettinetwork.com", 5001);
            MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
            data.MQTTVersion = 3;
            data.clientID.cstring = "mbed-publisher";
            data.username.cstring = "dev";
            data.password.cstring = "dev";
            nsapi_error_t error = client.connect(data);

            if(error==NSAPI_ERROR_OK){
                printf("Connesso a MQTT\r\n");
                error = client.publish("mbed-irrigation-bridge-topic", message);
                if(error==NSAPI_ERROR_OK){
                    printf("DATA PUBLISHED ON MQTT\r\n");
                }else printf("Impossibile inviare il messaggio. Codice errore: %d\r\n",error);
            }else {
                printf("Errore durante la riconnessione a MQTT\r\n");
                //reset
                NVIC_SystemReset();
            }

        }
    
    
    //client.disconnect();
}

void sendHeartBeat(){
    char* json;
    asprintf(&json,"{status: ok}");
    MQTT::Message message;
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*)json;
    message.payloadlen = 13;
    client.publish("mbed-irrigation-bridge-topic-heartbeat", message);

}

void service_discovery(const DiscoveredService *service) {
    if (service->getUUID().shortOrLong() == UUID::UUID_TYPE_SHORT) {
        printf("S UUID-%x attrs[%u %u]\r\n", service->getUUID().getShortUUID(), service->getStartHandle(), service->getEndHandle());
    } else {
        printf("S UUID-");
        const uint8_t *longUUIDBytes = service->getUUID().getBaseUUID();
        for (unsigned i = 0; i < UUID::LENGTH_OF_LONG_UUID; i++) {
            printf("%02x", longUUIDBytes[i]);
        }
        printf(" attrs[%u %u]\r\n", service->getStartHandle(), service->getEndHandle());
    }
}

void update_battery_characteristic(void) {
    if (!BLE::Instance().gattClient().isServiceDiscoveryActive()) {
        battery_characteristic.read();
    }
}

void update_blink_characteristic(void) {
    if (!BLE::Instance().gattClient().isServiceDiscoveryActive()) {

        if(connectedBT){
            ble_error_t error = blink_characteristic.write(2, bufmagic);
        
        if (error) {
                    print_error(error, "Error");
                    
        }else printf("sent magic packet\r\n");

        }
            
        
    }
}

void update_data_characteristic(void) {
    if (!BLE::Instance().gattClient().isServiceDiscoveryActive()) {
        printf("Trying to read data\r\n");
        data_characteristic.read();
    }
}

//Callback
void characteristic_discovery(const DiscoveredCharacteristic *characteristicP) {
    printf("  C UUID-%x valueAttr[%u] props[%x]\r\n", characteristicP->getUUID().getShortUUID(), characteristicP->getValueHandle(), (uint8_t)characteristicP->getProperties().broadcast());
    /*if (characteristicP->getUUID().getShortUUID() == UUID::ShortUUIDBytes_t(0x1a02)){
        printf("Battery characteristic triggered");
        battery_characteristic        = *characteristicP;
        trigger_battery_characteristic = true;
    }*/
   
    if (characteristicP->getUUID().getShortUUID() == UUID::ShortUUIDBytes_t(0x1a00)){
        printf("Write characteristic triggered");
        blink_characteristic        = *characteristicP;

        trigger_blink_characteristic = true;
    }

    /*if (characteristicP->getUUID().getShortUUID() == UUID::ShortUUIDBytes_t(0x1a01)){
        printf("Data characteristic triggered");
        data_characteristic        = *characteristicP;
        trigger_data_characteristic = true;
    }*/
}

void discovery_termination(Gap::Handle_t connectionHandle) {
    printf("terminated SD for handle %u\r\n", connectionHandle);
    
    if (trigger_battery_characteristic) {
        trigger_battery_characteristic = false;
        event_queue.call(update_battery_characteristic);
    }
    
    if (trigger_blink_characteristic) {
        trigger_blink_characteristic = false;
        event_queue.call(update_blink_characteristic);
    }

     /*if (trigger_data_characteristic) {
        trigger_data_characteristic = false;
        event_queue.call(update_data_characteristic);
    }*/
}

void on_read(const GattReadCallbackParams *response) {
    printf("trigger_data_read: handle %u, offset %u, len %u\r\n", response->handle, response->offset, response->len);
    printf("Data in HEX: ");
        for (unsigned index = 0; index < response->len; index++) {
            printf("%02x ", response->data[index]);
        }

        printf("\r\n");
        printf("\r\n");
        
        char* temp;
        asprintf(&temp,"%02x%02x",response->data[1],response->data[0]);
        float tempNumb = (int)strtol(temp,NULL, 16)/10.0;
        printf("Temperature: %.2f\xF8\x43",tempNumb);
        printf("\r\n");

        char* brightness;
        asprintf(&brightness,"%02x%02x%02x%02x",response->data[6],response->data[5],response->data[4],response->data[3]);
        int brightnessNumb = (int)strtol(brightness,NULL, 16);
        printf("Brightness: %dlux",brightnessNumb);
        printf("\r\n");

        char* moisture;
        asprintf(&moisture,"%02x",response->data[7]);
        int moistureNumb = (int)strtol(moisture,NULL, 16);
        printf("Moisture: %d%%",moistureNumb);
        printf("\r\n");

        char* cond;
        asprintf(&cond,"%02x%02x",response->data[9],response->data[8]);
        int condNumb = (int)strtol(cond,NULL, 16);
        printf("Conductivity: %d\xE6S/cm",condNumb);
        printf("\r\n");
        printf("\r\n");
        printf("\r\n");

        //Verifico che la lettura sia veritiera
        if(tempNumb>200 || moistureNumb>100){
            printf("Data error. Skipped\r\n");
        }else{
            sendDataMQTT(tempNumb,brightnessNumb,moistureNumb,condNumb);
        }
        
    /*if (response->handle == battery_characteristic.getValueHandle()) {
        printf("trigger_battery_read: handle %u, offset %u, len %u\r\n", response->handle, response->offset, response->len);
        
        uint8_t batteria = response->data[0];
        printf("Batteria: %d\r\n", batteria);
        
        for (unsigned index = 0; index < response->len; index++) {
            printf("%c[%02x]", response->data[index], response->data[index]);
        }
        printf("\r\n");

        
        //led_characteristic.write(1, &toggledValue);
    }
    if (response->handle == blink_characteristic.getValueHandle()) {
        printf("trigger_blink_read: handle %u, offset %u, len %u\r\n", response->handle, response->offset, response->len);
             
        for (unsigned index = 0; index < response->len; index++) {
            printf("%c[%02x]", response->data[index], response->data[index]);
        }
        //printf("\r\n");
        
        
    }
    if (response->handle == data_characteristic.getValueHandle()) {
        printf("trigger_data_read: handle %u, offset %u, len %u\r\n", response->handle, response->offset, response->len);
        
        for (unsigned index = 0; index < response->len; index++) {
            printf("%02x", response->data[index]);
        }
        printf("\r\n");

    }*/

}

void on_write(const GattWriteCallbackParams *response) {
    printf("On Write\r\n");
    

    uint16_t offset = 0;
    GattAttribute::Handle_t handle = GattAttribute::Handle_t(0x35);

    if(connectedBT){
        ble_error_t error = BLE::Instance().gattClient().read(
       connhandle,
         handle, 
        offset);
    
    if (error) {
                    printf("Error reading\r\n");
                    print_error(error, "Error");
                    
                }
    }
}

class LEDBlinkerDemo : ble::Gap::EventHandler {
public:
    LEDBlinkerDemo(BLE &ble, events::EventQueue &event_queue):
        _ble(ble),
        _event_queue(event_queue),
        _alive_led(LED1, 1),
        _actuated_led(LED2, 0),
        _is_connecting(false) { }

    ~LEDBlinkerDemo() { }

    void start() {
        _ble.gap().setEventHandler(this);
        _ble.init(this, &LEDBlinkerDemo::on_init_complete);
        _event_queue.call_every(5000, this, &LEDBlinkerDemo::blink);
        _event_queue.dispatch_forever();
    }

private:
    /** Callback triggered when the ble initialization process has finished */
    void on_init_complete(BLE::InitializationCompleteCallbackContext *params) {
        if (params->error != BLE_ERROR_NONE) {
            printf("Ble initialization failed.");
            return;
        }

        print_mac_address();

        _ble.gattClient().onDataRead(on_read);
        _ble.gattClient().onDataWritten(on_write);

        ble::ScanParameters scan_params;
        
       // _ble.gap().setScanParameters(scan_params);
        _ble.gap().setScanParams(GapScanningParams::SCAN_INTERVAL_MAX, GapScanningParams::SCAN_WINDOW_MAX);
        _ble.gap().startScan();
    }

    void blink() {
        _alive_led = !_alive_led;
        //sendHeartBeat();
    }

private:
    /* Event handler */

    void onDisconnectionComplete(const ble::DisconnectionCompleteEvent&) {
        connectedBT = false;
        _is_connecting = false;
        
        printf("Disconnect\r\n");
        thread_sleep_for(20000);
        printf("Start scan\r\n");
        _ble.gap().startScan();
        
    }

    void onConnectionComplete(const ble::ConnectionCompleteEvent& event) {
        
        if (event.getOwnRole() == ble::connection_role_t::CENTRAL) {
            connectedBT=true;

            connhandle = event.getConnectionHandle();

            _ble.gattClient().onServiceDiscoveryTermination(discovery_termination);

            /*GattAttribute::Handle_t handle = GattAttribute::Handle_t(0x33);
            ble_error_t error = _ble.gattClient().write(
                GattClient::WriteOp_t::GATT_OP_WRITE_CMD,               
                event.getConnectionHandle(),
                handle, 
                2, 
                bufmagic
            );*/

            ble_error_t error = _ble.gattClient().launchServiceDiscovery(
                event.getConnectionHandle(),
                service_discovery,
                characteristic_discovery,
                serviceUUID,
                UUID::ShortUUIDBytes_t(BLE_UUID_UNKNOWN)
            );


            if (error) {
                    print_error(error, "Error");
                    //reset
                    NVIC_SystemReset();
                    _is_connecting = false;
                }

        } else {
            _ble.gap().startScan();
        }
       // _is_connecting = false;
    }

    void onAdvertisingReport(const ble::AdvertisingReportEvent &event) {
        /* don't bother with analysing scan result if we're already connecting */
        if (_is_connecting) {
            //printf("Altready connecting\r\n");
            return;
        }

        ble::AdvertisingDataParser adv_data(event.getPayload());

        /* parse the advertising payload, looking for a discoverable device */
        while (adv_data.hasNext()) {
            ble::AdvertisingDataParser::element_t field = adv_data.next();

            /* connect to a discoverable device */
             char *macaddr;
            asprintf(&macaddr, "%02x%02x%02x%02x%02x%02x", event.getPeerAddress().data()[5], event.getPeerAddress().data()[4], event.getPeerAddress().data()[3], event.getPeerAddress().data()[2], event.getPeerAddress().data()[1], event.getPeerAddress().data()[0]);
            if(strcmp(macaddr, peerAddress) == 0) {

                printf("Adv from: ");
                print_address(event.getPeerAddress().data());
                printf(" rssi: %d, scan response: %u, connectable: %u\r\n",
                       event.getRssi(), event.getType().scan_response(), event.getType().connectable());

                ble_error_t error = _ble.gap().stopScan();

                if (error) {
                    print_error(error, "Error caused by Gap::stopScan");
                    return;
                }

                const ble::ConnectionParameters connection_params;

                error = _ble.gap().connect(
                    event.getPeerAddressType(),
                    event.getPeerAddress(),
                    connection_params
                );

                if (error) {
                    _ble.gap().startScan();
                    return;
                }

                /* we may have already scan events waiting
                 * to be processed so we need to remember
                 * that we are already connecting and ignore them */
                _is_connecting = true;

                return;
            }
        }
    }

private:
    BLE &_ble;
    events::EventQueue &_event_queue;
    DigitalOut _alive_led;
    DigitalOut _actuated_led;
    bool _is_connecting;
};

/** Schedule processing of events from the BLE middleware in the event queue. */
void schedule_ble_events(BLE::OnEventsToProcessCallbackContext *context) {
    event_queue.call(Callback<void()>(&context->ble, &BLE::processEvents));
}

int main()
{
    //connessione ad internet
     wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
        printf("ERROR: No WiFiInterface found.\n");
        return -1;
    }
    
    /*
    int count = scan_demo(wifi);
    if (count == 0) {
        printf("No WIFI APs found - can't continue further.\n");
        return -1;
    }
    */

    printf("\nConnecting to %s...\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
        printf("\nConnection error: %d\n", ret);
        return -1;
    }

    printf("Success\n\n");
    printf("MAC: %s\n", wifi->get_mac_address());
    printf("IP: %s\n", wifi->get_ip_address());
    printf("Netmask: %s\n", wifi->get_netmask());
    printf("Gateway: %s\n", wifi->get_gateway());
    //printf("RSSI: %d\n\n", wifi->get_rssi());

//    NetworkInterface *net = NetworkInterface::get_default_instance();
    
    socket.open(wifi);
    socket.connect("dev.nicolettinetwork.com", 5001);
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = "mbed-publisher";
    data.username.cstring = "dev";
    data.password.cstring = "dev";
    client.connect(data);
    nsapi_error_t error = client.connect(data);

    if(error==NSAPI_ERROR_OK){
            printf("Connesso a MQTT\r\n");
        }else {
            printf("Impossibile collegarsi ad MQTT. Codice errore: %d\r\n",error);
            printf("Riconnessione a MQTT\r\n");
            socket.close();
            socket.open(wifi);
            socket.connect("dev.nicolettinetwork.com", 5001);
            MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
            data.MQTTVersion = 3;
            data.clientID.cstring = "mbed-publisher";
            data.username.cstring = "dev";
            data.password.cstring = "dev";
            nsapi_error_t error = client.connect(data);

            if(error==NSAPI_ERROR_OK){
                printf("Connesso a MQTT\r\n");
            }else{
                printf("Errore durante la riconnessione a MQTT. Codice errore: %d\r\n",error);
                printf("Verra' effettuato un nuovo tentativo al prossimo invio\r\n");
            } 
        }



    BLE &ble = BLE::Instance();
    ble.onEventsToProcess(schedule_ble_events);

    LEDBlinkerDemo demo(ble, event_queue);
    demo.start();

    return 0;
}
