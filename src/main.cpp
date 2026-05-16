#include "main.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/CCTextInputNode.hpp>
#include <algorithm>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

using namespace geode::prelude;
using namespace smoothtextinput;

class $modify(CharFadeInput, CCTextInputNode) {
    struct Fields {
        std::string prevString;
        std::vector<Ghost> prevLabel;
        std::vector<std::optional<PendingFade>> pending;
        bool externalUpdate = false;
    };

    void captureLabelSnapshot() {
        auto& prev = m_fields->prevLabel;
        prev.clear();
        if (!m_textLabel) return;
        GLubyte fullOp = m_textLabel->getOpacity();
        size_t len = m_fields->prevString.size();
        prev.reserve(len);
        for (size_t i = 0; i < len; ++i) {
            auto sprite = typeinfo_cast<CCSprite*>(
                m_textLabel->getChildByTag(static_cast<int>(i)));
            if (!sprite) {
                prev.push_back({});
                continue;
            }
            prev.push_back({
                sprite->getTexture(),
                sprite->getTextureRect(),
                sprite->getPosition(),
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
        if (!m_textLabel) return;
        auto parent = m_textLabel->getParent();
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

    void fadeInRange(size_t start, size_t end) {
        if (start >= end || !m_textLabel) return;

        float dur = fadeInDuration();
        auto popSettings = popInSettings();
        auto now = std::chrono::steady_clock::now();

        if (m_fields->pending.size() < end) m_fields->pending.resize(end);

        for (size_t i = start; i < end; ++i) {
            auto sprite = typeinfo_cast<CCSprite*>(
                m_textLabel->getChildByTag(static_cast<int>(i)));
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
        if (!m_textLabel || idx >= m_fields->pending.size()) return;
        auto& slot = m_fields->pending[idx];
        if (!slot) return;

        auto sprite = typeinfo_cast<CCSprite*>(
            m_textLabel->getChildByTag(static_cast<int>(idx)));
        if (!sprite) { slot = std::nullopt; return; }

        float elapsed = std::chrono::duration<float>(
            std::chrono::steady_clock::now() - slot->startTime).count();
        if (elapsed >= slot->duration) { slot = std::nullopt; return; }

        float remaining = slot->duration - elapsed;
        float ratio = elapsed / slot->duration;
        GLubyte fullOp = m_textLabel->getOpacity();

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
            s->setPosition(parent->convertToNodeSpace(world));
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
