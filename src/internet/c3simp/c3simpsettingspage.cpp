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

#include "c3simpsettingspage.h"
#include "ui_c3simpsettingspage.h"

#include <QMessageBox>
#include <QSettings>

#include "c3simpservice.h"
#include "internet/core/internetmodel.h"
#include "core/application.h"
#include "ui/iconloader.h"
#include <QUuid>
#include <QDesktopServices>
#include <QCryptographicHash>

C3sImpSettingsPage::C3sImpSettingsPage(SettingsDialog* dialog)
    : SettingsPage(dialog),
      service_(static_cast<C3sImpService*>(dialog->app()->c3simpr())),
      ui_(new Ui_C3sImpSettingsPage),
      waiting_for_auth_(false) {
    ui_->setupUi(this);

  // Icons
  setWindowIcon(QIcon(":/providers/c3simp.png"));

/*  connect(service_, SIGNAL(AuthenticationComplete(bool, QString)),
          SLOT(AuthenticationComplete(bool, QString)));
  connect(ui_->login_state, SIGNAL(LogoutClicked()), SLOT(Logout()));
  connect(ui_->login_state, SIGNAL(LoginClicked()), SLOT(Login()));
*/  connect(ui_->register_token, SIGNAL(clicked()), SLOT(RegisterToken()));

  //ui_->login_state->AddCredentialField(ui_->mail);
  ui_->login_state->AddCredentialField(ui_->token);
  //ui_->login_state->AddCredentialField(ui_->password);
  ui_->login_state->AddCredentialGroup(ui_->groupBox);

  ui_->token->setMinimumWidth(QFontMetrics(QFont()).width("00000000-0000-0000-0000-000000000000"));

  resize(sizeHint());
}

C3sImpSettingsPage::~C3sImpSettingsPage() { /*delete ui_*/; }

void C3sImpSettingsPage::RegisterToken() {
  //waiting_for_auth_ = true;

  // create a new uuid
  QString uuid = QUuid::createUuid().toString();
  if (uuid.length() > 2)
  {
    uuid = uuid.mid(1, uuid.length()-2); // get rid of the brackets
    ui_->token->setText(uuid);
  }

  QSettings s;
  s.beginGroup(C3sImpService::kSettingsGroup);
  QString host = QString(C3sImpService::kUrl), port = QString::number(C3sImpService::kPort);
  QString temp_host = s.value("host").toString();
  if (temp_host != "") { if (temp_host.left(4) != "http") temp_host = "http://" + temp_host; host = temp_host; }
  QString temp_port = s.value("port").toString();
  if (temp_port != "") port = temp_port;
  QUrl url(host);
  QString path = (QString)C3sImpService::kAuthorize + "/" + uuid + "/"
          + QCryptographicHash::hash(uuid.toAscii(), QCryptographicHash::Sha1).toHex();

  url.setPath(path);
  url.setPort(port.toInt());
  //url.addQueryItem("uuid", uuid);          we don't use url params, just put 'em in the path
  //url.addQueryItem("hash", "DUMMY_HASH");
  QDesktopServices::openUrl(url);

//  ui_->login_state->SetLoggedIn(LoginStateWidget::LoginInProgress);
//  service_->Authenticate(ui_->username->text(), ui_->password->text());
}

void C3sImpSettingsPage::AuthenticationComplete(bool success,
                                                const QString& message) {
  if (!waiting_for_auth_) return;  // Wasn't us that was waiting for auth

  waiting_for_auth_ = false;

  if (success) {
    // Clear password just to be sure
//    ui_->password->clear();
    // Save settings
    Save();
  } else {
    QString dialog_text = tr("Your C3sImp credentials were incorrect");
    if (!message.isEmpty()) {
      dialog_text = message;
    }
    QMessageBox::warning(this, tr("Authentication failed"), dialog_text);
  }

  RefreshControls(success);
}

void C3sImpSettingsPage::Load() {

  QSettings s;
  s.beginGroup(C3sImpService::kSettingsGroup);

  //  ui_->lowRatingException->setChecked(service_->IsLowRatingException());
  //ui_->mail->setText(s.value("mail").toString());
  //ui_->password->setText(s.value("password").toString());
  ui_->token->setText(s.value("token").toString());

  //RefreshControls(service_->IsAuthenticated());
}

void C3sImpSettingsPage::Save() {
  QSettings s;
  s.beginGroup(C3sImpService::kSettingsGroup);

  //s.setValue("mail", ui_->mail->text());
  //s.setValue("password", ui_->password->text());
  s.setValue("token", ui_->token->text());
//  s.setValue("LowRatingException", ui_->lowRatingException->isChecked());
  s.endGroup();

//  service_->ReloadSettings();
}

void C3sImpSettingsPage::Logout() {
//  ui_->username->clear();
//  ui_->password->clear();
  RefreshControls(false);

//  service_->SignOut();
}

void C3sImpSettingsPage::RefreshControls(bool authenticated) {
//  ui_->login_state->SetLoggedIn(
//      authenticated ? LoginStateWidget::LoggedIn : LoginStateWidget::LoggedOut,
//      c3simp::ws::Username);
}
