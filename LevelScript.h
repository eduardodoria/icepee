#pragma once

#include "Object.h"
#include "ScriptProperty.h"

class IcePee;

class LevelScript : public doriax::Object {
public:
    DPROPERTY("Is Active")
    bool isActive = true;

    DPROPERTY("Ice Pee Script")
    IcePee* icePeeScript = nullptr;

    DPROPERTY("Drunk Sway Amount")
    float drunkSwayAmount = 1.0f;

    DPROPERTY("Drunk Sway Speed")
    float drunkSwaySpeed = 1.0f;

    LevelScript(doriax::Scene* scene, doriax::Entity entity);
    virtual ~LevelScript();

    void onButtonPress();
    void onPointerEnter(float x, float y);
    void onPointerLeave(float x, float y);
};
