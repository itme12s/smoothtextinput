#include <Geode/Geode.hpp>
#include <Geode/modify/CCTextInputNode.hpp>
#include <Geode/utils/random.hpp>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace geode::prelude;

class $modify(CharFadeInput, CCTextInputNode) {
    struct Fields {
        std::string prevString;
    };

    static float fadeInDuration() {
        return static_cast<float>(Mod::get()->getSettingValue<double>("fade-in-duration"));
    }

    static float fadeOutDuration() {
        return static_cast<float>(Mod::get()->getSettingValue<double>("fade-out-duration"));
    }

    struct PopSettings {
        bool enabled;
        int angle;
        bool randomAngle;
        float distance;
        bool randomDistance;
    };

    static PopSettings popInSettings() {
        auto mod = Mod::get();
        return {
            mod->getSettingValue<bool>("pop-in-enabled"),
            static_cast<int>(mod->getSettingValue<int64_t>("pop-in-angle")),
            mod->getSettingValue<bool>("pop-in-angle-random"),
            static_cast<float>(mod->getSettingValue<double>("pop-in-distance")),
            mod->getSettingValue<bool>("pop-in-distance-random"),
        };
    }

    static PopSettings popOutSettings() {
        auto mod = Mod::get();
        return {
            mod->getSettingValue<bool>("pop-out-enabled"),
            static_cast<int>(mod->getSettingValue<int64_t>("pop-out-angle")),
            mod->getSettingValue<bool>("pop-out-angle-random"),
            static_cast<float>(mod->getSettingValue<double>("pop-out-distance")),
            mod->getSettingValue<bool>("pop-out-distance-random"),
        };
    }

    // Motion vector, angle 0 = up.
    static CCPoint popMotion(float angleDeg, float distance) {
        float r = angleDeg * M_PI / 180.0f;
        return CCPoint(std::sin(r) * distance, std::cos(r) * distance);
    }

    static constexpr float kDistanceMax = 50.0f;

    static void resolvePop(PopSettings const& settings, float& outAngle, float& outDist) {
        if (!settings.enabled) { outAngle = 0.0f; outDist = 0.0f; return; }
        outAngle = settings.randomAngle ? utils::random::generate<float>(0.0f, 360.0f) : static_cast<float>(settings.angle);
        outDist  = settings.randomDistance ? utils::random::generate<float>(0.0f, kDistanceMax) : settings.distance;
    }

    struct Ghost {
        CCTexture2D* tex;
        CCRect rect;
        CCPoint pos;
        CCPoint anchor;
        float scaleX;
        float scaleY;
        float rotation;
        ccColor3B color;
        GLubyte opacity;
    };

    static int charIndexToChildIndex(std::string const& s, size_t charPos) {
        return static_cast<int>(
            std::count_if(s.begin(), s.begin() + charPos, [](char c) { return c != ' '; }));
    }

    std::vector<Ghost> snapshotRange(size_t start, size_t end) {
        std::vector<Ghost> out;
        if (start >= end || !m_textLabel) return out;
        out.reserve(end - start);

        auto children = m_textLabel->getChildren();
        if (!children || children->count() == 0) return out;
        int childCount = children->count();

        auto const& s = m_fields->prevString;
        int childIdx = charIndexToChildIndex(s, start);

        for (size_t i = start; i < end && childIdx < childCount; ++i) {
            if (s[i] == ' ') continue;
            auto sprite = typeinfo_cast<CCSprite*>(children->objectAtIndex(childIdx));
            if (sprite) {
                out.push_back({
                    sprite->getTexture(),
                    sprite->getTextureRect(),
                    sprite->getPosition(),
                    sprite->getAnchorPoint(),
                    sprite->getScaleX(),
                    sprite->getScaleY(),
                    sprite->getRotation(),
                    sprite->getColor(),
                    sprite->getOpacity(),
                });
            }
            ++childIdx;
        }
        return out;
    }

    void fadeInRange(size_t start, size_t end) {
        if (start >= end || !m_textLabel) return;

        auto children = m_textLabel->getChildren();
        if (!children || children->count() == 0) return;
        int childCount = children->count();

        if (!typeinfo_cast<CCSprite*>(children->objectAtIndex(0))) return;

        auto const& s = m_fields->prevString;
        int childIdx = charIndexToChildIndex(s, start);

        float dur = fadeInDuration();
        auto popSettings = popInSettings();

        for (size_t i = start; i < end && childIdx < childCount; ++i) {
            if (s[i] == ' ') continue;
            auto sprite = static_cast<CCSprite*>(children->objectAtIndex(childIdx));
            sprite->setOpacity(0);

            float angle, dist;
            resolvePop(popSettings, angle, dist);

            if (dist > 0.0f) {
                CCPoint finalPos = sprite->getPosition();
                CCPoint motion = popMotion(angle, dist);
                sprite->setPosition(finalPos - motion);
                sprite->runAction(CCSpawn::create(
                    CCFadeIn::create(dur),
                    CCMoveTo::create(dur, finalPos),
                    nullptr));
            } else {
                sprite->runAction(CCFadeIn::create(dur));
            }
            ++childIdx;
        }
    }

    void fadeOutGhosts(std::vector<Ghost> const& ghosts) {
        if (ghosts.empty() || !m_textLabel) return;
        auto parent = m_textLabel->getParent();
        if (!parent) return;

        float lblScaleX = m_textLabel->getScaleX();
        float lblScaleY = m_textLabel->getScaleY();
        float lblRot    = m_textLabel->getRotation();
        int   zOrder    = m_textLabel->getZOrder();

        float dur = fadeOutDuration();
        auto popSettings = popOutSettings();

        for (auto const& g : ghosts) {
            if (!g.tex) continue;
            auto s = CCSprite::createWithTexture(g.tex, g.rect);
            if (!s) continue;

            CCPoint world = m_textLabel->convertToWorldSpace(g.pos);
            CCPoint local = parent->convertToNodeSpace(world);

            s->setPosition(local);
            s->setAnchorPoint(g.anchor);
            s->setScaleX(g.scaleX * lblScaleX);
            s->setScaleY(g.scaleY * lblScaleY);
            s->setRotation(g.rotation + lblRot);
            s->setColor(g.color);
            s->setOpacity(g.opacity);

            parent->addChild(s, zOrder);

            float angle, dist;
            resolvePop(popSettings, angle, dist);

            if (dist > 0.0f) {
                s->runAction(CCSequence::create(
                    CCSpawn::create(
                        CCFadeOut::create(dur),
                        CCMoveBy::create(dur, popMotion(angle, dist)),
                        nullptr),
                    CCRemoveSelf::create(),
                    nullptr));
            } else {
                s->runAction(CCSequence::create(
                    CCFadeOut::create(dur),
                    CCRemoveSelf::create(),
                    nullptr));
            }
        }
    }

    void refreshLabel() {
        if (!m_selected) {
            CCTextInputNode::refreshLabel();
            m_fields->prevString = m_textField->getString();
            return;
        }

        std::string newStr = m_textField->getString();
        auto& oldStr = m_fields->prevString;

        if (oldStr == newStr) {
            CCTextInputNode::refreshLabel();
            return;
        }

        auto [oIt, nIt] = std::mismatch(oldStr.begin(), oldStr.end(), newStr.begin(), newStr.end());
        size_t p = static_cast<size_t>(nIt - newStr.begin());

        auto oRStop = oldStr.rend() - p;
        auto nRStop = newStr.rend() - p;
        auto [oRIt, nRIt] = std::mismatch(oldStr.rbegin(), oRStop, newStr.rbegin(), nRStop);
        size_t s = static_cast<size_t>(nRIt - newStr.rbegin());

        size_t newStart = p;
        size_t newEnd = newStr.size() - s;
        size_t oldStart = p;
        size_t oldEnd = oldStr.size() - s;

        auto ghosts = snapshotRange(oldStart, oldEnd);

        CCTextInputNode::refreshLabel();

        oldStr = std::move(newStr);

        fadeInRange(newStart, newEnd);
        fadeOutGhosts(ghosts);
    }
};
