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

#ifndef OUTPUT_OPTIONSWIDGET_H_
#define OUTPUT_OPTIONSWIDGET_H_

#include "ui/ui_OutputOptionsWidget.h"
#include "FilterOptionsWidget.h"
#include "IntrusivePtr.h"
#include "PageId.h"
#include "PageSelectionAccessor.h"
#include "ColorParams.h"
#include "DespeckleLevel.h"
#include "Dpi.h"
#include "ImageViewTab.h"
#include "Params.h"
#include <set>
#include <QMenu>

namespace dewarping
{
class DistortionModel;
}

namespace output
{

class Settings;
class DewarpingParams;

class OptionsWidget
    : public FilterOptionsWidget, private Ui::OutputOptionsWidget
{
    Q_OBJECT
public:
    OptionsWidget(IntrusivePtr<Settings> const& settings,
                  PageSelectionAccessor const& page_selection_accessor);

    virtual ~OptionsWidget();

    void preUpdateUI(PageId const& page_id);

    void postUpdateUI();

    ImageViewTab lastTab() const
    {
        return m_lastTab;
    }

signals:
    void despeckleLevelChanged(DespeckleLevel level, bool* handled);

public slots:
    void tabChanged(ImageViewTab tab);

    void settingsChanged();

    void disablePictureLayer();

    void copyZoneToPagesDlgRequest(void* zone);
    void deleteZoneFromPagesDlgRequest(void* zone);
private slots:

    void modeSelectorIndexChanged(int idx);

    void whiteMarginsToggled(bool checked);

    void equalizeIlluminationToggled(bool checked);

    void despeckleOffSelected();

    void despeckleCautiousSelected();

    void despeckleNormalSelected();

    void despeckleAggressiveSelected();

    void applyDespeckleButtonClicked();

    void applyColorsButtonClicked();

    void on_thresholdOtsuSlider_valueChanged();

    void on_thresholdSauvolaSlider_valueChanged();

    void on_thresholdWolfSlider_valueChanged();

    void on_thresholdGatosSlider_valueChanged();

    void on_thresholdForegroundSlider_valueChanged();

    void thresholdMethodChanged(int idx);

    void thresholdSauvolaWindowSizeChanged(int value);

    void thresholdSauvolaCoefChanged(double value);

    void thresholdWolfWindowSizeChanged(int value);

    void thresholdWolfCoefChanged(double value);

    void thresholdGatosWindowSizeChanged(int value);

    void thresholdGatosCoefChanged(double value);

    void thresholdGatosScaleChanged(double value);

    void dpiValueClicked();

    void applyDpiButtonClicked();

    void on_actionReset_to_default_value_otsu_triggered();

    void on_actionReset_to_default_value_sauvola_triggered();

    void on_actionReset_to_default_value_wolf_triggered();

    void on_actionReset_to_default_value_gatos_triggered();

    void applyThresholdButtonClicked();

    void on_pictureZonesLayerCB_toggled(bool checked);

    void on_foregroundLayerCB_toggled(bool checked);

    void on_autoLayerCB_toggled(bool checked);

    void applyForegroundThresholdButtonClicked();

    void on_actionReset_to_default_value_foreground_triggered();

    void on_actionactionDespeckleOff_triggered();

    void on_actionactionDespeckleCautious_triggered();

    void on_actionactionDespeckleNormal_triggered();

    void on_actionactionDespeckleAggressive_triggered();

private:

    void dpiChanged(std::set<PageId> const& pages, Dpi const& dpi);

    void applyColorsConfirmed(std::set<PageId> const& pages);

    void applyThresholdConfirmed(std::set<PageId> const& pages, std::vector<ThresholdFilter> const& thresholds);
    
    void applyForegroundThresholdConfirmed(std::set<PageId> const& pages);

    void applyDespeckleConfirmed(std::set<PageId> const& pages);

    bool eventFilter(QObject* obj, QEvent* event);

    void handleDespeckleLevelChange(DespeckleLevel level);

    void reloadIfNecessary();

    void updateDpiDisplay();

    void updateColorsDisplay();

    void updateLayersDisplay();

    void updateDespeckleValueText();

    void setDespeckleLevel(DespeckleLevel v)
    {
        m_despeckleLevel = v;
        updateDespeckleValueText();
    }

    void updateShortcuts();

    IntrusivePtr<Settings> m_ptrSettings;
    PageSelectionAccessor m_pageSelectionAccessor;
    PageId m_pageId;
    Dpi m_outputDpi;
    ColorParams m_colorParams;
    DespeckleLevel m_despeckleLevel;
    ImageViewTab m_lastTab;
    int m_ignoreThresholdChanges;
    QMenu m_menuMode;
    ColorParams::ColorMode m_currentMode;
    bool m_ignore_system_wheel_settings;
};

} // namespace output

#endif
