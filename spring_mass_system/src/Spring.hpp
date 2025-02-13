#ifndef SPRING_HPP
#define SPRING_HPP

#include "PMat.hpp"

class Spring {
public:
    Spring(PMat* p1, PMat* p2, float springConstant, float restLengthScale = 1.0f);
    void update();
    void Conditional_Update();
    void setSpringConstant(float newK);
    void setDampingCoefficient(float newZ);
    PMat* getP1() const { return p1; }
    PMat* getP2() const { return p2; }

private:
    PMat* p1;
    PMat* p2;
    float k;
    float z;
    float restLength;
};

#endif // SPRING_HPP