/*
 * ComputerControlClient.cpp - implementation of the ComputerControlClient class
 *
 * Copyright (c) 2017 Tobias Doerffel <tobydox/at/users/dot/sf/dot/net>
 *
 * This file is part of iTALC - http://italc.sourceforge.net
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include <QTcpSocket>
#include <QTimer>

#include "ComputerControlClient.h"
#include "ComputerControlServer.h"


ComputerControlClient::ComputerControlClient( ComputerControlServer* server, QTcpSocket* clientSocket, int vncServerPort, QObject* parent ) :
	VncProxyClient( clientSocket, vncServerPort, parent ),
	m_server( server ),
	m_serverProtocol( proxyClientSocket(), server->serverAuthenticationManager() ),
	m_clientProtocol( vncServerSocket() )
{
	m_serverProtocol.start();
	m_clientProtocol.start();
}



ComputerControlClient::~ComputerControlClient()
{
}



void ComputerControlClient::readFromServer()
{
	if( m_serverProtocol.state() != VncServerProtocol::Authenticated )
	{
		while( m_serverProtocol.read() )
		{
		}
	}
	else if( m_clientProtocol.state() == VncClientProtocol::Authenticated )
	{
		forwardAllDataToClient();
	}
	else
	{
		// try again as client connection is not yet ready and we can't forward data
		QTimer::singleShot( ProtocolRetryTime, this, &ComputerControlClient::readFromServer );
	}
}



void ComputerControlClient::readFromClient()
{
	if( m_clientProtocol.state() != VncClientProtocol::Authenticated )
	{
		while( m_clientProtocol.read() )
		{
		}
	}
	else if( m_serverProtocol.state() == VncServerProtocol::Authenticated )
	{
		while( proxyClientSocket()->bytesAvailable() > 0 && receiveMessage() )
		{
		}
	}
	else
	{
		// try again as server connection is not yet ready and we can't forward data
		QTimer::singleShot( ProtocolRetryTime, this, &ComputerControlClient::readFromClient );
	}
}



bool ComputerControlClient::receiveMessage()
{
	if( proxyClientSocket()->bytesAvailable() < 1 )
	{
		return false;
	}

	char messageType = 0;
	if( proxyClientSocket()->read( &messageType, sizeof(messageType) ) != sizeof(messageType) )
	{
		qCritical( "ComputerControlClient:::receiveMessage(): could not read message type - closing connection" );

		deleteLater();
		return false;
	}

	switch( messageType )
	{
	case rfbItalcCoreRequest:
		return m_server->handleCoreMessage( proxyClientSocket() );

	case rfbItalcFeatureRequest:
		return m_server->handleFeatureMessage( proxyClientSocket() );

	default:
		break;
	}

	vncServerSocket()->write( &messageType, sizeof(messageType) );
	forwardAllDataToServer();

	return true;
}