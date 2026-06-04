#include "LevelScript.h"

#include "IcePee.h"
#include "Log.h"
#include "SceneManager.h"

using namespace doriax;

LevelScript::LevelScript(Scene* scene, Entity entity): Object(scene, entity) {
    REGISTER_BUTTON_EVENT(onPress, onButtonPress);
}

LevelScript::~LevelScript() {
    if (scene && scene->isEntityCreated(entity)) {
        UNREGISTER_BUTTON_EVENT(onPress, onButtonPress);
    }
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
