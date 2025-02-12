#ifndef SPRING_HPP
#define SPRING_HPP

#include "PMat.hpp"

class Spring {
public:
    Spring(PMat* p1, PMat* p2, float springConstant);
    void update();
    void setSpringConstant(float newK);
    void setDampingCoefficient(float newZ);
private:
    PMat* p1;
    PMat* p2;
    float k;
    float z;
    float restLength;
};

#endif // SPRING_HPP