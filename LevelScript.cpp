#include "LevelScript.h"

#include "IcePee.h"
#include "Log.h"
#include "SceneManager.h"
#include "util/FunctionSubscribe.h"

using namespace doriax;

namespace {
    constexpr float HOVER_SCALE = 1.1f;
    constexpr float NORMAL_SCALE = 1.0f;
}

LevelScript::LevelScript(Scene* scene, Entity entity): Object(scene, entity) {
    REGISTER_BUTTON_EVENT(onPress, onButtonPress);
    REGISTER_UI_EVENT(onPointerEnter, onPointerEnter);
    REGISTER_UI_EVENT(onPointerLeave, onPointerLeave);
}

LevelScript::~LevelScript() {
    if (scene && scene->isEntityCreated(entity)) {
        UNREGISTER_BUTTON_EVENT(onPress, onButtonPress);
        UNREGISTER_UI_EVENT(onPointerEnter, onPointerEnter);
        UNREGISTER_UI_EVENT(onPointerLeave, onPointerLeave);
    }
}

void LevelScript::onPointerEnter(float, float) {
    setScale(HOVER_SCALE);
}

void LevelScript::onPointerLeave(float, float) {
    setScale(NORMAL_SCALE);
}

void LevelScript::onButtonPress() {
    if (!isActive || !icePeeScript) return;

    icePeeScript->drunkSwayAmount = drunkSwayAmount;
    icePeeScript->drunkSwaySpeed = drunkSwaySpeed;
    icePeeScript->resetGame();
    icePeeScript->isActive = true;

    Log::print("Level '%s' selected (sway amount: %.2f, speed: %.2f)", getName().c_str(), drunkSwayAmount, drunkSwaySpeed);
    SceneManager::addChildScene("Score Scene");
    SceneManager::removeChildScene("Level Scene");
}
