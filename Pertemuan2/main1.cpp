#include <iostream>
#include <string>
#include "Motion.h"

using namespace std;

int main() {
    Motion motion;

    // Change this filename to read a different JSON file
    string filePath = string(DATA_DIR) + "60.json";

    if (motion.loadFromJSON(filePath)) {
        motion.display();
    } else {
        cerr << "Failed to load JSON file: " << filePath << endl;
        return 1;
    }

    return 0;
}
