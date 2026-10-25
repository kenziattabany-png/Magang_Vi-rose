#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

using namespace std;
using namespace rapidjson;

#ifndef DATA_DIR
    #define DATA_DIR "./"
#endif



int main() {

    string files;
    cin>>files;

    string path = string(DATA_DIR) + files +".json";
    ifstream file(path);
    if(!file.is_open()){
        cout<<"Gagal membuka file "<<path<<endl;
        return 1;
    }


    stringstream buffer;
    buffer<<file.rdbuf();
    string teks = buffer.str();

    Document doc;
    doc.Parse(teks.c_str());

    if(doc.HasParseError()){
        cout<<"Format JSON salah"<<endl;
        return 1;
    }


    if (!doc.IsObject() ||
        !doc.HasMember("id") || !doc["id"].IsInt() ||
        !doc.HasMember("checksum") || !doc["checksum"].IsUint() ||
        !doc.HasMember("name") || !doc["name"].IsString() ||
        !doc.HasMember("total_unit") || !doc["total_unit"].IsInt() ||
        !doc.HasMember("motion_unit") || !doc["motion_unit"].IsArray()){

        cout << "Struktur JSON tidak sesuai!" << endl;
        return 1;
    }

    
    int motionId = doc["id"].GetInt();
    int checksum = doc["checksum"].GetUint();
    string motionName = doc["name"].GetString();
    int totalUnit = doc["total_unit"].GetInt();

    cout << "MOTION" << endl;
    cout << "Motion ID   : " << motionId << endl;
    cout <<  "Checksum    : " << checksum << endl;
    cout << "Motion Name : " << motionName << endl;
    cout << "Total Unit  : " << totalUnit << endl;

    
    const Value& units = doc["motion_unit"];

    cout << "\nMOTION UNITS: " << endl;

    for (SizeType i = 0; i < units.Size(); i++) {

        const Value& unit = units[i];

        if (!unit.IsObject() ||
            !unit.HasMember("id") || !unit["id"].IsInt() ||
            !unit.HasMember("speed") || !unit["speed"].IsNumber() ||
            !unit.HasMember("loop") || !unit["loop"].IsInt()) {

            cout << "Data motion unit ke-" << i
                 << " tidak valid!" << endl;
            return 1;
        }

        int unitId = unit["id"].GetInt();
        double speed = unit["speed"].GetDouble();
        int loop = unit["loop"].GetInt();

        cout << "Unit ke-" << i + 1 << endl;
        cout << "  ID    : " << unitId << endl;
        cout << "  Speed : " << speed << endl;
        cout << "  Loop  : " << loop << endl;
    }

    return 0;
}
