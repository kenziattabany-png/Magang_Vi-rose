#ifndef MOTION_H
#define MOTION_H

#include <string>
#include <vector>

struct MotionUnit {
    int id;
    double speed;
    int loop;
};

class Motion {
private:
    int id;
    int checksum;
    std::string name;
    int totalUnit;
    std::vector<MotionUnit> units;

public:
    Motion();

    bool loadFromJSON(const std::string& filePath);
    void display() const;
};

#endif