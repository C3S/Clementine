/* This file is part of Clementine.
   Copyright 2009-2011, 2013, David Sansome <me@davidsansome.com>
   Copyright 2011, Andrea Decorte <adecorte@gmail.com>
   Copyright 2011, 2014, John Maguire <john.maguire@gmail.com>
   Copyright 2012, Kacper "mattrick" Banasik <mattrick@jabster.pl>
   Copyright 2014, Krzysztof Sobiecki <sobkas@gmail.com>
   Copyright 2014-2015, Cultural Commons Collecting Society SCE mit
                        beschränkter Haftung (C3S SCE)
   Copyright 2014-2015, Thomas Mielke <thomas.mielke@c3s.cc>

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "adoresettingspage.h"
#include "ui_adoresettingspage.h"

#include <QMessageBox>
#include <QSettings>

#include "adoreservice.h"
#include "internet/core/internetmodel.h"
#include "core/application.h"
#include "ui/iconloader.h"
#include <QUuid>
#include <QDesktopServices>
#include <QCryptographicHash>

AdoreSettingsPage::AdoreSettingsPage(SettingsDialog* dialog)
    : SettingsPage(dialog),
      service_(static_cast<AdoreService*>(dialog->app()->adorer())),
      ui_(new Ui_AdoreSettingsPage),
      waiting_for_auth_(false) {
    ui_->setupUi(this);

  // Icons
  setWindowIcon(QIcon(":/providers/adore.png"));

  connect(ui_->register_token, SIGNAL(clicked()), SLOT(RegisterToken()));

  ui_->login_state->AddCredentialField(ui_->token);
  ui_->login_state->AddCredentialGroup(ui_->groupBox);

  ui_->token->setMinimumWidth(QFontMetrics(QFont()).width("00000000-0000-0000-0000-000000000000"));

  resize(sizeHint());
}

AdoreSettingsPage::~AdoreSettingsPage() { /*delete ui_*/; }

/// Create a unique client id and register it with the backend server using a browser window.
/// The client uuid is transferred in the url path along with a SHA1 hash
void AdoreSettingsPage::RegisterToken() {
  //waiting_for_auth_ = true;

  // create a new uuid
  QString uuid = QUuid::createUuid().toString();
  if (uuid.length() > 2)
  {
    uuid = uuid.mid(1, uuid.length()-2); // get rid of the brackets
    ui_->token->setText(uuid);
  }

  QSettings s;
  s.beginGroup(AdoreService::kSettingsGroup);
  QString host = QString(AdoreService::kWebUrl), port = QString::number(AdoreService::kWebPort);
  QString temp_host = s.value("webhost").toString();
  if (temp_host != "") { if (temp_host.left(4) != "http") temp_host = "http://" + temp_host; host = temp_host; }
  QString temp_port = s.value("webport").toString();
  if (temp_port != "") port = temp_port;
  QUrl url(host);
  QString path = (QString)AdoreService::kWebAuthorize /* + "/" + uuid + "/"
          + QCryptographicHash::hash(uuid.toAscii(), QCryptographicHash::Sha1).toHex() */;

  url.setPath(path);
  url.setPort(port.toInt());
  url.addQueryItem("uuid", uuid);         // this is not true: we don't use url params any more, we have just put 'em in the path
  url.addQueryItem("hash", QCryptographicHash::hash(uuid.toAscii(), QCryptographicHash::Sha1).toHex());
  QDesktopServices::openUrl(url);
}

/// remnant of the lastfm implementation. maybe use it in the future for better ui integration.
void AdoreSettingsPage::AuthenticationComplete(bool success, const QString& message) {
  if (!waiting_for_auth_) return;  // Wasn't us that was waiting for auth

  waiting_for_auth_ = false;

  if (success) {
    // Clear password just to be sure
//    ui_->password->clear();
    // Save settings
    Save();
  } else {
    QString dialog_text = tr("Your Adore credentials were incorrect");
    if (!message.isEmpty()) {
      dialog_text = message;
    }
    QMessageBox::warning(this, tr("Authentication failed"), dialog_text);
  }

  RefreshControls(success);
}

/// Load Settings from the config file.
void AdoreSettingsPage::Load() {

  QSettings s;
  s.beginGroup(AdoreService::kSettingsGroup);

  //  ui_->lowRatingException->setChecked(service_->IsLowRatingException());
  ui_->token->setText(s.value("token").toString());

  //RefreshControls(service_->IsAuthenticated());
}

/// Save settings to the config file.
void AdoreSettingsPage::Save() {
  QSettings s;
  s.beginGroup(AdoreService::kSettingsGroup);

  s.setValue("token", ui_->token->text());
//  s.setValue("LowRatingException", ui_->lowRatingException->isChecked());
  s.endGroup();

//  service_->ReloadSettings();
}

// for possible future use.
void AdoreSettingsPage::Logout() {
  RefreshControls(false);

//  service_->SignOut();
}

// for future proper control of the `LoginStateWidget`
void AdoreSettingsPage::RefreshControls(bool authenticated) {
//  ui_->login_state->SetLoggedIn(
//      authenticated ? LoginStateWidget::LoggedIn : LoginStateWidget::LoggedOut,
//      "");
}
