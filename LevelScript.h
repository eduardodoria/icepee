#pragma once

#include "Object.h"
#include "ScriptProperty.h"

class IcePee;

class LevelScript : public doriax::Object {
public:
    SPROPERTY("Is Active")
    bool isActive = true;

    SPROPERTY("Ice Pee Script")
    IcePee* icePeeScript = nullptr;

    SPROPERTY("Drunk Sway Amount")
    float drunkSwayAmount = 1.0f;

    SPROPERTY("Drunk Sway Speed")
    float drunkSwaySpeed = 1.0f;

    LevelScript(doriax::Scene* scene, doriax::Entity entity);
    virtual ~LevelScript();

    void onButtonPress();
    void onPointerEnter(float x, float y);
    void onPointerLeave(float x, float y);
};
