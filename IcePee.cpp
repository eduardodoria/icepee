#include "IcePee.h"
#include "Input.h"

#include "action/Particles.h"
#include "component/ActionComponent.h"
#include "component/MeshComponent.h"
#include "component/ParticlesComponent.h"
#include "object/Mesh.h"

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

struct IceMeltVisualState {
    float initialLargestScale = 0.0f;
    Vector4 initialColor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
};

static std::unordered_map<Entity, IceMeltVisualState>& getIceMeltVisualStates() {
    static std::unordered_map<Entity, IceMeltVisualState> states;
    return states;
}

static float getLargestScaleComponent(const Vector3& scale) {
    return std::max(scale.x, std::max(scale.y, scale.z));
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
    const float scaleRange = std::max(0.0001f, state.initialLargestScale - minIceScale);
    const float meltProgress = (state.initialLargestScale <= minIceScale)
        ? 1.0f
        : std::max(0.0f, std::min(1.0f - ((currentLargestScale - minIceScale) / scaleRange), 1.0f));
    const Vector4 meltedColor(kIceMeltTargetColor.x, kIceMeltTargetColor.y, kIceMeltTargetColor.z, state.initialColor.w);

    ice.setColor(lerpColor(state.initialColor, meltedColor, meltProgress));
}

static void hideMeltedIce(Entity entity, Mesh& ice) {
    ice.setVisible(false);
    getIceMeltVisualStates().erase(entity);
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

static void meltIceEntities(Scene* scene, const ParticlesComponent& particles, float deltaTime, const std::vector<Entity>& iceEntities) {
    if (!scene || deltaTime <= 0.0f) {
        return;
    }

    const float meltRatePerSecond = 0.015f;
    const float minIceScale = 0.025f;
    const float meltAmount = meltRatePerSecond * deltaTime;

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

        if (nextLargestScale <= minIceScale) {
            hideMeltedIce(candidate, ice);
            continue;
        }

        applyIceMeltColor(ice, visualState, nextLargestScale, minIceScale);
    }
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
}

IcePee::~IcePee() {

}

void IcePee::onUpdate() {
    if (!isActive) return;

    Vector2 mousePosition = Input::getMousePosition();
    bool isDragging = Input::isMousePressed(S_MOUSE_BUTTON_LEFT);

    if (isDragging && wasDragging) {
        float deltaX = mousePosition.x - lastMousePosition.x;
        float deltaY = mousePosition.y - lastMousePosition.y;
        float rotationSensitivity = speed * 0.1f;

        pointSpritesYaw -= deltaX * rotationSensitivity;
        pointSpritesPitch += deltaY * rotationSensitivity;
    }

    lastMousePosition = mousePosition;
    wasDragging = isDragging;

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
            fillWaterEntity(scene, *particles, waterEntity, Engine::getDeltatime(), waterMaxScaleY);
            if (!iceEntitiesCached) {
                std::vector<Entity> allEntities = scene->getEntityList();
                for (Entity candidate : allEntities) {
                    if (isIceEntity(scene, candidate)) {
                        iceEntities.push_back(candidate);
                    }
                }
                iceEntitiesCached = true;
            }
            meltIceEntities(scene, *particles, Engine::getDeltatime(), iceEntities);
        }
    }

    // Example: Increment counter every frame
    counter++;
}

