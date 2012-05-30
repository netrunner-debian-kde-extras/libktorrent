/***************************************************************************
 *   Copyright (C) 2005 by Joris Guisson                                   *
 *   joris.guisson@gmail.com                                               *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ***************************************************************************/
#include "rpcmsg.h"
#include <util/log.h>
#include <util/functions.h>
#include <bcodec/bnode.h>
#include <bcodec/bencoder.h>
#include "rpccall.h"
#include "rpcserver.h"
#include "dht.h"

using namespace bt;

namespace dht
{
	const QString TID = "t";
	const QString REQ = "q";
	const QString RSP = "r";
	const QString TYP = "y";
	const QString ARG = "a";
	const QString ERR_DHT = "e";

	
	MsgBase::Ptr MakeMsg(bt::BDictNode* dict);
	
	
	MsgBase::Ptr ParseReq(bt::BDictNode* dict)
	{
		BValueNode* vn = dict->getValue(REQ);
		BDictNode*	args = dict->getDict(ARG);
		if (!vn || !args)
			return MsgBase::Ptr();
		
		if (!args->getValue("id"))
			return MsgBase::Ptr();
		
		if (!dict->getValue(TID))
			return MsgBase::Ptr();
			
		Key id = Key(args->getValue("id")->data().toByteArray());
		QByteArray mtid_d = dict->getValue(TID)->data().toByteArray();
		if (mtid_d.size() == 0)
			return MsgBase::Ptr();
		Uint8 mtid = (Uint8)mtid_d.at(0);
		MsgBase::Ptr msg;
		
		QString str = vn->data().toString();
		if (str == "ping")
		{
			msg = MsgBase::Ptr(new PingReq(id));
		}
		else if (str == "find_node")
		{
			if (args->getValue("target"))
				msg = MsgBase::Ptr(new FindNodeReq(id,Key(args->getValue("target")->data().toByteArray())));
		}
		else if (str == "get_peers")
		{
			if (args->getValue("info_hash"))
				msg = MsgBase::Ptr(new GetPeersReq(id,Key(args->getValue("info_hash")->data().toByteArray())));
		}
		else if (str == "announce_peer")
		{
			if (args->getValue("info_hash") && args->getValue("port") && args->getValue("token"))
			{
				msg = MsgBase::Ptr(new AnnounceReq(id,
						Key(args->getValue("info_hash")->data().toByteArray()),
						args->getValue("port")->data().toInt(),
						Key(args->getValue("token")->data().toByteArray())));
			}
		}
		
		if (msg)
			msg->setMTID(mtid);
		
		return msg;
	}
	
	MsgBase::Ptr ParseRsp(bt::BDictNode* dict,dht::Method req_method,Uint8 mtid)
	{
		BDictNode* args = dict->getDict(RSP);
		if (!args || !args->getValue("id"))
			return MsgBase::Ptr();
		
		Key id = Key(args->getValue("id")->data().toByteArray());
		
		switch (req_method)
		{
			case PING : 
				return MsgBase::Ptr(new PingRsp(mtid,id));
			case FIND_NODE :
				if (!args->getValue("nodes") && !args->getList("nodes2"))
					return MsgBase::Ptr();
				else
				{
					QByteArray nodes;
					BValueNode* v = args->getValue("nodes");
					if (v)
						nodes = v->data().toByteArray();
					
					FindNodeRsp::Ptr rsp(new FindNodeRsp(mtid,id));
					rsp->setNodes(nodes);
					
					BListNode* l = args->getList("nodes2");
					if (l)
					{
						for (Uint32 i = 0;i < l->getNumChildren();i++)
							rsp->addNode(l->getByteArray(i));
					}
					return rsp;
				}
			case GET_PEERS :
				if (args->getValue("token"))
				{
					Key token = args->getValue("token")->data().toByteArray();
					QByteArray data;
					BListNode* vals = args->getList("values");
					DBItemList dbl;
					if (vals)
					{
						for (Uint32 i = 0;i < vals->getNumChildren();i++)
						{
							QByteArray d = vals->getByteArray(i);
							if (d.length() == 6) // IPv4
							{
								Uint16 port = bt::ReadUint16((const Uint8*)d.data(),4);
								Uint32 ip = bt::ReadUint32((const Uint8*)d.data(),0);
								net::Address addr(QHostAddress(ip),port);
								dbl.append(DBItem(addr));
							}
							else if (d.length() == 18)// IPv6
							{
								Uint16 port = bt::ReadUint16((const Uint8*)d.data(),16);
								Q_IPV6ADDR ip;
								memcpy(ip.c, d.data(), 16);
								net::Address addr(QHostAddress(ip), port);
								dbl.append(DBItem(addr));
							}
						}
						return MsgBase::Ptr(new GetPeersRsp(mtid,id,dbl,token));
					}
					else if (args->getValue("nodes") || args->getList("nodes2"))
					{
						BValueNode* v = args->getValue("nodes");
						if (v)
							data = v->data().toByteArray();
						
						GetPeersRsp::Ptr rsp(new GetPeersRsp(mtid,id,token));
						rsp->setNodes(data);
						BListNode* l = args->getList("nodes2");
						if (l)
						{
							for (Uint32 i = 0;i < l->getNumChildren();i++)
								rsp->addNode(l->getByteArray(i));
						}
						
						return rsp;
					}
					else
					{
						Out(SYS_DHT|LOG_DEBUG) << "No nodes or values in get_peers response" << endl;
						return MsgBase::Ptr();
					}
				}
				else
				{
					Out(SYS_DHT|LOG_DEBUG) << "No token in get_peers response" << endl;
				}
				break;
			case ANNOUNCE_PEER :
				return MsgBase::Ptr(new AnnounceRsp(mtid,id));
			default:
				return MsgBase::Ptr();
		}
		return MsgBase::Ptr();
	}

	MsgBase::Ptr ParseRsp(bt::BDictNode* dict,RPCServer* srv)
	{
		BDictNode*	args = dict->getDict(RSP);
		if (!args || !dict->getValue(TID))
		{
			Out(SYS_DHT|LOG_DEBUG) << "ParseRsp : args || !args->getValue(id) || !dict->getValue(TID)" << endl;
			return MsgBase::Ptr();
		}
			
		
		QByteArray ba = dict->getValue(TID)->data().toByteArray();
		// check for empty byte arrays should prevent 144416
		if (ba.size() == 0)
			return MsgBase::Ptr();
		
		Uint8 mtid = (Uint8)ba.at(0);
		// find the call
		Method method = srv->findMethod(mtid);
		if (method == NONE)
			return MsgBase::Ptr();
		else
			return ParseRsp(dict,method,mtid);
	}
	
	MsgBase::Ptr ParseErr(bt::BDictNode* dict)
	{
		BValueNode* vn = dict->getValue(RSP);
		BDictNode*	args = dict->getDict(ARG);
		if (!vn || !args || !args->getValue("id") || !dict->getValue(TID))
			return MsgBase::Ptr();
			
		Key id = Key(args->getValue("id")->data().toByteArray());
		QString mt_id = dict->getValue(TID)->data().toString();
		if (mt_id.length() == 0)
			return MsgBase::Ptr();

		Uint8 mtid = (char)mt_id.at(0).toLatin1();
		QString str = vn->data().toString();
		
		return MsgBase::Ptr(new ErrMsg(mtid,id,str));
	}
	
	
	MsgBase::Ptr MakeRPCMsg(bt::BDictNode* dict,RPCServer* srv)
	{
		BValueNode* vn = dict->getValue(TYP);
		if (!vn)
			return MsgBase::Ptr();
		
		if (vn->data().toString() == REQ)
		{
			return ParseReq(dict);
		}
		else if (vn->data().toString() == RSP)
		{
			return ParseRsp(dict,srv);
		}
		else if (vn->data().toString() == ERR_DHT)
		{
			return ParseErr(dict);
		}
		
		return MsgBase::Ptr();
	}
	
	MsgBase::Ptr MakeRPCMsgTest(bt::BDictNode* dict,dht::Method req_method)
	{
		BValueNode* vn = dict->getValue(TYP);
		if (!vn)
			return MsgBase::Ptr();
		
		if (vn->data().toString() == REQ)
		{
			return ParseReq(dict);
		}
		else if (vn->data().toString() == RSP)
		{
			return ParseRsp(dict,req_method,0);
		}
		else if (vn->data().toString() == ERR_DHT)
		{
			return ParseErr(dict);
		}
		
		return MsgBase::Ptr();
	}
	
	MsgBase::MsgBase(Uint8 mtid,Method m,Type type,const Key & id)
	: mtid(mtid),method(m),type(type),id(id) 
	{}
	
	MsgBase::~MsgBase()
	{}
	
	////////////////////////////////
	
	PingReq::PingReq(const Key & id) : MsgBase(0xFF,PING,REQ_MSG,id)
	{
	}
	
	PingReq::~PingReq()
	{}
		
	void PingReq::apply(DHT* dh_table,MsgBase::Ptr self)
	{
		dh_table->ping(self.dynamicCast<PingReq>());
	}
	
	void PingReq::print()
	{
		Out(SYS_DHT|LOG_DEBUG) << QString("REQ: %1 %2 : ping").arg(mtid).arg(id.toString()) << endl;
	}
	
	void PingReq::encode(QByteArray & arr) const
	{
		BEncoder enc(new BEncoderBufferOutput(arr));
		enc.beginDict();
		{
			enc.write(ARG); enc.beginDict();
			{
				enc.write(QString("id")); enc.write(id.getData(),20);
			}
			enc.end();
			enc.write(REQ); enc.write(QString("ping"));
			enc.write(TID); enc.write(&mtid,1);
			enc.write(TYP); enc.write(REQ);
		}
		enc.end();
	}
	
	////////////////////////////////
	
	FindNodeReq::FindNodeReq(const Key & id,const Key & target)
	: MsgBase(0xFF,FIND_NODE,REQ_MSG,id),target(target)
	{}
	
	FindNodeReq::~FindNodeReq()
	{}
		
	void FindNodeReq::apply(DHT* dh_table,MsgBase::Ptr self)
	{
		dh_table->findNode(self.dynamicCast<FindNodeReq>());
	}
	
	void FindNodeReq::print()
	{
		Out(SYS_DHT|LOG_NOTICE) << QString("REQ: %1 %2 : find_node %3")
				.arg(mtid).arg(id.toString()).arg(target.toString()) << endl;
	}
	
	void FindNodeReq::encode(QByteArray & arr) const
	{
		BEncoder enc(new BEncoderBufferOutput(arr));
		enc.beginDict();
		{
			enc.write(ARG); enc.beginDict();
			{
				enc.write(QString("id")); enc.write(id.getData(),20);
				enc.write(QString("target")); enc.write(target.getData(),20);
			}
			enc.end();
			enc.write(REQ); enc.write(QString("find_node"));
			enc.write(TID); enc.write(&mtid,1);
			enc.write(TYP); enc.write(REQ);
		}
		enc.end();
	}
	
	////////////////////////////////

	////////////////////////////////
	GetPeersReq::GetPeersReq(const Key & id,const Key & info_hash) 
		: MsgBase(0xFF,GET_PEERS,REQ_MSG,id),info_hash(info_hash)
	{}
	
	GetPeersReq::~GetPeersReq()
	{}
		
	void GetPeersReq::apply(DHT* dh_table,MsgBase::Ptr self)
	{
		dh_table->getPeers(self.dynamicCast<GetPeersReq>());
	}
	
	void GetPeersReq::print()
	{
		Out(SYS_DHT|LOG_DEBUG) << QString("REQ: %1 %2 : get_peers %3")
				.arg(mtid).arg(id.toString()).arg(info_hash.toString()) << endl;
	}
	
	void GetPeersReq::encode(QByteArray & arr) const
	{
		BEncoder enc(new BEncoderBufferOutput(arr));
		enc.beginDict();
		{
			enc.write(ARG); enc.beginDict();
			{
				enc.write(QString("id")); enc.write(id.getData(),20);
				enc.write(QString("info_hash")); enc.write(info_hash.getData(),20);
			}
			enc.end();
			enc.write(REQ); enc.write(QString("get_peers"));
			enc.write(TID); enc.write(&mtid,1);
			enc.write(TYP); enc.write(REQ);
		}
		enc.end();
	}
	
	////////////////////////////////
	
	AnnounceReq::AnnounceReq(const Key & id,const Key & info_hash,Uint16 port,const Key & token) 
	: GetPeersReq(id,info_hash),port(port),token(token)
	{
		method = dht::ANNOUNCE_PEER;
	}
	
	AnnounceReq::~AnnounceReq() {}
		
	void AnnounceReq::apply(DHT* dh_table,MsgBase::Ptr self)
	{
		dh_table->announce(self.dynamicCast<AnnounceReq>());
	}
	
	void AnnounceReq::print()
	{
		Out(SYS_DHT|LOG_DEBUG) << QString("REQ: %1 %2 : announce_peer %3 %4 %5")
				.arg(mtid).arg(id.toString()).arg(info_hash.toString())
				.arg(port).arg(token.toString()) << endl;
	}
	
	void AnnounceReq::encode(QByteArray & arr) const
	{
		BEncoder enc(new BEncoderBufferOutput(arr));
		enc.beginDict();
		{
			enc.write(ARG); enc.beginDict();
			{
				enc.write(QString("id")); enc.write(id.getData(),20);
				enc.write(QString("info_hash")); enc.write(info_hash.getData(),20);
				enc.write(QString("port")); enc.write((Uint32)port);
				enc.write(QString("token")); enc.write(token.getData(),20);
			}
			enc.end();
			enc.write(REQ); enc.write(QString("announce_peer"));
			enc.write(TID); enc.write(&mtid,1);
			enc.write(TYP); enc.write(REQ);
		}
		enc.end();
	}
	
	////////////////////////////////
	
	PingRsp::PingRsp(Uint8 mtid,const Key & id)
	: MsgBase(mtid,PING,RSP_MSG,id)
	{}
	
	PingRsp::~PingRsp() {}
		
	void PingRsp::apply(DHT* dh_table,MsgBase::Ptr self) 
	{
		dh_table->response(self.dynamicCast<PingRsp>());
	}
	
	void PingRsp::print()
	{
		Out(SYS_DHT|LOG_DEBUG) << QString("RSP: %1 %2 : ping")
					.arg(mtid).arg(id.toString()) << endl;
	}
	
	void PingRsp::encode(QByteArray & arr) const
	{
		BEncoder enc(new BEncoderBufferOutput(arr));
		enc.beginDict();
		{
			enc.write(RSP); enc.beginDict();
			{
				enc.write(QString("id")); enc.write(id.getData(),20);
			}
			enc.end();
			enc.write(TID); enc.write(&mtid,1);
			enc.write(TYP); enc.write(RSP);
		}
		enc.end();
	}
	
	////////////////////////////////
	
	PackedNodeContainer::PackedNodeContainer() {}
	PackedNodeContainer::~PackedNodeContainer() {}
	
	void PackedNodeContainer::addNode(const QByteArray & a)
	{
		if (a.size() == 26)
			nodes.append(a);
		else
			nodes2.append(a);
	}
	
	////////////////////////////////
	
	FindNodeRsp::FindNodeRsp(Uint8 mtid,const Key & id)
	: MsgBase(mtid,FIND_NODE,RSP_MSG,id)
	{}
	
	FindNodeRsp::~FindNodeRsp() {}
		
	void FindNodeRsp::apply(DHT* dh_table,MsgBase::Ptr self) 
	{
		dh_table->response(self.dynamicCast<FindNodeRsp>());
	}
	
	void FindNodeRsp::print()
	{
		Out(SYS_DHT|LOG_DEBUG) << QString("RSP: %1 %2 : find_node")
				.arg(mtid).arg(id.toString()) << endl;
	}
	
	void FindNodeRsp::encode(QByteArray & arr) const
	{
		BEncoder enc(new BEncoderBufferOutput(arr));
		enc.beginDict();
		{
			enc.write(RSP); enc.beginDict();
			{
				enc.write(QString("id")); enc.write(id.getData(),20);
				enc.write(QString("nodes")); enc.write(nodes);
				
				if (nodes2.count() > 0)
				{
					enc.write(QString("nodes2"));
					enc.beginList();
					foreach (const QByteArray & item,nodes2)
					{
						enc.write(item);
					}
					enc.end();
				}
			}
			enc.end();
			enc.write(TID); enc.write(&mtid,1);
			enc.write(TYP); enc.write(RSP);
		}
		enc.end();
	}
	
	////////////////////////////////
	
	GetPeersRsp::GetPeersRsp(Uint8 mtid,const Key & id,const Key & token)
	: MsgBase(mtid,dht::GET_PEERS,dht::RSP_MSG,id),token(token)
	{
	}
	
	GetPeersRsp::GetPeersRsp(Uint8 mtid,const Key & id,const DBItemList & values,const Key & token)
	: MsgBase(mtid,dht::GET_PEERS,dht::RSP_MSG,id),token(token),items(values)
	{}
		
	GetPeersRsp::~GetPeersRsp()
	{}
		
	void GetPeersRsp::apply(DHT* dh_table,MsgBase::Ptr self) 
	{
		dh_table->response(self.dynamicCast<GetPeersRsp>());
	}
	void GetPeersRsp::print()
	{
		Out(SYS_DHT|LOG_DEBUG) << QString("RSP: %1 %2 : get_peers(%3)")
				.arg(mtid).arg(id.toString()).arg(nodes.size() > 0 ? "nodes" : "values") << endl;
	}

	void GetPeersRsp::encode(QByteArray & arr) const
	{
		BEncoder enc(new BEncoderBufferOutput(arr));
		enc.beginDict();
		{
			enc.write(RSP); enc.beginDict();
			{
				enc.write(QString("id")); enc.write(id.getData(),20);
				if (nodes.size() > 0 || nodes2.count() > 0)
				{
					enc.write(QString("nodes")); enc.write(nodes);
					if (nodes2.count() > 0)
					{
						enc.write(QString("nodes2"));
						enc.beginList();
						foreach (const QByteArray & item,nodes2)
						{
							enc.write(item);
						}
						enc.end();
					}
				
					enc.write(QString("token")); enc.write(token.getData(),20);
				}
				else
				{
					enc.write(QString("token")); enc.write(token.getData(),20);
					enc.write(QString("values")); enc.beginList();
					DBItemList::const_iterator i = items.begin();
					while (i != items.end())
					{
						const DBItem & item = *i;
						Uint8 tmp[18];
						Uint32 b = item.pack(tmp);
						enc.write(tmp,b);
						i++;
					}
					enc.end();
				}
			}
			enc.end();
			enc.write(TID); enc.write(&mtid,1);
			enc.write(TYP); enc.write(RSP);
		}
		enc.end();
	}

	
	////////////////////////////////
	////////////////////////////////
	
	AnnounceRsp::AnnounceRsp(Uint8 mtid,const Key & id) : MsgBase(mtid,ANNOUNCE_PEER,RSP_MSG,id)
	{}
	
	AnnounceRsp::~AnnounceRsp(){}
	
	void AnnounceRsp::apply(DHT* dh_table,MsgBase::Ptr self)
	{
		dh_table->response(self.dynamicCast<AnnounceRsp>());
	}
	
	void AnnounceRsp::print()
	{
		Out(SYS_DHT|LOG_DEBUG) << QString("RSP: %1 %2 : announce_peer")
				.arg(mtid).arg(id.toString()) << endl;
	}
	
	void AnnounceRsp::encode(QByteArray & arr) const
	{
		BEncoder enc(new BEncoderBufferOutput(arr));
		enc.beginDict();
		{
			enc.write(RSP); enc.beginDict();
			{
				enc.write(QString("id")); enc.write(id.getData(),20);
			}
			enc.end();
			enc.write(TID); enc.write(&mtid,1);
			enc.write(TYP); enc.write(RSP);
		}
		enc.end();
	}
	
	
	////////////////////////////////
	
	ErrMsg::ErrMsg(Uint8 mtid,const Key & id,const QString & msg)
	: MsgBase(mtid,NONE,ERR_MSG,id),msg(msg)
	{}
	
	ErrMsg::~ErrMsg()
	{}
		
	void ErrMsg::apply(DHT* dh_table,MsgBase::Ptr self)
	{
		dh_table->error(self.dynamicCast<ErrMsg>());
	}
	
	void ErrMsg::print()
	{
		Out(SYS_DHT|LOG_NOTICE) << "ERR: " << mtid << " " << msg << endl;
	}
	
	void ErrMsg::encode(QByteArray & ) const
	{}
}