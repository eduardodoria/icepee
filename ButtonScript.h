#pragma once

#include "Shape.h"
#include "Object.h"
#include "ScriptProperty.h"

class ButtonScript : public doriax::Object {
public:
    // Example properties
    SPROPERTY("Is Active")
    bool isActive = true;

    SPROPERTY("Speed")
    float speed = 5.0f;

    SPROPERTY("Target Position")
    doriax::Vector3 targetPosition = doriax::Vector3(0, 0, 0);

    ButtonScript(doriax::Scene* scene, doriax::Entity entity);
    virtual ~ButtonScript();

    void onButtonPress();
};
