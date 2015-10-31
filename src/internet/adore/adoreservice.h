/* This file is part of Clementine.
   Copyright 2009-2013, David Sansome <me@davidsansome.com>
   Copyright 2010-2012, 2014, John Maguire <john.maguire@gmail.com>
   Copyright 2011, Andrea Decorte <adecorte@gmail.com>
   Copyright 2012, Kacper "mattrick" Banasik <mattrick@jabster.pl>
   Copyright 2012, Harald Sitter <sitter@kde.org>
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

#ifndef INTERNET_LASTFM_ADORESERVICE_H_
#define INTERNET_LASTFM_ADORESERVICE_H_

#include <memory>

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QAuthenticator>

/// \mainpage Adore Developer Documentation Start Page
///
/// \section intro_sec Introduction
///
/// In the IMP project specification it was planned to implement an
/// audio fingerprinting algorithm into a popular music player to allow
/// for a unbribable music identification independently from possibly
/// stored metadata. The fingerprints had to be submitted to the backend
/// server without any user interaction being necessary.
///
/// From all music players we tested as candidates for implementing the
/// Adore prototype, *Clementine* stood out with respect to OS coverage,
/// stability, feature richness, code maintain- and exdendability, and
/// of course a cool user interface.
///
/// \section install_sec Installation
///
/// To provide an easier way to get to a debugging environment, two
/// alternatives exist:
/// * a shell script for Debian/Ubuntu and
/// * a Virtual Box image based on Lubuntu
///
/// \subsection shellscript_subsec Shellscript
///
/// The script is suitable to install everything necessary from scratch
/// on a freshly installed Ubuntu.
///
///
///     wget http://files.c3s.cc/setup_qt_and_clementine.sh
///     chmod 700 setup_qt_and_clementine.sh
///     ./setup_qt_and_clementine.sh
///
/// In case of a not quite so virgin system it is recommended to first
/// take a look on the script and make changes if necessary.
///
/// \subsection virtualboximage_subsec VirtualBox Image
///
/// The tarball (3,5 GB) includes a VM with the directory name Lubuntu32
/// (unzipped nearly 10GB):
///
///     cd "~/VirtualBox VMs"
///     wget http://files.c3s.cc/Lubuntu32.tgz
///     tar -xzf Lubuntu32.tgz
///     rm Lubuntu32.tgz
///     vitualbox
///
/// \section debuggingsettings_sec Debugging Settings of the Clementine Mods
///
/// To allow access to arbitrary test installations of the REST API and web
/// frontend, it is possible to override the hard-coded urls
/// https://restapitest.c3s.cc:443 and https://betatest.c3s.cc:443
/// in the Clementine.conf (subdirectory ~/.config/Clementine) using the
/// keys `webhost`, `webport`, `apihost`, and `apiport`:
///
///     [Adore]
///     webhost=http://localhost
///     webport=6543
///     apihost=http://localhost
///     apiport=6543
///     token=40a1b0d8-1f10-4490-b4b2-04743c61d3ea
///
/// \section systemrequirements_sec System Requirements
///
/// For optimal performance a main memory of at least 4GB is recommended.
/// On multi-core systems provide make with the parameter -jX (where X is
/// the number of processor cores). Find the make settings on the Projects
/// section of QtCreator's navi bar.
///
/// \section theardorecode_sec The Adore Code
///
/// You will find everything essential in the src/internet/adore folder.
/// Namely the classes AdoreService, AdoreDb, AdoreStreamer, and AdoreSettingsPage.
/// The EchoPrint code generator was put in the src/echoprint folder.
/// Other than this, only minor changes have been made to other files:
/// * CMakeLists.txt (to acutally add the .h & .cpp files to the project),
/// * to src/core/application.*,
/// * to src/core/player.*,
/// * to src/ui/settingsdialog.*,
/// * to data/data.qrc (to add the icon to the ressources), and
/// * added data/provider/adore.png (the actual icon image)
///
/// In order to take off quickly and get into the framework Clementine provides
/// for internet services, we cannibalized the lastfm code, so don't be surprised
/// if you stumble upon remains of the lastfm scrobbler service. In the future,
/// when the demands are more clear, we might implement an own base class for
/// seamless user interface integration from it.
///
/// To start exploring the source code, we recommend to first take a peek in AdoreService.
///
/// @see AdoreService @see AdoreDb @see AdoreStreamer @see AdoreSettingsPage

/*! To store track metadata the Track class was incorporated from from lastfm.
  * Here is the original comment:
  * Our track type. It's quite good, you may want to use it as your track type
  * in general. It is explicitly shared. Which means when you make a copy, they
  * both point to the same data still. This is like Qt's implicitly shared
  * classes, eg. QString, however if you mod a copy of a QString, the copy
  * detaches first, so then you have two copies. Our Track object doesn't
  * detach, which is very handy for our usage in the client, but perhaps not
  * what you want. If you need a deep copy for eg. work in a thread, call
  * clone(). */
namespace lastfm {
class Track;
}

#include <QtGlobal>
uint qHash(const lastfm::Track& track);

#include "adoredb.h"
#include "adorestreamer.h"

#include "../lastfm/lastfmcompat.h"

#include "internet/core/scrobbler.h"

class Application;
class QNetworkAccessManager;
class Song;

/*! The central class of the Adore mod, controlling all other AdoreXXX classes.
 *  Note: This class is derived from the `Scrobbler` service as it fulfills
 *  a similar purpose. Only a subset of the `Scrobbler` capabilities are used
 *  because, for example, no direct user interface elements in the Clementine
 *  main window have to be accessed.
 *  TODO: Maybe get rid of the `Scrobbler` heritage in the future.
 */
class AdoreService : public Scrobbler {
  Q_OBJECT

 public:
  explicit AdoreService(Application* app, QObject* parent = nullptr);
  ~AdoreService();

  static const char* kServiceName;    ///< as appears in the settings window
  static const char* kSettingsGroup;  ///< as appears in the settings navibar
  static const char* kWebUrl;         ///< URL of the web interface, can be overridden in ~/.config/Clementine/Clementine.conf section [Adore] key 'host'
  static const int   kWebPort;        ///< port of the web interface, can be overridden in ~/.config/Clementine/Clementine.conf section [Adore] key 'host'
  static const char* kWebAuthorize;   ///< base path to the authorization web page
  static const char* kApiUrl;         ///< URL of the REST API, can be overridden in ~/.config/Clementine/Clementine.conf section [Adore] key 'host'
  static const int   kApiPort;        ///< port of the REST API, can be overridden in ~/.config/Clementine/Clementine.conf section [Adore] key 'host'
  static const char* kApiUtilizeImp;  ///< path to the utilization api call of the REST API
  static const char* kApiRegister;    ///< path to the player client registration api call of the REST API (meant to submit some extra information about the client at program start)
  static const char* kPlayerName;     ///< self explanatory
  static const char* kPluginVendor;   ///< the creator of the Adore code
  static const char* kPluginName;     ///< as Clementine doesn't have a real plugin system, just translate it as ModName or PatchName
  static const char* kPluginVersion;  ///< version of the the Adore code

  void ReloadSettings();

  virtual QString Icon() { return ":providers/adore.png"; }

  // Last.fm specific stuff
  bool IsAuthenticated() const;
  bool IsScrobblingEnabled() const { return scrobbling_enabled_; }
  bool AreButtonsVisible() const { return buttons_visible_; }
  bool IsScrobbleButtonVisible() const { return scrobble_button_visible_; }
  bool PreferAlbumArtist() const { return prefer_albumartist_; }
  bool HasConnectionProblems() const { return connection_problems_; }

  void UpdateSubscriberStatus();

 public slots:
  void NowPlaying(const Song& song);  ///< This is a remnant of the scrobbler type service, maybe deploy an own type in the future...
  void Scrobble();                    ///< This is a remnant of the scrobbler type service, maybe deploy an own type in the future...
  void Love();                        ///< This is a remnant of the scrobbler type service, maybe deploy an own type in the future...
  void Ban();                         ///< This is a remnant of the scrobbler type service, maybe deploy an own type in the future...
  void ShowConfig();                  ///< This is a remnant of the scrobbler type service, maybe deploy an own type in the future...
  void ToggleScrobbling();            ///< This is a remnant of the scrobbler type service, maybe deploy an own type in the future...

 signals:
  void AuthenticationComplete(bool success, const QString& error_message); ///< remnant of the lastfm implementation. maybe use it in the future for better ui integration

  void SavedItemsChanged();

 private slots:
  void ProvideAuthenication(QNetworkReply *reply,QAuthenticator *auth); // just provided for the test site
  void UtilizeImpReplyFinished(QNetworkReply* reply);
  void RegisterClientReplyFinished(QNetworkReply* reply);


 private:
  QNetworkAccessManager* network_;
  lastfm::Track TrackFromSong(const Song& song) const;
  void RegisterClient();
  void ProcessQueued();

 private:
  lastfm::Audioscrobbler* scrobbler_; ///< zombie base object. remnant of the lastfm implementation.
  lastfm::Track last_track_;       ///< needed to keep track of the metadata.
  lastfm::Track next_metadata_;    ///< needed to keep track of the metadata.

  bool scrobbling_enabled_;        ///< remnant of the lastfm implementation. maybe use it in the future for better ui integration
  bool buttons_visible_;           ///< remnant of the lastfm implementation. maybe use it in the future for better ui integration
  bool scrobble_button_visible_;   ///< remnant of the lastfm implementation. maybe use it in the future for better ui integration
  bool prefer_albumartist_;        ///< remnant of the lastfm implementation. maybe use it in the future for better ui integration
  bool connection_problems_;       ///< Useful to inform the user that we can't scrobble right now. remnant of the lastfm implementation. maybe use it in the future for better ui integration.
  bool client_already_registered_; ///< controls `RegisterClient()` to be called only once after Clementine has been started.

  Application* app_;

  // handy helper classes:
  AdoreDb adb;              ///< SQLite db for batch processing. @see AdoreDb class
  AdoreStreamer astreamer;  ///< GStreamer audio buffer grabber, converter to 11050 Hz mono, and fingerprinter. @see AdoreStreamer class
};

#endif  // INTERNET_LASTFM_ADORESERVICE_H_
