#include "DSP/ControlCalibration.h"
#include <cassert>
#include <cmath>
#include <iostream>
template<typename T> void checkClockwise(const T& points) {
    for (std::size_t i=1;i<points.size();++i) assert(points[i]>points[i-1]);
}
int main() {
    using namespace ba6a::panel;
    static_assert(inputDetents==25 && outputDetents==16);
    static_assert(inputStepFromLabel(-24)==0 && inputStepFromLabel(24)==24);
    static_assert(outputStepFromLabel(0)==0 && outputStepFromLabel(30)==15);
    static_assert(modeChoiceToDspIndex(0)==1 && modeChoiceToDspIndex(1)==0);
    for (int i=0;i<inputDetents;++i) {
        const int label=-24+2*i;
        assert(inputStepFromLabel(label)==i);
        assert(std::abs(angleForKnob(0,float(i)/24.f)-inputDegrees[std::size_t(i)])<0.0001f);
    }
    for (int i=0;i<outputDetents;++i) {
        assert(outputStepFromLabel(i*2)==i);
        assert(std::abs(angleForKnob(3,float(i)/15.f)-outputDegrees[std::size_t(i)])<0.0001f);
    }
    checkClockwise(inputDegrees);
    checkClockwise(outputDegrees);
    checkClockwise(meterDegrees);
    checkClockwise(balLimitDegrees);
    assert(angleForKnob(2,0.f)==balLimitDegrees[0]);   // B reachable
    assert(angleForKnob(2,0.25f)==balLimitDegrees[1]); // A reachable
    std::cout << "25 input, 16 output, 10 meter and 5 BAL-LIMIT detents OK\n";
}
