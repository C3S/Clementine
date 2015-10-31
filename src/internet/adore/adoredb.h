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
#ifndef ADOREDB_H
#define ADOREDB_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QDir>

#define ADOREDB_VERSION "101"   ///< the version for this code hadling db access

//! server response types as in column 'type' in the db, derived from the http codes, e.g. 200 = OK, 400 = Error
#define TYPE_UNKNOWN 0
#define TYPE_DELAY 1
#define TYPE_SUCCESS 2
#define TYPE_WARNING 3
#define TYPE_ERROR 4

/*! This class handles SQLite access to the queue file.
 *  The queue is used e.g. when no internet access is possible
 *  and utilizations have to be stored until they can be
 *  transferred when an internet connection is available again.
 *  All member functions except `LastError()` return `true`
 *  upon success.
 */
class AdoreDb : public QObject
{
public:
  AdoreDb();
  ~AdoreDb();

public:
  bool Open();
  bool Delete();
  QSqlError LastError();
  bool Add(QStringList &names, QStringList &values);
  bool GetQueued(QStringList &names, QStringList &values);

private:
  QSqlDatabase db;

  static const char* kFileName;   ///< the queue db filename is defined here.
  static const char* kLinuxPath;  ///< under Linux the extra Clementine config path (based on the home directory) is defined here.
};

#endif // ADOREDB_H
