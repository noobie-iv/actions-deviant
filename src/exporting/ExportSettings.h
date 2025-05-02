/*
    Scan Tailor Deviant - Interactive post-processing tool for scanned
    pages. A fork of Scan Tailor by Joseph Artsimovich.
    Copyright (C) 2020 Alexander Trufanov <trufanovan@gmail.com>

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

#ifndef EXPORTSETTINGS_H
#define EXPORTSETTINGS_H

#include <QString>
#include "ExportModes.h"

namespace exporting {

enum PageGenTweak {
    NoTweaks = 0,
    KeepOriginalColorIllumForeSubscans = 1,
    IgnoreOutputProcessingStage = 2
};

Q_DECLARE_FLAGS(PageGenTweaks, PageGenTweak)

struct ExportSettings {
    ExportModes mode;
    bool default_out_dir;
    QString export_dir_path;
    bool export_to_multipage;
    bool generate_blank_back_subscans;
    bool use_sep_suffix_for_pics;
    PageGenTweaks page_gen_tweaks;
    bool export_selected_pages_only;
};

}

#endif // EXPORTSETTINGS_H
