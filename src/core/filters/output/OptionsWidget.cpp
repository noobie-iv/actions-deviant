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

#include "OptionsWidget.h"

#include "ChangeDpiDialog.h"
#include "ApplyToDialog.h"
#include "Settings.h"
#include "Params.h"
#include "DespeckleLevel.h"
#include "ZoneSet.h"
#include "PictureZoneComparator.h"
#include "FillZoneComparator.h"
#include "../../Utils.h"
#include "ScopedIncDec.h"
#include "config.h"
#include "StatusBarProvider.h"
#include "VirtualZoneProperty.h"
#include "ThresholdsApplyWidget.h"
#include <QtGlobal>
#include <QVariant>
#include <QColorDialog>
#include <QToolTip>
#include <QString>
#include <QCursor>
#include <QPoint>
#include <QSize>
#include <Qt>
#include <QDebug>
#include "settings/ini_keys.h"
#include <tiff.h>
#include <QMenu>
#include <QWheelEvent>
#include <QPainter>

namespace output
{

OptionsWidget::OptionsWidget(
    IntrusivePtr<Settings> const& settings,
    PageSelectionAccessor const& page_selection_accessor)
    :   m_ptrSettings(settings),
        m_pageSelectionAccessor(page_selection_accessor),
        m_despeckleLevel(DESPECKLE_NORMAL),
        m_lastTab(TAB_OUTPUT),
        m_ignoreThresholdChanges(0)
{
    setupUi(this);

    thresholdMethodSelector->addItem(tr("Otsu"), OTSU);
    thresholdMethodSelector->addItem(tr("Sauvola"), SAUVOLA);
    thresholdMethodSelector->addItem(tr("Wolf"), WOLF);
    thresholdMethodSelector->addItem(tr("Gatos"), GATOS);

    setDespeckleLevel(DESPECKLE_NORMAL);

    void on_thresholdOtsuSlider_valueChanged();

    void on_thresholdSauvolaSlider_valueChanged();

    void on_thresholdWolfSlider_valueChanged();

    void on_thresholdGatosSlider_valueChanged();

    thresholdOtsuSlider->setToolTip(QString::number(thresholdOtsuSlider->value()));
    thresholdSauvolaSlider->setToolTip(QString::number(thresholdSauvolaSlider->value()));
    thresholdWolfSlider ->setToolTip(QString::number(thresholdWolfSlider->value()));
    thresholdGatosSlider->setToolTip(QString::number(thresholdGatosSlider->value()));

    thresholdOtsuSlider->addAction(actionReset_to_default_value_otsu);
    thresholdSauvolaSlider->addAction(actionReset_to_default_value_sauvola);
    thresholdWolfSlider->addAction(actionReset_to_default_value_wolf);
    thresholdGatosSlider->addAction(actionReset_to_default_value_gatos);

    QSettings _settings;
    bwForegroundOptions->setVisible(_settings.value(_key_output_foreground_layer_control_threshold, _key_output_foreground_layer_control_threshold_def).toBool());
    thresholdForegroundSlider->setToolTip(QString::number(thresholdForegroundSlider->value()));
    thresholdForegroundSlider->addAction(actionReset_to_default_value_foreground);
    m_ignore_system_wheel_settings = _settings.value(_key_mouse_ignore_system_wheel_settings, _key_mouse_ignore_system_wheel_settings_def).toBool();

    thresholdOtsuSlider->installEventFilter(this);
    thresholdSauvolaSlider->installEventFilter(this);
    thresholdWolfSlider->installEventFilter(this);
    thresholdGatosSlider->installEventFilter(this);
    thresholdForegroundSlider->installEventFilter(this);

    updateDpiDisplay();
    updateColorsDisplay();
    updateLayersDisplay();

    connect(
        dpiValue, SIGNAL(clicked(bool)),
        this, SLOT(dpiValueClicked())
    );

    connect(
        applyDpiButton, SIGNAL(clicked(bool)),
        this, SLOT(applyDpiButtonClicked())
    );

    connect(
        modeSelector, SIGNAL(currentIndexChanged(int)),
        this, SLOT(modeSelectorIndexChanged(int))
    );

    connect(
        applyColorsButton, SIGNAL(clicked(bool)),
        this, SLOT(applyColorsButtonClicked())
    );

    connect(
        applyThresholdButton, SIGNAL(clicked(bool)),
        this, SLOT(applyThresholdButtonClicked())
    );

    connect(
        applyForegroundThresholdButton, SIGNAL(clicked(bool)),
        this, SLOT(applyForegroundThresholdButtonClicked())
    );
    
    connect(
        applyDespeckleButton, SIGNAL(clicked(bool)),
        this, SLOT(applyDespeckleButtonClicked())
    );

    connect(
        whiteMarginsCB, SIGNAL(clicked(bool)),
        this, SLOT(whiteMarginsToggled(bool))
    );
    connect(
        equalizeIlluminationCB, SIGNAL(clicked(bool)),
        this, SLOT(equalizeIlluminationToggled(bool))
    );

    connect(
        despeckleOffBtn, SIGNAL(clicked()),
        this, SLOT(despeckleOffSelected())
    );
    connect(
        despeckleCautiousBtn, SIGNAL(clicked()),
        this, SLOT(despeckleCautiousSelected())
    );
    connect(
        despeckleNormalBtn, SIGNAL(clicked()),
        this, SLOT(despeckleNormalSelected())
    );
    connect(
        despeckleAggressiveBtn, SIGNAL(clicked()),
        this, SLOT(despeckleAggressiveSelected())
    );

    connect(
        thresholdMethodSelector, SIGNAL(currentIndexChanged(int)),
        this, SLOT(thresholdMethodChanged(int))
    );

    connect(
        thresholdOtsuSlider, &QSlider::sliderReleased,
        this, &OptionsWidget::on_thresholdOtsuSlider_valueChanged
    );
    connect(
        thresholdSauvolaSlider, &QSlider::sliderReleased,
        this, &OptionsWidget::on_thresholdSauvolaSlider_valueChanged
    );
    connect(
        thresholdWolfSlider, &QSlider::sliderReleased,
        this, &OptionsWidget::on_thresholdWolfSlider_valueChanged
    );
    connect(
        thresholdGatosSlider, &QSlider::sliderReleased,
        this, &OptionsWidget::on_thresholdGatosSlider_valueChanged
    );

    connect(
        thresholdForegroundSlider, &QSlider::sliderReleased,
        this, &OptionsWidget::on_thresholdForegroundSlider_valueChanged
    );

   connect(
        thresholdSauvolaWindowSize, SIGNAL(valueChanged(int)),
        this, SLOT(thresholdSauvolaWindowSizeChanged(int))
    );
    connect(
        thresholdSauvolaCoef, SIGNAL(valueChanged(double)),
        this, SLOT(thresholdSauvolaCoefChanged(double))
    );

    connect(
        thresholdWolfWindowSize, SIGNAL(valueChanged(int)),
        this, SLOT(thresholdWolfWindowSizeChanged(int))
    );
    connect(
        thresholdWolfCoef, SIGNAL(valueChanged(double)),
        this, SLOT(thresholdWolfCoefChanged(double))
    );
    
    connect(
        thresholdGatosWindowSize, SIGNAL(valueChanged(int)),
        this, SLOT(thresholdGatosWindowSizeChanged(int))
    );
    connect(
        thresholdGatosCoef, SIGNAL(valueChanged(double)),
        this, SLOT(thresholdGatosCoefChanged(double))
    );
    connect(
        thresholdGatosScale, SIGNAL(valueChanged(double)),
        this, SLOT(thresholdGatosScaleChanged(double))
    );

    addAction(actionactionDespeckleOff);
    addAction(actionactionDespeckleNormal);
    addAction(actionactionDespeckleCautious);
    addAction(actionactionDespeckleAggressive);
    updateShortcuts();

    settingsChanged();
}

void
OptionsWidget::settingsChanged()
{
    QSettings settings;
    thresholdOtsuSlider->setMinimum(settings.value(_key_output_bin_threshold_min, _key_output_bin_threshold_min_def).toInt());
    thresholdSauvolaSlider->setMinimum(settings.value(_key_output_bin_threshold_min, _key_output_bin_threshold_min_def).toInt());
    thresholdWolfSlider->setMinimum(settings.value(_key_output_bin_threshold_min, _key_output_bin_threshold_min_def).toInt());
    thresholdGatosSlider->setMinimum(settings.value(_key_output_bin_threshold_min, _key_output_bin_threshold_min_def).toInt());

    thresholdOtsuSlider->setMaximum(settings.value(_key_output_bin_threshold_max, _key_output_bin_threshold_max_def).toInt());
    thresholdSauvolaSlider->setMaximum(settings.value(_key_output_bin_threshold_max, _key_output_bin_threshold_max_def).toInt());
    thresholdWolfSlider->setMaximum(settings.value(_key_output_bin_threshold_max, _key_output_bin_threshold_max_def).toInt());
    thresholdGatosSlider->setMaximum(settings.value(_key_output_bin_threshold_max, _key_output_bin_threshold_max_def).toInt());

    switch (m_colorParams.blackWhiteOptions().thresholdMethod())
    {
    case OTSU:
        thresholdLabel->setText(QString::number(thresholdOtsuSlider->value()));
        break;
    case SAUVOLA:
        thresholdLabel->setText(QString::number(thresholdSauvolaSlider->value()));
        break;
    case WOLF:
        thresholdLabel->setText(QString::number(thresholdWolfSlider->value()));
        break;
    case GATOS:
        thresholdLabel->setText(QString::number(thresholdGatosSlider->value()));
        break;
    }

    thresholdForegroundSlider->setMinimum(settings.value(_key_output_bin_threshold_min, _key_output_bin_threshold_min_def).toInt());
    thresholdForegroundSlider->setMaximum(settings.value(_key_output_bin_threshold_max, _key_output_bin_threshold_max_def).toInt());
    thresholdForegroundLabel->setText(QString::number(thresholdForegroundSlider->value()));

    updateShortcuts();
    updateLayersDisplay();
}

void
OptionsWidget::updateShortcuts()
{
    actionactionDespeckleOff->setShortcut(GlobalStaticSettings::createShortcut(DespeckleMode0));
    actionactionDespeckleCautious->setShortcut(GlobalStaticSettings::createShortcut(DespeckleMode1));
    actionactionDespeckleNormal->setShortcut(GlobalStaticSettings::createShortcut(DespeckleMode2));
    actionactionDespeckleAggressive->setShortcut(GlobalStaticSettings::createShortcut(DespeckleMode3));
}

void
OptionsWidget::disablePictureLayer()
{
    pictureZonesLayerCB->setChecked(false);
}

OptionsWidget::~OptionsWidget()
{
}

void
OptionsWidget::preUpdateUI(PageId const& page_id)
{
    Params const params(m_ptrSettings->getParams(page_id));
    m_pageId = page_id;
    m_outputDpi = params.outputDpi();
    m_colorParams = params.colorParams();
    setDespeckleLevel(params.despeckleLevel());
    updateDpiDisplay();
    updateColorsDisplay();
    updateLayersDisplay();
}

void
OptionsWidget::postUpdateUI()
{
}

void
OptionsWidget::tabChanged(ImageViewTab const tab)
{
    m_lastTab = tab;
    updateDpiDisplay();
    updateColorsDisplay();
    updateLayersDisplay();
    reloadIfNecessary();
}

void
OptionsWidget::thresholdMethodChanged(int idx)
{
    ThresholdFilter const method = (ThresholdFilter) thresholdMethodSelector->itemData(idx).toInt();
    BlackWhiteOptions blackWhiteOptions(m_colorParams.blackWhiteOptions());
    blackWhiteOptions.setThresholdMethod(method);
    m_colorParams.setBlackWhiteOptions(blackWhiteOptions);
    m_ptrSettings->setColorParams(m_pageId, m_colorParams);
    emit reloadRequested();
}

void
OptionsWidget::thresholdSauvolaWindowSizeChanged(int value)
{
    BlackWhiteOptions blackWhiteOptions(m_colorParams.blackWhiteOptions());
    blackWhiteOptions.setThresholdSauvolaWindowSize(value);
    m_colorParams.setBlackWhiteOptions(blackWhiteOptions);
    m_ptrSettings->setColorParams(m_pageId, m_colorParams);
    if (blackWhiteOptions.thresholdMethod() != OTSU)
        emit reloadRequested();
}

void
OptionsWidget::thresholdSauvolaCoefChanged(double value)
{
    BlackWhiteOptions blackWhiteOptions(m_colorParams.blackWhiteOptions());
    blackWhiteOptions.setThresholdSauvolaCoef(value);
    m_colorParams.setBlackWhiteOptions(blackWhiteOptions);
    m_ptrSettings->setColorParams(m_pageId, m_colorParams);
    if (blackWhiteOptions.thresholdMethod() != OTSU)
        emit reloadRequested();
}

void
OptionsWidget::thresholdWolfWindowSizeChanged(int value)
{
    BlackWhiteOptions blackWhiteOptions(m_colorParams.blackWhiteOptions());
    blackWhiteOptions.setThresholdWolfWindowSize(value);
    m_colorParams.setBlackWhiteOptions(blackWhiteOptions);
    m_ptrSettings->setColorParams(m_pageId, m_colorParams);
    if (blackWhiteOptions.thresholdMethod() != OTSU)
        emit reloadRequested();
}

void
OptionsWidget::thresholdWolfCoefChanged(double value)
{
    BlackWhiteOptions blackWhiteOptions(m_colorParams.blackWhiteOptions());
    blackWhiteOptions.setThresholdWolfCoef(value);
    m_colorParams.setBlackWhiteOptions(blackWhiteOptions);
    m_ptrSettings->setColorParams(m_pageId, m_colorParams);
    if (blackWhiteOptions.thresholdMethod() != OTSU)
        emit reloadRequested();
}

void
OptionsWidget::thresholdGatosWindowSizeChanged(int value)
{
    BlackWhiteOptions blackWhiteOptions(m_colorParams.blackWhiteOptions());
    blackWhiteOptions.setThresholdGatosWindowSize(value);
    m_colorParams.setBlackWhiteOptions(blackWhiteOptions);
    m_ptrSettings->setColorParams(m_pageId, m_colorParams);
    if (blackWhiteOptions.thresholdMethod() != OTSU)
        emit reloadRequested();
}

void
OptionsWidget::thresholdGatosCoefChanged(double value)
{
    BlackWhiteOptions blackWhiteOptions(m_colorParams.blackWhiteOptions());
    blackWhiteOptions.setThresholdGatosCoef(value);
    m_colorParams.setBlackWhiteOptions(blackWhiteOptions);
    m_ptrSettings->setColorParams(m_pageId, m_colorParams);
    if (blackWhiteOptions.thresholdMethod() != OTSU)
        emit reloadRequested();
}

void
OptionsWidget::thresholdGatosScaleChanged(double value)
{
    BlackWhiteOptions blackWhiteOptions(m_colorParams.blackWhiteOptions());
    blackWhiteOptions.setThresholdGatosScale(value);
    m_colorParams.setBlackWhiteOptions(blackWhiteOptions);
    m_ptrSettings->setColorParams(m_pageId, m_colorParams);
    if (blackWhiteOptions.thresholdMethod() != OTSU)
        emit reloadRequested();
}

void
OptionsWidget::modeSelectorIndexChanged(int idx)
{
    m_currentMode = static_cast<ColorParams::ColorMode>(idx);
    m_colorParams.setColorMode(static_cast<ColorParams::ColorMode>(idx));

    ColorGrayscaleOptions opt = m_colorParams.colorGrayscaleOptions();
    if (opt.foregroundLayerEnabled()) {
        opt.setForegroundLayerEnabled(false);
        m_colorParams.setColorGrayscaleOptions(opt);
    }

    m_ptrSettings->setColorParams(m_pageId, m_colorParams, ColorParamsApplyFilter::CopyMode);
    autoLayerCB->setChecked(true);
    pictureZonesLayerCB->setChecked(false);
    foregroundLayerCB->setChecked(false);
    updateColorsDisplay();
    updateLayersDisplay();
    emit reloadRequested();
}

void
OptionsWidget::whiteMarginsToggled(bool const checked)
{
    ColorGrayscaleOptions opt(m_colorParams.colorGrayscaleOptions());
    opt.setWhiteMargins(checked);
    if (!checked) {
        opt.setNormalizeIllumination(false);
        equalizeIlluminationCB->setChecked(false);
    }
    m_colorParams.setColorGrayscaleOptions(opt);
    m_ptrSettings->setColorParams(m_pageId, m_colorParams, ColorParamsApplyFilter::CopyMode);
    equalizeIlluminationCB->setEnabled(checked);
    emit reloadRequested();
}

void
OptionsWidget::equalizeIlluminationToggled(bool const checked)
{
    ColorGrayscaleOptions opt(m_colorParams.colorGrayscaleOptions());
    opt.setNormalizeIllumination(checked);
    m_colorParams.setColorGrayscaleOptions(opt);
    m_ptrSettings->setColorParams(m_pageId, m_colorParams, ColorParamsApplyFilter::CopyMode);
    emit reloadRequested();
}

void
OptionsWidget::dpiChanged(std::set<PageId> const& pages, Dpi const& dpi)
{
    for (PageId const& page_id : pages) {
        m_ptrSettings->setDpi(page_id, dpi);
    }
    emit invalidateAllThumbnails();
}

void
OptionsWidget::applyColorsConfirmed(std::set<PageId> const& pages)
{
    for (PageId const& page_id : pages) {
        m_ptrSettings->setColorParams(page_id, m_colorParams, ColorParamsApplyFilter::CopyMode);
    }

    emit invalidateAllThumbnails();
}

void
OptionsWidget::despeckleOffSelected()
{
    handleDespeckleLevelChange(DESPECKLE_OFF);
}

void
OptionsWidget::despeckleCautiousSelected()
{
    handleDespeckleLevelChange(DESPECKLE_CAUTIOUS);
}

void
OptionsWidget::despeckleNormalSelected()
{
    handleDespeckleLevelChange(DESPECKLE_NORMAL);
}

void
OptionsWidget::despeckleAggressiveSelected()
{
    handleDespeckleLevelChange(DESPECKLE_AGGRESSIVE);
}

void
OptionsWidget::handleDespeckleLevelChange(DespeckleLevel const level)
{
    setDespeckleLevel(level);
    m_ptrSettings->setDespeckleLevel(m_pageId, level);

    bool handled = false;
    emit despeckleLevelChanged(level, &handled);

    if (handled) {
        // This means we are on the "Despeckling" tab.
        emit invalidateThumbnail(m_pageId);
    } else {
        emit reloadRequested();
    }
}

void
OptionsWidget::applyDespeckleConfirmed(std::set<PageId> const& pages)
{
    for (PageId const& page_id : pages) {
        m_ptrSettings->setDespeckleLevel(page_id, m_despeckleLevel);
    }
    emit invalidateAllThumbnails();
}

void
OptionsWidget::reloadIfNecessary()
{
    ZoneSet saved_picture_zones;
    ZoneSet saved_fill_zones;
    DespeckleLevel saved_despeckle_level = DESPECKLE_CAUTIOUS;

    std::unique_ptr<OutputParams> output_params(m_ptrSettings->getOutputParams(m_pageId));
    if (output_params.get()) {
        saved_picture_zones = output_params->pictureZones();
        saved_fill_zones = output_params->fillZones();
        saved_despeckle_level = output_params->outputImageParams().despeckleLevel();
    }

    if (!PictureZoneComparator::equal(saved_picture_zones, m_ptrSettings->pictureZonesForPage(m_pageId))) {
        emit reloadRequested();
        return;
    } else if (!FillZoneComparator::equal(saved_fill_zones, m_ptrSettings->fillZonesForPage(m_pageId))) {
        emit reloadRequested();
        return;
    }

    Params const params(m_ptrSettings->getParams(m_pageId));

    if (saved_despeckle_level != params.despeckleLevel()) {
        emit reloadRequested();
        return;
    }
}

void
OptionsWidget::updateDpiDisplay()
{
    if (m_outputDpi.horizontal() != m_outputDpi.vertical()) {
        QString dpi_label = tr("%1 x %2 dpi")
                            .arg(m_outputDpi.horizontal())
                            .arg(m_outputDpi.vertical());
        dpiValue->setText(dpi_label);
    } else {
        QString dpi_label = tr("%1 dpi").arg(QString::number(m_outputDpi.horizontal()));
        dpiValue->setText(dpi_label);
    }

    StatusBarProvider::setSettingsDPi(m_outputDpi);
}

void
OptionsWidget::updateLayersDisplay()
{
    QSettings settings;

    autoLayerCB->setEnabled(true);
    bool isChecked = m_colorParams.colorGrayscaleOptions().autoLayerEnabled();
    if (isChecked == autoLayerCB->isChecked()) {
        on_autoLayerCB_toggled(autoLayerCB->isChecked());
    } else {
        autoLayerCB->setChecked(isChecked);
    }

    bool isVisible = settings.value(_key_output_picture_layer_enabled, _key_output_picture_layer_enabled_def).toBool();
    isChecked = m_colorParams.colorGrayscaleOptions().pictureZonesLayerEnabled();
    pictureZonesLayerCB->setVisible(isVisible);
    if ((isVisible && isChecked) == pictureZonesLayerCB->isChecked()) {
        on_pictureZonesLayerCB_toggled(pictureZonesLayerCB->isChecked());
    } else {
        pictureZonesLayerCB->setChecked(isVisible && isChecked);
    }

    isVisible = settings.value(_key_output_foreground_layer_enabled, _key_output_foreground_layer_enabled_def).toBool();
    isChecked = m_colorParams.colorGrayscaleOptions().foregroundLayerEnabled();
    foregroundLayerCB->setVisible(isVisible);

    if ((isVisible && isChecked) == foregroundLayerCB->isChecked()) {
        on_foregroundLayerCB_toggled(foregroundLayerCB->isChecked());
    } else {
        foregroundLayerCB->setChecked(isVisible && isChecked);
    }
}

void
OptionsWidget::updateColorsDisplay()
{
    m_currentMode = m_colorParams.colorMode();
    modeSelector->setCurrentIndex(m_currentMode);

    bool color_grayscale_options_visible = false;
    bool bw_options_visible = false;
    bool foreground_treshhold_options_visible = false;
    bool despeckle_controls_enbled = true;

    switch (m_currentMode) {
    case ColorParams::BLACK_AND_WHITE:
        bw_options_visible = true;
        break;
    case ColorParams::COLOR_GRAYSCALE:
        color_grayscale_options_visible = true;
        despeckle_controls_enbled = false;
        break;
    case ColorParams::MIXED:
        bw_options_visible = true;
        color_grayscale_options_visible = true;
        foreground_treshhold_options_visible = QSettings().value(_key_output_foreground_layer_control_threshold, _key_output_foreground_layer_control_threshold_def).toBool();
        break;
    }

    illuminationPanel->setVisible(color_grayscale_options_visible);
    if (color_grayscale_options_visible) {
        ColorGrayscaleOptions const opt(
            m_colorParams.colorGrayscaleOptions()
        );
        whiteMarginsCB->setChecked(opt.whiteMargins());
        whiteMarginsCB->setEnabled(m_currentMode != ColorParams::MIXED); // Mixed must have margins
        equalizeIlluminationCB->setChecked(opt.normalizeIllumination());
        equalizeIlluminationCB->setEnabled(opt.whiteMargins());
    }

    layersPanel->setVisible(m_currentMode == ColorParams::MIXED);
    bwOptions->setVisible(bw_options_visible);
    despecklingPanel->setVisible(despeckle_controls_enbled);

    if (bw_options_visible) {
        ScopedIncDec<int> const guard(m_ignoreThresholdChanges);

        BlackWhiteOptions blackWhiteOptions(m_colorParams.blackWhiteOptions());
        thresholdMethodSelector->setCurrentIndex((int) blackWhiteOptions.thresholdMethod());
        switch (blackWhiteOptions.thresholdMethod())
        {
        case OTSU:
            bwOtsuOptionsPanel->setVisible(true);
            bwSauvolaOptionsPanel->setVisible(false);
            bwWolfOptionsPanel->setVisible(false);
            bwGatosOptionsPanel->setVisible(false);

            thresholdLabel->setNum(thresholdOtsuSlider->value());
            thresholdOtsuSlider->setValue(blackWhiteOptions.thresholdOtsuAdjustment());

            break;
        case SAUVOLA:
            bwOtsuOptionsPanel->setVisible(false);
            bwSauvolaOptionsPanel->setVisible(true);
            bwWolfOptionsPanel->setVisible(false);
            bwGatosOptionsPanel->setVisible(false);

            thresholdLabel->setNum(thresholdSauvolaSlider->value());
            thresholdSauvolaSlider->setValue(blackWhiteOptions.thresholdSauvolaAdjustment());
            thresholdSauvolaWindowSize->setValue(blackWhiteOptions.thresholdSauvolaWindowSize());
            thresholdSauvolaCoef->setValue(blackWhiteOptions.thresholdSauvolaCoef());

            break;
        case WOLF:
            bwOtsuOptionsPanel->setVisible(false);
            bwSauvolaOptionsPanel->setVisible(false);
            bwWolfOptionsPanel->setVisible(true);
            bwGatosOptionsPanel->setVisible(false);

            thresholdLabel->setNum(thresholdWolfSlider->value());
            thresholdWolfSlider->setValue(blackWhiteOptions.thresholdWolfAdjustment());
            thresholdWolfWindowSize->setValue(blackWhiteOptions.thresholdWolfWindowSize());
            thresholdWolfCoef->setValue(blackWhiteOptions.thresholdWolfCoef());

            break;
        case GATOS:
            bwOtsuOptionsPanel->setVisible(false);
            bwSauvolaOptionsPanel->setVisible(false);
            bwWolfOptionsPanel->setVisible(false);
            bwGatosOptionsPanel->setVisible(true);

            thresholdLabel->setNum(thresholdGatosSlider->value());
            thresholdGatosSlider->setValue(blackWhiteOptions.thresholdGatosAdjustment());
            thresholdGatosWindowSize->setValue(blackWhiteOptions.thresholdGatosWindowSize());
            thresholdGatosCoef->setValue(blackWhiteOptions.thresholdGatosCoef());
            thresholdGatosScale->setValue(blackWhiteOptions.thresholdGatosScale());

            break;
        }
    }

    if (despeckle_controls_enbled) {
        switch (m_despeckleLevel) {
        case DESPECKLE_OFF:
            despeckleOffBtn->setChecked(true);
            break;
        case DESPECKLE_CAUTIOUS:
            despeckleCautiousBtn->setChecked(true);
            break;
        case DESPECKLE_NORMAL:
            despeckleNormalBtn->setChecked(true);
            break;
        case DESPECKLE_AGGRESSIVE:
            despeckleAggressiveBtn->setChecked(true);
            break;
        }
    } else {
        despeckleOffBtn->setChecked(true);
    }

    bwForegroundOptions->setVisible(foreground_treshhold_options_visible);
    if (foreground_treshhold_options_visible) {
        thresholdForegroundSlider->setValue(m_colorParams.blackWhiteOptions().thresholdForegroundAdjustment());
    }

}

void
OptionsWidget::updateDespeckleValueText()
{
    switch (m_despeckleLevel) {
    case DESPECKLE_OFF: despeckleValue->setText(tr("Off")); break;
    case DESPECKLE_CAUTIOUS: despeckleValue->setText(tr("Cautious")); break;
    case DESPECKLE_NORMAL: despeckleValue->setText(tr("Normal")); break;
    case DESPECKLE_AGGRESSIVE: despeckleValue->setText(tr("Aggressive")); break;
    default: ;
    }
}

} // namespace output

void output::OptionsWidget::applyDespeckleButtonClicked()
{
    ApplyToDialog* dialog = new ApplyToDialog(this, m_pageId, m_pageSelectionAccessor);
    dialog->setWindowTitle(tr("Apply Despeckling Level"));
    connect(
        dialog, &ApplyToDialog::accepted,
        this, [ = ]() {
            std::vector<PageId> vec = dialog->getPageRangeSelectorWidget().result();
            std::set<PageId> pages;
            std::copy_if(
                vec.begin(),
                vec.end(),
                std::inserter(pages, pages.end()),
                [this](PageId const& page_id)
                {
                    return this->m_pageId != page_id;
                }
            );
            applyDespeckleConfirmed(pages);
        }
    );

    dialog->show();
}

void output::OptionsWidget::applyColorsButtonClicked()
{
    ApplyToDialog* dialog = new ApplyToDialog(this, m_pageId, m_pageSelectionAccessor);
    dialog->setWindowTitle(tr("Apply Mode"));
    connect(
        dialog, &ApplyToDialog::accepted,
        this, [ = ]() {
            std::vector<PageId> vec = dialog->getPageRangeSelectorWidget().result();
            std::set<PageId> pages;
            std::copy_if(
                vec.begin(),
                vec.end(),
                std::inserter(pages, pages.end()),
                [this](PageId const& page_id)
                {
                    return this->m_pageId != page_id;
                }
            );
            applyColorsConfirmed(pages);
        }
    );

    dialog->show();
}

int sum_y = 0;

bool output::OptionsWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (!(obj && event
            && (QString(obj->metaObject()->className()) == "QSlider"))) {
        return false;
    }

    if (m_ignore_system_wheel_settings && event->type() == QEvent::Wheel) {
        QWheelEvent* e = (QWheelEvent*) event;
        if (e->modifiers() == Qt::NoModifier) {
            const QPoint& angleDelta = e->angleDelta();
            if (!angleDelta.isNull()) {
                sum_y += angleDelta.y();
                if (abs(sum_y) >= 30) {
                    QSlider* slider = (QSlider*) obj;
                    int dy = (sum_y > 0) ? slider->singleStep() : -1 * slider->singleStep();
                    slider->setValue(slider->value() + dy);
                    sum_y = 0;
                    e->accept();
                    return true;
                }
            }
        }
    } else if (event->type() == QEvent::Paint) {
        QSlider* slider = (QSlider*) obj;
        if (slider->minimum() <= 0 && slider->maximum() >= 0) {
            int position = QStyle::sliderPositionFromValue(slider->minimum(),
                           slider->maximum(),
                           0,
                           slider->width());
            QPainter painter(slider);
            QPen p(painter.pen());
            p.setColor(QColor(Qt::blue));
            p.setWidth(3);
            painter.setPen(p);
            //        painter.drawText(QPointF(position-5, 0, position+5, slider->height()/2), "0");
            painter.drawLine(position, 0, position, slider->height() / 2 - 6);
        }
    }

    return false;
}

void output::OptionsWidget::on_thresholdOtsuSlider_valueChanged()
{
    int value = thresholdOtsuSlider->value();
    QString const tooltip_text(QString::number(value));
    thresholdOtsuSlider->setToolTip(tooltip_text);

    thresholdLabel->setNum(value);

    if (m_ignoreThresholdChanges) {
        return;
    }

    // Show the tooltip immediately.
    QPoint const center(thresholdOtsuSlider->rect().center());
    QPoint tooltip_pos(thresholdOtsuSlider->mapFromGlobal(QCursor::pos()));
    tooltip_pos.setY(center.y());
    tooltip_pos.setX(qBound(0, tooltip_pos.x(), thresholdOtsuSlider->width()));
    tooltip_pos = thresholdOtsuSlider->mapToGlobal(tooltip_pos);
    QToolTip::showText(tooltip_pos, tooltip_text, thresholdOtsuSlider);

    if (thresholdOtsuSlider->isSliderDown()) {
        // Wait for it to be released.
        // We could have just disabled tracking, but in that case we wouldn't
        // be able to show tooltips with a precise value.
        return;
    }

    BlackWhiteOptions opt(m_colorParams.blackWhiteOptions());
    if (opt.thresholdOtsuAdjustment() == value) {
        // Didn't change.
        return;
    }

    opt.setThresholdOtsuAdjustment(value);
    if (!bwForegroundOptions->isVisible()) {
        opt.setThresholdForegroundAdjustment(value);
        m_colorParams.setBlackWhiteOptions(opt);
        m_ptrSettings->setColorParams(m_pageId, m_colorParams, ColorParamsApplyFilter::CopyAllThresholds);
    } else {
        m_colorParams.setBlackWhiteOptions(opt);
        m_ptrSettings->setColorParams(m_pageId, m_colorParams, ColorParamsApplyFilter::CopyThreshold);
    }

    emit reloadRequested();
    emit invalidateThumbnail(m_pageId);
}

void output::OptionsWidget::on_thresholdSauvolaSlider_valueChanged()
{
    int value = thresholdSauvolaSlider->value();
    QString const tooltip_text(QString::number(value));
    thresholdSauvolaSlider->setToolTip(tooltip_text);

    thresholdLabel->setNum(value);

    if (m_ignoreThresholdChanges) {
        return;
    }

    // Show the tooltip immediately.
    QPoint const center(thresholdSauvolaSlider->rect().center());
    QPoint tooltip_pos(thresholdSauvolaSlider->mapFromGlobal(QCursor::pos()));
    tooltip_pos.setY(center.y());
    tooltip_pos.setX(qBound(0, tooltip_pos.x(), thresholdSauvolaSlider->width()));
    tooltip_pos = thresholdSauvolaSlider->mapToGlobal(tooltip_pos);
    QToolTip::showText(tooltip_pos, tooltip_text, thresholdSauvolaSlider);

    if (thresholdSauvolaSlider->isSliderDown()) {
        // Wait for it to be released.
        // We could have just disabled tracking, but in that case we wouldn't
        // be able to show tooltips with a precise value.
        return;
    }

    BlackWhiteOptions opt(m_colorParams.blackWhiteOptions());
    if (opt.thresholdSauvolaAdjustment() == value) {
        // Didn't change.
        return;
    }

    opt.setThresholdSauvolaAdjustment(value);
    if (!bwForegroundOptions->isVisible()) {
        opt.setThresholdForegroundAdjustment(value);
        m_colorParams.setBlackWhiteOptions(opt);
        m_ptrSettings->setColorParams(m_pageId, m_colorParams, ColorParamsApplyFilter::CopyAllThresholds);
    }
    else {
        m_colorParams.setBlackWhiteOptions(opt);
        m_ptrSettings->setColorParams(m_pageId, m_colorParams, ColorParamsApplyFilter::CopyThreshold);
    }

    emit reloadRequested();
    emit invalidateThumbnail(m_pageId);
}

void output::OptionsWidget::on_thresholdWolfSlider_valueChanged()
{
    int value = thresholdWolfSlider->value();
    QString const tooltip_text(QString::number(value));
    thresholdWolfSlider->setToolTip(tooltip_text);

    thresholdLabel->setNum(value);

    if (m_ignoreThresholdChanges) {
        return;
    }

    // Show the tooltip immediately.
    QPoint const center(thresholdWolfSlider->rect().center());
    QPoint tooltip_pos(thresholdWolfSlider->mapFromGlobal(QCursor::pos()));
    tooltip_pos.setY(center.y());
    tooltip_pos.setX(qBound(0, tooltip_pos.x(), thresholdWolfSlider->width()));
    tooltip_pos = thresholdWolfSlider->mapToGlobal(tooltip_pos);
    QToolTip::showText(tooltip_pos, tooltip_text, thresholdWolfSlider);

    if (thresholdWolfSlider->isSliderDown()) {
        // Wait for it to be released.
        // We could have just disabled tracking, but in that case we wouldn't
        // be able to show tooltips with a precise value.
        return;
    }

    BlackWhiteOptions opt(m_colorParams.blackWhiteOptions());
    if (opt.thresholdWolfAdjustment() == value) {
        // Didn't change.
        return;
    }

    opt.setThresholdWolfAdjustment(value);
    if (!bwForegroundOptions->isVisible()) {
        opt.setThresholdForegroundAdjustment(value);
        m_colorParams.setBlackWhiteOptions(opt);
        m_ptrSettings->setColorParams(m_pageId, m_colorParams, ColorParamsApplyFilter::CopyAllThresholds);
    }
    else {
        m_colorParams.setBlackWhiteOptions(opt);
        m_ptrSettings->setColorParams(m_pageId, m_colorParams, ColorParamsApplyFilter::CopyThreshold);
    }

    emit reloadRequested();
    emit invalidateThumbnail(m_pageId);
}

void output::OptionsWidget::on_thresholdGatosSlider_valueChanged()
{
    int value = thresholdGatosSlider->value();
    QString const tooltip_text(QString::number(value));
    thresholdGatosSlider->setToolTip(tooltip_text);

    thresholdLabel->setNum(value);

    if (m_ignoreThresholdChanges) {
        return;
    }

    // Show the tooltip immediately.
    QPoint const center(thresholdGatosSlider->rect().center());
    QPoint tooltip_pos(thresholdGatosSlider->mapFromGlobal(QCursor::pos()));
    tooltip_pos.setY(center.y());
    tooltip_pos.setX(qBound(0, tooltip_pos.x(), thresholdGatosSlider->width()));
    tooltip_pos = thresholdGatosSlider->mapToGlobal(tooltip_pos);
    QToolTip::showText(tooltip_pos, tooltip_text, thresholdGatosSlider);

    if (thresholdGatosSlider->isSliderDown()) {
        // Wait for it to be released.
        // We could have just disabled tracking, but in that case we wouldn't
        // be able to show tooltips with a precise value.
        return;
    }

    BlackWhiteOptions opt(m_colorParams.blackWhiteOptions());
    if (opt.thresholdGatosAdjustment() == value) {
        // Didn't change.
        return;
    }

    opt.setThresholdGatosAdjustment(value);
    if (!bwForegroundOptions->isVisible()) {
        opt.setThresholdForegroundAdjustment(value);
        m_colorParams.setBlackWhiteOptions(opt);
        m_ptrSettings->setColorParams(m_pageId, m_colorParams, ColorParamsApplyFilter::CopyAllThresholds);
    }
    else {
        m_colorParams.setBlackWhiteOptions(opt);
        m_ptrSettings->setColorParams(m_pageId, m_colorParams, ColorParamsApplyFilter::CopyThreshold);
    }

    emit reloadRequested();
    emit invalidateThumbnail(m_pageId);
}

void output::OptionsWidget::on_thresholdForegroundSlider_valueChanged()
{
    if (m_ignoreThresholdChanges) {
        return;
    }

    int value = thresholdForegroundSlider->value();

    QString const tooltip_text(QString::number(value));
    thresholdForegroundSlider->setToolTip(tooltip_text);
    thresholdForegroundLabel->setNum(value);

    // Show the tooltip immediately.
    QPoint const center(thresholdForegroundSlider->rect().center());
    QPoint tooltip_pos(thresholdForegroundSlider->mapFromGlobal(QCursor::pos()));
    tooltip_pos.setY(center.y());
    tooltip_pos.setX(qBound(0, tooltip_pos.x(), thresholdForegroundSlider->width()));
    tooltip_pos = thresholdForegroundSlider->mapToGlobal(tooltip_pos);
    QToolTip::showText(tooltip_pos, tooltip_text, thresholdForegroundSlider);

    if (thresholdForegroundSlider->isSliderDown()) {
        // Wait for it to be released.
        // We could have just disabled tracking, but in that case we wouldn't
        // be able to show tooltips with a precise value.
        return;
    }

    BlackWhiteOptions opt(m_colorParams.blackWhiteOptions());
    if (opt.thresholdForegroundAdjustment() == value) {
        // Didn't change.
        return;
    }

    opt.setThresholdForegroundAdjustment(value);
    m_colorParams.setBlackWhiteOptions(opt);
    m_ptrSettings->setColorParams(m_pageId, m_colorParams, ColorParamsApplyFilter::CopyForegroundThreshold);

    emit reloadRequested();
    emit invalidateThumbnail(m_pageId);
}

void output::OptionsWidget::dpiValueClicked()
{
    ChangeDpiDialog* dialog = new ChangeDpiDialog(this, m_outputDpi);
    connect(dialog, &ApplyToDialog::accepted, this, [=]()
        {
            const int dpi = dialog->dpi();
            m_outputDpi = Dpi(dpi, dpi);
            m_ptrSettings->setDpi(m_pageId, m_outputDpi);

            updateDpiDisplay();

            emit reloadRequested();
            emit invalidateAllThumbnails();
        }
    );
    dialog->show();
}

void
output::OptionsWidget::applyDpiButtonClicked()
{
    ApplyToDialog* dialog = new ApplyToDialog(this, m_pageId, m_pageSelectionAccessor);
    dialog->setWindowTitle(tr("Apply Output Resolution"));
    connect(
        dialog, &ApplyToDialog::accepted,
        this, [=]() {
            std::vector<PageId> vec = dialog->getPageRangeSelectorWidget().result();
            std::set<PageId> pages;
            std::copy_if(
                vec.begin(),
                vec.end(),
                std::inserter(pages, pages.end()),
                [this](PageId const& page_id)
                {
                    return this->m_pageId != page_id;
                }
            );
            dpiChanged(pages, m_outputDpi);
        }
    );

    dialog->show();
}

void output::OptionsWidget::on_actionReset_to_default_value_otsu_triggered()
{
    int def = QSettings().value(_key_output_bin_threshold_default, _key_output_bin_threshold_default_def).toInt();
    thresholdOtsuSlider->setValue(def);
}

void output::OptionsWidget::on_actionReset_to_default_value_sauvola_triggered()
{
    int def = QSettings().value(_key_output_bin_threshold_default, _key_output_bin_threshold_default_def).toInt();
    thresholdSauvolaSlider->setValue(def);
}

void output::OptionsWidget::on_actionReset_to_default_value_wolf_triggered()
{
    int def = QSettings().value(_key_output_bin_threshold_default, _key_output_bin_threshold_default_def).toInt();
    thresholdWolfSlider->setValue(def);
}

void output::OptionsWidget::on_actionReset_to_default_value_gatos_triggered()
{
    int def = QSettings().value(_key_output_bin_threshold_default, _key_output_bin_threshold_default_def).toInt();
    thresholdGatosSlider->setValue(def);
}

void output::OptionsWidget::applyThresholdConfirmed(std::set<PageId> const& pages, std::vector<ThresholdFilter> const& thresholds)
{
    bool const set_foreground = !bwForegroundOptions->isVisible();

    for (PageId const& page_id : pages) {
        m_ptrSettings->setColorParams(page_id, m_colorParams, thresholds, set_foreground);
    }

    emit invalidateAllThumbnails();
}

void output::OptionsWidget::applyForegroundThresholdConfirmed(std::set<PageId> const& pages)
{
    for (PageId const& page_id : pages) {
        m_ptrSettings->setColorParams(page_id, m_colorParams, ColorParamsApplyFilter::CopyForegroundThreshold);
    }

    emit invalidateAllThumbnails();
}

void output::OptionsWidget::applyThresholdButtonClicked()
{
    ApplyToDialog* dialog = new ApplyToDialog(this, m_pageId, m_pageSelectionAccessor);
    ThresholdsWidget* options = new ThresholdsWidget(dialog, m_colorParams.blackWhiteOptions().thresholdMethod());
    QLayout& layout = dialog->initNewTopSettingsPanel();
    layout.addWidget(options);
    dialog->setWindowTitle(tr("Apply Threshold"));
    connect(
        dialog, &ApplyToDialog::accepted,
        this, [ = ]() {
            std::vector<PageId> vec = dialog->getPageRangeSelectorWidget().result();
            std::set<PageId> pages;
            std::copy_if(
                vec.begin(),
                vec.end(),
                std::inserter(pages, pages.end()),
                [this](PageId const& page_id)
                {
                    return this->m_pageId != page_id;
                }
            );
            applyThresholdConfirmed(pages, options->thresholdsChecked());
        }
    );

    dialog->show();
}

void output::OptionsWidget::on_pictureZonesLayerCB_toggled(bool checked)
{
    ColorGrayscaleOptions opt = m_colorParams.colorGrayscaleOptions();
    if (opt.pictureZonesLayerEnabled() != checked) {
        opt.setPictureZonesLayerEnabled(checked);
        m_colorParams.setColorGrayscaleOptions(opt);

        bool need_reload = true;
        if (!checked) {
            ZoneSet zones = m_ptrSettings->pictureZonesForPage(m_pageId);
            need_reload = zones.auto_zones_found();
            if (need_reload) {
                zones.remove_auto_zones();
                m_ptrSettings->setPictureZones(m_pageId, zones);
            }
        }

        m_ptrSettings->setColorParams(m_pageId, m_colorParams, ColorParamsApplyFilter::CopyMode);

        if (need_reload) {
            emit reloadRequested();
        }
    }
    emit invalidateThumbnail(m_pageId);
}

void output::OptionsWidget::on_foregroundLayerCB_toggled(bool checked)
{
    ColorGrayscaleOptions opt = m_colorParams.colorGrayscaleOptions();
    if (opt.foregroundLayerEnabled() != checked) {
        opt.setForegroundLayerEnabled(checked);
        m_colorParams.setColorGrayscaleOptions(opt);

        m_ptrSettings->setColorParams(m_pageId, m_colorParams, ColorParamsApplyFilter::CopyMode);

        emit reloadRequested();
        emit invalidateThumbnail(m_pageId);
    }
}

void output::OptionsWidget::on_autoLayerCB_toggled(bool checked)
{
    ColorGrayscaleOptions opt = m_colorParams.colorGrayscaleOptions();
    if (opt.autoLayerEnabled() != checked) {
        opt.setAutoLayerEnabled(checked);
        m_colorParams.setColorGrayscaleOptions(opt);

        m_ptrSettings->setColorParams(m_pageId, m_colorParams, ColorParamsApplyFilter::CopyMode);

        emit reloadRequested();
        emit invalidateThumbnail(m_pageId);
    }
}

void output::OptionsWidget::applyForegroundThresholdButtonClicked()
{
    ApplyToDialog* dialog = new ApplyToDialog(this, m_pageId, m_pageSelectionAccessor);
    dialog->setWindowTitle(tr("Apply Foreground layer threshold"));
    connect(
        dialog, &ApplyToDialog::accepted,
        this, [ = ]() {
            std::vector<PageId> vec = dialog->getPageRangeSelectorWidget().result();
            std::set<PageId> pages;
            std::copy_if(
                vec.begin(),
                vec.end(),
                std::inserter(pages, pages.end()),
                [this](PageId const& page_id)
                {
                    return this->m_pageId != page_id;
                }
            );
            applyForegroundThresholdConfirmed(pages);
        }
    );

    dialog->show();
}

void output::OptionsWidget::on_actionReset_to_default_value_foreground_triggered()
{
    int def = QSettings().value(_key_output_bin_threshold_default, _key_output_bin_threshold_default_def).toInt();
    thresholdForegroundSlider->setValue(def);
}

void output::OptionsWidget::on_actionactionDespeckleOff_triggered()
{
    despeckleOffBtn->click();
}

void output::OptionsWidget::on_actionactionDespeckleCautious_triggered()
{
    despeckleCautiousBtn->click();
}

void output::OptionsWidget::on_actionactionDespeckleNormal_triggered()
{
    despeckleNormalBtn->click();
}

void output::OptionsWidget::on_actionactionDespeckleAggressive_triggered()
{
    despeckleAggressiveBtn->click();
}

void output::OptionsWidget::copyZoneToPagesDlgRequest(void* z)
{

    if (!z) {
        return;
    }

    const Zone* pz = static_cast<const Zone*>(z);
    Zone zone = *pz;
    delete pz;

    zone.properties().locateOrCreate<output::VirtualZoneProperty>()->setVirtual(true);

    bool is_fill_zone = m_lastTab == TAB_FILL_ZONES;

    ApplyToDialog* dialog = new ApplyToDialog(this, m_pageId, m_pageSelectionAccessor);
    dialog->setWindowTitle(tr("Copy zone and its settings to:"));
    connect(
        dialog, &ApplyToDialog::accepted,
    this, [ = ]() {
        std::vector<PageId> vec = dialog->getPageRangeSelectorWidget().result();
        std::set<PageId> pages(vec.begin(), vec.end());
        for (PageId const& page_id : pages) {
            if (page_id != m_pageId) {
                ZoneSet zones = is_fill_zone ? m_ptrSettings->fillZonesForPage(page_id)
                                : m_ptrSettings->pictureZonesForPage(page_id);
                zones.add(zone);
                if (is_fill_zone) {
                    m_ptrSettings->setFillZones(page_id, zones);
                } else {
                    m_ptrSettings->setPictureZones(page_id, zones);
                }
            }
        }

        emit invalidateAllThumbnails();
    }
    );

    dialog->show();
}

bool removeZonesWithUUID(const ZoneSet& zones, const QString& uuid, ZoneSet& new_zones)
{
    new_zones.clear();
    bool changed = false;
    for (const Zone& z : zones) {
        IntrusivePtr<const output::VirtualZoneProperty> ptrSet =
            z.properties().locate<output::VirtualZoneProperty>();
        if (ptrSet.get()) {
            if (ptrSet->uuid() == uuid) {
                changed = true;
                continue;
            }
        }
        new_zones.add(z);
    }

    return changed;
}

void output::OptionsWidget::deleteZoneFromPagesDlgRequest(void* z)
{
    if (!z) {
        return;
    }

    const Zone* zone = static_cast<const Zone*>(z);
    const IntrusivePtr<const output::VirtualZoneProperty> ptrSet =
        zone->properties().locate<output::VirtualZoneProperty>();

    if (!ptrSet.get()) {
        return;
    }

    QString uuid = ptrSet->uuid();
    delete zone;

    bool is_fill_zone = m_lastTab == TAB_FILL_ZONES;

    ApplyToDialog* dialog = new ApplyToDialog(this, m_pageId, m_pageSelectionAccessor);
    dialog->setWindowTitle(tr("Find and remove this zone from:"));
    connect(
        dialog, &ApplyToDialog::accepted,
    this, [ = ]() {
        std::vector<PageId> vec = dialog->getPageRangeSelectorWidget().result();
        std::set<PageId> pages(vec.begin(), vec.end());
        bool changed = false;
        for (PageId const& page_id : pages) {
            ZoneSet zones = is_fill_zone ? m_ptrSettings->fillZonesForPage(page_id)
                            : m_ptrSettings->pictureZonesForPage(page_id);
            ZoneSet new_zones;
            if (removeZonesWithUUID(zones, uuid, new_zones)) {
                changed = true;
                if (is_fill_zone) {
                    m_ptrSettings->setFillZones(page_id, new_zones);
                } else {
                    m_ptrSettings->setPictureZones(page_id, new_zones);
                }
            }
        }

        if (changed) {
            emit invalidateAllThumbnails();
        }
    }
    );

    dialog->show();
}
