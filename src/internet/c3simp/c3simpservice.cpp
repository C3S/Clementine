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

#include "c3simpservice.h"

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

const char* C3sImpService::kServiceName = "C3S IMP";
const char* C3sImpService::kSettingsGroup = "C3S IMP";
const char* C3sImpService::kUrl = "https://restapitest.c3s.cc";
const int   C3sImpService::kPort = 443;
const char* C3sImpService::kUtilizeImp = "/api/v1/util_c3simp";
const char* C3sImpService::kAuthorize = "/account/authorize_client";
const char* C3sImpService::kRegister = "/api/v1/register";
const char* C3sImpService::kPluginVendor = "C3S";
const char* C3sImpService::kPlayerName = "Clementine";
const char* C3sImpService::kPluginName = "Clementine C3sImp Mod";
const char* C3sImpService::kPluginVersion = "0.2";
const char* C3sImpService::kFingerprintingAlgorithm = "echoprint";
const char* C3sImpService::kFingerprintingVersion = "4.12";


const char* C3sImpService::kAudioscrobblerClientId = "tng";
const char* C3sImpService::kApiKey = "75d20fb472be99275392aefa2760ea09";
const char* C3sImpService::kSecret = "d3072b60ae626be12be69448f5c46e70";

C3sImpService::C3sImpService(Application* app, QObject* parent)
    : Scrobbler(parent),
      network_(new QNetworkAccessManager(this)),
      scrobbler_(nullptr),
      already_scrobbled_(false),
      scrobbling_enabled_(false),
      connection_problems_(false),
      app_(app)
{
  ReloadSettings();

  // we emit the signal the first time to be sure the buttons are in the right
  // state
  emit ScrobblingEnabledChanged(scrobbling_enabled_);

  connect(network_, SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)),
              SLOT(ProvideAuthenication(QNetworkReply*,QAuthenticator*)));

  app_->cover_providers()->AddProvider(new LastFmCoverProvider(this));

  adb.Open();

  astreamer.SetEngine(reinterpret_cast<GstEngine*>(app_->player()->engine()));
}

C3sImpService::~C3sImpService() {}

// this slot is just provided for the test site -- disable in C3sImpService::C3sImpService()
void C3sImpService::ProvideAuthenication(QNetworkReply *reply,QAuthenticator *auth)
{
    auth->setUser(QString("hat"));
    auth->setPassword(QString("pocket"));
}

void C3sImpService::ReloadSettings() {
  bool scrobbling_enabled_old = scrobbling_enabled_;
  QSettings settings;
  settings.beginGroup(kSettingsGroup);
//  mail_ = settings.value("mail").toString();
  //lastfm::ws::Username = settings.value("Username").toString();
  //lastfm::ws::SessionKey = settings.value("Session").toString();
  //scrobbling_enabled_ = settings.value("ScrobblingEnabled", true).toBool();
  //buttons_visible_ = settings.value("ShowLoveBanButtons", true).toBool();
  //scrobble_button_visible_ =
  //    settings.value("ShowScrobbleButton", true).toBool();
  //prefer_albumartist_ = settings.value("PreferAlbumArtist", false).toBool();

  // avoid emitting signal if it's not changed
  if (scrobbling_enabled_old != scrobbling_enabled_)
    emit ScrobblingEnabledChanged(scrobbling_enabled_);
  emit ButtonVisibilityChanged(buttons_visible_);
  emit ScrobbleButtonVisibilityChanged(scrobble_button_visible_);
  emit PreferAlbumArtistChanged(prefer_albumartist_);
}

void C3sImpService::ShowConfig() {
  app_->OpenSettingsDialogAtPage(SettingsDialog::Page_Lastfm);
}

bool C3sImpService::IsAuthenticated() const {
  return !lastfm::ws::SessionKey.isEmpty();
}

bool C3sImpService::IsSubscriber() const {
  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  return settings.value("Subscriber", false).toBool();
}

void C3sImpService::Authenticate(const QString& username,
                                 const QString& password) {
  QMap<QString, QString> params;
  params["method"] = "auth.getMobileSession";
  params["username"] = username;
  params["authToken"] =
      lastfm::md5((username + lastfm::md5(password.toUtf8())).toUtf8());

  QNetworkReply* reply = lastfm::ws::post(params);
  NewClosure(reply, SIGNAL(finished()), this,
             SLOT(AuthenticateReplyFinished(QNetworkReply*)), reply);
  // If we need more detailed error reporting, handle error(NetworkError) signal
}

void C3sImpService::SignOut() {
  lastfm::ws::Username.clear();
  lastfm::ws::SessionKey.clear();

  QSettings settings;
  settings.beginGroup(kSettingsGroup);

  settings.setValue("Username", QString());
  settings.setValue("Session", QString());
}

void C3sImpService::AuthenticateReplyFinished(QNetworkReply* reply) {
  reply->deleteLater();

  // Parse the reply
  lastfm::XmlQuery lfm(lastfm::compat::EmptyXmlQuery());
  if (lastfm::compat::ParseQuery(reply->readAll(), &lfm)) {
    lastfm::ws::Username = lfm["session"]["name"].text();
    lastfm::ws::SessionKey = lfm["session"]["key"].text();
    QString subscribed = lfm["session"]["subscriber"].text();
    const bool is_subscriber = (subscribed.toInt() == 1);

    // Save the session key
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue("Username", lastfm::ws::Username);
    settings.setValue("Session", lastfm::ws::SessionKey);
    settings.setValue("Subscriber", is_subscriber);
  } else {
    emit AuthenticationComplete(false, lfm["error"].text().trimmed());
    return;
  }

  // Invalidate the scrobbler - it will get recreated later
  delete scrobbler_;
  scrobbler_ = nullptr;

  emit AuthenticationComplete(true, QString());
}

void C3sImpService::UpdateSubscriberStatus() {
  QMap<QString, QString> params;
  params["method"] = "user.getInfo";
  params["user"] = lastfm::ws::Username;

  QNetworkReply* reply = lastfm::ws::post(params);
  NewClosure(reply, SIGNAL(finished()), this,
             SLOT(UpdateSubscriberStatusFinished(QNetworkReply*)), reply);
}

void C3sImpService::UpdateSubscriberStatusFinished(QNetworkReply* reply) {
  reply->deleteLater();

  bool is_subscriber = false;

  lastfm::XmlQuery lfm(lastfm::compat::EmptyXmlQuery());
  if (lastfm::compat::ParseQuery(reply->readAll(), &lfm,
                                 &connection_problems_)) {
    QString subscriber = lfm["user"]["subscriber"].text();
    is_subscriber = (subscriber.toInt() == 1);

    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue("Subscriber", is_subscriber);
    qLog(Info) << lastfm::ws::Username << "Subscriber status:" << is_subscriber;
  }

  emit UpdatedSubscriberStatus(is_subscriber);
}

QUrl C3sImpService::FixupUrl(const QUrl& url) {
  QUrl ret;
  ret.setEncodedUrl(url.toEncoded().replace(
      "USERNAME", QUrl::toPercentEncoding(lastfm::ws::Username)));
  return ret;
}

QString C3sImpService::ErrorString(lastfm::ws::Error error) const {
  switch (error) {
    case lastfm::ws::InvalidService:
      return tr("Invalid service");
    case lastfm::ws::InvalidMethod:
      return tr("Invalid method");
    case lastfm::ws::AuthenticationFailed:
      return tr("Authentication failed");
    case lastfm::ws::InvalidFormat:
      return tr("Invalid format");
    case lastfm::ws::InvalidParameters:
      return tr("Invalid parameters");
    case lastfm::ws::InvalidResourceSpecified:
      return tr("Invalid resource specified");
    case lastfm::ws::OperationFailed:
      return tr("Operation failed");
    case lastfm::ws::InvalidSessionKey:
      return tr("Invalid session key");
    case lastfm::ws::InvalidApiKey:
      return tr("Invalid API key");
    case lastfm::ws::ServiceOffline:
      return tr("Service offline");
    case lastfm::ws::SubscribersOnly:
      return tr("This stream is for paid subscribers only");

    case lastfm::ws::TryAgainLater:
      return tr("Last.fm is currently busy, please try again in a few minutes");

    case lastfm::ws::NotEnoughContent:
      return tr("Not enough content");
    case lastfm::ws::NotEnoughMembers:
      return tr("Not enough members");
    case lastfm::ws::NotEnoughFans:
      return tr("Not enough fans");
    case lastfm::ws::NotEnoughNeighbours:
      return tr("Not enough neighbors");

    case lastfm::ws::MalformedResponse:
      return tr("Malformed response");

    case lastfm::ws::UnknownError:
    default:
      return tr("Unknown error");
  }
}

bool C3sImpService::InitScrobbler() {
  if (!IsAuthenticated() || !IsScrobblingEnabled()) return false;

  if (!scrobbler_)
    scrobbler_ = new lastfm::Audioscrobbler(kAudioscrobblerClientId);

// reemit the signal since the sender is private
#ifdef HAVE_LIBLASTFM1
  connect(scrobbler_, SIGNAL(scrobblesSubmitted(QList<lastfm::Track>)),
          SIGNAL(ScrobbleSubmitted()));
  connect(scrobbler_, SIGNAL(nowPlayingError(int, QString)),
          SIGNAL(ScrobbleError(int)));
#else
  connect(scrobbler_, SIGNAL(status(int)), SLOT(ScrobblerStatus(int)));
#endif
  return true;
}

void C3sImpService::ScrobblerStatus(int value) {
  switch (value) {
    case 2:
    case 3:
      emit ScrobbleSubmitted();
      break;

    default:
      emit ScrobbleError(value);
      break;
  }
}

lastfm::Track C3sImpService::TrackFromSong(const Song& song) const {
  if (song.title() == last_track_.title() &&
      song.artist() == last_track_.artist() &&
      song.album() == last_track_.album())
    return last_track_;

  lastfm::Track ret;
  song.ToLastFM(&ret, PreferAlbumArtist());
  return ret;
}

#define FINGERPRINT "DUMMY_FINGERPRINT"

void C3sImpService::RegisterClient() {
    QUrl url(kUrl);
    url.setPort(kPort);
    url.setPath(kRegister);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("User-Agent", "Clementine");
    req.setRawHeader("Accept-Language", "en-us");
    QByteArray postdata = "{ \"client_uuid\": \"" + QString("DUMMY_DEVICE_TOKEN").toUtf8() +
            "\", \"email\": \"" + QString("DUMMY_EMAIL").toUtf8() +
            "\", \"password\": \"" + QString("DUMMY_PASSWORD").toUtf8() +
            "\", \"player_name\": \"" + QString(kPluginVersion).toUtf8() +
            "\", \"player_version\": \"" + QCoreApplication::applicationVersion().toUtf8() +
            "\", \"plugin_vendor\": \"" + QString(kPluginVendor).toUtf8() +
            "\", \"plugin_name\": \"" + QString(kPluginName).toUtf8() +
            "\", \"plugin_version\": \"" + QString(kPluginVersion).toUtf8() + "\"  }";
    QNetworkReply* reply = network_->post(req, postdata);
    NewClosure(reply, SIGNAL(finished()), this, SLOT(RegisterClientReplyFinished(QNetworkReply*)), reply);

    qLog(Debug) << "Registering client";
}

void C3sImpService::NowPlaying(const Song& song) {

//!!!!!!!!!XXXXXXXXXXXXXXXXXX  if (!InitScrobbler()) return;

  // Scrobbling streams is difficult if we don't have length of each individual
  // part.  In Song::ToLastFm we set the Track's source to
  // NonPersonalisedBroadcast if it's such a stream, so we have to scrobble it
  // when we change to a different track, but only if enough time has elapsed
  // since it started playing.
  if (!last_track_.isNull() &&
      last_track_.source() == lastfm::Track::NonPersonalisedBroadcast) {
    const int duration_secs =
        last_track_.timestamp().secsTo(QDateTime::currentDateTime());
    if (duration_secs >= lastfm::compat::ScrobbleTimeMin()) {
      lastfm::MutableTrack mtrack(last_track_);
      mtrack.setDuration(duration_secs);

//      qLog(Info) << "Adoring stream track" << mtrack.title() << "length"
//                 << duration_secs;
//      scrobbler_->cache(mtrack);
//     scrobbler_->submit();

//      emit ScrobbledRadioStream();
    }
  }

  lastfm::MutableTrack mtrack(TrackFromSong(song));
  mtrack.stamp();
  already_scrobbled_ = false;
  last_track_ = mtrack;

  //scrobbler_->nowPlaying(mtrack);



  if (astreamer.GetLastFingerprint() != "")
  {
    QDateTime now = QDateTime::currentDateTime();
    QString now_string = now.toString("yyyy-MM-dd hh:mm:ss");
    QString host = QString(kUrl), port = QString(kPort);
    //qLog(Debug) << "--- the host: " + host;
    //qLog(Debug) << "--- the port: " + port;
    QSettings s;
    s.beginGroup(C3sImpService::kSettingsGroup);
    QString uuid = s.value("token").toString();
    QString temp_host = s.value("host").toString();
    //qLog(Debug) << "--- the temp_host: " + temp_host;
    if (temp_host != "") host = temp_host;
    if (host.length() < 4 || host.left(4) != "http") host = "http://" + host;
    QString temp_port = s.value("port").toString();
    //qLog(Debug) << "--- the temp_port: " + temp_port;
    if (temp_port != "") port = temp_port;
    QUrl url(host);
    url.setPort(port.toInt());
    url.setPath(kUtilizeImp);
    QNetworkRequest req(url);
    qLog(Debug) << "C3sImp request url: " + url.toString();
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
    qLog(Debug) << "C3sImp request body: " + QString(postdata);
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

    qLog(Debug) << "Adoring track " << mtrack.title() << " by " << mtrack.artist().toString() << "";

    astreamer.ResetLastFingerprint();
  }

  astreamer.StartProbing();
}

void C3sImpService::ProcessQueued()
{
    QStringList column_names;
    QStringList column_values;

    if (!adb.GetQueued(column_names, column_values))
    {   // no queued records pending? nothing to do.
        return;
    }

    QSettings s;
    s.beginGroup(C3sImpService::kSettingsGroup);
    QString uuid = s.value("token").toString();

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
    QUrl url(kUrl);
    url.setPort(kPort);
    url.setPath(kUtilizeImp);
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

#include <QMessageBox>
void C3sImpService::UtilizeImpReplyFinished(QNetworkReply* reply)
{
  int type = TYPE_UNKNOWN;
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NetworkError::NoError)
  {
    type = TYPE_ERROR;
    qLog(Debug) << "C3sImp server response #" << QString::number((int)reply->error()) << ":" << reply->errorString();
  }
  else
  {
    type = TYPE_SUCCESS;
    qLog(Debug) << "C3sImp server response body:" << reply->readAll();
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

  // finally append type (see #defines in c3simpdb.h)
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

void C3sImpService::RegisterClientReplyFinished(QNetworkReply* reply)
{
  reply->deleteLater();
};



void C3sImpService::Scrobble() {
  if (!InitScrobbler()) return;

  lastfm::compat::ScrobbleCache cache(lastfm::ws::Username);
  qLog(Debug) << "There are" << cache.tracks().count()
              << "tracks in the last.fm cache.";
  scrobbler_->cache(last_track_);

  // Let's mark a track as cached, useful when the connection is down
  emit ScrobbleError(30);
  scrobbler_->submit();

  already_scrobbled_ = true;
}

void C3sImpService::Love() {
  if (!IsAuthenticated()) ShowConfig();

  lastfm::MutableTrack mtrack(last_track_);
  mtrack.love();
  last_track_ = mtrack;

  if (already_scrobbled_) {
    // The love only takes effect when the song is scrobbled, but we've already
    // scrobbled this one so we have to do it again.
    Scrobble();
  }
}

void C3sImpService::Ban() {
  lastfm::MutableTrack mtrack(last_track_);
  mtrack.ban();
  last_track_ = mtrack;

  Scrobble();
  app_->player()->Next();
}

void C3sImpService::ToggleScrobbling() {
  // toggle status
  scrobbling_enabled_ = !scrobbling_enabled_;

  // save to the settings
  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("ScrobblingEnabled", scrobbling_enabled_);
  s.endGroup();

  emit ScrobblingEnabledChanged(scrobbling_enabled_);
}
