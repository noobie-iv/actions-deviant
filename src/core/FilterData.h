/*
    Scan Tailor - Interactive post-processing tool for scanned pages.
    Copyright (C)  Joseph Artsimovich <joseph.artsimovich@gmail.com>

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

#ifndef FILTERDATA_H_
#define FILTERDATA_H_

#include "imageproc/BinaryThreshold.h"
#include "imageproc/GrayImage.h"
#include "ImageTransformation.h"
#include <QImage>
#include <QPolygonF>

class FilterData
{
    // Member-wise copying is OK.
public:
    FilterData(QString const & filename, QImage const& image);

    FilterData(QString const& filename, QImage const& image, QPolygonF const& preCropArea);

    FilterData(FilterData const& other, ImageTransformation const& xform);

    imageproc::BinaryThreshold bwThreshold() const
    {
        return m_bwThreshold;
    }

    ImageTransformation const& xform() const
    {
        return m_xform;
    }

    QString origImageFilename() const
    {
        return m_origImageFilename;
    }

    QImage const& origImage() const
    {
        return m_origImage;
    }

    imageproc::GrayImage const& grayImage() const
    {
        return m_grayImage;
    }
private:
    QString m_origImageFilename;
    QImage m_origImage;
    imageproc::GrayImage m_grayImage;
    ImageTransformation m_xform;
    imageproc::BinaryThreshold m_bwThreshold;
};

#endif
