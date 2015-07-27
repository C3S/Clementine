/* This file is part of Clementine.
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

#include "c3simpdb.h"
#include "core/logging.h"
#include <QVariant>
#include <QSqlRecord>

C3sImpDb::C3sImpDb(QObject *parent)
{

}

C3sImpDb::~C3sImpDb()
{

}

const char* C3sImpDb::kFileName = ".C3SImp.sqlite";

bool C3sImpDb::Open()
{
  // Find QSLite driver
  db = QSqlDatabase::addDatabase("QSQLITE");

  #ifdef Q_OS_LINUX
  // NOTE: We have to store database file into user home folder in Linux
  QString path(QDir::home().path());
  path.append(QDir::separator()).append(kFileName);
  path = QDir::toNativeSeparators(path);
  db.setDatabaseName(path);
  #else
  // NOTE: File exists in the application private folder, in Symbian Qt implementation
  db.setDatabaseName(kFileName);
  #endif

  // Open database
  if (!db.open()) return false;

  // check for tables existing
  QStringList t = db.tables();
  if (!t.contains("config"))
  {
    QSqlQuery config_query("CREATE TABLE config (cfgkey TEXT PRIMARY KEY, cfgvalue TEXT)", db);
    config_query.exec();
    QSqlQuery version_adder_query("INSERT INTO config (cfgkey, cfgvalue) VALUES ('version', :ver)", db);
    version_adder_query.bindValue(":ver", QVariant(C3SIMPDB_VERSION));
    version_adder_query.exec();
    QSqlQuery log_query("CREATE TABLE log (time_played TEXT PRIMARY KEY, time_submitted TEXT, artist TEXT, title TEXT, release TEXT, track_number TEXT, duration TEXT, fingerprinting_algorithm TEXT, fingerprinting_version TEXT, fingerprint TEXT, status TEXT, type INTEGER)", db);
    log_query.exec();
  }
  else
  {
    QSqlQuery config_checker_query("SELECT cfgvalue FROM config WHERE cfgkey = 'version'", db);
    config_checker_query.exec();
    QSqlRecord r = config_checker_query.record();
    int version_col = r.indexOf("cfgvalue");
    if (version_col >= 0 && config_checker_query.next())
    {
      int db_version = config_checker_query.value(version_col).toInt(); // db version of the file
      int code_version = QString(C3SIMPDB_VERSION).toInt(); // db version this program code understands

      // This is just to clean pre-release db versions
      if (db_version <= 100) { db.close(); if (Delete()) return Open(); else return false; }

      if (db_version / 100 > code_version / 100)
          db.close(); // major version too new? don't write to it. Minor version above it: code should deal with newer database

      // first official db version is 101
      Q_ASSERT(code_version >= 101);

      // example how to update the database:
      //if (db_version < 102)
      {
          // modify database to comply with v1.01

          //QSqlQuery version_updater_query("UPDATE config SET cfgvalue='101' WHERE cfgkey='101'", db);
          //version_updater_query.exec();
      }
    }
  }

  return true;
}

QSqlError C3sImpDb::LastError()
{
  // If opening database has failed user can ask
  // error description by QSqlError::text()
  return db.lastError();
}

bool C3sImpDb::Delete()
{
  // Close database
  db.close();

  #ifdef Q_OS_LINUX
  // NOTE: We have to store the database file into user home folder in Linux
  QString path(QDir::home().path());
  path.append(QDir::separator()).append(kFileName);
  path = QDir::toNativeSeparators(path);
  return QFile::remove(path);
  #else
  // Remove created database binary file
  return QFile::remove(kFileName);
  #endif
}

bool C3sImpDb::Add(QStringList &names, QStringList &values)
{
  // first see if the song is alread in the database
  QString time_played;
  int played_pos = names.indexOf("time_played");
  if (played_pos >= 0)
      time_played = values[played_pos];

  // depending on this, create an INSERT or UPDATE sqlite command string
  QSqlQuery select_query("SELECT * FROM log WHERE time_played = '" + time_played + "'", db);
  if (!select_query.exec()) { qLog(Debug) << "C3sImpDb: preliminary query failed: " << LastError().text(); return false; }
  QSqlRecord r = select_query.record();
  QString query_string;
  if (!select_query.numRowsAffected())
    query_string = "INSERT INTO log (" + names.join(", ") + ") VALUES (:" + names.join(", :") + ")";
  else
  {
    query_string = "UPDATE log SET ";
    foreach (QString name, names)
    {
      int pos = names.indexOf(name);
      if (pos >= 0)
        query_string += name + "=:" + name + ", ";
    }
    query_string = query_string.left(query_string.size()-2); // remove ", "
    query_string += " WHERE time_played = '" + time_played + "'";
  }

  // do query
  QSqlQuery query;
  if (!query.prepare(query_string)) { qLog(Debug) << "C3sImpDb: Query preparation failed for '" << query_string << "': " << LastError().text(); return false; }
  foreach (QString name, names)
  {
    int pos = names.indexOf(name);
    if (pos >= 0)
      query.bindValue(":" + name, QVariant(values[pos]));
  }
  if (!query.exec())
  {
    qLog(Debug) << "C3sImp database update failed for '" + query_string + "': " << LastError().text();
    return false;
  }
  return true;
}

// gets the next record not marked as TYPE_SUCCESS
bool C3sImpDb::GetQueued(QStringList &names, QStringList &values)
{
    QSqlQuery select_query("SELECT * FROM log WHERE type <> '" + QString::number(TYPE_SUCCESS) + "'", db);
    select_query.exec();
    QSqlRecord r = select_query.record();

    if (!select_query.next()) return false;

    for (int i = 0; i < r.count(); ++i )
    {
        names.append(r.fieldName(i));
        values.append(select_query.value(i).toString());
    }

    return true;
}

