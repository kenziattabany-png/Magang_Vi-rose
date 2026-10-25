#include "main.h"

esp_now_peer_info_t peer_info;

uint8_t mac_addresses[MAC_ADDRESS_TOTAL][MAC_ADDRESS_LENGTH] = {
    {0x24, 0x0A, 0xC4, 0x0A, 0x21, 0x11},  // FARUQ
    {0x24, 0x0A, 0xC4, 0x0A, 0x21, 0x10},  // FAUZAN
    {0x24, 0x0A, 0xC4, 0x0A, 0x20, 0x11},  // RAFAINA
    {0x24, 0x0A, 0xC4, 0x0A, 0x10, 0x10},  // Danish Fattah Azizi
    {0x24, 0x0A, 0xC4, 0x0A, 0x10, 0x11},  // Marsya Safira
    {0x24, 0x0A, 0xC4, 0x0A, 0x11, 0x10},  // Faqih Al Minanji
    {0x24, 0x0A, 0xC4, 0x0A, 0x11, 0x11},  // Farid Azzam Arrayyan
    {0x24, 0x0A, 0xC4, 0x0A, 0x12, 0x10},  // Kenzi Attabany
    {0x24, 0x0A, 0xC4, 0x0A, 0x12, 0x11},  // Nabila Fathania Rizqi Romadlon
    {0x24, 0x0A, 0xC4, 0x0A, 0x13, 0x10},  // Felicia Metta Tania
};

const char *mac_names[MAC_ADDRESS_TOTAL] = {
    "FARUQ",                               // 0
    "FAUZAN",                              // 1
    "RAFAINA",                             // 2
    "Danish Fattah Azizi",                 // 3
    "Marsya Safira",                       // 4
    "Faqih Al Minanji",                    // 5
    "Farid Azzam Arrayyan",                // 6
    "Kenzi Attabany",                      // 7
    "Nabila Fathania Rizqi Romadlon",      // 8
    "Felicia Metta Tania",                 // 9
};

esp_err_t mulai_esp_now(int index_mac_address) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    /* Init ESP-NOW */
    esp_err_t result = esp_now_init();
    if (result != ESP_OK)
        return result;

    /* Set callback function to handle received data */
    result = esp_now_register_recv_cb(callback_data_esp_now);
    if (result != ESP_OK)
        return result;

    result = esp_now_register_send_cb(callback_pengiriman_esp_now);
    //     if (result != ESP_OK)
    //         return result;

    /* Set MAC Address */
    uint8_t mac[MAC_ADDRESS_LENGTH];
    memcpy(mac, mac_addresses[index_mac_address], MAC_ADDRESS_LENGTH);
    result = esp_wifi_set_mac(WIFI_IF_STA, mac);
    if (result != ESP_OK)
        return result;

    /* Initialize peer_info and set fields*/
    memset(&peer_info, 0, sizeof(esp_now_peer_info_t));
    peer_info.channel = 0;
    peer_info.encrypt = false;

    /* Add All MAC to peer list  */
    for (int i = 0; i < MAC_ADDRESS_TOTAL; i++) {
        memcpy(peer_info.peer_addr, mac_addresses[i], MAC_ADDRESS_LENGTH);
        result = esp_now_add_peer(&peer_info);
        if (result != ESP_OK)
            return result;
    }

    return ESP_OK;
}

int cari_mac_index(const uint8_t *mac) {
    for (int i = 0; i < MAC_ADDRESS_TOTAL; i++) {
        // Compare the MAC address
        if (memcmp(mac, mac_addresses[i], MAC_ADDRESS_LENGTH) == 0)
            return i;
    }

    // if not found return -1
    return -1;
}

String mac_index_to_names(int mac_index) {
    if ((mac_index < 0 || mac_index >= MAC_ADDRESS_TOTAL) || (mac_index == -1)) {
        return "Unknown";
    }
    return mac_names[mac_index];
}

void callback_data_esp_now(const uint8_t *mac, const uint8_t *data, int len) {
    int index_mac_asal = cari_mac_index(mac);
    process_perintah(data, len, index_mac_asal);
}
void callback_pengiriman_esp_now(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.printf("\nStatus pengiriman ESP-NOW: %s\n", esp_err_to_name(status));
}
void callback_data_serial(const uint8_t *data, int len) {
    process_perintah(data, len);
}
void baca_serial(void (*callback)(const uint8_t *data, int len)) {
    // TODO 2: implementasi kode buat nerima perintah dari serial
    static uint8_t buffer[BUFFER_SIZE];
    static int length = 0;

    while (Serial.available() > 0) {
        char c = Serial.read();

        // Mengirimkan data saat menerima Enter ('\n' atau '\r')
        if (c == '\n' || c == '\r') {
            if (length > 0) {
                buffer[length] = '\0';
                callback(buffer, length);
                length = 0; // Reset buffer
            }
        } else {
            if (length < BUFFER_SIZE - 1) {
                buffer[length++] = (uint8_t)c;
            }
        }
    }
}

void process_perintah(const uint8_t *data, int len, int index_mac_address_asal) {
    // Ubah data byte ke bentuk String agar mudah dipparsing
    String input = "";
    for (int i = 0; i < len; i++) {
        input += (char)data[i];
    }
    input.trim();

    // ==========================================
    // 1. JIKA PERINTAH BERASAL DARI SERIAL (index_mac_address_asal == -1)
    // ==========================================
    if (index_mac_address_asal == -1) {
        // Format input dari serial diasumsikan misal: "HALO <NamaTujuan>" atau "CEK <NamaTujuan>"
        // Atau Anda bisa menyesuaikan cara parsing sesuai format tugas Anda.
        // Contoh sederhana parsing command & target:
        
        int spaceIndex = input.indexOf(' ');
        String cmdStr = (spaceIndex == -1) ? input : input.substring(0, spaceIndex);
        String targetNama = (spaceIndex == -1) ? "" : input.substring(spaceIndex + 1);
        targetNama.trim();

        // Cari index MAC tujuan berdasarkan nama
        int targetIndex = -1;
        for (int i = 0; i < MAC_ADDRESS_TOTAL; i++) {
            if (String(mac_names[i]).equalsIgnoreCase(targetNama)) {
                targetIndex = i;
                break;
            }
        }

        if (targetIndex == -1) {
            Serial.println("[ERROR] Nama tujuan tidak ditemukan!");
            return;
        }

        String namaKu = mac_index_to_names(mac_index_ku);

        if (cmdStr.equalsIgnoreCase("HALO")) {
            // Ubah 'HAL' menjadi 'HALO'
            String pesanKirim = String(HALO) + ", 'Halo " + targetNama + " Aku " + namaKu + "'"; 
            esp_now_send(mac_addresses[targetIndex], (uint8_t *)pesanKirim.c_str(), pesanKirim.length());
            Serial.printf("[KIRIM SERIAL -> ESP-NOW]: %s\n", pesanKirim.c_str());
        }
        
        else if (cmdStr.equalsIgnoreCase("CEK")) {
            // Ketentuan: Kirim packet perintah ke pengirim dengan format: "CEK(enum), '[Nama tujuan] ini [Namamu] apa kamu disana?'"
            String pesanKirim = String(CEK) + ", '" + targetNama + " ini " + namaKu + " apa kamu disana?'";
            esp_now_send(mac_addresses[targetIndex], (uint8_t *)pesanKirim.c_str(), pesanKirim.length());
            Serial.printf("[KIRIM SERIAL -> ESP-NOW]: %s\n", pesanKirim.c_str());
        }
    } 
    // ==========================================
    // 2. JIKA PERINTAH BERASAL Dari ESP-NOW (index_mac_address_asal >= 0)
    // ==========================================
    else {
        String namaPengirim = mac_index_to_names(index_mac_address_asal);
        String namaKu = mac_index_to_names(mac_index_ku);

        // Parsing enum di awal data (misal format: "0, 'Isi pesan'")
        int commaIndex = input.indexOf(',');
        if (commaIndex == -1) return;

        int enumCommand = input.substring(0, commaIndex).toInt();
        String isiPesan = input.substring(commaIndex + 1);
        isiPesan.trim();

        if (enumCommand == HALO) {
            // Ketentuan: Jika menerima HALO dari ESP-NOW, kirim JAWABAN ke tujuan pengirim: "JAWAB(enum), 'Halo Juga [Nama Pengirim] Aku [Namamu]'"
            String balas = String(JAWAB) + ", 'Halo Juga " + namaPengirim + " Aku " + namaKu + "'";
            esp_now_send(mac_addresses[index_mac_address_asal], (uint8_t *)balas.c_str(), balas.length());
            Serial.printf("[ESP-NOW] Terima HALO dari %s. Mengirim balasan...\n", namaPengirim.c_str());

        } else if (enumCommand == CEK) {
            // Ketentuan: Jika menerima CEK dari ESP-NOW, kirim JAWABAN ke tujuan pengirim: "JAWAB(enum), 'Iya Aku [Nama pengirim] Disini - [Namamu]'"
            // Catatan: Sesuai gambar, [Nama pengirim] di sini merujuk pada nama pengirim asli/lawan bicara
            String balas = String(JAWAB) + ", 'Iya Aku " + namaPengirim + " Disini - " + namaKu + "'";
            esp_now_send(mac_addresses[index_mac_address_asal], (uint8_t *)balas.c_str(), balas.length());
            Serial.printf("[ESP-NOW] Terima CEK dari %s. Mengirim balasan...\n", namaPengirim.c_str());
            
        } else if (enumCommand == JAWAB) {
            // Ketentuan: Perintah JAWAB cuma menerima dari ESP-NOW, cetak ke Serial Monitor String yang diterima
            Serial.printf("[JAWAB DARI %s]: %s\n", namaPengirim.c_str(), isiPesan.c_str());
        }
    }
}