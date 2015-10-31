/* This file is part of Clementine.
   Copyright 2009-2013, David Sansome <me@davidsansome.com>
   Copyright 2010-2012, 2014, John Maguire <john.maguire@gmail.com>
   Copyright 2011, Andrea Decorte <adecorte@gmail.com>
   Copyright 2012, Arnaud Bienner <arnaud.bienner@gmail.com>
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

// StringBuilder is activated to speed-up QString concatenation. As explained
// here:
// http://labs.qt.nokia.com/2011/06/13/string-concatenation-with-qstringbuilder/
// this cause some compilation errors in some cases. As Lasfm library inlines
// some functions in their includes files, which aren't compatible with
// QStringBuilder, we undef it here
#include <QtGlobal>
#if QT_VERSION >= 0x040600
#if QT_VERSION >= 0x040800
#undef QT_USE_QSTRINGBUILDER
#else
#undef QT_USE_FAST_CONCATENATION
#undef QT_USE_FAST_OPERATOR_PLUS
#endif  // QT_VERSION >= 0x040800
#endif  // QT_VERSION >= 0x040600

#include "adoreservice.h"

#include <QMenu>
#include <QSettings>
#include <QDateTime>
#include <QCoreApplication>

#ifdef HAVE_LIBLASTFM1
#include <lastfm/RadioStation.h>
#else
#include <lastfm/RadioStation>
#endif

#include "../lastfm/lastfmcompat.h"
#include "internet/core/internetmodel.h"
#include "internet/core/internetplaylistitem.h"
#include "core/application.h"
#include "core/closure.h"
#include "core/logging.h"
#include "core/player.h"
#include "core/song.h"
#include "core/taskmanager.h"
#include "covers/coverproviders.h"
#include "covers/lastfmcoverprovider.h"
#include "ui/iconloader.h"
#include "ui/settingsdialog.h"

using lastfm::XmlQuery;

uint qHash(const lastfm::Track& track);

const char* AdoreService::kServiceName = "Adore";
const char* AdoreService::kSettingsGroup = "Adore";
const char* AdoreService::kWebUrl = "https://betatest.c3s.cc";
const int   AdoreService::kWebPort = 443;
const char* AdoreService::kWebAuthorize = "/musicfan/clients/add";
const char* AdoreService::kApiUrl = "https://restapitest.c3s.cc";
const int   AdoreService::kApiPort = 443;
const char* AdoreService::kApiUtilizeImp = "/v1/util_imp";
const char* AdoreService::kApiRegister = "/v1/register";
const char* AdoreService::kPluginVendor = "C3S";
const char* AdoreService::kPlayerName = "Clementine";
const char* AdoreService::kPluginName = "Clementine Adore Mod";
const char* AdoreService::kPluginVersion = "0.5";

AdoreService::AdoreService(Application* app, QObject* parent)
    : Scrobbler(parent),
      network_(new QNetworkAccessManager(this)),
      scrobbler_(nullptr),
      scrobbling_enabled_(false),
      connection_problems_(false),
      client_already_registered_(false),
      app_(app)
{
  ReloadSettings();

  connect(network_, SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)),
              SLOT(ProvideAuthenication(QNetworkReply*,QAuthenticator*))); // see ProvideAuthenication()

  adb.Open();

  astreamer.SetEngine(reinterpret_cast<GstEngine*>(app_->player()->engine()));
}

AdoreService::~AdoreService() {}

//! This slot is just provided for handling a possible http authorisation of the test site.
//! Can be disabled in AdoreService::AdoreService().
void AdoreService::ProvideAuthenication(QNetworkReply *reply,QAuthenticator *auth)
{
    auth->setUser(QString("hat"));
    auth->setPassword(QString("pocket"));
}

//! preserved for future use, does nothing i.t.m.
void AdoreService::ReloadSettings() {
  ;
}

//! provided by Clementines service framework to display the settings dialog
void AdoreService::ShowConfig() {
  app_->OpenSettingsDialogAtPage(SettingsDialog::Page_Lastfm);
}

//! Returns the authentication status.
//! TODO: not just look if the client uuid is set but evaluate
//! the Register API call if the backend recognizes the uuid!
bool AdoreService::IsAuthenticated() const {
  QSettings s;
  s.beginGroup(AdoreService::kSettingsGroup);
  QString uuid = s.value("token").toString();
  return uuid.length() > 0;
}

//! Converts a Clementine song to a lastfm track. @see Track class
lastfm::Track AdoreService::TrackFromSong(const Song& song) const {
  if (song.title() == last_track_.title() &&
      song.artist() == last_track_.artist() &&
      song.album() == last_track_.album())
    return last_track_;

  lastfm::Track ret;
  song.ToLastFM(&ret, PreferAlbumArtist());
  return ret;
}

//! Player client registration api call of the REST API (meant to
//! submit some extra information about the client at program start)
void AdoreService::RegisterClient() {
  QString host = QString(kApiUrl), port = QString::number(kApiPort);

  // patch url and port with possible debug settings from config file
  QSettings s;
  s.beginGroup(AdoreService::kSettingsGroup);
  QString uuid = s.value("token").toString();
  QString temp_host = s.value("apihost").toString();
  if (temp_host != "") host = temp_host;
  if (host.length() < 4 || host.left(4) != "http") host = "http://" + host;
  QString temp_port = s.value("apiport").toString();
  if (temp_port != "") port = temp_port;

  QUrl url(host);
  url.setPort(port.toInt());
  url.setPath(kApiRegister);
  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  req.setRawHeader("Accept", "application/json");
  req.setRawHeader("User-Agent", "Clementine");
  req.setRawHeader("Accept-Language", "en-us");
  QByteArray postdata = "{ \"client_uuid\": \"" + uuid.toUtf8() +
            "\", \"player_name\": \"" + QString(kPluginVersion).toUtf8() +
            "\", \"player_version\": \"" + QCoreApplication::applicationVersion().toUtf8() +
            "\", \"plugin_vendor\": \"" + QString(kPluginVendor).toUtf8() +
            "\", \"plugin_name\": \"" + QString(kPluginName).toUtf8() +
            "\", \"plugin_version\": \"" + QString(kPluginVersion).toUtf8() + "\"  }";
  QNetworkReply* reply = network_->post(req, postdata);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(RegisterClientReplyFinished(QNetworkReply*)), reply);

  qLog(Debug) << "Registering client with uuid " + uuid;
}

//! Handle the clean-up of the reply to the RegisterClient API call
void AdoreService::RegisterClientReplyFinished(QNetworkReply* reply)
{

#ifdef QT_DEBUG
/* scrap it!  if (reply->request().attribute(QNetworkRequest::User) == "debug-lookup")
  {
    qLog(Debug) << "Adore debug fingerprint lookup" << reply->errorString();
  }
*/
#endif
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NetworkError::NoError)
  {
    qLog(Debug) << "Client registration -- Adore server response #" << QString::number((int)reply->error()) << ":" << reply->errorString();
  }
  else
    qLog(Debug) << "Client registration -- Adore server response body:" << reply->readAll();
  client_already_registered_ = true;
}

//! Analog to the scrobbler slot NowPlaying(). This member marks a new track
//! needs to be fingerprinted and if there is an old track, transfers it to
//! the REST API.
void AdoreService::NowPlaying(const Song& song) {

  // Scrobbling streams is difficult if we don't have length of each individual
  // part.  In Song::ToLastFm we set the Track's source to
  // NonPersonalisedBroadcast if it's such a stream, so we have to scrobble it
  // when we change to a different track, but only if enough time has elapsed
  // since it started playing.
  // Adore relevance: we want to use the duration for later handling in the backend
  if (!last_track_.isNull() &&
      last_track_.source() == lastfm::Track::NonPersonalisedBroadcast) {
    const int duration_secs =
        last_track_.timestamp().secsTo(QDateTime::currentDateTime());
    if (duration_secs >= lastfm::compat::ScrobbleTimeMin()) {
      lastfm::MutableTrack mtrack(last_track_);
      mtrack.setDuration(duration_secs);
    }
  }

  lastfm::MutableTrack mtrack(TrackFromSong(song)); // Adore uses lastfm's nice track model
  mtrack.stamp();
  last_track_ = mtrack;

  if (astreamer.GetLastFingerprint() != "")
  {
    QDateTime now = QDateTime::currentDateTime();
    QString now_string = now.toString("yyyy-MM-dd hh:mm:ss");
    QString host = QString(kApiUrl), port = QString::number(kApiPort);

    // patch url and port with possible debug settings from config file
    QSettings s;
    s.beginGroup(AdoreService::kSettingsGroup);
    QString uuid = s.value("token").toString();
    QString temp_host = s.value("apihost").toString();
    if (temp_host != "") host = temp_host;
    if (host.length() < 4 || host.left(4) != "http") host = "http://" + host;
    QString temp_port = s.value("apiport").toString();
    if (temp_port != "") port = temp_port;

    QUrl url(host);
    url.setPort(port.toInt());
    url.setPath(kApiUtilizeImp);
    QNetworkRequest req(url);
    qLog(Debug) << "Adore request url: " + url.toString();
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("User-Agent", "Clementine");
    req.setRawHeader("Accept-Language", "en-us");
    QByteArray postdata = "{ \"client_uuid\": \"" + uuid.toUtf8() +
            "\", \"time_played\": \"" + now_string.toUtf8() +
            "\", \"time_submitted\": \"" + now_string.toUtf8() +
            "\", \"artist\": \"" + mtrack.artist().toString().toUtf8() +
            "\", \"title\": \"" + mtrack.title().toUtf8() +
            "\", \"release\": \"" + mtrack.album().toString().toUtf8() +
            "\", \"track_number\": \"" + QString::number(mtrack.trackNumber()).toUtf8() +
            "\", \"duration\": \"" + mtrack.durationString().toUtf8() +
            "\", \"fingerprinting_algorithm\": \"" + astreamer.GetFingerprintingAlgorithm().toUtf8() +
            "\", \"fingerprinting_version\": \"" + astreamer.GetFingerprintingAlgorithmVersion().toUtf8() +
            "\", \"fingerprint\": \"" + astreamer.GetLastFingerprint().toUtf8() + "\"  }";
    qLog(Debug) << "Adore request body: " + QString(postdata);
    QStringList column_names;
    QStringList column_values;
    column_names << "time_played" << "time_submitted" << "artist" << "title" << "release" << "track_number"
                 << "duration" << "fingerprinting_algorithm" << "fingerprinting_version" << "fingerprint" << "status";
    column_values << now_string << now_string << mtrack.artist().toString() << mtrack.title() << mtrack.album().toString()
                  << QString::number(mtrack.trackNumber()) << mtrack.durationString() << astreamer.GetFingerprintingAlgorithm()
                  << astreamer.GetFingerprintingAlgorithmVersion() << astreamer.GetLastFingerprint() << "OK";
    req.setAttribute(QNetworkRequest::User, QVariant(column_names));
    req.setAttribute(QNetworkRequest::UserMax, QVariant(column_values));
    QNetworkReply* reply = network_->post(req, postdata);
    NewClosure(reply, SIGNAL(finished()), this, SLOT(UtilizeImpReplyFinished(QNetworkReply*)), reply);

    if (!client_already_registered_) RegisterClient();

#ifdef QT_DEBUG
    {
/*  scrap this!    QUrl url("https://echoprint.c3s.cc/query?fp_code=" + astreamer.GetLastFingerprint());
      QNetworkRequest req = QNetworkRequest(url);
      QNetworkReply* reply = network_.get(req);

      url.setPort(443);
      url.setPath("query?fp_code=" + astreamer.GetLastFingerprint());
      QNetworkRequest req(url);
      req.setHeader(QNetworkRequest::ContentTypeHeader, "text/html");
      req.setRawHeader("Accept", "text/html");
      req.setRawHeader("User-Agent", "Clementine");
      req.setRawHeader("Accept-Language", "en-us");
      req.setAttribute(QNetworkRequest::User, QVariant("debug-lookup"));
      QNetworkReply* reply = network_->post(req, postdata);
      NewClosure(reply, SIGNAL(finished()), this, SLOT(RegisterClientReplyFinished(QNetworkReply*)), reply);
*/    }
#endif

    qLog(Debug) << "Adoring track " << mtrack.title() << " by " << mtrack.artist().toString() << "";

    astreamer.ResetLastFingerprint();
  }

  astreamer.StartProbing();
}

//! This member looks in the queue db (AdoreDb) for played songs that
//! couldn't be transferred to the backend server and processes them.
//! Note: In the corresponding UtilizeImpReplyFinished() this member
//! is called again till no more songs are in the queue (or an error
//! occurs).
void AdoreService::ProcessQueued()
{
    QStringList column_names;
    QStringList column_values;

    if (!adb.GetQueued(column_names, column_values))
    {   // no queued records pending? nothing to do.
        return;
    }

    // get track meta data for QNetworkRequest
    int pos;
    QString artist;
    pos = column_names.indexOf("artist");
    if (pos >= 0) artist = column_values.value(pos);
    QString release;
    // TODO: release
    QString title;
    pos = column_names.indexOf("title");
    if (pos >= 0) title = column_values.value(pos);
    QString fingerprint;
    pos = column_names.indexOf("fingerprint");
    if (pos >= 0) fingerprint = column_values.value(pos);

    // get 'submitted' time just to write the update value to db
    QDateTime now = QDateTime::currentDateTime();
    QString now_string = now.toString("yyyy-MM-dd hh:mm:ss");
    pos = column_names.indexOf("submitted");
    if (pos >= 0) column_values.value(pos) = now_string;

    // Transmit queued track
    QString host = QString(kApiUrl), port = QString::number(kApiPort);

    // patch url and port with possible debug settings from config file
    QSettings s;
    s.beginGroup(AdoreService::kSettingsGroup);
    QString uuid = s.value("token").toString();
    QString temp_host = s.value("apihost").toString();
    if (temp_host != "") host = temp_host;
    if (host.length() < 4 || host.left(4) != "http") host = "http://" + host;
    QString temp_port = s.value("apiport").toString();
    if (temp_port != "") port = temp_port;

    QUrl url(host);
    url.setPort(port.toInt());
    url.setPath(kApiUtilizeImp);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("User-Agent", "Clementine");
    req.setRawHeader("Accept-Language", "en-us");
    QByteArray postdata = "{ \"client_uuid\": \"" + QString(uuid).toUtf8() + "\"";

    foreach (QString name, column_names)
    {
      int pos = column_names.indexOf(name);
      if (pos >= 0)
      {
        postdata += ", \"" + name.toUtf8() + "\": \"" + column_values[pos].toUtf8() + "\" ";
      }
    }
    postdata += "}";

    req.setAttribute(QNetworkRequest::User, QVariant(column_names));
    req.setAttribute(QNetworkRequest::UserMax, QVariant(column_values));
    QNetworkReply* reply = network_->post(req, postdata);
    NewClosure(reply, SIGNAL(finished()), this, SLOT(UtilizeImpReplyFinished(QNetworkReply*)), reply);

    qLog(Debug) << "Adoring queued track " << title << " by " << artist;
}

//! Handle the clean-up of the reply to the RegisterClient API call
void AdoreService::UtilizeImpReplyFinished(QNetworkReply* reply)
{
  int type = TYPE_UNKNOWN;
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NetworkError::NoError)
  {
    type = TYPE_ERROR;
    qLog(Debug) << "Adore server response #" << QString::number((int)reply->error()) << ":" << reply->errorString();
  }
  else
  {
    type = TYPE_SUCCESS;
    qLog(Debug) << "Adore server response body:" << reply->readAll();
  }
  // THOMIEL TODO: support TYPE_DELAY & TYPE_WARNING depending on server response

  // QNetworkRequest::User and QNetworkRequest::UserMax holding column names and associated values to be stored in the database
  QVariant column_names_var = reply->request().attribute(QNetworkRequest::User);
  QVariant column_values_var = reply->request().attribute(QNetworkRequest::UserMax);
  QStringList column_names = column_names_var.toStringList();
  QStringList column_values = column_values_var.toStringList();

  int submitted_pos = column_names.indexOf("submitted");
  int status_pos = column_names.indexOf("status");
  int type_pos = column_names.indexOf("type");

  // if a network error occurs, modify log record to be stored in database
  if (type == TYPE_ERROR)
  {
    // store network error message in database
    if (status_pos >= 0)
      column_values[status_pos] = reply->errorString();

    // remove 'submitted' date & time
    if (submitted_pos >= 0)
      column_values[submitted_pos] = "";
  }

  // finally append type (see #defines in adoredb.h)
  if (type_pos >= 0)
      column_values[type_pos] = QString::number(type);
  else
  {
      column_names.append("type");
      column_values.append(QString::number(type));
  }

  // write to log table; if this was successful
  bool db_update_succeeded = adb.Add(column_names, column_values);

  // if no network error occured, check for queued utilizations
  if (db_update_succeeded && type != TYPE_ERROR)
  {
    ProcessQueued();
  }
}


//! This is a remnant of the scrobbler type service, maybe deploy an own type in the future...
void AdoreService::Scrobble() {
}

//! This is a remnant of the scrobbler type service, maybe deploy an own type in the future...
void AdoreService::Love() {
}

//! This is a remnant of the scrobbler type service, maybe deploy an own type in the future...
void AdoreService::Ban() {
}

//! This is a remnant of the scrobbler type service, maybe deploy an own type in the future...
void AdoreService::ToggleScrobbling() {
}
