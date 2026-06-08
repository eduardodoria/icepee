#pragma once

#include "Shape.h"
#include "Object.h"
#include "ScriptProperty.h"

namespace doriax {
    class Particles;
}

class IcePee;

class ButtonScript : public doriax::Object {
public:
    // Example properties
    DPROPERTY("Is Active")
    bool isActive = true;

    DPROPERTY("Speed")
    float speed = 5.0f;

    DPROPERTY("Target Position")
    doriax::Vector3 targetPosition = doriax::Vector3(0, 0, 0);

    DPROPERTY("Ice Pee Script")
    IcePee* icePeeScript = nullptr;

    ButtonScript(doriax::Scene* scene, doriax::Entity entity);
    virtual ~ButtonScript();

    void onButtonPress();
};
