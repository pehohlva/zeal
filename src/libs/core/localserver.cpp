/****************************************************************************
**
** Copyright (C) 2015-2016 Oleg Shparber
** Contact: https://go.zealdocs.org/l/contact
**
** This file is part of Zeal.
**
** Zeal is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** Zeal is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Zeal. If not, see <https://www.gnu.org/licenses/>.
**
****************************************************************************/

#include "localserver.h"

#include <registry/searchquery.h>

#include <QDataStream>
#include <QLocalServer>
#include <QLocalSocket>

using namespace Zeal;
using namespace Zeal::Core;

namespace {
const char LocalServerName[] = "ZealLocalServer";
}

LocalServer::LocalServer(QObject *parent)
    : QObject(parent)
    , m_localServer(new QLocalServer(this))
{
    connect(m_localServer, &QLocalServer::newConnection, [this] {
        QLocalSocket *socket = m_localServer->nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, [this, socket] {
            Registry::SearchQuery query;
            bool preventActivation;

            QDataStream in(socket);
            in >> query;
            in >> preventActivation;

            emit newQuery(query, preventActivation);
            socket->deleteLater();
        });
    });
}

/*!
 * \internal
 * \brief Returns the error message about the last occured error.
 * \return Human-readable error message, or an empty string.
 */
QString LocalServer::errorString() const
{
    return m_localServer->errorString();
}

/*!
 * \internal
 * \brief Instructs server to listen for incoming connections.
 * \param force If \c true, an attempt to remove socket file will be made. No effect on Windows.
 * \return \c true if successful, \c false otherwise.
 */
bool LocalServer::start(bool force)
{
#ifndef Q_OS_WIN32
    if (force && !QLocalServer::removeServer(LocalServerName)) {
        return false;
    }
#else
    Q_UNUSED(force)
#endif

    return m_localServer->listen(LocalServerName);
}

/*!
 * \internal
 * \brief Sends \a query to an already running application instance, if it exists.
 * \param query A query to execute search with.
 * \param preventActivation If \c true, application window will not activated.
 * \return \c true if communication with another instance has been successful.
 */
bool LocalServer::sendQuery(const Registry::SearchQuery &query, bool preventActivation)
{
    QByteArray ba;
    QDataStream out(&ba, QIODevice::WriteOnly);
    out << query << preventActivation;

    QLocalSocket *socket = new QLocalSocket();
    connect(socket, &QLocalSocket::connected, [socket, ba] {
        socket->write(ba);
        socket->flush(); // Required for Linux
        socket->deleteLater();
    });

    socket->connectToServer(LocalServerName);
    return socket->waitForConnected(500);
}
