/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "trill.h"

#include <cmath>

#include "types/typesconv.h"

#include "iengravingfont.h"

#include "accidental.h"
#include "chord.h"
#include "factory.h"
#include "ornament.h"
#include "score.h"
#include "system.h"
#include "undo.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   trillStyle
//---------------------------------------------------------

static const ElementStyle trillStyle {
    { Sid::trillPlacement, Pid::PLACEMENT },
    { Sid::trillPosAbove,  Pid::OFFSET },
};

TrillSegment::TrillSegment(Trill* sp, System* parent)
    : LineSegment(ElementType::TRILL_SEGMENT, sp, parent, ElementFlag::MOVABLE | ElementFlag::ON_STAFF)
{
}

TrillSegment::TrillSegment(System* parent)
    : LineSegment(ElementType::TRILL_SEGMENT, parent, ElementFlag::MOVABLE | ElementFlag::ON_STAFF)
{
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void TrillSegment::remove(EngravingItem* e)
{
    if (trill()->accidental() == e) {
        // accidental is part of trill
        trill()->setAccidental(nullptr);
        e->removed();
    }
}

//---------------------------------------------------------
//   symbolLine
//---------------------------------------------------------

void TrillSegment::symbolLine(SymId start, SymId fill)
{
    double x1 = 0.0;
    double x2 = pos2().x();
    double w   = x2 - x1;
    double mag = magS();
    IEngravingFontPtr f = score()->engravingFont();

    m_symbols.clear();
    m_symbols.push_back(start);
    double w1 = f->advance(start, mag);
    double w2 = f->advance(fill, mag);
    int n    = lrint((w - w1) / w2);
    for (int i = 0; i < n; ++i) {
        m_symbols.push_back(fill);
    }
    RectF r(f->bbox(m_symbols, mag));
    setbbox(r);
}

void TrillSegment::symbolLine(SymId start, SymId fill, SymId end)
{
    double x1 = 0.0;
    double x2 = pos2().x();
    double w   = x2 - x1;
    double mag = magS();
    IEngravingFontPtr f = score()->engravingFont();

    m_symbols.clear();
    m_symbols.push_back(start);
    double w1 = f->advance(start, mag);
    double w2 = f->advance(fill, mag);
    double w3 = f->advance(end, mag);
    int n    = lrint((w - w1 - w3) / w2);
    for (int i = 0; i < n; ++i) {
        m_symbols.push_back(fill);
    }
    m_symbols.push_back(end);
    RectF r(f->bbox(m_symbols, mag));
    setbbox(r);
}

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void TrillSegment::scanElements(void* data, void (* func)(void*, EngravingItem*), bool)
{
    func(data, this);
    if (isSingleType() || isBeginType()) {
        Accidental* a = trill()->accidental();
        if (a) {
            func(data, a);
        }
        Chord* cueNoteChord = trill()->cueNoteChord();
        if (cueNoteChord) {
            cueNoteChord->scanElements(data, func);
        }
    }
}

//---------------------------------------------------------
//   propertyDelegate
//---------------------------------------------------------

EngravingItem* TrillSegment::propertyDelegate(Pid pid)
{
    if (pid == Pid::TRILL_TYPE || pid == Pid::ORNAMENT_STYLE || pid == Pid::PLACEMENT || pid == Pid::PLAY) {
        return spanner();
    }
    return LineSegment::propertyDelegate(pid);
}

//---------------------------------------------------------
//   getPropertyStyle
//---------------------------------------------------------

Sid TrillSegment::getPropertyStyle(Pid pid) const
{
    if (pid == Pid::OFFSET) {
        return spanner()->placeAbove() ? Sid::trillPosAbove : Sid::trillPosBelow;
    }
    return LineSegment::getPropertyStyle(pid);
}

Sid Trill::getPropertyStyle(Pid pid) const
{
    if (pid == Pid::OFFSET) {
        return placeAbove() ? Sid::trillPosAbove : Sid::trillPosBelow;
    }
    return SLine::getPropertyStyle(pid);
}

//---------------------------------------------------------
//   Trill
//---------------------------------------------------------

Trill::Trill(EngravingItem* parent)
    : SLine(ElementType::TRILL, parent)
{
    m_trillType     = TrillType::TRILL_LINE;
    m_ornament = nullptr;
    m_accidental = nullptr;
    m_cueNoteChord = nullptr;
    m_ornamentStyle = OrnamentStyle::DEFAULT;
    setPlayArticulation(true);
    initElementStyle(&trillStyle);
}

Trill::Trill(const Trill& t)
    : SLine(t)
{
    m_trillType = t.m_trillType;
    m_ornament = t.m_ornament ? t.m_ornament->clone() : nullptr;
    m_ornamentStyle = t.m_ornamentStyle;
    m_playArticulation = t.m_playArticulation;
    initElementStyle(&trillStyle);
}

EngravingItem* Trill::linkedClone()
{
    Trill* linkedTrill = clone();
    Ornament* linkedOrnament = toOrnament(m_ornament->linkedClone());
    linkedTrill->setOrnament(linkedOrnament);
    linkedTrill->setAutoplace(true);
    score()->undo(new Link(linkedTrill, this));
    return linkedTrill;
}

Trill::~Trill()
{
    delete m_ornament;
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void Trill::remove(EngravingItem* e)
{
    if (e == m_accidental) {
        m_accidental = nullptr;
        e->removed();
    }
}

void Trill::setTrack(track_idx_t n)
{
    EngravingItem::setTrack(n);

    for (SpannerSegment* ss : spannerSegments()) {
        ss->setTrack(n);
    }

    if (m_ornament) {
        m_ornament->setTrack(n);
    }
}

void Trill::setTrillType(TrillType tt)
{
    m_trillType = tt;
    if (!m_ornament) {
        // ornament parent will be explicitely set at layout stage
        m_ornament = Factory::createOrnament((ChordRest*)score()->dummy()->chord());
    }
    m_ornament->setTrack(track());
    m_ornament->setSymId(Ornament::fromTrillType(tt));
}

//---------------------------------------------------------
//   createLineSegment
//---------------------------------------------------------

static const ElementStyle trillSegmentStyle {
    { Sid::trillPosAbove, Pid::OFFSET },
    { Sid::trillMinDistance, Pid::MIN_DISTANCE },
};

LineSegment* Trill::createLineSegment(System* parent)
{
    TrillSegment* seg = new TrillSegment(this, parent);
    seg->setTrack(track());
    seg->setColor(color());
    seg->initElementStyle(&trillSegmentStyle);
    return seg;
}

//---------------------------------------------------------
//   trillTypeName
//---------------------------------------------------------

String Trill::trillTypeUserName() const
{
    return TConv::translatedUserName(trillType());
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Trill::getProperty(Pid propertyId) const
{
    switch (propertyId) {
    case Pid::TRILL_TYPE:
        return int(trillType());
    case Pid::ORNAMENT_STYLE:
        return ornamentStyle();
    case Pid::PLAY:
        return bool(playArticulation());
    default:
        break;
    }
    return SLine::getProperty(propertyId);
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Trill::setProperty(Pid propertyId, const PropertyValue& val)
{
    switch (propertyId) {
    case Pid::TRILL_TYPE:
        setTrillType(TrillType(val.toInt()));
        break;
    case Pid::PLAY:
        setPlayArticulation(val.toBool());
        break;
    case Pid::ORNAMENT_STYLE:
        setOrnamentStyle(val.value<OrnamentStyle>());
        break;
    case Pid::COLOR:
        setColor(val.value<Color>());
        [[fallthrough]];
    default:
        if (!SLine::setProperty(propertyId, val)) {
            return false;
        }
        break;
    }
    triggerLayout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Trill::propertyDefault(Pid propertyId) const
{
    switch (propertyId) {
    case Pid::TRILL_TYPE:
        return 0;
    case Pid::ORNAMENT_STYLE:
        return OrnamentStyle::DEFAULT;
    case Pid::PLAY:
        return true;
    case Pid::PLACEMENT:
        return style().styleV(Sid::trillPlacement);

    default:
        return SLine::propertyDefault(propertyId);
    }
}

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

String Trill::accessibleInfo() const
{
    return String(u"%1: %2").arg(EngravingItem::accessibleInfo(), trillTypeUserName());
}
}
