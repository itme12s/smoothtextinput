#include "main.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/CCTextInputNode.hpp>
#include <Geode/binding/MultilineBitmapFont.hpp>
#include <Geode/binding/TextArea.hpp>
#include <Geode/ui/Popup.hpp>
#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

using namespace geode::prelude;
using namespace smoothtextinput;

class ModSettingsPopup : public geode::Popup {};

class $modify(CharFadeInput, CCTextInputNode) {
    struct Fields {
        std::string prevString;
        std::vector<Ghost> prevLabel;
        std::vector<std::optional<PendingFade>> pending;
        bool externalUpdate = false;
    };

    CCNode* activeLabel() {
        if (m_textArea && m_textArea->m_label) return m_textArea->m_label;
        return m_textLabel;
    }

    GLubyte activeLabelOpacity() {
        if (m_textArea && m_textArea->m_label) return m_textArea->m_label->getOpacity();
        return m_textLabel ? m_textLabel->getOpacity() : 255;
    }

    CCSprite* glyphAtRawIndex(std::string const& text, size_t rawIndex) {
        if (rawIndex >= text.size()) return nullptr;

        if (m_textArea && m_textArea->m_label) {
            if (text[rawIndex] == '\n' || text[rawIndex] == '\r') return nullptr;

            auto chars = m_textArea->m_label->m_characters;
            if (!chars) return nullptr;

            size_t glyphIndex = 0;
            for (size_t i = 0; i < rawIndex; ++i) {
                if (text[i] != '\n' && text[i] != '\r') ++glyphIndex;
            }
            if (glyphIndex >= chars->count()) return nullptr;

            return typeinfo_cast<CCSprite*>(chars->objectAtIndex(static_cast<unsigned int>(glyphIndex)));
        }

        if (!m_textLabel) return nullptr;
        return typeinfo_cast<CCSprite*>(m_textLabel->getChildByTag(static_cast<int>(rawIndex)));
    }

    void purgeGhostsFromParent(CCNode* parent) {
        if (!parent) return;
        auto children = parent->getChildren();
        if (!children) return;

        std::vector<CCNode*> toRemove;
        for (unsigned int j = 0; j < children->count(); ++j) {
            auto node = typeinfo_cast<CCNode*>(children->objectAtIndex(j));
            if (node && node->getTag() == kGhostTag) toRemove.push_back(node);
        }
        for (auto n : toRemove) n->removeFromParent();
    }

    void captureLabelSnapshot() {
        auto& prev = m_fields->prevLabel;
        prev.clear();
        auto label = activeLabel();
        if (!label) return;
        GLubyte fullOp = activeLabelOpacity();
        size_t len = m_fields->prevString.size();
        prev.reserve(len);
        for (size_t i = 0; i < len; ++i) {
            auto sprite = glyphAtRawIndex(m_fields->prevString, i);
            if (!sprite) {
                prev.push_back({});
                continue;
            }
            auto spriteParent = sprite->getParent();
            prev.push_back({
                sprite->getTexture(),
                sprite->getTextureRect(),
                spriteParent
                    ? spriteParent->convertToWorldSpace(sprite->getPosition())
                    : sprite->convertToWorldSpace(CCPointZero),
                sprite->getAnchorPoint(),
                sprite->getScaleX(),
                sprite->getScaleY(),
                sprite->getRotation(),
                sprite->getColor(),
                fullOp,
            });
        }
    }

    void plainRefresh() {
        CCTextInputNode::refreshLabel();
        m_fields->prevString = m_textField->getString();
        m_fields->pending.clear();
        captureLabelSnapshot();
    }

    void purgeGhosts() {
        purgeGhostsFromParent(m_textLabel ? m_textLabel->getParent() : nullptr);
        purgeGhostsFromParent(m_textArea && m_textArea->m_label ? m_textArea->m_label->getParent() : nullptr);
    }

    void fadeInRange(size_t start, size_t end) {
        if (start >= end || !activeLabel()) return;

        float dur = fadeInDuration();
        auto popSettings = popInSettings();
        auto now = std::chrono::steady_clock::now();
        auto const& text = m_fields->prevString;

        // if (m_fields->pending.size() < end) m_fields->pending.resize(end);

        for (size_t i = start; i < end; ++i) {
            auto sprite = glyphAtRawIndex(text, i);
            if (!sprite) continue;

            sprite->stopActionByTag(kFadeInTag);
            sprite->setOpacity(0);

            float angle, dist;
            resolvePop(popSettings, angle, dist);

            CCPoint motion(0, 0);
            CCAction* action;
            if (dist > 0.0f) {
                CCPoint finalPos = sprite->getPosition();
                motion = popMotion(angle, dist);
                sprite->setPosition(finalPos - motion);
                action = CCSpawn::create(
                    CCFadeIn::create(dur),
                    CCMoveTo::create(dur, finalPos),
                    nullptr);
            } else {
                action = CCFadeIn::create(dur);
            }
            action->setTag(kFadeInTag);
            sprite->runAction(action);

            m_fields->pending[i] = PendingFade{now, dur, motion};
        }
    }

    void continueFade(size_t idx) {
        if (!activeLabel() || idx >= m_fields->pending.size()) return;
        auto& slot = m_fields->pending[idx];
        if (!slot) return;

        auto sprite = glyphAtRawIndex(m_fields->prevString, idx);
        if (!sprite) { slot = std::nullopt; return; }

        float elapsed = std::chrono::duration<float>(
            std::chrono::steady_clock::now() - slot->startTime).count();
        if (elapsed >= slot->duration) { slot = std::nullopt; return; }

        float remaining = slot->duration - elapsed;
        float ratio = elapsed / slot->duration;
        GLubyte fullOp = activeLabelOpacity();

        sprite->stopActionByTag(kFadeInTag);
        sprite->setOpacity(static_cast<GLubyte>(ratio * fullOp));

        CCAction* action;
        if (slot->motion.x != 0.0f || slot->motion.y != 0.0f) {
            CCPoint finalPos = sprite->getPosition();
            CCPoint offset = slot->motion * (1.0f - ratio);
            sprite->setPosition(finalPos - offset);
            action = CCSpawn::create(
                CCFadeTo::create(remaining, fullOp),
                CCMoveTo::create(remaining, finalPos),
                nullptr);
        } else {
            action = CCFadeTo::create(remaining, fullOp);
        }
        action->setTag(kFadeInTag);
        sprite->runAction(action);
    }

    void fadeOutGhosts(std::vector<Ghost> const& ghosts) {
        auto label = activeLabel();
        if (ghosts.empty() || !label) return;
        auto parent = label->getParent();
        if (!parent) return;

        float lblScaleX = label->getScaleX();
        float lblScaleY = label->getScaleY();
        float lblRot    = label->getRotation();
        int   zOrder    = label->getZOrder();

        float dur = fadeOutDuration();
        auto popSettings = popOutSettings();

        for (auto const& g : ghosts) {
            if (!g.tex) continue;
            auto s = CCSprite::createWithTexture(g.tex, g.rect);
            if (!s) continue;

            s->setPosition(parent->convertToNodeSpace(g.pos));
            s->setAnchorPoint(g.anchor);
            s->setScaleX(g.scaleX * lblScaleX);
            s->setScaleY(g.scaleY * lblScaleY);
            s->setRotation(g.rotation + lblRot);
            s->setColor(g.color);
            s->setOpacity(g.opacity);

            parent->addChild(s, zOrder);
            s->setTag(kGhostTag);

            float angle, dist;
            resolvePop(popSettings, angle, dist);

            CCActionInterval* fade = (dist > 0.0f)
                ? static_cast<CCActionInterval*>(CCSpawn::create(
                    CCFadeOut::create(dur),
                    CCMoveBy::create(dur, popMotion(angle, dist)),
                    nullptr))
                : static_cast<CCActionInterval*>(CCFadeOut::create(dur));
            s->runAction(CCSequence::create(fade, CCRemoveSelf::create(), nullptr));
        }
    }

    void setString(gd::string text) {
        m_fields->externalUpdate = true;
        CCTextInputNode::setString(text);
        m_fields->externalUpdate = false;
    }

    void refreshLabel() {
        if (m_fields->externalUpdate) {
            purgeGhosts();
            plainRefresh();
            return;
        }

        if (!m_selected) {
            plainRefresh();
            return;
        }

        if (auto scene = CCScene::get(); scene && scene->getChildByType<ModSettingsPopup>(0)) {
            purgeGhosts();
            plainRefresh();
            return;
        }

        std::string newStr = m_textField->getString();
        auto& oldStr = m_fields->prevString;

        if (oldStr == newStr) {
            CCTextInputNode::refreshLabel();
            for (size_t i = 0; i < m_fields->pending.size(); ++i) continueFade(i);
            captureLabelSnapshot();
            return;
        }

        auto [oIt, nIt] = std::mismatch(oldStr.begin(), oldStr.end(), newStr.begin(), newStr.end());
        size_t p = static_cast<size_t>(nIt - newStr.begin());

        auto [oRIt, nRIt] = std::mismatch(
            oldStr.rbegin(), oldStr.rend() - p,
            newStr.rbegin(), newStr.rend() - p);
        size_t s = static_cast<size_t>(nRIt - newStr.rbegin());

        size_t newStart = p, newEnd = newStr.size() - s;
        size_t oldStart = p, oldEnd = oldStr.size() - s;
        size_t newLen = newStr.size();

        std::vector<Ghost> ghosts;
        if (oldStart < oldEnd) {
            ghosts.reserve(oldEnd - oldStart);
            for (size_t i = oldStart; i < oldEnd && i < m_fields->prevLabel.size(); ++i)
                ghosts.push_back(m_fields->prevLabel[i]);
        }

        // Remap pending fades from old indices to new indices.
        std::vector<std::optional<PendingFade>> remapped(newLen);
        auto& oldPending = m_fields->pending;
        for (size_t i = 0; i < newStart && i < oldPending.size(); ++i)
            remapped[i] = oldPending[i];
        for (size_t i = newEnd; i < newLen; ++i) {
            size_t oldIdx = (newEnd >= oldEnd)
                ? i - (newEnd - oldEnd)
                : i + (oldEnd - newEnd);
            if (oldIdx < oldPending.size()) remapped[i] = oldPending[oldIdx];
        }
        m_fields->pending = std::move(remapped);

        CCTextInputNode::refreshLabel();
        oldStr = std::move(newStr);

        for (size_t i = 0; i < newStart; ++i) continueFade(i);
        for (size_t i = newEnd; i < newLen; ++i) continueFade(i);

        fadeInRange(newStart, newEnd);
        fadeOutGhosts(ghosts);

        captureLabelSnapshot();
    }
};
