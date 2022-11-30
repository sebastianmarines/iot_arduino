#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include "sdk_structs.h"
#include "ieee80211_structs.h"
#include "string_utils.h"
#include <set>


extern "C" {
#include "user_interface.h"
}


int eeAddress = 0;
std::set <String> macs;
int macs_size = 0;

int writeStringToEEPROM(int addrOffset, const String &strToWrite) {
    byte len = strToWrite.length();
    EEPROM.write(addrOffset, len);
    for (int i = 0; i < len; i++) {
        EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
    }
    return addrOffset + 1 + len;
}

int readStringFromEEPROM(int addrOffset, String *strToRead) {
    int newStrLen = EEPROM.read(addrOffset);
    char data[newStrLen + 1];
    for (int i = 0; i < newStrLen; i++) {
        data[i] = EEPROM.read(addrOffset + 1 + i);
    }
    data[newStrLen] = '\0'; // !!! NOTE !!! Remove the space between the slash "/" and "0" (I've added a space because otherwise there is a display bug)
    *strToRead = String(data);
    return addrOffset + 1 + newStrLen;
}


void scan();

void send();

void invertFlag();

void setup() {
    EEPROM.begin(512);
    Serial.begin(9600);

    delay(10);
    Serial.println("EEPROM test");

    int value = 0;
    EEPROM.get(eeAddress, value);
    Serial.print("eeAddress: ");
    Serial.println(eeAddress);

    Serial.print("Value: ");
    Serial.println(value);

    if (value == 0) {
        send();
    } else {
        scan();
    }
}

void scan() {
    Serial.println("Scanning...");
    wifi_set_channel(9);

    // Wifi setup
    wifi_set_opmode(STATION_MODE);
    wifi_promiscuous_enable(0);
    WiFi.disconnect();

    // Set sniffer callback
    wifi_set_promiscuous_rx_cb(wifi_sniffer_packet_handler);
    wifi_promiscuous_enable(1);

}

void send() {
    Serial.println("Sending...");
    // Read from EEPROM
    int size;
    int addrOffset = eeAddress + sizeof(int);
    Serial.print("addrOffset: ");
    Serial.println(addrOffset);

    EEPROM.get(addrOffset, size);
    Serial.print("size: ");
    Serial.println(size);
    if (size > 100) {
        size = 0;
    }

    int eeOffset = eeAddress + 1 + sizeof(size);
    for (int i = 0; i < size; i++) {
        String mac;
        eeOffset = readStringFromEEPROM(eeOffset, &mac);
        Serial.println(mac);
        Serial.println(eeOffset);
    }

    delay(10000);
    invertFlag();
    ESP.restart();
}

// According to the SDK documentation, the packet type can be inferred from the
// size of the buffer. We are ignoring this information and parsing the type-subtype
// from the packet header itself. Still, this is here for reference.
wifi_promiscuous_pkt_type_t packet_type_parser(uint16_t len) {
    switch (len) {
        // If only rx_ctrl is returned, this is an unsupported packet
        case sizeof(wifi_pkt_rx_ctrl_t):
            return WIFI_PKT_MISC;

            // Management packet
        case sizeof(wifi_pkt_mgmt_t):
            return WIFI_PKT_MGMT;

            // Data packet
        default:
            return WIFI_PKT_DATA;
    }
}

void wifi_sniffer_packet_handler(uint8_t *buff, uint16_t len) {
    Serial.println("Fucking interrupted");
    // First layer: type cast the received buffer into our generic SDK structure
    const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *) buff;
    // Second layer: define pointer to where the actual 802.11 packet is within the structure
    const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t *) ppkt->payload;
    // Third layer: define pointers to the 802.11 packet header and payload
    const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;
    const uint8_t *data = ipkt->payload;

    // Pointer to the frame control section within the packet header
    const wifi_header_frame_control_t *frame_ctrl = (wifi_header_frame_control_t *) &hdr->frame_ctrl;

    // Parse MAC addresses contained in packet header into human-readable strings
    char addr1[] = "00:00:00:00:00:00\0";
    char addr2[] = "00:00:00:00:00:00\0";
    char addr3[] = "00:00:00:00:00:00\0";

    mac2str(hdr->addr1, addr1);
    mac2str(hdr->addr2, addr2);
    mac2str(hdr->addr3, addr3);
    if (frame_ctrl->type == WIFI_PKT_MGMT && frame_ctrl->subtype != BEACON) {
        if (addr1 != "ff:ff:ff:ff:ff:ff") {
            macs.insert(addr1);
            macs_size++;
        }
        if (addr2 != "ff:ff:ff:ff:ff:ff") {
            macs.insert(addr2);
            macs_size++;
        }
        if (addr3 != "ff:ff:ff:ff:ff:ff") {
            macs.insert(addr3);
            macs_size++;
        }
    }

    if (macs_size > 10) {
        noInterrupts();
        wifi_set_promiscuous_rx_cb(NULL);
        wifi_promiscuous_enable(0);

        // Write mac_size to EEPROM
        Serial.print("Address: ");
        int addrOffset = eeAddress + sizeof(int);
        Serial.println(addrOffset);
        EEPROM.put(eeAddress, macs_size);
        EEPROM.commit();
        addrOffset += sizeof(macs_size);
        for (auto mac: macs) {
            eeAddress = writeStringToEEPROM(eeAddress, mac);
            Serial.print("Address where we are writing ");
            Serial.print(mac);
            Serial.print(" to: ");
            Serial.println(eeAddress);
            EEPROM.commit();
        }
        invertFlag();
        ESP.restart();
    }
}

void invertFlag() {
    int value = 0;
    eeAddress = 0;
    EEPROM.get(eeAddress, value);
    value = !value;
    EEPROM.put(eeAddress, value);
    EEPROM.commit();
}

void loop() {

}
