#pragma once
// Read-only geometry and interaction evidence for the curve editor regression.
struct CurveEditorAxisState {
    RECT plot{}, timeBar{}, valueBar{};
    int timeFirst=0,timeLast=0,valueFirst=0,valueLast=0;
    double timeLow=0,timeHigh=0,valueLow=0,valueHigh=0;
    double timeStart=0,timeEnd=0,viewLow=0,viewHigh=0;
    double mouseFrame=0,mouseValue=0;
    bool mouseValid=false;
    int viewMode=0;
};
