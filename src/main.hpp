#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/random.hpp>
#include <cmath>

namespace smoothtextinput {

constexpr float kDistanceMax = 50.0f;
constexpr int   kFadeInTag   = 0x7AD0;

struct PopSettings {
    bool  enabled;
    int   angle;
    bool  randomAngle;
    float distance;
    bool  randomDistance;
};

struct Ghost {
    cocos2d::CCTexture2D* tex;
    cocos2d::CCRect       rect;
    cocos2d::CCPoint      pos;
    cocos2d::CCPoint      anchor;
    float                 scaleX;
    float                 scaleY;
    float                 rotation;
    cocos2d::ccColor3B    color;
    GLubyte               opacity;
};

inline float fadeInDuration() {
    return static_cast<float>(geode::Mod::get()->getSettingValue<double>("fade-in-duration"));
}

inline float fadeOutDuration() {
    return static_cast<float>(geode::Mod::get()->getSettingValue<double>("fade-out-duration"));
}

inline PopSettings popInSettings() {
    auto mod = geode::Mod::get();
    return {
        mod->getSettingValue<bool>("pop-in-enabled"),
        static_cast<int>(mod->getSettingValue<int64_t>("pop-in-angle")),
        mod->getSettingValue<bool>("pop-in-angle-random"),
        static_cast<float>(mod->getSettingValue<double>("pop-in-distance")),
        mod->getSettingValue<bool>("pop-in-distance-random"),
    };
}

inline PopSettings popOutSettings() {
    auto mod = geode::Mod::get();
    return {
        mod->getSettingValue<bool>("pop-out-enabled"),
        static_cast<int>(mod->getSettingValue<int64_t>("pop-out-angle")),
        mod->getSettingValue<bool>("pop-out-angle-random"),
        static_cast<float>(mod->getSettingValue<double>("pop-out-distance")),
        mod->getSettingValue<bool>("pop-out-distance-random"),
    };
}

// Motion vector, angle 0 = up.
inline cocos2d::CCPoint popMotion(float angleDeg, float distance) {
    float r = angleDeg * M_PI / 180.0f; // I'm sorry dankmeme, you're right.
    return cocos2d::CCPoint(std::sin(r) * distance, std::cos(r) * distance);
}

inline void resolvePop(PopSettings const& settings, float& outAngle, float& outDist) {
    if (!settings.enabled) { outAngle = 0.0f; outDist = 0.0f; return; }
    outAngle = settings.randomAngle
        ? geode::utils::random::generate<float>(0.0f, 360.0f)
        : static_cast<float>(settings.angle);
    outDist  = settings.randomDistance
        ? geode::utils::random::generate<float>(0.0f, kDistanceMax)
        : settings.distance;
}

} // namespace smoothtextinput
