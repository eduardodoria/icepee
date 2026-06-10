#include "IcePee.h"
#include "Input.h"
#include "SceneManager.h"

#include "action/Particles.h"
#include "component/ActionComponent.h"
#include "component/MeshComponent.h"
#include "component/ParticlesComponent.h"
#include "object/Mesh.h"
#include "object/Object.h"
#include "component/Transform.h"
#include "io/UserSettings.h"

#include <cmath>
#include <unordered_map>

using namespace doriax;

static Quaternion getPointSpritesBaseRotation() {
    return Quaternion(
        Vector3(0.0f, 0.0f, 1.0f),
        Vector3(0.0f, 1.0f, 0.0f),
        Vector3(-1.0f, 0.0f, 0.0f));
}

static Quaternion getPointSpritesControlRotation(float yaw, float pitch) {
    Quaternion yawRotation(yaw, Vector3(0.0f, 1.0f, 0.0f));
    Quaternion pitchRotation(pitch, Vector3(0.0f, 0.0f, 1.0f));
    return yawRotation * pitchRotation;
}

static Entity findParticlesEntityForTarget(Scene* scene, Entity targetEntity) {
    if (!scene || targetEntity == NULL_ENTITY) {
        return NULL_ENTITY;
    }

    std::vector<Entity> entities = scene->getEntityList();
    for (Entity candidate : entities) {
        ActionComponent* action = scene->findComponent<ActionComponent>(candidate);
        ParticlesComponent* particles = scene->findComponent<ParticlesComponent>(candidate);
        if (action && particles && action->target == targetEntity) {
            return candidate;
        }
    }

    return NULL_ENTITY;
}

static Entity getActorEntityFromPointSprites(Scene* scene, Entity pointSpritesEntity) {
    if (!scene || pointSpritesEntity == NULL_ENTITY) {
        return NULL_ENTITY;
    }

    const Transform* transform = scene->findComponent<Transform>(pointSpritesEntity);
    if (!transform || transform->parent == NULL_ENTITY) {
        return NULL_ENTITY;
    }

    return transform->parent;
}

static void applyDrunkSway(
    Scene* scene,
    Entity actorEntity,
    const Vector3& basePosition,
    const Quaternion& baseRotation,
    float swayTime,
    float amount,
    float speed) {
    if (!scene || actorEntity == NULL_ENTITY || amount <= 0.0f) {
        return;
    }

    const float t = swayTime * std::max(0.0f, speed);
    const float positionScale = 0.04f * amount;
    const float rotationScale = 0.035f * amount;

    const Vector3 positionOffset(
        (std::sin(t * 0.91f) + std::sin(t * 1.73f) * 0.5f) * positionScale,
        (std::sin(t * 1.13f) + std::cos(t * 2.27f) * 0.5f) * positionScale * 0.7f,
        (std::cos(t * 0.79f) + std::sin(t * 1.97f) * 0.5f) * positionScale);

    const Quaternion swayRotation(
        std::sin(t * 0.67f) * rotationScale + std::sin(t * 1.31f) * rotationScale * 0.5f,
        std::cos(t * 1.07f) * rotationScale + std::sin(t * 2.17f) * rotationScale * 0.5f,
        std::sin(t * 1.49f) * rotationScale * 0.6f);

    Object actor(scene, actorEntity);
    actor.setPosition(basePosition + positionOffset);
    actor.setRotation(baseRotation * swayRotation);
    actor.updateTransform();
}

static Entity findMeshEntityByName(Scene* scene, const std::string& entityName) {
    if (!scene || entityName.empty()) {
        return NULL_ENTITY;
    }

    std::vector<Entity> entities = scene->getEntityList();
    for (Entity candidate : entities) {
        if (scene->findComponent<MeshComponent>(candidate) && scene->getEntityName(candidate) == entityName) {
            return candidate;
        }
    }

    return NULL_ENTITY;
}

static bool isIceEntity(Scene* scene, Entity entity) {
    if (!scene || entity == NULL_ENTITY || !scene->findComponent<MeshComponent>(entity)) {
        return false;
    }

    const std::string entityName = scene->getEntityName(entity);
    return entityName.compare(0, 4, "gelo") == 0;
}

static bool isParticleAlive(const ParticleData& particle) {
    return particle.life > particle.time;
}

static bool hasParticleHitAABB(const ParticlesComponent& particles, const AABB& aabb) {
    if (aabb.isNull() || !aabb.isFinite()) {
        return false;
    }

    for (const ParticleData& particle : particles.particles) {
        if (isParticleAlive(particle) && aabb.contains(particle.position)) {
            return true;
        }
    }

    return false;
}

static const Vector3 kIceMeltTargetColor(0.88f, 0.95f, 1.0f);
static constexpr int kScorePerIceEntity = 100;
static const char* kBestScoreKey = "icepee_best_score";

struct IceMeltVisualState {
    float initialLargestScale = 0.0f;
    Vector4 initialColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    int awardedScore = 0;
};

struct MeshResetState {
    Entity entity = NULL_ENTITY;
    Vector3 position = Vector3(0.0f, 0.0f, 0.0f);
    Vector3 scale = Vector3(1.0f, 1.0f, 1.0f);
    Vector4 color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    bool visible = true;
};

struct ObjectResetState {
    Entity entity = NULL_ENTITY;
    Vector3 position = Vector3(0.0f, 0.0f, 0.0f);
    Quaternion rotation;
    Vector3 scale = Vector3(1.0f, 1.0f, 1.0f);
    bool visible = true;
};

struct IcePeeResetState {
    std::vector<MeshResetState> iceStates;
    MeshResetState waterState;
    ObjectResetState pointSpritesState;
    ObjectResetState actorState;
    ParticleVelocityInitializer particleVelocity;
    bool hasWaterState = false;
    bool hasPointSpritesState = false;
    bool hasActorState = false;
    bool hasParticleVelocity = false;
    bool cached = false;
};

static std::unordered_map<Entity, IceMeltVisualState>& getIceMeltVisualStates() {
    static std::unordered_map<Entity, IceMeltVisualState> states;
    return states;
}

static std::unordered_map<Entity, IcePeeResetState>& getIcePeeResetStates() {
    static std::unordered_map<Entity, IcePeeResetState> states;
    return states;
}

static void cacheResetState(IcePee& icePee) {
    Scene* scene = icePee.getScene();
    if (!scene) {
        return;
    }

    IcePeeResetState& resetState = getIcePeeResetStates()[icePee.getEntity()];
    if (resetState.cached) {
        return;
    }

    resetState.iceStates.clear();
    icePee.iceEntities.clear();

    std::vector<Entity> entities = scene->getEntityList();
    for (Entity candidate : entities) {
        if (!isIceEntity(scene, candidate)) {
            continue;
        }

        Mesh ice(scene, candidate);

        MeshResetState iceState;
        iceState.entity = candidate;
        iceState.position = ice.getPosition();
        iceState.scale = ice.getScale();
        iceState.color = ice.getColor();
        iceState.visible = ice.isVisible();

        resetState.iceStates.push_back(iceState);
        icePee.iceEntities.push_back(candidate);
    }

    icePee.iceEntitiesCached = !icePee.iceEntities.empty();
    icePee.waterEntity = findMeshEntityByName(scene, "water");

    if (icePee.waterEntity != NULL_ENTITY && scene->findComponent<MeshComponent>(icePee.waterEntity)) {
        Mesh water(scene, icePee.waterEntity);

        resetState.waterState.entity = icePee.waterEntity;
        resetState.waterState.position = water.getPosition();
        resetState.waterState.scale = water.getScale();
        resetState.waterState.color = water.getColor();
        resetState.waterState.visible = water.isVisible();
        resetState.hasWaterState = true;
    }

    if (icePee.pointSprites) {
        const Entity pointSpritesEntity = icePee.pointSprites->getEntity();

        resetState.pointSpritesState.entity = pointSpritesEntity;
        resetState.pointSpritesState.position = icePee.pointSprites->getPosition();
        resetState.pointSpritesState.rotation = icePee.pointSprites->getRotation();
        resetState.pointSpritesState.scale = icePee.pointSprites->getScale();
        resetState.pointSpritesState.visible = icePee.pointSprites->isVisible();
        resetState.hasPointSpritesState = true;

        icePee.actorEntity = getActorEntityFromPointSprites(scene, pointSpritesEntity);
        if (icePee.actorEntity != NULL_ENTITY) {
            Object actor(scene, icePee.actorEntity);
            resetState.actorState.entity = icePee.actorEntity;
            resetState.actorState.position = actor.getPosition();
            resetState.actorState.rotation = actor.getRotation();
            resetState.actorState.scale = actor.getScale();
            resetState.actorState.visible = actor.isVisible();
            resetState.hasActorState = true;
        }

        Entity particlesEntity = findParticlesEntityForTarget(scene, pointSpritesEntity);
        if (particlesEntity != NULL_ENTITY) {
            if (const ParticlesComponent* pc = scene->findComponent<ParticlesComponent>(particlesEntity)) {
                resetState.particleVelocity = pc->velocityInitializer;
                resetState.hasParticleVelocity = true;
            }
        }
    }

    resetState.cached = true;
}

static void resetParticleRuntime(IcePee& icePee) {
    Scene* scene = icePee.getScene();
    if (!scene) {
        return;
    }

    if (icePee.particlesEntity == NULL_ENTITY && icePee.pointSprites) {
        icePee.particlesEntity = findParticlesEntityForTarget(scene, icePee.pointSprites->getEntity());
    }

    if (icePee.particlesEntity != NULL_ENTITY) {
        Particles(scene, icePee.particlesEntity).reset();
    }
}

static float getLargestScaleComponent(const Vector3& scale) {
    return std::max(scale.x, std::max(scale.y, scale.z));
}

static float getIceMeltProgress(const IceMeltVisualState& state, float currentLargestScale, float minIceScale) {
    const float scaleRange = std::max(0.0001f, state.initialLargestScale - minIceScale);
    return (state.initialLargestScale <= minIceScale)
        ? 1.0f
        : std::max(0.0f, std::min(1.0f - ((currentLargestScale - minIceScale) / scaleRange), 1.0f));
}

static Vector4 lerpColor(const Vector4& from, const Vector4& to, float factor) {
    const float clampedFactor = std::max(0.0f, std::min(factor, 1.0f));

    return Vector4(
        from.x + (to.x - from.x) * clampedFactor,
        from.y + (to.y - from.y) * clampedFactor,
        from.z + (to.z - from.z) * clampedFactor,
        from.w + (to.w - from.w) * clampedFactor);
}

static IceMeltVisualState& getIceMeltVisualState(Entity entity, const Mesh& ice, float currentLargestScale) {
    IceMeltVisualState& state = getIceMeltVisualStates()[entity];

    if (state.initialLargestScale <= 0.0f) {
        state.initialLargestScale = currentLargestScale;
        state.initialColor = ice.getColor();
    }

    return state;
}

static void applyIceMeltColor(Mesh& ice, IceMeltVisualState& state, float currentLargestScale, float minIceScale) {
    const float meltProgress = getIceMeltProgress(state, currentLargestScale, minIceScale);
    const Vector4 meltedColor(kIceMeltTargetColor.x, kIceMeltTargetColor.y, kIceMeltTargetColor.z, state.initialColor.w);

    ice.setColor(lerpColor(state.initialColor, meltedColor, meltProgress));
}

static int awardIceMeltScore(IceMeltVisualState& state, float currentLargestScale, float minIceScale) {
    const int targetScore = static_cast<int>(std::floor(getIceMeltProgress(state, currentLargestScale, minIceScale) * kScorePerIceEntity));
    const int gainedScore = std::max(0, targetScore - state.awardedScore);

    state.awardedScore += gainedScore;
    return gainedScore;
}

static void hideMeltedIce(Entity entity, Mesh& ice) {
    ice.setVisible(false);
    getIceMeltVisualStates().erase(entity);
}

static void startPeeSound(Sound* sound) {
    if (!sound) {
        return;
    }

    sound->play();
}

static void stopPeeSound(Sound* sound) {
    if (!sound) {
        return;
    }

    sound->stop();
}

static void syncScoreText(Text* text, int score) {
    if (!text) {
        return;
    }

    text->setText("Score: " + std::to_string(score));
}

static int getMaxScore(const std::vector<Entity>& iceEntities) {
    return static_cast<int>(iceEntities.size()) * kScorePerIceEntity;
}

static void syncGameOverTexts(
    Text* bestScoreText,
    Text* yourScoreText,
    Text* messageText,
    int currentScore,
    int maxScore) {
    const int storedBest = UserSettings::getIntegerForKey(kBestScoreKey, 0);
    const bool isNewBest = currentScore > storedBest;

    if (isNewBest) {
        UserSettings::setIntegerForKey(kBestScoreKey, currentScore);
    }

    const int bestScore = isNewBest ? currentScore : storedBest;
    const bool reachedMaxScore = maxScore > 0 && currentScore >= maxScore;

    if (bestScoreText) {
        bestScoreText->setText("Best score: " + std::to_string(bestScore));
    }

    if (yourScoreText) {
        yourScoreText->setVisible(!reachedMaxScore);
        if (!reachedMaxScore) {
            yourScoreText->setText("Your score: " + std::to_string(currentScore));
        }
    }

    if (messageText) {
        messageText->setText(isNewBest ? "New best score!" : "Keep trying!");
    }
}

static constexpr float kRoundTimeSeconds = 15.0f;

static void syncTimeProgressbar(Progressbar* progressbar, float remainingTimeSeconds) {
    if (!progressbar) {
        return;
    }

    const float clampedRemaining = std::max(0.0f, remainingTimeSeconds);
    progressbar->setValue(clampedRemaining / kRoundTimeSeconds);
}

static void fillWaterEntity(Scene* scene, const ParticlesComponent& particles, Entity waterEntity, float deltaTime, float& waterMaxScaleY) {
    if (!scene || waterEntity == NULL_ENTITY || deltaTime <= 0.0f) {
        return;
    }

    Mesh water(scene, waterEntity);
    if (!water.isVisible()) {
        return;
    }

    const AABB waterAABB = water.getWorldAABB();
    if (!hasParticleHitAABB(particles, waterAABB)) {
        return;
    }

    const float fillRatePerSecond = 0.010f;
    const float maxScaleMultiplier = 7.0f;

    Vector3 currentScale = water.getScale();
    if (waterMaxScaleY <= 0.0f) {
        waterMaxScaleY = currentScale.y * maxScaleMultiplier;
    }

    const float nextScaleY = std::min(waterMaxScaleY, currentScale.y + fillRatePerSecond * deltaTime);
    if (nextScaleY <= currentScale.y) {
        return;
    }

    const float baseBottomY = waterAABB.getMinimum().y;
    water.setScale(Vector3(currentScale.x, nextScaleY, currentScale.z));
    water.updateTransform();

    Vector3 waterPosition = water.getPosition();
    const float shiftedBottomY = water.getWorldAABB().getMinimum().y;
    waterPosition.y += baseBottomY - shiftedBottomY;
    water.setPosition(waterPosition);
    water.updateTransform();
}

static int meltIceEntities(Scene* scene, const ParticlesComponent& particles, float deltaTime, const std::vector<Entity>& iceEntities) {
    if (!scene || deltaTime <= 0.0f) {
        return 0;
    }

    const float meltRatePerSecond = 0.015f;
    const float minIceScale = 0.025f;
    const float meltAmount = meltRatePerSecond * deltaTime;
    int gainedScore = 0;

    for (Entity candidate : iceEntities) {
        Mesh ice(scene, candidate);
        if (!ice.isVisible()) {
            continue;
        }

        Vector3 currentScale = ice.getScale();
        float largestScale = getLargestScaleComponent(currentScale);
        IceMeltVisualState& visualState = getIceMeltVisualState(candidate, ice, largestScale);

        if (!hasParticleHitAABB(particles, ice.getWorldAABB())) {
            continue;
        }

        if (largestScale <= minIceScale) {
            gainedScore += awardIceMeltScore(visualState, minIceScale, minIceScale);
            hideMeltedIce(candidate, ice);
            continue;
        }

        float nextLargestScale = std::max(minIceScale, largestScale - meltAmount);
        if (nextLargestScale >= largestScale) {
            continue;
        }

        float scaleFactor = nextLargestScale / largestScale;
        ice.setScale(Vector3(
            currentScale.x * scaleFactor,
            currentScale.y * scaleFactor,
            currentScale.z * scaleFactor));
        ice.updateTransform();

        gainedScore += awardIceMeltScore(visualState, nextLargestScale, minIceScale);

        if (nextLargestScale <= minIceScale) {
            hideMeltedIce(candidate, ice);
            continue;
        }

        applyIceMeltColor(ice, visualState, nextLargestScale, minIceScale);
    }

    return gainedScore;
}

static void cacheVelocityConeFromParticles(const ParticlesComponent& particles, float& minSpeed, float& maxSpeed, float& coneAngle) {
    const Vector3& minVelocity = particles.velocityInitializer.minVelocity;
    const Vector3& maxVelocity = particles.velocityInitializer.maxVelocity;

    float minForward = std::min(std::fabs(minVelocity.x), std::fabs(maxVelocity.x));
    float maxForward = std::max(std::fabs(minVelocity.x), std::fabs(maxVelocity.x));
    float lateralX = std::max(std::fabs(minVelocity.y), std::fabs(maxVelocity.y));
    float lateralY = std::max(std::fabs(minVelocity.z), std::fabs(maxVelocity.z));
    float lateralRadius = std::sqrt(lateralX * lateralX + lateralY * lateralY);

    minSpeed = std::max(0.0f, minForward);
    maxSpeed = std::max(minSpeed, maxForward);
    coneAngle = (minSpeed > 0.0f) ? std::atan2(lateralRadius, minSpeed) * 57.2957795130823208768f : 0.0f;
}

static void applyVelocityConeToParticles(ParticlesComponent& particles, float minSpeed, float maxSpeed, float coneAngle) {
    const float clampedMinSpeed = std::max(0.0f, std::min(minSpeed, maxSpeed));
    const float clampedMaxSpeed = std::max(clampedMinSpeed, std::max(minSpeed, maxSpeed));
    const float clampedConeAngle = std::min(std::max(coneAngle, 0.0f), 89.0f);
    const float coneAngleRad = clampedConeAngle * 0.01745329251994329577f;

    // The point sprite base rotation maps local +Z to world -X, so a local cone around +Z
    // produces the requested default direction while drag rotation steers the cone.
    const float lateralExtent = (clampedMinSpeed > 0.0f)
        ? std::tan(coneAngleRad) * clampedMinSpeed * 0.7071067811865475244f
        : 0.0f;

    particles.velocityInitializer.minVelocity = Vector3(-lateralExtent, -lateralExtent, clampedMinSpeed);
    particles.velocityInitializer.maxVelocity = Vector3(lateralExtent, lateralExtent, clampedMaxSpeed);
}

IcePee::IcePee(Scene* scene, Entity entity): ScriptBase(scene, entity) {
    REGISTER_ENGINE_EVENT(onUpdate);
    getIceMeltVisualStates().clear();
    
    Engine::setCallMouseInTouchEvent(true);
}

IcePee::~IcePee() {
    getIcePeeResetStates().erase(getEntity());
}

void IcePee::onUpdate() {
    cacheResetState(*this);

    if (!isActive) {
        if (wasGameActive) {
            stopPeeSound(peeSound);
            wasGameActive = false;
        }
        if (pointSprites) pointSprites->setVisible(false);
        return;
    }

    if (!wasGameActive) {
        startPeeSound(peeSound);
        wasGameActive = true;
    }

    if (pointSprites) pointSprites->setVisible(true);

    const float deltaTime = Engine::getDeltatime();
    drunkSwayTime += deltaTime;
    remainingTimeSeconds = std::max(0.0f, remainingTimeSeconds - deltaTime);
    syncTimeProgressbar(time, remainingTimeSeconds);
    if (remainingTimeSeconds <= 0.0f) {
        isActive = false;
        stopPeeSound(peeSound);
        wasGameActive = false;
        if (pointSprites) pointSprites->setVisible(false);
        if (particlesEntity != NULL_ENTITY && scene->findComponent<ParticlesComponent>(particlesEntity)) {
            Particles(scene, particlesEntity).stop();
        }
        syncGameOverTexts(
            gameOverBestScore,
            gameOverYourScore,
            gameOverMessage,
            counter,
            getMaxScore(iceEntities));
        SceneManager::addChildScene("Game Over Scene");
        SceneManager::removeChildScene("Score Scene");
        return;
    }

    Vector2 mousePosition = Input::getMousePosition();
    bool isDragging = Input::isMousePressed(D_MOUSE_BUTTON_LEFT);

    if (isDragging && wasDragging) {
        float deltaX = mousePosition.x - lastMousePosition.x;
        float deltaY = mousePosition.y - lastMousePosition.y;
        float rotationSensitivity = speed * 0.1f;

        pointSpritesYaw -= deltaX * rotationSensitivity;
        pointSpritesPitch += deltaY * rotationSensitivity;
    }

    lastMousePosition = mousePosition;
    wasDragging = isDragging;

    if (actorEntity == NULL_ENTITY && pointSprites) {
        actorEntity = getActorEntityFromPointSprites(scene, pointSprites->getEntity());
    }

    if (actorEntity != NULL_ENTITY) {
        const IcePeeResetState& resetState = getIcePeeResetStates()[getEntity()];
        if (resetState.hasActorState) {
            applyDrunkSway(
                scene,
                actorEntity,
                resetState.actorState.position,
                resetState.actorState.rotation,
                drunkSwayTime,
                drunkSwayAmount,
                drunkSwaySpeed);
        }
    }

    if (pointSprites) {
        Quaternion controlRotation = getPointSpritesControlRotation(pointSpritesYaw, pointSpritesPitch);
        pointSprites->setRotation(controlRotation * getPointSpritesBaseRotation());

        Entity targetEntity = pointSprites->getEntity();
        if (particlesEntity == NULL_ENTITY || !scene->findComponent<ParticlesComponent>(particlesEntity)) {
            particlesEntity = findParticlesEntityForTarget(scene, targetEntity);
            particlesConeCached = false;
        }

        if (!particlesStarted && particlesEntity != NULL_ENTITY) {
            Particles(scene, particlesEntity).start();
            particlesStarted = true;
        }

        if (waterEntity == NULL_ENTITY || !scene->findComponent<MeshComponent>(waterEntity)) {
            waterEntity = findMeshEntityByName(scene, "water");
            waterMaxScaleY = 0.0f;
        }

        ParticlesComponent* particles = scene->findComponent<ParticlesComponent>(particlesEntity);
        if (particles) {
            if (!particlesConeCached) {
                cacheVelocityConeFromParticles(*particles, particlesMinSpeed, particlesMaxSpeed, particlesConeAngle);
                particlesConeCached = true;
            }
            applyVelocityConeToParticles(*particles, particlesMinSpeed, particlesMaxSpeed, particlesConeAngle);
            fillWaterEntity(scene, *particles, waterEntity, deltaTime, waterMaxScaleY);
            if (!iceEntitiesCached) {
                std::vector<Entity> allEntities = scene->getEntityList();
                for (Entity candidate : allEntities) {
                    if (isIceEntity(scene, candidate)) {
                        iceEntities.push_back(candidate);
                    }
                }
                iceEntitiesCached = true;
            }
            const int gainedScore = meltIceEntities(scene, *particles, deltaTime, iceEntities);
            if (gainedScore > 0) {
                counter += gainedScore;
                syncScoreText(score, counter);
            }
        }
    }
}

void IcePee::resetGame() {
    Scene* currentScene = getScene();
    if (!currentScene) {
        return;
    }

    cacheResetState(*this);
    IcePeeResetState& resetState = getIcePeeResetStates()[getEntity()];

    getIceMeltVisualStates().clear();
    resetParticleRuntime(*this);

    pointSpritesYaw = 0.0f;
    pointSpritesPitch = 0.0f;
    drunkSwayTime = 0.0f;
    wasDragging = false;
    lastMousePosition = Vector2(0.0f, 0.0f);

    particlesMinSpeed = 0.0f;
    particlesMaxSpeed = 0.0f;
    particlesConeAngle = 0.0f;
    particlesConeCached = false;
    particlesStarted = false;

    counter = 0;
    remainingTimeSeconds = kRoundTimeSeconds;
    waterMaxScaleY = 0.0f;
    isActive = false;
    stopPeeSound(peeSound);
    wasGameActive = false;

    iceEntities.clear();
    for (const MeshResetState& iceState : resetState.iceStates) {
        if (!currentScene->findComponent<MeshComponent>(iceState.entity)) {
            continue;
        }

        Mesh ice(currentScene, iceState.entity);
        ice.setPosition(iceState.position);
        ice.setVisible(iceState.visible);
        ice.setScale(iceState.scale);
        ice.setColor(iceState.color);
        ice.updateTransform();

        iceEntities.push_back(iceState.entity);
    }
    iceEntitiesCached = !iceEntities.empty();

    if (resetState.hasWaterState && currentScene->findComponent<MeshComponent>(resetState.waterState.entity)) {
        Mesh water(currentScene, resetState.waterState.entity);
        water.setVisible(resetState.waterState.visible);
        water.setPosition(resetState.waterState.position);
        water.setScale(resetState.waterState.scale);
        water.setColor(resetState.waterState.color);
        water.updateTransform();
        waterEntity = resetState.waterState.entity;
    }

    if (resetState.hasActorState && currentScene->findComponent<Transform>(resetState.actorState.entity)) {
        Object actor(currentScene, resetState.actorState.entity);
        actor.setPosition(resetState.actorState.position);
        actor.setRotation(resetState.actorState.rotation);
        actor.setScale(resetState.actorState.scale);
        actor.setVisible(resetState.actorState.visible);
        actor.updateTransform();
        actorEntity = resetState.actorState.entity;
    }

    if (pointSprites) {
        if (resetState.hasPointSpritesState) {
            pointSprites->setPosition(resetState.pointSpritesState.position);
            pointSprites->setScale(resetState.pointSpritesState.scale);
        }
        pointSprites->setRotation(getPointSpritesBaseRotation());
        pointSprites->setVisible(false);
        pointSprites->updateTransform();

        if (resetState.hasParticleVelocity && particlesEntity != NULL_ENTITY) {
            if (ParticlesComponent* pc = currentScene->findComponent<ParticlesComponent>(particlesEntity)) {
                pc->velocityInitializer = resetState.particleVelocity;
            }
        }
    }

    syncScoreText(score, counter);
    syncTimeProgressbar(time, remainingTimeSeconds);
}

