/*
    Scan Tailor - Interactive post-processing tool for scanned pages.
    Copyright (C) 2015  Joseph Artsimovich <joseph.artsimovich@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef DEWARPING_DISTORTION_MODEL_H_
#define DEWARPING_DISTORTION_MODEL_H_

#include "Curve.h"

class QDomDocument;
class QDomElement;
class QString;

namespace dewarping
{

class CylindricalSurfaceDewarper;

class DistortionModel
{
public:
    /**
     * \brief Constructs a null distortion model.
     */
    DistortionModel();

    explicit DistortionModel(QDomElement const& el);

    QDomElement toXml(QDomDocument& doc, QString const& name) const;

    /**
     * Returns true if the model is not null and in addition meets certain
     * criteria, like curve endpoints forming a convex quadrilateral.
     */
    bool isValid() const;

    void setTopCurve(Curve const& curve)
    {
        m_topCurve = curve;
    }

    void setBottomCurve(Curve const& curve)
    {
        m_bottomCurve = curve;
    }

    Curve const& topCurve() const
    {
        return m_topCurve;
    }

    Curve const& bottomCurve() const
    {
        return m_bottomCurve;
    }

    bool matches(DistortionModel const& other) const;
private:
    Curve m_topCurve;
    Curve m_bottomCurve;
};

} // namespace dewarping

#endif
