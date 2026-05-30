#pragma once

#include <vector>
#include "ScriptBase.h"
#include "Engine.h"
#include "math/Quaternion.h"
#include "math/Vector2.h"
#include "object/Points.h"
#include "ScriptProperty.h"
#include "object/ui/Text.h"

class IcePee : public doriax::ScriptBase {
public:
    // Example properties - you can add more!
    SPROPERTY("Speed")
    float speed = 5.0f;

    SPROPERTY("Is Active")
    bool isActive = false;

    SPROPERTY("Counter")
    int counter = 0;

    SPROPERTY("Point Sprites")
    doriax::Points* pointSprites = nullptr;

    SPROPERTY("Time")
    doriax::Text* time = nullptr;

    SPROPERTY("Score")
    doriax::Text* score = nullptr;

    doriax::Vector2 lastMousePosition;
    float pointSpritesYaw = 0.0f;
    float pointSpritesPitch = 0.0f;
    bool wasDragging = false;

    doriax::Entity particlesEntity = NULL_ENTITY;
    float particlesMinSpeed = 0.0f;
    float particlesMaxSpeed = 0.0f;
    float particlesConeAngle = 0.0f;
    bool particlesConeCached = false;
    bool particlesStarted = false;

    doriax::Entity waterEntity = NULL_ENTITY;
    float waterMaxScaleY = 0.0f;

    std::vector<doriax::Entity> iceEntities;
    bool iceEntitiesCached = false;
    float remainingTimeSeconds = 30.0f;

    IcePee(doriax::Scene* scene, doriax::Entity entity);
    ~IcePee();

    void onUpdate();
};
