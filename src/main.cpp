#include <Geode/Geode.hpp>
#include <Geode/modify/CCTextInputNode.hpp>
#include <Geode/ui/Popup.hpp>
#include <algorithm>
#include <string>
#include <vector>

using namespace geode::prelude;

class ModSettingsPopup : public geode::Popup {};

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

        for (size_t i = start; i < end && childIdx < childCount; ++i) {
            if (s[i] == ' ') continue;
            auto sprite = static_cast<CCSprite*>(children->objectAtIndex(childIdx));
            sprite->setOpacity(0);
            sprite->runAction(CCFadeIn::create(fadeInDuration()));
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
            s->runAction(CCSequence::create(
                CCFadeOut::create(fadeOutDuration()),
                CCRemoveSelf::create(),
                nullptr));
        }
    }

    void refreshLabel() {
        if (CCScene::get() && CCScene::get()->getChildByType<ModSettingsPopup>(0)) {
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
