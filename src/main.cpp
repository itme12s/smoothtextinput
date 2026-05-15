#include <Geode/Geode.hpp>
#include <Geode/modify/CCTextInputNode.hpp>
#include <algorithm>
#include <string>

using namespace geode::prelude;

class $modify(CharFadeInput, CCTextInputNode) {
    struct Fields {
        std::string prevString;
    };

    static constexpr float kFadeDuration = 0.15f;

    void refreshLabel() {
        CCTextInputNode::refreshLabel();

        std::string newStr = m_textField->getString();
        auto& oldStr = m_fields->prevString;

        if (oldStr == newStr) return;

        auto [oIt, nIt] = std::mismatch(oldStr.begin(), oldStr.end(), newStr.begin(), newStr.end());
        size_t p = static_cast<size_t>(nIt - newStr.begin());

        auto oRStop = oldStr.rend() - p;
        auto nRStop = newStr.rend() - p;
        auto [oRIt, nRIt] = std::mismatch(oldStr.rbegin(), oRStop, newStr.rbegin(), nRStop);
        size_t s = static_cast<size_t>(nRIt - newStr.rbegin());

        size_t newStart = p;
        size_t newEnd = newStr.size() - s;

        oldStr = std::move(newStr);

        if (newStart >= newEnd || !m_textLabel) return;

        auto children = m_textLabel->getChildren();
        if (!children || children->count() == 0) return;
        int childCount = children->count();

        if (!typeinfo_cast<CCSprite*>(children->objectAtIndex(0))) return;

        auto const& s2 = m_fields->prevString;
        int childIdx = static_cast<int>(
            std::count_if(s2.begin(), s2.begin() + newStart, [](char c) { return c != ' '; }));

        for (size_t i = newStart; i < newEnd && childIdx < childCount; ++i) {
            if (s2[i] == ' ') continue;
            auto sprite = static_cast<CCSprite*>(children->objectAtIndex(childIdx));
            sprite->setOpacity(0);
            sprite->runAction(CCFadeIn::create(kFadeDuration));
            ++childIdx;
        }
    }
};
