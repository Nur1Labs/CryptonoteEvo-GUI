// Copyright (c) 2015-2018, The Bytecoin developers, [ ] developers.
// Licensed under the GNU Lesser General Public License. See LICENSE for details.

#include <QDateTime>

#include "aboutdialog.h"
#include "settings.h"
#include "ui/ui_aboutdialog.h"

namespace WalletGUI {

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent, static_cast<Qt::WindowFlags>(Qt::WindowCloseButtonHint))
    , m_ui(new Ui::AboutDialog)
{
    m_ui->setupUi(this);

    QString aboutText = m_ui->m_aboutLabel->text();
    m_ui->m_aboutLabel->setText(
            aboutText
                .arg(Settings::getVersion())
//                .arg(Settings::getVersionSuffix())
//                .arg(Settings::getRevision())
                .arg(QDateTime::currentDateTime().date().year()));
}

AboutDialog::~AboutDialog()
{}

}
