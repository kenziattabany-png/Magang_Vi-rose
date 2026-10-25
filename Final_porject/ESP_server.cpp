// ITS-Parkir - ESP-SERVER
// Program ini mengatur proses parkir: kendaraan masuk, keluar, dan melihat zona.

#include <Arduino.h>      
#include <ArduinoJson.h>  
#include <WiFi.h>        
#include <esp_now.h>     
#include <esp_wifi.h>     

// ==================== KONFIGURASI ====================

// Nilai token harus sama dengan yang digunakan oleh ESP-BRIDGE.
const uint8_t TOKEN[6] = {0x50, 0x41, 0x52, 0x4B, 0x49, 0x52};

// MAC address ESP-BRIDGE dan ESP-SERVER.
uint8_t MAC_BRIDGE[6] = {0x24, 0x0A, 0xC4, 0x0A, 0x99, 0x10};
uint8_t MAC_SERVER[6] = {0x24, 0x0A, 0xC4, 0x0A, 0x99, 0x11};

// Format paket:
// Payload berisi teks JSON yang diubah menjadi karakter hex.
#define HEADER_SIZE 14
#define MAX_PAYLOAD 236  // Ukuran maksimal payload
#define ADDR_BRIDGE 1    // Alamat ESP-BRIDGE
#define ADDR_SERVER 2    // Alamat ESP-SERVER

// Parkir selama 5 menit atau kurang tidak dikenakan biaya.
#define GRACE_MIN 5

// DATA ZONA PARKIR

struct Zona {
    const char *id;       // ID zona, misalnya Z1
    const char *jenis;    // Jenis kendaraan: mobil atau motor
    int total, terisi;    // Kapasitas maksimal dan jumlah yang terisi
    long jam1, berikut, maks;
    // jam1   = tarif jam pertama
    // berikut = tarif setiap jam berikutnya
    // maks    = batas tarif untuk satu hari
};

Zona daftarZona[] = {
    {"Z1", "mobil", 20, 0, 5000, 3000, 25000},
    {"Z2", "motor", 100, 0, 2000, 1000, 8000},
    {"Z3", "mobil", 2, 0, 5000, 3000, 25000},
};

// Jumlah zona yang terdaftar dalam array.
const int JUMLAH_ZONA = 3;

//DATA LANGGANAN 

// Struct untuk menyimpan data kendaraan berlangganan.
struct Langganan {
    const char *plat;   // Plat nomor pelanggan
    const char *jenis;  // Jenis kendaraan
    long tarif;         // Tarif tetap per kunjungan
};

// Array berisi daftar kendaraan yang terdaftar sebagai pelanggan.
Langganan daftarLangganan[] = {
    {"L1234AB", "mobil", 3000},
    {"L5678CD", "motor", 1000}
};

const int JUMLAH_LANGGANAN = 2;

//DATA TIKET 

// Struct untuk menyimpan informasi setiap tiket parkir.
struct Tiket {
    bool aktif;       // true jika kendaraan masih parkir
    char plat[10];    // Plat nomor kendaraan
    int zona;         // Indeks zona di daftarZona[]
    long masuk;       // Waktu masuk dalam menit
    bool langganan;   // true jika kendaraan berlangganan
    int id;           // Nomor tiket
};

// Menyediakan 100 slot tiket di memori ESP32.
Tiket tiket[100];

// Nomor tiket berikutnya dimulai dari 1.
int idBerikut = 1;

//MENGIRIM RESPONS 

// Mengirim hasil proses atau pesan error ke ESP-BRIDGE.
void kirim(const char *teks) {
    int L = strlen(teks);

    // Jika teks terlalu panjang, kirim kode error E12.
    if (L * 2 > MAX_PAYLOAD) {
        teks = "E12";
        L = 3;
    }

    uint8_t paket[260];

    // Mengisi bagian header paket.
    memcpy(paket, "ITSP", 4);
    paket[4] = ADDR_SERVER;
    paket[5] = ADDR_BRIDGE;
    memcpy(paket + 6, TOKEN, 6);

    // Menyimpan panjang payload dalam dua byte.
    paket[12] = (L * 2) >> 8;
    paket[13] = (L * 2) & 0xFF;

    // Mengubah setiap karakter teks menjadi dua digit hex.
    for (int i = 0; i < L; i++) {
        sprintf((char *)paket + 14 + 2 * i, "%02X", (uint8_t)teks[i]);
    }

    // Mengirim paket ke ESP-BRIDGE.
    esp_now_send(MAC_BRIDGE, paket, HEADER_SIZE + L * 2);

    // Menampilkan respons di Serial Monitor.
    Serial.printf("[SRV] kirim: %s\n", teks);
}

// Mengirim pesan error dengan kode tertentu.
void gagal(const char *kode) {
    Serial.printf("[SRV] error %s\n", kode);
    kirim(kode);
}

//VALIDASI PLAT
// Memeriksa apakah format plat nomor sesuai aturan program.
bool platValid(const char *p) {
    int i = 0, huruf1 = 0, angka = 0, huruf2 = 0;

    // Membaca huruf di bagian depan plat.
    while (p[i] >= 'A' && p[i] <= 'Z') {
        i++;
        huruf1++;
    }

    // Membaca angka setelah huruf depan.
    while (p[i] >= '0' && p[i] <= '9') {
        i++;
        angka++;
    }

    // Membaca huruf di bagian belakang.
    while (p[i] >= 'A' && p[i] <= 'Z') {
        i++;
        huruf2++;
    }

    // Format: 1-2 huruf, 1-4 angka, dan maksimal 3 huruf belakang.
    return p[i] == '\0' &&
           huruf1 >= 1 && huruf1 <= 2 &&
           angka >= 1 && angka <= 4 &&
           huruf2 <= 3;
}

//PENCARIAN TIKET

// Mencari tiket aktif berdasarkan plat nomor.
// Mengembalikan indeks tiket jika ditemukan, atau -1 jika tidak ada.
int cariTiket(const char *plat) {
    for (int i = 0; i < 100; i++) {
        if (tiket[i].aktif &&
            strcmp(tiket[i].plat, plat) == 0) {
            return i;
        }
    }

    return -1;
}

//PENCARIAN LANGGANAN
// Memeriksa apakah plat dan jenis kendaraan terdaftar sebagai pelanggan.
// Mengembalikan tarif jika ditemukan, atau -1 jika bukan pelanggan.
long tarifLangganan(const char *plat, const char *jenis) {
    for (int i = 0; i < JUMLAH_LANGGANAN; i++) {
        if (strcmp(daftarLangganan[i].plat, plat) == 0 &&
            strcmp(daftarLangganan[i].jenis, jenis) == 0) {
            return daftarLangganan[i].tarif;
        }
    }

    return -1;
}

//MENGHITUNG BIAYA

// Menghitung biaya parkir berdasarkan zona dan lama parkir.
long hitungBiaya(const Zona &z, long menit) {

    // Parkir 5 menit atau kurang gratis.
    if (menit <= GRACE_MIN) return 0;

    // Menghitung biaya untuk setiap hari penuh (1440 menit).
    long biaya = (menit / 1440) * z.maks;

    // Menghitung sisa waktu yang belum mencapai satu hari penuh.
    long sisa = menit % 1440;

    if (sisa > 0) {
        // Membulatkan sisa waktu ke atas menjadi jam.
        long jam = (sisa + 59) / 60;

        // Jam pertama menggunakan tarif jam1.
        // Jam berikutnya menggunakan tarif berikut.
        long f = z.jam1 + (jam - 1) * z.berikut;

        // Biaya untuk bagian hari ini tidak boleh melebihi tarif maksimum.
        if (f > z.maks) f = z.maks;

        biaya += f;
    }

    return biaya;
}

//PROSES KENDARAAN MASUK 

void prosesMasuk(JsonDocument &d) {

    // Memastikan plat, zona, dan waktu memiliki tipe data yang benar.
    if (!d["plat"].is<const char *>() ||
        !d["zona"].is<const char *>() ||
        !d["t"].is<long>()) {
        gagal("E07");
        return;
    }

    // Mengambil data dari JSON.
    const char *plat = d["plat"];
    const char *idZona = d["zona"];
    long t = d["t"];

    // Memeriksa format plat nomor.
    if (!platValid(plat)) {
        gagal("E07");
        return;
    }

    // Mencari indeks zona berdasarkan ID yang diterima.
    int zi = -1;

    for (int i = 0; i < JUMLAH_ZONA; i++) {
        if (strcmp(daftarZona[i].id, idZona) == 0) {
            zi = i;
        }
    }

    // Jika ID zona tidak ditemukan, proses dihentikan.
    if (zi < 0) {
        gagal("E08");
        return;
    }


    Zona &z = daftarZona[zi];
    // Kendaraan yang sudah memiliki tiket aktif tidak boleh masuk lagi.
    if (cariTiket(plat) >= 0) {
        gagal("E14");
        return;
    }

    // Memeriksa apakah kendaraan termasuk pelanggan.
    bool langganan = tarifLangganan(plat, z.jenis) >= 0;

    // Kendaraan biasa ditolak jika zona sudah penuh.
    // Pelanggan berlangganan tidak dihitung dalam kapasitas.
    if (!langganan && z.terisi >= z.total) {
        gagal("E13");
        return;
    }

    // Mencari slot tiket yang belum aktif.
    int s = -1;

    for (int i = 0; i < 100 && s < 0; i++) {
        if (!tiket[i].aktif) s = i;
    }

    // Jika tidak ada slot tiket yang tersedia, kirim error.
    if (s < 0) {
        gagal("E16");
        return;
    }

    // Membuat nomor tiket baru.
    int idBaru = idBerikut++;
    tiket[s].aktif = true;

    // Menyalin plat ke tiket agar tidak melewati ukuran array.
    strncpy(tiket[s].plat, plat, sizeof(tiket[s].plat) - 1);
    tiket[s].plat[sizeof(tiket[s].plat) - 1] = '\0';

    // Menyimpan informasi kendaraan ke tiket.
    tiket[s].zona = zi;
    tiket[s].masuk = t;
    tiket[s].langganan = langganan;
    tiket[s].id = idBaru;

    // Jumlah terisi bertambah untuk kendaraan nonlangganan.
    if (!langganan) z.terisi++;

    // Membuat respons JSON untuk dikirim ke laptop.
    char out[160];

    snprintf(out, sizeof(out),
             "{\"s\":\"masuk\",\"tk\":\"T%04d\",\"zona\":\"%s\",\"sub\":%d,\"sisa\":%d}",
             tiket[s].id, z.id, langganan, z.total - z.terisi);

    kirim(out);
}

// ==================== PROSES KENDARAAN KELUAR ====================

void prosesKeluar(JsonDocument &d) {

    // Memastikan plat dan waktu tersedia dengan tipe yang benar.
    if (!d["plat"].is<const char *>() ||
        !d["t"].is<long>()) {
        gagal("E07");
        return;
    }

    const char *plat = d["plat"];
    long t = d["t"];

    // Memeriksa format plat.
    if (!platValid(plat)) {
        gagal("E07");
        return;
    }

    // Mencari tiket aktif milik kendaraan.
    int i = cariTiket(plat);

    // Kendaraan harus memiliki tiket aktif untuk keluar.
    if (i < 0) {
        gagal("E15");
        return;
    }

    // Mengakses data tiket dan zona kendaraan.
    Tiket &k = tiket[i];
    Zona &z = daftarZona[k.zona];

    // Memeriksa konsistensi jumlah kendaraan di zona.
    if (!k.langganan && z.terisi == 0) {
        gagal("E16");
        return;
    }

    // Menghitung lama parkir dalam menit.
    long menit = t - k.masuk;

    // Pelanggan membayar tarif tetap.
    // Kendaraan biasa membayar berdasarkan durasi parkir.
    long biaya = k.langganan
        ? tarifLangganan(plat, z.jenis)
        : hitungBiaya(z, menit);

    // Mengurangi jumlah kendaraan terisi saat kendaraan biasa keluar.
    if (!k.langganan) z.terisi--;

    // Tiket dinonaktifkan agar bisa digunakan kembali.
    k.aktif = false;

    // Membuat respons JSON yang berisi hasil transaksi keluar.
    char out[200];

    snprintf(out, sizeof(out),
             "{\"s\":\"keluar\",\"tk\":\"T%04d\",\"plat\":\"%s\",\"zona\":\"%s\",\"mnt\":%ld,\"biaya\":%ld,\"sub\":%d,\"sisa\":%d}",
             k.id, plat, z.id, menit, biaya, k.langganan,
             z.total - z.terisi);

    kirim(out);
}

//INFORMASI ZONA

// Mengirim informasi semua zona ke laptop dalam format JSON.
void prosesZona() {
    char out[200];

    // Memulai objek JSON dengan daftar zona kosong.
    int n = snprintf(out, sizeof(out), "{\"s\":\"zona\",\"z\":[");

    // Memasukkan data ID, jenis, kapasitas, dan jumlah terisi.
    for (int i = 0; i < JUMLAH_ZONA; i++) {
        n += snprintf(out + n, sizeof(out) - n,
                      "%s[\"%s\",\"%s\",%d,%d]",
                      i ? "," : "",
                      daftarZona[i].id,
                      daftarZona[i].jenis,
                      daftarZona[i].total,
                      daftarZona[i].terisi);
    }

    // Menutup struktur JSON.
    snprintf(out + n, sizeof(out) - n, "]}");

    kirim(out);
}

//VALIDASI PAKET 

// Mengubah satu karakter hex menjadi angka 0-15.
// Mengembalikan -1 jika karakter bukan hex yang valid.
int hexNilai(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;

    return -1;
}

// Memeriksa paket masuk sebelum menjalankan perintah parkir.
void prosesPaket(const uint8_t *p, int len) {

    // Paket harus memiliki header lengkap.
    if (len < HEADER_SIZE) {
        gagal("E03");
        return;
    }

    // Memeriksa penanda paket dan alamat pengirim.
    if (memcmp(p, "ITSP", 4) != 0 || p[4] != ADDR_BRIDGE) {
        gagal("E02");
        return;
    }

    // Memastikan paket memang ditujukan ke ESP-SERVER.
    if (p[5] != ADDR_SERVER) {
        gagal("E11");
        return;
    }

    // Memeriksa kecocokan token.
    if (memcmp(p + 6, TOKEN, 6) != 0) {
        gagal("E01");
        return;
    }

    // Membaca panjang payload dari header.
    int plen = (p[12] << 8) | p[13];

    // Panjang payload harus sesuai dengan ukuran paket.
    if (plen != len - HEADER_SIZE) {
        gagal("E03");
        return;
    }

    // Payload hex harus memiliki panjang genap dan tidak kosong.
    if (plen == 0 || plen % 2 != 0) {
        gagal("E04");
        return;
    }

    // Menyiapkan tempat untuk JSON hasil konversi hex.
    char json[MAX_PAYLOAD / 2 + 1];
    int n = plen / 2;

    // Mengubah setiap pasangan karakter hex menjadi satu byte.
    for (int i = 0; i < n; i++) {
        int a = hexNilai(p[14 + 2 * i]);
        int b = hexNilai(p[15 + 2 * i]);

        // Jika karakter hex tidak valid, paket ditolak.
        if (a < 0 || b < 0) {
            gagal("E04");
            return;
        }

        json[i] = a * 16 + b;
    }

    // Mengakhiri teks JSON.
    json[n] = '\0';

    Serial.printf("[SRV] JSON: %s\n", json);

    // Mengubah teks JSON menjadi struktur data yang bisa dibaca.
    JsonDocument doc;

    if (deserializeJson(doc, json)) {
        gagal("E05");
        return;
    }

    // JSON harus memiliki field action berbentuk teks.
    if (!doc["action"].is<const char *>()) {
        gagal("E07");
        return;
    }

    // Menjalankan fungsi sesuai perintah yang diterima.
    const char *aksi = doc["action"];

    if (strcmp(aksi, "masuk") == 0)
        prosesMasuk(doc);
    else if (strcmp(aksi, "keluar") == 0)
        prosesKeluar(doc);
    else if (strcmp(aksi, "zona") == 0)
        prosesZona();
    else
        gagal("E06");  // Perintah tidak dikenal
}

//  KOMUNIKASI ESP-NOW

// Buffer untuk menyimpan satu paket yang diterima.
uint8_t paketMasuk[250];

// Menyiman panjang paket dan status apakah paket tersedia.
volatile int panjangMasuk = 0;
volatile bool adaPaket = false;

// Callback yang dipanggil otomatis saat ESP-NOW menerima data.
void terimaData(const uint8_t *mac, const uint8_t *data, int len) {

    // Mengabaikan pengirim yang salah, buffer yang sedang terisi,
    // atau ukuran paket yang tidak valid.
    if (memcmp(mac, MAC_BRIDGE, 6) != 0 ||
        adaPaket ||
        len <= 0 ||
        len > 250) {
        return;
    }

    // Menyalin data masuk ke buffer.
    memcpy(paketMasuk, data, len);

    // Menyimpan panjang paket dan menandai bahwa paket tersedia.
    panjangMasuk = len;
    adaPaket = true;
}


// Dijalankan satu kali saat ESP32 mulai menyala.
void setup() {
    Serial.begin(115200);  // Memulai Serial Monitor

    WiFi.mode(WIFI_STA);   // Menggunakan mode WiFi Station
    WiFi.disconnect();     // Memutuskan koneksi WiFi biasa

    // Mengatur MAC address ESP-SERVER.
    esp_wifi_set_mac(WIFI_IF_STA, MAC_SERVER);

    // Menginisialisasi komunikasi ESP-NOW.
    if (esp_now_init() != ESP_OK) {
        Serial.println("[SRV] ESP-NOW gagal");
        return;
    }

    // Mendaftarkan fungsi penerima data.
    esp_now_register_recv_cb(terimaData);

    // Mendaftarkan ESP-BRIDGE sebagai perangkat tujuan.
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, MAC_BRIDGE, 6);
    esp_now_add_peer(&peer);

    Serial.println("[SRV] server siap");
}

// OOP UTAMA 

// Dijalankan terus-menerus selama ESP32 menyala.
void loop() {

    // Memproses paket jika callback sudah menerima data.
    if (adaPaket) {
        uint8_t salinan[250];

        // Menyalin panjang dan isi paket ke buffer lokal.
        int n = panjangMasuk;
        memcpy(salinan, paketMasuk, n);

        // Menandai buffer utama sebagai kosong.
        adaPaket = false;

        Serial.printf("[SRV] terima %d byte\n", n);

        // Memeriksa paket dan menjalankan perintah yang diterima.
        prosesPaket(salinan, n);
    }

    // Memberi kesempatan sistem menjalankan tugas lainnya.
    delay(1);
}