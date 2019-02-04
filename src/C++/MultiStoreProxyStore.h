#ifndef FIX_MULTISTOREPROXYSTORE_H
#define FIX_MULTISTOREPROXYSTORE_H

#ifdef _MSC_VER
#pragma warning( disable : 4503 4355 4786 4290 )
#endif

#include "Message.h"
#include "MessageStore.h"
#include <map>
#include <vector>
#include <string>

namespace FIX
{

/**
 * Creates a memory based implementation of MessageStore.
 *
 * This will lose all data on process termination. This class should only
 * be used for test applications, never in production.
 */
class MultiStoreProxyStoreFactory : public MessageStoreFactory
{
public:
  MessageStore* create( const SessionID& );
  void destroy( MessageStore* );
};

/**
 * Memory based implementation of MessageStore.
 *
 * This will lose all data on process terminition. This class should only
 * be used for test applications, never in production.
 */
class MultiStoreProxyStore : public MessageStore
{
public:
  MultiStoreProxyStore( const SessionID& sessionID ) :
    m_sessionID(sessionID), m_stores(), m_match_all(false) {}
  virtual ~MultiStoreProxyStore();

  void add(MessageStore* store)
  { m_stores.push_back( store ); }

  bool set( int, const std::string& ) throw ( IOException );
  void get( int, int, std::vector < std::string > & ) const throw ( IOException );

  int getNextSenderMsgSeqNum() const throw ( IOException )
  { return m_stores[0]->getNextSenderMsgSeqNum(); }
  int getNextTargetMsgSeqNum() const throw ( IOException )
  { return m_stores[0]->getNextTargetMsgSeqNum(); }
  void setNextSenderMsgSeqNum( int value ) throw ( IOException );
  void setNextTargetMsgSeqNum( int value ) throw ( IOException );
  void incrNextSenderMsgSeqNum() throw ( IOException );
  void incrNextTargetMsgSeqNum() throw ( IOException );

  void setCreationTime( const UtcTimeStamp& creationTime ) throw ( IOException );

  UtcTimeStamp getCreationTime() const throw ( IOException )
  { return m_stores[0]->getCreationTime(); }

  void reset() throw ( IOException );
  void refresh() throw ( IOException );

private:
  SessionID m_sessionID;
  std::vector<MessageStore*> m_stores;
  bool m_match_all;
};

}

#endif // FIX_MULTISTOREPROXYSTORE_H
