#ifndef FIX_ZMQDEALERFORWARDERSTORE_H
#define FIX_ZMQDEALERFORWARDERSTORE_H

#include "zmq.hpp"

#ifdef _MSC_VER
#pragma warning( disable : 4503 4355 4786 4290 )
#endif

#include "Message.h"
#include "MessageStore.h"
#include "SessionSettings.h"
#include <map>
#include <vector>
#include <string>

namespace FIX
{

/**
 * Creates a ZMQ MessageStore.
 *
 */
class ZMQDealerForwarderStoreFactory : public MessageStoreFactory
{
public:
  ZMQDealerForwarderStoreFactory( const SessionSettings& settings )
      : m_settings( settings ) {}
  MessageStore* create( const SessionID& );
  void destroy( MessageStore* );
private:
    const SessionSettings& m_settings;
};

/**
 * MessageStore that forwards messages to a remote ZMQ Router.
 *
 */
class ZMQDealerForwarderStore : public MessageStore
{
public:
  ZMQDealerForwarderStore( const SessionID& sessionID, const std::string& url );
  virtual ~ZMQDealerForwarderStore();

  bool set( int, const std::string& ) throw ( IOException );
  void get( int, int, std::vector < std::string > & ) const throw ( IOException );

  int getNextSenderMsgSeqNum() const throw ( IOException )
  { return m_nextSenderMsgSeqNum; }
  int getNextTargetMsgSeqNum() const throw ( IOException )
  { return m_nextTargetMsgSeqNum; }
  void setNextSenderMsgSeqNum( int value ) throw ( IOException );
  void setNextTargetMsgSeqNum( int value ) throw ( IOException );
  void incrNextSenderMsgSeqNum() throw ( IOException );
  void incrNextTargetMsgSeqNum() throw ( IOException );

  void setCreationTime( const UtcTimeStamp& creationTime ) throw ( IOException );
  UtcTimeStamp getCreationTime() const throw ( IOException )
  { return m_creationTime; }

  void reset() throw ( IOException );
  void refresh() throw ( IOException );

private:
  SessionID m_sessionID;
  std::string m_url;
  std::string m_sessionid_string;

  int m_nextSenderMsgSeqNum;
  int m_nextTargetMsgSeqNum;
  UtcTimeStamp m_creationTime;

  zmq::context_t m_zmq_context;
  zmq::socket_t m_socket;
  uint64_t m_request_counter;
};

}

#endif // FIX_ZMQDEALERFORWARDERSTORE_H
