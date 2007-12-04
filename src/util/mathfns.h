/*
 * Inkscape::Util::... some mathmatical functions 
 *
 * Authors:
 *   Johan Engelen <goejendaagh@zonnet.nl>
 *
 * Copyright (C) 2007 Johan Engelen
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifndef SEEN_INKSCAPE_UTIL_MATHFNS_H
#define SEEN_INKSCAPE_UTIL_MATHFNS_H


namespace Inkscape {

namespace Util {

/**
 * Returns area in triangle given by points; may be negative.
 */
inline double
triangle_area (NR::Point p1, NR::Point p2, NR::Point p3)
{
    return (p1[NR::X]*p2[NR::Y] + p1[NR::Y]*p3[NR::X] + p2[NR::X]*p3[NR::Y] - p2[NR::Y]*p3[NR::X] - p1[NR::Y]*p2[NR::X] - p1[NR::X]*p3[NR::Y]);
}

/**
 * \return x rounded to the nearest multiple of c1 plus c0.
 *
 * \note
 * If c1==0 (and c0 is finite), then returns +/-inf.  This makes grid spacing of zero
 * mean "ignore the grid in this dimension".
 */
inline double round_to_nearest_multiple_plus(double x, double const c1, double const c0)
{
    return floor((x - c0) / c1 + .5) * c1 + c0;
}

/**
 * \return x rounded to the lower multiple of c1 plus c0.
 *
 * \note
 * If c1==0 (and c0 is finite), then returns +/-inf.  This makes grid spacing of zero
 * mean "ignore the grid in this dimension".
 */
inline double round_to_lower_multiple_plus(double x, double const c1, double const c0)
{
    return floor((x - c0) / c1) * c1 + c0;
}

/**
 * \return x rounded to the upper multiple of c1 plus c0.
 *
 * \note
 * If c1==0 (and c0 is finite), then returns +/-inf.  This makes grid spacing of zero
 * mean "ignore the grid in this dimension".
 */
inline double round_to_upper_multiple_plus(double x, double const c1, double const c0)
{
    return ceil((x - c0) / c1) * c1 + c0;
}


}

}

#endif
/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:encoding=utf-8:textwidth=99 :
