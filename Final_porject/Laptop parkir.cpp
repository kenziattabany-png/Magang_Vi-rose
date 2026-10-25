#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

// ===================== STRUKTUR DATA =====================

// Menyimpan informasi setiap zona parkir
struct Zona {
    const char *id;
    const char *jenis;
    int total;
    int terisi;
    long jam1, berikut, maks;
};

// Menyimpan informasi tiket kendaraan
struct TiketInfo {
    bool aktif;
    String plat;
    String zona;
    long waktuMasuk;
    bool langganan;
    int idTiket;
};

// Data zona: ID, jenis, kapasitas, terisi, dan tarif
Zona daftarZona[] = {
    {"Z1", "mobil", 20, 0, 5000, 3000, 25000},
    {"Z2", "motor", 100, 0, 2000, 1000, 8000},
    {"Z3", "mobil", 2, 0, 5000, 3000, 25000},
};

const int JUMLAH_ZONA = 3;

// Menyimpan semua tiket dan ID tiket berikutnya
std::vector<TiketInfo> daftarTiket;
int idBerikut = 1;

// Token untuk memeriksa permintaan dari laptop
String serverToken = "505050505052";

// ===================== FUNGSI PEMBANTU =====================

// Mengubah satu karakter HEX menjadi angka
int hv(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// Mengubah teks HEX menjadi teks biasa
String fromHex(const String& h) {
    String r = "";

    for (size_t i = 0; i + 1 < h.length(); i += 2) {
        int v1 = hv(h[i]);
        int v2 = hv(h[i + 1]);

        if (v1 < 0 || v2 < 0) break;

        r += (char)((v1 << 4) | v2);
    }

    return r;
}

// ===================== SETUP =====================

void setup() {
    Serial.begin(115200);  // Memulai komunikasi Serial
    delay(1000);
    Serial.println("# ESP32 Parking Server & Bridge aktif.");
}

// ===================== PEMROSESAN PERINTAH =====================

void processCommand(const String& jsonStr) {
    JsonDocument doc;

    // Membaca data JSON yang diterima
    DeserializationError error = deserializeJson(doc, jsonStr);

    if (error) {
        Serial.println("ERR E05 Isi permintaan bukan JSON yang valid.");
        return;
    }

    // Membaca jenis operasi yang diminta
    String action = doc["action"].as<String>();

    if (action == "null" || action == "") {
        action = "";
    }

    // 1. Menampilkan informasi semua zona
    if (action == "zona") {
        JsonDocument res;
        res["s"] = "zona";
        JsonArray zArr = res["z"].to<JsonArray>();

        // Memasukkan data setiap zona ke JSON
        for (int i = 0; i < JUMLAH_ZONA; i++) {
            const auto& z = daftarZona[i];

            JsonArray item = zArr.add<JsonArray>();
            item.add(z.id);
            item.add(z.jenis);
            item.add(z.total);
            item.add(z.terisi);
        }

        // Mengirim hasil melalui Serial
        String outJson;
        serializeJson(res, outJson);
        Serial.print("OK ");
        Serial.println(outJson);
    }

    // 2. Memproses kendaraan masuk
    else if (action == "masuk") {
        String plat = doc["plat"] | "";
        String zonaTarget = doc["zona"] | "";
        long t = doc["t"] | 0;

        // Mencari zona berdasarkan ID
        int zi = -1;

        for (int i = 0; i < JUMLAH_ZONA; i++) {
            if (String(daftarZona[i].id) == zonaTarget) {
                zi = i;
                break;
            }
        }

        // Menolak jika zona tidak ditemukan
        if (zi < 0) {
            Serial.println("ERR E08 Zona tidak ada.");
            return;
        }

        Zona &z = daftarZona[zi];

        // Memeriksa apakah zona sudah penuh
        if (z.terisi >= z.total) {
            Serial.println("ERR E13 Arahkan kendaraan ke zona lain.");
            return;
        }

        // Menambah jumlah kendaraan dan menghitung slot tersisa
        z.terisi++;
        int sisaSlot = z.total - z.terisi;

        // Membuat dan menyimpan tiket baru
        int idBaru = idBerikut++;
        TiketInfo tNew = {true, plat, zonaTarget, t, false, idBaru};
        daftarTiket.push_back(tNew);

        // Menyiapkan respons kendaraan masuk
        JsonDocument res;
        res["s"] = "masuk";
        res["tk"] = "T" + String(idBaru);
        res["zona"] = zonaTarget;
        res["sub"] = false;
        res["sisa"] = sisaSlot;

        String outJson;
        serializeJson(res, outJson);
        Serial.print("OK ");
        Serial.println(outJson);
    }

    // 3. Memproses kendaraan keluar
    else if (action == "keluar") {
        String plat = doc["plat"] | "";

        // Mencari tiket aktif berdasarkan pelat kendaraan
        int tiketIndex = -1;

        for (size_t i = 0; i < daftarTiket.size(); i++) {
            if (daftarTiket[i].aktif &&
                daftarTiket[i].plat == plat) {
                tiketIndex = i;
                break;
            }
        }

        // Menolak jika tiket aktif tidak ditemukan
        if (tiketIndex == -1) {
            Serial.println("ERR E15 Pelat tidak punya tiket aktif.");
            return;
        }

        TiketInfo &tk = daftarTiket[tiketIndex];

        // Mengurangi jumlah kendaraan pada zona terkait
        int sisaSlot = 0;

        for (int i = 0; i < JUMLAH_ZONA; i++) {
            if (String(daftarZona[i].id) == tk.zona) {
                if (daftarZona[i].terisi > 0)
                    daftarZona[i].terisi--;

                sisaSlot = daftarZona[i].total - daftarZona[i].terisi;
                break;
            }
        }

        tk.aktif = false;  // Menonaktifkan tiket

        // Menyiapkan respons kendaraan keluar
        JsonDocument res;
        res["s"] = "keluar";
        res["tk"] = "T" + String(tk.idTiket);
        res["plat"] = plat;
        res["zona"] = tk.zona;
        res["mnt"] = 60;       // Durasi simulasi 60 menit
        res["biaya"] = 5000;   // Biaya simulasi Rp5.000
        res["sub"] = tk.langganan;
        res["sisa"] = sisaSlot;

        String outJson;
        serializeJson(res, outJson);
        Serial.print("OK ");
        Serial.println(outJson);
    }

    // Menolak operasi yang tidak dikenal
    else {
        Serial.println("ERR E06 Operasi tidak dikenal server.");
    }
}

// ===================== LOOP =====================

void loop() {
    // Membaca perintah dari laptop melalui Serial
    if (Serial.available() > 0) {
        String line = Serial.readStringUntil('\n');
        line.trim();

        if (line.length() == 0) return;

        // Memisahkan token dan payload HEX jika tersedia
        String token = "";
        String payload = line;

        if (line.startsWith("T")) {
            int colonIdx = line.indexOf(':');

            if (colonIdx != -1) {
                token = line.substring(1, colonIdx);
                payload = line.substring(colonIdx + 1);
            }
        }

        // Memeriksa token jika disertakan
        if (token.length() > 0 && token != serverToken) {
            Serial.println("ERR E01 Cek token dengan perintah 'token'; samakan dengan token di ESP-Bridge/ESP-Server.");
            return;
        }

        // Mengubah HEX menjadi JSON biasa
        String jsonPayload = fromHex(payload);

        // Jika bukan HEX, gunakan input aslinya
        if (jsonPayload.length() == 0) {
            jsonPayload = payload;
        }

        // Memproses perintah JSON
        processCommand(jsonPayload);
    }
}