#include <Geode/Geode.hpp>
#include <Geode/modify/CCTextInputNode.hpp>

using namespace geode::prelude;

class $modify(CharFadeInput, CCTextInputNode) {
    struct Fields {
        gd::string prevString;
    };

    static constexpr float kFadeDuration = 0.15f;

    void refreshLabel() {
        gd::string oldStr = m_fields->prevString;
        CCTextInputNode::refreshLabel();

        gd::string newStr = m_textField ? gd::string(m_textField->getString()) : gd::string("");
        m_fields->prevString = newStr;

        if (!m_textLabel || newStr.empty()) return;

        size_t p = 0;
        size_t maxP = std::min(oldStr.size(), newStr.size());
        while (p < maxP && oldStr[p] == newStr[p]) ++p;

        size_t s = 0;
        size_t maxS = std::min(oldStr.size() - p, newStr.size() - p);
        while (s < maxS && oldStr[oldStr.size() - 1 - s] == newStr[newStr.size() - 1 - s]) ++s;

        size_t newStart = p;
        size_t newEnd = newStr.size() - s;
        if (newStart >= newEnd) return;

        auto children = m_textLabel->getChildren();
        if (!children) return;
        int childCount = children->count();

        int childIdx = 0;
        for (size_t i = 0; i < newEnd && childIdx < childCount; ++i) {
            if (newStr[i] == ' ') continue;
            if (i >= newStart) {
                auto sprite = typeinfo_cast<CCSprite*>(children->objectAtIndex(childIdx));
                if (sprite) {
                    sprite->setOpacity(0);
                    sprite->runAction(CCFadeIn::create(kFadeDuration));
                }
            }
            ++childIdx;
        }
    }
};
