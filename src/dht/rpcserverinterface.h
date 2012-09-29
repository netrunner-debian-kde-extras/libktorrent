/***************************************************************************
 *   Copyright (C) 2012 by                                                 *
 *   Joris Guisson <joris.guisson@gmail.com>                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 ***************************************************************************/

#ifndef DHT_RPCSERVERINTERFACE_H
#define DHT_RPCSERVERINTERFACE_H

#include <dht/rpcmsg.h>

namespace dht
{
	class RPCCall;

	/**
	 * Interface class for an RPCServer
	 */
	class RPCServerInterface
	{
	public:
		RPCServerInterface();
		virtual ~RPCServerInterface();
		
		/**
		 * Do a RPC call.
		 * @param msg The message to send
		 * @return The call object
		 */
		virtual RPCCall* doCall(RPCMsg::Ptr msg) = 0;
	};

}

#endif // DHT_RPCSERVERINTERFACE_H
