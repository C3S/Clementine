/* This file is part of Clementine.
   Copyright 2009-2011, 2013, David Sansome <me@davidsansome.com>
   Copyright 2011, Andrea Decorte <adecorte@gmail.com>
   Copyright 2014, Krzysztof Sobiecki <sobkas@gmail.com>
   Copyright 2014, John Maguire <john.maguire@gmail.com>
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

#ifndef INTERNET_ADORE_ADORESETTINGSPAGE_H_
#define INTERNET_ADORE_ADORESETTINGSPAGE_H_

#include "ui/settingspage.h"
//#include "adoreservice.h"

class AdoreService;
class Ui_AdoreSettingsPage;

/*! This class handles the settings dialog.
 */
class AdoreSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit AdoreSettingsPage(SettingsDialog* dialog);
  ~AdoreSettingsPage();

  void Load();
  void Save();

 private slots:
  void RegisterToken();
  void AuthenticationComplete(bool success, const QString& error_message);
  void Logout();

 private:
  AdoreService* service_;
  Ui_AdoreSettingsPage* ui_;

  bool waiting_for_auth_; ///< remnant of the lastfm implementation. maybe use it in the future for better ui integration

  void RefreshControls(bool authenticated); ///< this controls the 'logged in' status control of the Adore settings page
};

#endif  // INTERNET_ADORE_ADORESETTINGSPAGE_H_
