// ITS-Parkir - ESP-BRIDGE
// Menghubungkan laptop dengan ESP-Server melalui ESP-NOW.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#define TEST_MODE 0  // 0 normal, 1 panjang salah, 2 header salah, 3 tujuan salah

// Samakan token dan MAC dengan esp_server.cpp
const uint8_t TOKEN[6] = {
    0x50, 0x41, 0x52, 0x4B, 0x49, 0x52
};  // Token "PARKIR" dalam hexadecimal

uint8_t MAC_BRIDGE[6] = {
    0x24, 0x0A, 0xC4, 0x0A, 0x99, 0x10
};

uint8_t MAC_SERVER[6] = {
    0x24, 0x0A, 0xC4, 0x0A, 0x99, 0x11
};

// Format paket: HEADER | SOURCE | DEST | TOKEN | LENGTH | PAYLOAD
#define HEADER_SIZE 14
#define MAX_PAYLOAD 236  // 250 byte maksimum - 14 byte header
#define ADDR_BRIDGE 1
#define ADDR_SERVER 2

// Pesan error berdasarkan nomor kode E01 sampai E16
const char *PESAN[] = {
    "",
    "Token tidak valid.",
    "Header tidak valid.",
    "Panjang data tidak valid.",
    "Hexadecimal tidak valid.",
    "JSON tidak valid.",
    "Action tidak dikenal.",
    "Parameter tidak valid.",
    "Data tidak ditemukan.",
    "Proses bisnis gagal.",
    "Timeout, respons tidak diterima.",
    "Tujuan tidak valid.",
    "Pemrosesan gagal.",
    "Zona parkir penuh.",
    "Pelat sudah aktif di area parkir.",
    "Pelat tidak punya tiket aktif.",
    "Kapasitas tidak konsisten."
};

// Buffer untuk menyimpan paket respons dari server
uint8_t respons[250];
volatile int responsLen = 0;
volatile bool responsAda = false;

// Menampilkan kode error beserta pesannya ke laptop
void lapor(const char *kode) {
    int no = atoi(kode + 1);

    Serial.printf(
        "ERR %s %s\n",
        kode,
        (no >= 1 && no <= 16)
            ? PESAN[no]
            : "Kode error tidak dikenal."
    );
}

// Mengubah satu karakter hexadecimal menjadi angka 0-15
int hexNilai(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;  // Karakter bukan hexadecimal
}

// FUNGSI TAMBAHAN: Mengubah string teks biasa (JSON) menjadi HEX secara otomatis
String toHex(const char *input) {
    String output = "";
    int len = strlen(input);
    for (int i = 0; i < len; i++) {
        char c = input[i];
        char buf[3];
        sprintf(buf, "%02X", c);
        output += buf;
    }
    return output;
}

// Callback saat paket ESP-NOW diterima
void terimaData(
    const uint8_t *mac,
    const uint8_t *data,
    int len
) {
    // Abaikan paket dari MAC lain atau panjang yang tidak valid
    if (memcmp(mac, MAC_SERVER, 6) != 0 ||
        len <= 0 || len > 250) return;

    // Simpan respons agar dapat diproses di loop utama
    memcpy(respons, data, len);
    responsLen = len;
    responsAda = true;
}

// Memvalidasi respons server dan mengubah payload menjadi teks
void terjemahkan(const uint8_t *p, int len) {
    // Periksa panjang sebelum membaca isi header
    if (len < HEADER_SIZE) {
        lapor("E03");
        return;
    }

    // Pastikan header dan alamat perangkat sesuai
    if (memcmp(p, "ITSP", 4) != 0 ||
        p[4] != ADDR_SERVER ||
        p[5] != ADDR_BRIDGE) {
        lapor("E02");
        return;
    }

    // Ambil panjang payload dari dua byte LENGTH
    int plen = (p[12] << 8) | p[13];

    // Panjang harus sesuai dan jumlah karakter hexadecimal genap
    if (plen != len - HEADER_SIZE ||
        plen > MAX_PAYLOAD || plen % 2 != 0) {
        lapor("E03");
        return;
    }

    // Konversi dua digit hexadecimal menjadi satu byte
    char teks[MAX_PAYLOAD / 2 + 1];
    int n = plen / 2;

    for (int i = 0; i < n; i++) {
        int a = hexNilai(p[14 + 2 * i]);
        int b = hexNilai(p[15 + 2 * i]);

        if (a < 0 || b < 0) {
            lapor("E04");
            return;
        }

        teks[i] = a * 16 + b;
    }

    teks[n] = '\0';  // Penanda akhir string C

    // Tampilkan kode error atau respons sukses
    if (n == 3 && teks[0] == 'E')
        lapor(teks);
    else
        Serial.printf("OK %s\n", teks);
}

// Memproses input dari laptop (bisa Hex langsung atau teks JSON biasa) dan mengirimnya ke server
void proses(char *baris) {
    uint8_t token[6];

    // Gunakan token default, kecuali mode pengujian token
    memcpy(token, TOKEN, 6);
    char *payloadTarget = baris;

    // Format uji: T<12 digit token hexadecimal>:<payload>
    if (baris[0] == 'T') {
        if (strlen(baris) < 14 || baris[13] != ':') {
            lapor("E04");
            return;
        }

        // Konversi 12 digit hexadecimal menjadi 6 byte token
        for (int i = 0; i < 6; i++) {
            int a = hexNilai(baris[1 + 2 * i]);
            int b = hexNilai(baris[2 + 2 * i]);

            if (a < 0 || b < 0) {
                lapor("E04");
                return;
            }

            token[i] = a * 16 + b;
        }

        payloadTarget = baris + 14;  // Lewati token dan tanda titik dua
    }

    // Pengecekan cerdas: Jika input diawali '{', ubah otomatis ke string Hex oleh ESP-Bridge!
    String hexStr = "";
    if (payloadTarget[0] == '{') {
        hexStr = toHex(payloadTarget);
        payloadTarget = (char*)hexStr.c_str();
    }

    // Validasi panjang payload
    int n = strlen(payloadTarget);

    if (n == 0 || n % 2 != 0) {
        lapor("E04");
        return;
    }

    if (n > MAX_PAYLOAD) {
        lapor("E03");
        return;
    }

    // Pastikan semua karakter payload adalah hexadecimal
    for (int i = 0; i < n; i++) {
        if (hexNilai(payloadTarget[i]) < 0) {
            lapor("E04");
            return;
        }
    }

    uint8_t paket[250];

    // Susun header dan alamat perangkat
    memcpy(paket, "ITSP", 4);
    paket[4] = ADDR_BRIDGE;
    paket[5] = ADDR_SERVER;

    // Masukkan token ke paket
    memcpy(paket + 6, token, 6);

    // Simpan panjang payload dalam dua byte
    paket[12] = n >> 8;
    paket[13] = n & 0xFF;

    // Salin payload ke paket dalam huruf hexadecimal besar
    for (int i = 0; i < n; i++)
        paket[14 + i] = toupper(payloadTarget[i]);

    // Mode pengujian: sengaja merusak bagian paket
#if TEST_MODE == 1
    paket[13] += 1;  // Panjang salah
#elif TEST_MODE == 2
    paket[0] = 'X';  // Header salah
#elif TEST_MODE == 3
    paket[5] = 9;    // Tujuan salah
#endif

    Serial.printf(
        "# kirim ke server: %d byte, token %02X..%02X\n",
        HEADER_SIZE + n, token[0], token[5]
    );

    // Reset penanda sebelum menunggu respons baru
    responsAda = false;

    // Kirim paket melalui ESP-NOW
    if (esp_now_send(
            MAC_SERVER, paket, HEADER_SIZE + n
        ) != ESP_OK) {
        lapor("E12");
        return;
    }

    // Tunggu respons maksimal 3 detik
    unsigned long mulai = millis();

    while (!responsAda && millis() - mulai < 3000)
        delay(1);

    if (!responsAda) {
        lapor("E10");
        return;
    }

    // Proses respons yang diterima dari server
    Serial.printf("# terima dari server: %d byte\n",
                  (int)responsLen);

    terjemahkan(respons, responsLen);
}

// Dijalankan sekali saat ESP32 mulai
void setup() {
    Serial.begin(115200);  // Komunikasi Serial dengan laptop

    WiFi.mode(WIFI_STA);   // Gunakan mode Wi-Fi station
    WiFi.disconnect();

    // Atur MAC Bridge
    esp_wifi_set_mac(WIFI_IF_STA, MAC_BRIDGE);

    // Inisialisasi komunikasi ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("# ESP-NOW gagal");
        return;
    }

    // Hubungkan callback penerimaan paket
    esp_now_register_recv_cb(terimaData);

    // Daftarkan ESP-Server sebagai peer
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, MAC_SERVER, 6);
    esp_now_add_peer(&peer);

    Serial.println("# bridge siap");
}

// Berjalan berulang kali untuk membaca input laptop
void loop() {
    static char baris[600];  // Buffer input Serial
    static int n = 0;        // Jumlah karakter yang terkumpul

    while (Serial.available() > 0) {
        char c = Serial.read();

        // Enter menandakan akhir satu baris input
        if (c == '\n' || c == '\r') {
            if (n > 0) {
                baris[n] = '\0';
                proses(baris);
            }

            n = 0;  // Siapkan buffer untuk input berikutnya
        }
        else if (n < 599) {
            baris[n++] = c;  // Simpan karakter ke buffer
        }
    }
}
