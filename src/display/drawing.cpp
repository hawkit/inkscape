/**
 * @file
 * @brief SVG drawing for display
 *//*
 * Authors:
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *
 * Copyright (C) 2011 Authors
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#include <algorithm>
#include "display/drawing.h"
#include "nr-filter-gaussian.h"
#include "nr-filter-types.h"

namespace Inkscape {

Drawing::Drawing(SPCanvasArena *arena)
    : _root(NULL)
    , outlinecolor(0x000000ff)
    , delta(0)
    , _exact(false)
    , _rendermode(RENDERMODE_NORMAL)
    , _colormode(COLORMODE_NORMAL)
    , _blur_quality(BLUR_QUALITY_BEST)
    , _filter_quality(Filters::FILTER_QUALITY_BEST)
    , _cache_score_threshold(50000.0)
    , _cache_budget(0)
    , _canvasarena(arena)
{

}

Drawing::~Drawing()
{
    delete _root;
}

void
Drawing::setRoot(DrawingItem *item)
{
    delete _root;
    _root = item;
    if (item) {
        assert(item->_child_type == DrawingItem::CHILD_ORPHAN);
        item->_child_type = DrawingItem::CHILD_ROOT;
    }
}

RenderMode
Drawing::renderMode() const
{
    return _exact ? RENDERMODE_NORMAL : _rendermode;
}
ColorMode
Drawing::colorMode() const
{
    return (outline() || _exact) ? COLORMODE_NORMAL : _colormode;
}
bool
Drawing::outline() const
{
    return renderMode() == RENDERMODE_OUTLINE;
}
bool
Drawing::renderFilters() const
{
    return renderMode() == RENDERMODE_NORMAL;
}
int
Drawing::blurQuality() const
{
    if (renderMode() == RENDERMODE_NORMAL) {
        return _exact ? BLUR_QUALITY_BEST : _blur_quality;
    } else {
        return BLUR_QUALITY_WORST;
    }
}
int
Drawing::filterQuality() const
{
    if (renderMode() == RENDERMODE_NORMAL) {
        return _exact ? Filters::FILTER_QUALITY_BEST : _filter_quality;
    } else {
        return Filters::FILTER_QUALITY_WORST;
    }
}

void
Drawing::setRenderMode(RenderMode mode)
{
    _rendermode = mode;
}
void
Drawing::setColorMode(ColorMode mode)
{
    _colormode = mode;
}
void
Drawing::setBlurQuality(int q)
{
    _blur_quality = q;
}
void
Drawing::setFilterQuality(int q)
{
    _filter_quality = q;
}
void
Drawing::setExact(bool e)
{
    _exact = e;
}

Geom::OptIntRect const &
Drawing::cacheLimit() const
{
    return _cache_limit;
}
void
Drawing::setCacheLimit(Geom::OptIntRect const &r)
{
    _cache_limit = r;
    for (std::set<DrawingItem *>::iterator i = _cached_items.begin();
         i != _cached_items.end(); ++i)
    {
        (*i)->_markForUpdate(DrawingItem::STATE_CACHE, false);
    }
}
void
Drawing::setCacheBudget(size_t bytes)
{
    _cache_budget = bytes;
    _pickItemsForCaching();
}

void
Drawing::update(Geom::IntRect const &area, UpdateContext const &ctx, unsigned flags, unsigned reset)
{
    if (_root) {
        _root->update(area, ctx, flags, reset);
    }
    // process the updated cache scores
    _pickItemsForCaching();
}

void
Drawing::render(DrawingContext &ct, Geom::IntRect const &area, unsigned flags)
{
    if (_root) {
        _root->render(ct, area, flags);
    }
}

DrawingItem *
Drawing::pick(Geom::Point const &p, double delta, unsigned flags)
{
    if (_root) {
        return _root->pick(p, delta, flags);
    }
    return NULL;
}

void
Drawing::_pickItemsForCaching()
{
    // we cache the objects with the highest score until the budget is exhausted
    _candidate_items.sort(std::greater<CacheRecord>());
    size_t used = 0;
    CandidateList::iterator i;
    for (i = _candidate_items.begin(); i != _candidate_items.end(); ++i) {
        if (used + i->cache_size > _cache_budget) break;
        used += i->cache_size;
    }

    std::set<DrawingItem*> to_cache;
    for (i = _candidate_items.begin(); i != _candidate_items.end(); ++i) {
        i->item->setCached(true);
        to_cache.insert(i->item);
    }
    // Everything which is now in _cached_items but not in to_cache must be uncached
    // Note that calling setCached on an item modifies _cached_items
    // TODO: find a way to avoid the set copy
    std::set<DrawingItem*> to_uncache;
    std::set_difference(_cached_items.begin(), _cached_items.end(),
                        to_cache.begin(), to_cache.end(),
                        std::inserter(to_uncache, to_uncache.end()));
    for (std::set<DrawingItem*>::iterator j = to_uncache.begin(); j != to_uncache.end(); ++j) {
        (*j)->setCached(false);
    }
}

} // end namespace Inkscape

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
