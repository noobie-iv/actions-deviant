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

#include "Filter.h"
#include "ApplyToDialog.h"
#include "Settings.h"
#include "Params.h"
#include "LayoutType.h"
#include "PageId.h"
#include "ProjectPages.h"
#include "ScopedIncDec.h"
#include "SplitModeWidget.h"
#include <QPixmap>
#include <QButtonGroup>
#include <assert.h>

namespace page_split
{

OptionsWidget::OptionsWidget(
    IntrusivePtr<Settings> const& settings,
    IntrusivePtr<ProjectPages> const& page_sequence,
    PageSelectionAccessor const& page_selection_accessor)
    :   m_ptrSettings(settings),
        m_ptrPages(page_sequence),
        m_pageSelectionAccessor(page_selection_accessor),
        m_ignoreAutoManualToggle(0),
        m_ignoreLayoutTypeToggle(0)
{
    setupUi(this);

    // Workaround for QTBUG-182
    QButtonGroup* grp = new QButtonGroup(this);
    grp->addButton(autoBtn);
    grp->addButton(manualBtn);

    connect(
        singlePageUncutBtn, SIGNAL(toggled(bool)),
        this, SLOT(layoutTypeButtonToggled(bool))
    );
    connect(
        pagePlusOffcutBtn, SIGNAL(toggled(bool)),
        this, SLOT(layoutTypeButtonToggled(bool))
    );
    connect(
        twoPagesBtn, SIGNAL(toggled(bool)),
        this, SLOT(layoutTypeButtonToggled(bool))
    );
    connect(
        layoutApplyBtn, SIGNAL(clicked()),
        this, SLOT(showChangeDialog())
    );
    connect(
        autoBtn, SIGNAL(toggled(bool)),
        this, SLOT(splitLineModeChanged(bool))
    );
}

OptionsWidget::~OptionsWidget()
{
}

void
OptionsWidget::preUpdateUI(PageId const& page_id)
{
    ScopedIncDec<int> guard1(m_ignoreAutoManualToggle);
    ScopedIncDec<int> guard2(m_ignoreLayoutTypeToggle);

    m_pageId = page_id;
    Settings::Record const record(m_ptrSettings->getPageRecord(page_id.imageId()));
    LayoutType const layout_type(record.combinedLayoutType());

    switch (layout_type) {
    case AUTO_LAYOUT_TYPE:
        // Uncheck all buttons.  Can only be done
        // by playing with exclusiveness.
        twoPagesBtn->setChecked(true);
        twoPagesBtn->setAutoExclusive(false);
        twoPagesBtn->setChecked(false);
        twoPagesBtn->setAutoExclusive(true);
        break;
    case SINGLE_PAGE_UNCUT:
        singlePageUncutBtn->setChecked(true);
        break;
    case PAGE_PLUS_OFFCUT:
        pagePlusOffcutBtn->setChecked(true);
        break;
    case TWO_PAGES:
        twoPagesBtn->setChecked(true);
        break;
    }

    pageSplitLineGroup->setVisible(layout_type != SINGLE_PAGE_UNCUT);

    if (layout_type == AUTO_LAYOUT_TYPE) {
        layoutApplyBtn->setEnabled(false);
        scopeLabel->setText("?");
    } else {
        layoutApplyBtn->setEnabled(true);
        scopeLabel->setText(tr("Set manually"));
    }

    // Uncheck both the Auto and Manual buttons.
    autoBtn->setChecked(true);
    autoBtn->setAutoExclusive(false);
    autoBtn->setChecked(false);
    autoBtn->setAutoExclusive(true);

    // And disable both of them.
    autoBtn->setEnabled(false);
    manualBtn->setEnabled(false);
}

void
OptionsWidget::postUpdateUI(UiData const& ui_data)
{
    ScopedIncDec<int> guard1(m_ignoreAutoManualToggle);
    ScopedIncDec<int> guard2(m_ignoreLayoutTypeToggle);

    m_uiData = ui_data;

    layoutApplyBtn->setEnabled(true);
    autoBtn->setEnabled(true);
    manualBtn->setEnabled(true);

    if (ui_data.splitLineMode() == MODE_AUTO) {
        autoBtn->setChecked(true);
    } else {
        manualBtn->setChecked(true);
    }

    PageLayout::Type const layout_type = ui_data.pageLayout().type();

    switch (layout_type) {
    case PageLayout::SINGLE_PAGE_UNCUT:
        singlePageUncutBtn->setChecked(true);
        break;
    case PageLayout::SINGLE_PAGE_CUT:
        pagePlusOffcutBtn->setChecked(true);
        break;
    case PageLayout::TWO_PAGES:
        twoPagesBtn->setChecked(true);
        break;
    }

    pageSplitLineGroup->setVisible(layout_type != PageLayout::SINGLE_PAGE_UNCUT);

    if (ui_data.layoutTypeAutoDetected()) {
        scopeLabel->setText(tr("Auto detected"));
    }
}

void
OptionsWidget::pageLayoutSetExternally(PageLayout const& page_layout)
{
    ScopedIncDec<int> guard(m_ignoreAutoManualToggle);

    m_uiData.setPageLayout(page_layout);
    m_uiData.setSplitLineMode(MODE_MANUAL);
    commitCurrentParams();

    manualBtn->setChecked(true);

    emit invalidateThumbnail(m_pageId);
}

void
OptionsWidget::layoutTypeButtonToggled(bool const checked)
{
    if (!checked || m_ignoreLayoutTypeToggle) {
        return;
    }

    LayoutType lt;
    ProjectPages::LayoutType plt = ProjectPages::ONE_PAGE_LAYOUT;

    QObject* button = sender();
    if (button == singlePageUncutBtn) {
        lt = SINGLE_PAGE_UNCUT;
    } else if (button == pagePlusOffcutBtn) {
        lt = PAGE_PLUS_OFFCUT;
    } else {
        assert(button == twoPagesBtn);
        lt = TWO_PAGES;
        plt = ProjectPages::TWO_PAGE_LAYOUT;
    }

    Settings::UpdateAction update;
    update.setLayoutType(lt);

    pageSplitLineGroup->setVisible(lt != SINGLE_PAGE_UNCUT);
    scopeLabel->setText(tr("Set manually"));

    m_ptrPages->setLayoutTypeFor(m_pageId.imageId(), plt);

    if (lt == PAGE_PLUS_OFFCUT ||
            (lt != SINGLE_PAGE_UNCUT &&
             m_uiData.splitLineMode() == MODE_AUTO)) {
        m_ptrSettings->updatePage(m_pageId.imageId(), update);
        emit reloadRequested();
    } else {
        PageLayout::Type plt;
        if (lt == SINGLE_PAGE_UNCUT) {
            plt = PageLayout::SINGLE_PAGE_UNCUT;
        } else {
            assert(lt == TWO_PAGES);
            plt = PageLayout::TWO_PAGES;
        }

        PageLayout new_layout(m_uiData.pageLayout());
        new_layout.setType(plt);
        Params const new_params(
            new_layout, m_uiData.dependencies(),
            m_uiData.splitLineMode()
        );

        update.setParams(new_params);
        m_ptrSettings->updatePage(m_pageId.imageId(), update);

        m_uiData.setPageLayout(new_layout);

        emit pageLayoutSetLocally(new_layout);
        emit invalidateThumbnail(m_pageId);
    }
}

void
OptionsWidget::showChangeDialog()
{
    Settings::Record const record(m_ptrSettings->getPageRecord(m_pageId.imageId()));
    Params const* params = record.params();
    if (!params) {
        return;
    }

    ApplyToDialog* dialog = new ApplyToDialog(
        this, m_pageId, m_pageSelectionAccessor, PageView::IMAGE_VIEW);
    QLayout& l = dialog->initNewLeftSettingsPanel();
    SplitModeWidget* options = new SplitModeWidget(dialog, record.combinedLayoutType(),
            params->pageLayout().type(), params->splitLineMode() == MODE_AUTO);
    l.addWidget(options);
    dialog->setWindowTitle(tr("Split Pages"));

    connect(
        dialog, &ApplyToDialog::accepted,
    this, [ = ]() {
        std::vector<PageId> vec = dialog->getPageRangeSelectorWidget().result();
        std::set<PageId> pages(vec.begin(), vec.end());
        layoutTypeSet(pages, dialog->getPageRangeSelectorWidget().allPagesSelected(),
                      options->layoutType(), options->isApplyCutChecked());
    }
    );
    dialog->show();
}

void
OptionsWidget::layoutTypeSet(
    std::set<PageId> const& pages, bool all_pages, LayoutType const layout_type, bool apply_cut)
{
    Q_UNUSED(all_pages);

    if (pages.empty()) {
        return;
    }

    Params const params = *(m_ptrSettings->getPageRecord(m_pageId.imageId()).params());

    if (layout_type != AUTO_LAYOUT_TYPE) {
        for (PageId const& page_id : pages) {
            Settings::UpdateAction update_params;
            update_params.setLayoutType(layout_type);
            if (apply_cut) {
                update_params.setParams(params);
            }
            m_ptrSettings->updatePage(page_id.imageId(), update_params);
        }
    } else {
        Settings::UpdateAction update;
        update.clearParams();
        for (PageId const& page_id : pages) {
            m_ptrSettings->updatePage(page_id.imageId(), update);
            if (page_id == m_pageId) {
                autoBtn->setChecked(true);
                emit reloadRequested();
            }
        }
    }

    emit invalidateAllThumbnails();

    if (layout_type == AUTO_LAYOUT_TYPE) {
        scopeLabel->setText(tr("Auto detected"));
        emit reloadRequested();
    } else {
        scopeLabel->setText(tr("Set manually"));
    }

}

void
OptionsWidget::splitLineModeChanged(bool const auto_mode)
{
    if (m_ignoreAutoManualToggle) {
        return;
    }

    if (auto_mode) {
        Settings::UpdateAction update;
        update.clearParams();
        m_ptrSettings->updatePage(m_pageId.imageId(), update);
        m_uiData.setSplitLineMode(MODE_AUTO);
        emit reloadRequested();
    } else {
        m_uiData.setSplitLineMode(MODE_MANUAL);
        commitCurrentParams();
    }
}

void
OptionsWidget::commitCurrentParams()
{
    Params const params(
        m_uiData.pageLayout(),
        m_uiData.dependencies(), m_uiData.splitLineMode()
    );
    Settings::UpdateAction update;
    update.setParams(params);
    m_ptrSettings->updatePage(m_pageId.imageId(), update);
}

/*============================= Widget::UiData ==========================*/

OptionsWidget::UiData::UiData()
    :   m_splitLineMode(MODE_AUTO),
        m_layoutTypeAutoDetected(false)
{
}

OptionsWidget::UiData::~UiData()
{
}

void
OptionsWidget::UiData::setPageLayout(PageLayout const& layout)
{
    m_pageLayout = layout;
}

PageLayout const&
OptionsWidget::UiData::pageLayout() const
{
    return m_pageLayout;
}

void
OptionsWidget::UiData::setDependencies(Dependencies const& deps)
{
    m_deps = deps;
}

Dependencies const&
OptionsWidget::UiData::dependencies() const
{
    return m_deps;
}

void
OptionsWidget::UiData::setSplitLineMode(AutoManualMode const mode)
{
    m_splitLineMode = mode;
}

AutoManualMode
OptionsWidget::UiData::splitLineMode() const
{
    return m_splitLineMode;
}

bool
OptionsWidget::UiData::layoutTypeAutoDetected() const
{
    return m_layoutTypeAutoDetected;
}

void
OptionsWidget::UiData::setLayoutTypeAutoDetected(bool const val)
{
    m_layoutTypeAutoDetected = val;
}

} // namespace page_split
