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

#ifndef DESKEW_TASK_H_
#define DESKEW_TASK_H_

#include "NonCopyable.h"
#include "RefCountable.h"
#include "FilterResult.h"
#include "PageId.h"
#include <memory>

class TaskStatus;
class FilterData;
class DebugImages;
class ThumbnailPixmapCache;

namespace imageproc
{
class BinaryImage;
};

namespace select_content
{
class Task;
}

namespace deskew
{

class Filter;
class Settings;
class Params;

class Task : public RefCountable
{
    DECLARE_NON_COPYABLE(Task)
public:
    Task(IntrusivePtr<Filter> const& filter,
        IntrusivePtr<Settings> const& settings,
        IntrusivePtr<ThumbnailPixmapCache> const& thumbnail_cache,
        IntrusivePtr<select_content::Task> const& next_task,
        PageId const& page_id, bool batch_processing, bool debug);

    virtual ~Task();

    FilterResultPtr process(
        TaskStatus const& status,
        FilterData const& data);
private:
    class NoDistortionUiUpdater;
    class RotationUiUpdater;
    class PerspectiveUiUpdater;
    class DewarpingUiUpdater;

    FilterResultPtr processNoDistortion(
        TaskStatus const& status,
        FilterData const& data,
        Params& params);

    FilterResultPtr processRotationDistortion(
        TaskStatus const& status,
        FilterData const& data,
        Params& params);

    FilterResultPtr processPerspectiveDistortion(
        TaskStatus const& status,
        FilterData const& data,
        Params& params);

    FilterResultPtr processWarpDistortion(
        TaskStatus const& status,
        FilterData const& data,
        Params& params);

    static void cleanup(TaskStatus const& status, imageproc::BinaryImage& img);

    IntrusivePtr<Filter> m_ptrFilter;
    IntrusivePtr<Settings> m_ptrSettings;
    IntrusivePtr<ThumbnailPixmapCache> m_ptrThumbnailCache;
    IntrusivePtr<select_content::Task> m_ptrNextTask;
    std::unique_ptr<DebugImages> m_ptrDbg;
    PageId m_pageId;
    bool m_batchProcessing;
};

} // namespace deskew

#endif
