
#include "Motion.h"

#include <iostream>
#include <fstream>
#include <sstream>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

using namespace std;
using namespace rapidjson;

Motion::Motion()
    : id(0), checksum(0), totalUnit(0) {}

bool Motion::loadFromJSON(const string& filePath) {
    ifstream file(filePath);

    if (!file.is_open()) {
        cerr << "Gagal membuka file: "
             << filePath << endl;
        return false;
    }

    stringstream buffer;
    buffer << file.rdbuf();
    string text = buffer.str();

    Document doc;
    doc.Parse(text.c_str());

    if (doc.HasParseError()) {
        cerr << "JSON tidak valid: "
             << GetParseError_En(doc.GetParseError())
             << endl;
        return false;
    }

    if (!doc.IsObject() ||
        !doc.HasMember("id") || !doc["id"].IsInt() ||
        !doc.HasMember("checksum") ||
        !doc["checksum"].IsUint64() ||
        !doc.HasMember("name") ||
        !doc["name"].IsString() ||
        !doc.HasMember("total_unit") ||
        !doc["total_unit"].IsInt() ||
        !doc.HasMember("motion_unit") ||
        !doc["motion_unit"].IsArray()) {

        cerr << "Struktur JSON tidak sesuai!" << endl;
        return false;
    }

    const Value& jsonUnits = doc["motion_unit"];
    vector<MotionUnit> parsedUnits;

    for (SizeType i = 0; i < jsonUnits.Size(); i++) {
        const Value& unit = jsonUnits[i];

        if (!unit.IsObject() ||
            !unit.HasMember("id") || !unit["id"].IsInt() ||
            !unit.HasMember("speed") ||
            !unit["speed"].IsNumber() ||
            !unit.HasMember("loop") || !unit["loop"].IsInt()) {

            cerr << "Motion unit tidak valid!" << endl;
            return false;
        }

        MotionUnit item;
        item.id = unit["id"].GetInt();
        item.speed = unit["speed"].GetDouble();
        item.loop = unit["loop"].GetInt();

        parsedUnits.push_back(item);
    }

    if (doc["total_unit"].GetInt() !=
        static_cast<int>(parsedUnits.size())) {
        cerr << "total_unit tidak cocok dengan jumlah data!"
             << endl;
        return false;
    }

    // Update object after successful validation.
    id = doc["id"].GetInt();
    checksum = doc["checksum"].GetUint64();
    name = doc["name"].GetString();
    totalUnit = doc["total_unit"].GetInt();
    units = parsedUnits;

    return true;
}

void Motion::display() const {
    cout << "=== MOTION INFORMATION ===\n";
    cout << "ID       : " << id << '\n';
    cout << "Checksum : " << checksum << '\n';
    cout << "Name     : " << name << '\n';
    cout << "Total    : " << totalUnit << '\n';

    cout << "\n=== MOTION UNITS ===\n";

    for (size_t i = 0; i < units.size(); i++) {
        cout << "Unit " << i + 1 << '\n';
        cout << "  ID    : " << units[i].id << '\n';
        cout << "  Speed : " << units[i].speed << '\n';
        cout << "  Loop  : " << units[i].loop << '\n';
    }
}